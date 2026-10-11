/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.media.MediaCodec;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaFormat;
import android.os.SystemClock;
import android.view.Surface;
import androidx.annotation.Nullable;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.BlockingDeque;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.TimeUnit;

import org.telegram.messenger.FileLog;
import org.webrtc.ThreadUtils.ThreadChecker;

class AndroidVideoDecoder implements VideoDecoder, VideoSink {
  private static final String TAG = "AndroidVideoDecoder";

  private static final int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000;

  private static final int DEQUEUE_INPUT_TIMEOUT_US = 500000;

  private static final int DEQUEUE_OUTPUT_BUFFER_TIMEOUT_US = 100000;

  private final MediaCodecWrapperFactory mediaCodecWrapperFactory;
  private final String codecName;
  private final VideoCodecMimeType codecType;

  private static class FrameInfo {
    final long decodeStartTimeMs;
    final int rotation;

    FrameInfo(long decodeStartTimeMs, int rotation) {
      this.decodeStartTimeMs = decodeStartTimeMs;
      this.rotation = rotation;
    }
  }

  private final BlockingDeque<FrameInfo> frameInfos;
  private int colorFormat;

  @Nullable private Thread outputThread;

  private ThreadChecker outputThreadChecker;

  private ThreadChecker decoderThreadChecker;

  private volatile boolean running;
  @Nullable private volatile Exception shutdownException;

  private final Object dimensionLock = new Object();
  private int width;
  private int height;
  private int stride;
  private int sliceHeight;

  private boolean hasDecodedFirstFrame;

  private boolean keyFrameRequired;

  private final @Nullable EglBase.Context sharedContext;

  @Nullable private SurfaceTextureHelper surfaceTextureHelper;
  @Nullable private Surface surface;

  private static class DecodedTextureMetadata {
    final long presentationTimestampUs;
    final Integer decodeTimeMs;

    DecodedTextureMetadata(long presentationTimestampUs, Integer decodeTimeMs) {
      this.presentationTimestampUs = presentationTimestampUs;
      this.decodeTimeMs = decodeTimeMs;
    }
  }

  private final Object renderedTextureMetadataLock = new Object();
  @Nullable private DecodedTextureMetadata renderedTextureMetadata;

  @Nullable private Callback callback;

  @Nullable private MediaCodecWrapper codec;

  AndroidVideoDecoder(MediaCodecWrapperFactory mediaCodecWrapperFactory, String codecName,
      VideoCodecMimeType codecType, int colorFormat, @Nullable EglBase.Context sharedContext) {
    if (!isSupportedColorFormat(colorFormat)) {
      throw new IllegalArgumentException("Unsupported color format: " + colorFormat);
    }
    Logging.d(TAG,
        "ctor name: " + codecName + " type: " + codecType + " color format: " + colorFormat
            + " context: " + sharedContext);
    this.mediaCodecWrapperFactory = mediaCodecWrapperFactory;
    this.codecName = codecName;
    this.codecType = codecType;
    this.colorFormat = colorFormat;
    this.sharedContext = sharedContext;
    this.frameInfos = new LinkedBlockingDeque<>();
  }

  @Override
  public VideoCodecStatus initDecode(Settings settings, Callback callback) {
    this.decoderThreadChecker = new ThreadChecker();

    this.callback = callback;
    if (sharedContext != null) {
      surfaceTextureHelper = createSurfaceTextureHelper();
      surface = new Surface(surfaceTextureHelper.getSurfaceTexture());
      surfaceTextureHelper.startListening(this);
    }
    return initDecodeInternal(settings.width, settings.height);
  }

  private VideoCodecStatus initDecodeInternal(int width, int height) {
    decoderThreadChecker.checkIsOnValidThread();
    Logging.d(TAG,
        "initDecodeInternal name: " + codecName + " type: " + codecType + " width: " + width
            + " height: " + height + " color format: " + colorFormat);
    if (outputThread != null) {
      Logging.e(TAG, "initDecodeInternal called while the codec is already running");
      return VideoCodecStatus.FALLBACK_SOFTWARE;
    }

    this.width = width;
    this.height = height;

    stride = width;
    sliceHeight = height;
    hasDecodedFirstFrame = false;
    keyFrameRequired = true;

    try {
      codec = mediaCodecWrapperFactory.createByCodecName(codecName);
    } catch (IOException | IllegalArgumentException | IllegalStateException e) {
      Logging.e(TAG, "Cannot create media decoder " + codecName);
      return VideoCodecStatus.FALLBACK_SOFTWARE;
    }
    try {
      MediaFormat format = MediaFormat.createVideoFormat(codecType.mimeType(), width, height);
      if (sharedContext == null) {
        format.setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat);
      }
      codec.configure(format, surface, null, 0);
      codec.start();
    } catch (IllegalStateException | IllegalArgumentException e) {
      Logging.e(TAG, "initDecode failed", e);
      release();
      return VideoCodecStatus.FALLBACK_SOFTWARE;
    }
    running = true;
    outputThread = createOutputThread();
    outputThread.start();

    Logging.d(TAG, "initDecodeInternal done");
    return VideoCodecStatus.OK;
  }

  @Override
  public VideoCodecStatus decode(EncodedImage frame, DecodeInfo info) {
    decoderThreadChecker.checkIsOnValidThread();
    if (codec == null || callback == null) {
      Logging.d(TAG, "decode uninitalized, codec: " + (codec != null) + ", callback: " + callback);
      return VideoCodecStatus.UNINITIALIZED;
    }

    if (frame.buffer == null) {
      Logging.e(TAG, "decode() - no input data");
      return VideoCodecStatus.ERR_PARAMETER;
    }

    int size = frame.buffer.remaining();
    if (size == 0) {
      Logging.e(TAG, "decode() - input buffer empty");
      return VideoCodecStatus.ERR_PARAMETER;
    }

    final int width;
    final int height;
    synchronized (dimensionLock) {
      width = this.width;
      height = this.height;
    }

    if (frame.encodedWidth * frame.encodedHeight > 0
        && (frame.encodedWidth != width || frame.encodedHeight != height)) {
      VideoCodecStatus status = reinitDecode(frame.encodedWidth, frame.encodedHeight);
      if (status != VideoCodecStatus.OK) {
        return status;
      }
    }

    if (keyFrameRequired) {

      if (frame.frameType != EncodedImage.FrameType.VideoFrameKey) {
        Logging.e(TAG, "decode() - key frame required first");
        return VideoCodecStatus.NO_OUTPUT;
      }
    }

    int index;
    try {
      index = codec.dequeueInputBuffer(DEQUEUE_INPUT_TIMEOUT_US);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "dequeueInputBuffer failed", e);
      return VideoCodecStatus.ERROR;
    }
    if (index < 0) {

      Logging.e(TAG, "decode() - no HW buffers available; decoder falling behind");
      return VideoCodecStatus.ERROR;
    }

    ByteBuffer buffer;
    try {
      buffer = codec.getInputBuffer(index);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "getInputBuffer with index=" + index + " failed", e);
      return VideoCodecStatus.ERROR;
    }

    if (buffer.capacity() < size) {
      Logging.e(TAG, "decode() - HW buffer too small");
      return VideoCodecStatus.ERROR;
    }
    buffer.put(frame.buffer);

    frameInfos.offer(new FrameInfo(SystemClock.elapsedRealtime(), frame.rotation));
    try {
      codec.queueInputBuffer(index, 0             , size,
          TimeUnit.NANOSECONDS.toMicros(frame.captureTimeNs), 0            );
    } catch (IllegalStateException e) {
      Logging.e(TAG, "queueInputBuffer failed", e);
      frameInfos.pollLast();
      return VideoCodecStatus.ERROR;
    }
    if (keyFrameRequired) {
      keyFrameRequired = false;
    }
    return VideoCodecStatus.OK;
  }

  @Override
  public String getImplementationName() {
    return codecName;
  }

  @Override
  public VideoCodecStatus release() {

    Logging.d(TAG, "release");
    VideoCodecStatus status = releaseInternal();
    if (surface != null) {
      releaseSurface();
      surface = null;
      surfaceTextureHelper.stopListening();
      surfaceTextureHelper.dispose();
      surfaceTextureHelper = null;
    }
    synchronized (renderedTextureMetadataLock) {
      renderedTextureMetadata = null;
    }
    callback = null;
    frameInfos.clear();
    return status;
  }

  private VideoCodecStatus releaseInternal() {
    if (!running) {
      Logging.d(TAG, "release: Decoder is not running.");
      return VideoCodecStatus.OK;
    }
    try {

      running = false;
      if (!ThreadUtils.joinUninterruptibly(outputThread, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {

        Logging.e(TAG, "Media decoder release timeout", new RuntimeException());
        return VideoCodecStatus.TIMEOUT;
      }
      if (shutdownException != null) {

        Logging.e(TAG, "Media decoder release error", new RuntimeException(shutdownException));
        shutdownException = null;
        return VideoCodecStatus.ERROR;
      }
    } finally {
      codec = null;
      outputThread = null;
    }
    return VideoCodecStatus.OK;
  }

  private VideoCodecStatus reinitDecode(int newWidth, int newHeight) {
    decoderThreadChecker.checkIsOnValidThread();
    VideoCodecStatus status = releaseInternal();
    if (status != VideoCodecStatus.OK) {
      return status;
    }
    return initDecodeInternal(newWidth, newHeight);
  }

  private Thread createOutputThread() {
    return new Thread("AndroidVideoDecoder.outputThread") {
      @Override
      public void run() {
        outputThreadChecker = new ThreadChecker();
        while (running) {
          deliverDecodedFrame();
        }
        releaseCodecOnOutputThread();
      }
    };
  }

  protected void deliverDecodedFrame() {
    outputThreadChecker.checkIsOnValidThread();
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();

      int index = codec.dequeueOutputBuffer(info, DEQUEUE_OUTPUT_BUFFER_TIMEOUT_US);
      if (index == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        reformat(codec.getOutputFormat());
        return;
      }

      if (index < 0) {
        Logging.v(TAG, "dequeueOutputBuffer returned " + index);
        return;
      }

      FrameInfo frameInfo = frameInfos.poll();
      Integer decodeTimeMs = null;
      int rotation = 0;
      if (frameInfo != null) {
        decodeTimeMs = (int) (SystemClock.elapsedRealtime() - frameInfo.decodeStartTimeMs);
        rotation = frameInfo.rotation;
      }

      hasDecodedFirstFrame = true;

      if (surfaceTextureHelper != null) {
        deliverTextureFrame(index, info, rotation, decodeTimeMs);
      } else {
        deliverByteFrame(index, info, rotation, decodeTimeMs);
      }

    } catch (IllegalStateException e) {
      Logging.e(TAG, "deliverDecodedFrame failed", e);
    }
  }

  private void deliverTextureFrame(final int index, final MediaCodec.BufferInfo info,
      final int rotation, final Integer decodeTimeMs) {

    final int width;
    final int height;
    synchronized (dimensionLock) {
      width = this.width;
      height = this.height;
    }

    synchronized (renderedTextureMetadataLock) {
      if (renderedTextureMetadata != null) {
        codec.releaseOutputBuffer(index, false);
        return;
      }
      surfaceTextureHelper.setTextureSize(width, height);
      surfaceTextureHelper.setFrameRotation(rotation);
      renderedTextureMetadata = new DecodedTextureMetadata(info.presentationTimeUs, decodeTimeMs);
      codec.releaseOutputBuffer(index,               true);
    }
  }

  @Override
  public void onFrame(VideoFrame frame) {
    final VideoFrame newFrame;
    final Integer decodeTimeMs;
    final long timestampNs;
    synchronized (renderedTextureMetadataLock) {
      if (renderedTextureMetadata == null) {
        throw new IllegalStateException(
            "Rendered texture metadata was null in onTextureFrameAvailable.");
      }
      timestampNs = renderedTextureMetadata.presentationTimestampUs * 1000;
      decodeTimeMs = renderedTextureMetadata.decodeTimeMs;
      renderedTextureMetadata = null;
    }

    final VideoFrame frameWithModifiedTimeStamp =
        new VideoFrame(frame.getBuffer(), frame.getRotation(), timestampNs);
    callback.onDecodedFrame(frameWithModifiedTimeStamp, decodeTimeMs, null         );
  }

  private void deliverByteFrame(
      int index, MediaCodec.BufferInfo info, int rotation, Integer decodeTimeMs) {

    int width;
    int height;
    int stride;
    int sliceHeight;
    synchronized (dimensionLock) {
      width = this.width;
      height = this.height;
      stride = this.stride;
      sliceHeight = this.sliceHeight;
    }

    if (info.size < width * height * 3 / 2) {
      Logging.e(TAG, "Insufficient output buffer size: " + info.size);
      return;
    }

    if (info.size < stride * height * 3 / 2 && sliceHeight == height && stride > width) {

      stride = info.size * 2 / (height * 3);
    }

    ByteBuffer buffer = codec.getOutputBuffer(index);
    buffer.position(info.offset);
    buffer.limit(info.offset + info.size);
    buffer = buffer.slice();

    final VideoFrame.Buffer frameBuffer;
    if (colorFormat == CodecCapabilities.COLOR_FormatYUV420Planar) {
      frameBuffer = copyI420Buffer(buffer, stride, sliceHeight, width, height);
    } else {

      frameBuffer = copyNV12ToI420Buffer(buffer, stride, sliceHeight, width, height);
    }
    codec.releaseOutputBuffer(index,               false);

    long presentationTimeNs = info.presentationTimeUs * 1000;
    VideoFrame frame = new VideoFrame(frameBuffer, rotation, presentationTimeNs);

    callback.onDecodedFrame(frame, decodeTimeMs, null         );
    frame.release();
  }

  private VideoFrame.Buffer copyNV12ToI420Buffer(
      ByteBuffer buffer, int stride, int sliceHeight, int width, int height) {

    return new NV12Buffer(width, height, stride, sliceHeight, buffer, null                      )
        .toI420();
  }

  private VideoFrame.Buffer copyI420Buffer(
      ByteBuffer buffer, int stride, int sliceHeight, int width, int height) {
    if (stride % 2 != 0) {
      throw new AssertionError("Stride is not divisible by two: " + stride);
    }

    final int chromaWidth = (width + 1) / 2;
    final int chromaHeight = (sliceHeight % 2 == 0) ? (height + 1) / 2 : height / 2;

    final int uvStride = stride / 2;

    final int yPos = 0;
    final int yEnd = yPos + stride * height;
    final int uPos = yPos + stride * sliceHeight;
    final int uEnd = uPos + uvStride * chromaHeight;
    final int vPos = uPos + uvStride * sliceHeight / 2;
    final int vEnd = vPos + uvStride * chromaHeight;

    VideoFrame.I420Buffer frameBuffer = allocateI420Buffer(width, height);
    try {
      buffer.limit(yEnd);
      buffer.position(yPos);
      copyPlane(
              buffer.slice(), stride, frameBuffer.getDataY(), frameBuffer.getStrideY(), width, height);

      buffer.limit(uEnd);
      buffer.position(uPos);
      copyPlane(buffer.slice(), uvStride, frameBuffer.getDataU(), frameBuffer.getStrideU(),
              chromaWidth, chromaHeight);
      if (sliceHeight % 2 == 1) {
        buffer.position(uPos + uvStride * (chromaHeight - 1));

        ByteBuffer dataU = frameBuffer.getDataU();
        dataU.position(frameBuffer.getStrideU() * chromaHeight);
        dataU.put(buffer);
      }

      buffer.limit(vEnd);
      buffer.position(vPos);
      copyPlane(buffer.slice(), uvStride, frameBuffer.getDataV(), frameBuffer.getStrideV(),
              chromaWidth, chromaHeight);
      if (sliceHeight % 2 == 1) {
        buffer.position(vPos + uvStride * (chromaHeight - 1));

        ByteBuffer dataV = frameBuffer.getDataV();
        dataV.position(frameBuffer.getStrideV() * chromaHeight);
        dataV.put(buffer);
      }
    } catch (Throwable e) {
      FileLog.e(e);
    }

    return frameBuffer;
  }

  private void reformat(MediaFormat format) {
    outputThreadChecker.checkIsOnValidThread();
    Logging.d(TAG, "Decoder format changed: " + format);
    final int newWidth;
    final int newHeight;
    if (format.containsKey(MediaFormat.KEY_CROP_LEFT)
        && format.containsKey(MediaFormat.KEY_CROP_RIGHT)
        && format.containsKey(MediaFormat.KEY_CROP_BOTTOM)
        && format.containsKey(MediaFormat.KEY_CROP_TOP)) {
      newWidth = 1 + format.getInteger(MediaFormat.KEY_CROP_RIGHT)
          - format.getInteger(MediaFormat.KEY_CROP_LEFT);
      newHeight = 1 + format.getInteger(MediaFormat.KEY_CROP_BOTTOM)
          - format.getInteger(MediaFormat.KEY_CROP_TOP);
    } else {
      newWidth = format.getInteger(MediaFormat.KEY_WIDTH);
      newHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
    }

    synchronized (dimensionLock) {
      if (newWidth != width || newHeight != height) {
        if (hasDecodedFirstFrame) {
          stopOnOutputThread(new RuntimeException("Unexpected size change. "
              + "Configured " + width + "*" + height + ". "
              + "New " + newWidth + "*" + newHeight));
          return;
        } else if (newWidth <= 0 || newHeight <= 0) {
          Logging.w(TAG,
              "Unexpected format dimensions. Configured " + width + "*" + height + ". "
                  + "New " + newWidth + "*" + newHeight + ". Skip it");
          return;
        }
        width = newWidth;
        height = newHeight;
      }
    }

    if (surfaceTextureHelper == null && format.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
      colorFormat = format.getInteger(MediaFormat.KEY_COLOR_FORMAT);
      Logging.d(TAG, "Color: 0x" + Integer.toHexString(colorFormat));
      if (!isSupportedColorFormat(colorFormat)) {
        stopOnOutputThread(new IllegalStateException("Unsupported color format: " + colorFormat));
        return;
      }
    }

    synchronized (dimensionLock) {
      if (format.containsKey(MediaFormat.KEY_STRIDE)) {
        stride = format.getInteger(MediaFormat.KEY_STRIDE);
      }
      if (format.containsKey(MediaFormat.KEY_SLICE_HEIGHT)) {
        sliceHeight = format.getInteger(MediaFormat.KEY_SLICE_HEIGHT);
      }
      Logging.d(TAG, "Frame stride and slice height: " + stride + " x " + sliceHeight);
      stride = Math.max(width, stride);
      sliceHeight = Math.max(height, sliceHeight);
    }
  }

  private void releaseCodecOnOutputThread() {
    outputThreadChecker.checkIsOnValidThread();
    Logging.d(TAG, "Releasing MediaCodec on output thread");
    try {
      codec.stop();
    } catch (Exception e) {
      Logging.e(TAG, "Media decoder stop failed", e);
    }
    try {
      codec.release();
    } catch (Exception e) {
      Logging.e(TAG, "Media decoder release failed", e);

      shutdownException = e;
    }
    Logging.d(TAG, "Release on output thread done");
  }

  private void stopOnOutputThread(Exception e) {
    outputThreadChecker.checkIsOnValidThread();
    running = false;
    shutdownException = e;
  }

  private boolean isSupportedColorFormat(int colorFormat) {
    for (int supported : MediaCodecUtils.DECODER_COLOR_FORMATS) {
      if (supported == colorFormat) {
        return true;
      }
    }
    return false;
  }

  protected SurfaceTextureHelper createSurfaceTextureHelper() {
    return SurfaceTextureHelper.create("decoder-texture-thread", sharedContext);
  }

  protected void releaseSurface() {
    surface.release();
  }

  protected VideoFrame.I420Buffer allocateI420Buffer(int width, int height) {
    return JavaI420Buffer.allocate(width, height);
  }

  protected void copyPlane(
      ByteBuffer src, int srcStride, ByteBuffer dst, int dstStride, int width, int height) {
    YuvHelper.copyPlane(src, srcStride, dst, dstStride, width, height);
  }
}
