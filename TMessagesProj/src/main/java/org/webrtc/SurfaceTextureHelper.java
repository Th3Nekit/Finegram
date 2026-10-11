/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.annotation.TargetApi;
import android.graphics.SurfaceTexture;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import androidx.annotation.Nullable;
import java.util.concurrent.Callable;
import org.webrtc.EglBase.Context;
import org.webrtc.TextureBufferImpl.RefCountMonitor;
import org.webrtc.VideoFrame.TextureBuffer;

public class SurfaceTextureHelper {

  public interface FrameRefMonitor {

    void onNewBuffer(TextureBuffer textureBuffer);

    void onRetainBuffer(TextureBuffer textureBuffer);

    void onReleaseBuffer(TextureBuffer textureBuffer);

    void onDestroyBuffer(TextureBuffer textureBuffer);
  }

  private static final String TAG = "SurfaceTextureHelper";

  public static SurfaceTextureHelper create(final String threadName,
                                            final EglBase.Context sharedContext, boolean alignTimestamps, final YuvConverter yuvConverter,
                                            FrameRefMonitor frameRefMonitor) {
    final HandlerThread thread = new HandlerThread(threadName);
    thread.start();
    final Handler handler = new Handler(thread.getLooper());

    return ThreadUtils.invokeAtFrontUninterruptibly(handler, new Callable<SurfaceTextureHelper>() {
      @Nullable
      @Override
      public SurfaceTextureHelper call() {
        try {
          return new SurfaceTextureHelper(
              sharedContext, handler, alignTimestamps, yuvConverter, frameRefMonitor);
        } catch (RuntimeException e) {
          Logging.e(TAG, threadName + " create failure", e);
          return null;
        }
      }
    });
  }

  public static SurfaceTextureHelper create(
      final String threadName, final EglBase.Context sharedContext) {
    return create(threadName, sharedContext,                        false, new YuvConverter(),
                            null);
  }

  public static SurfaceTextureHelper create(
      final String threadName, final EglBase.Context sharedContext, boolean alignTimestamps) {
    return create(
        threadName, sharedContext, alignTimestamps, new YuvConverter(),                     null);
  }

  public static SurfaceTextureHelper create(final String threadName,
                                            final EglBase.Context sharedContext, boolean alignTimestamps, YuvConverter yuvConverter) {
    return create(
        threadName, sharedContext, alignTimestamps, yuvConverter,                     null);
  }

  private final RefCountMonitor textureRefCountMonitor = new RefCountMonitor() {
    @Override
    public void onRetain(TextureBufferImpl textureBuffer) {
      if (frameRefMonitor != null) {
        frameRefMonitor.onRetainBuffer(textureBuffer);
      }
    }

    @Override
    public void onRelease(TextureBufferImpl textureBuffer) {
      if (frameRefMonitor != null) {
        frameRefMonitor.onReleaseBuffer(textureBuffer);
      }
    }

    @Override
    public void onDestroy(TextureBufferImpl textureBuffer) {
      returnTextureFrame();
      if (frameRefMonitor != null) {
        frameRefMonitor.onDestroyBuffer(textureBuffer);
      }
    }
  };

  private final Handler handler;
  private final EglBase eglBase;
  private final SurfaceTexture surfaceTexture;
  private final int oesTextureId;
  private final YuvConverter yuvConverter;
  @Nullable private final TimestampAligner timestampAligner;
  private final FrameRefMonitor frameRefMonitor;

  @Nullable private VideoSink listener;

  private boolean hasPendingTexture;
  private volatile boolean isTextureInUse;
  private boolean isQuitting;
  private int frameRotation;
  private int textureWidth;
  private int textureHeight;

  @Nullable private VideoSink pendingListener;
  final Runnable setListenerRunnable = new Runnable() {
    @Override
    public void run() {
      Logging.d(TAG, "Setting listener to " + pendingListener);
      listener = pendingListener;
      pendingListener = null;

      if (hasPendingTexture) {

        updateTexImage();
        hasPendingTexture = false;
      }
    }
  };

  private SurfaceTextureHelper(Context sharedContext, Handler handler, boolean alignTimestamps,
                               YuvConverter yuvConverter, FrameRefMonitor frameRefMonitor) {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("SurfaceTextureHelper must be created on the handler thread");
    }
    this.handler = handler;
    this.timestampAligner = alignTimestamps ? new TimestampAligner() : null;
    this.yuvConverter = yuvConverter;
    this.frameRefMonitor = frameRefMonitor;

    eglBase = EglBase.create(sharedContext, EglBase.CONFIG_PIXEL_BUFFER);
    try {

      eglBase.createDummyPbufferSurface();
      eglBase.makeCurrent();
    } catch (RuntimeException e) {

      eglBase.release();
      handler.getLooper().quit();
      throw e;
    }

    oesTextureId = GlUtil.generateTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
    surfaceTexture = new SurfaceTexture(oesTextureId);
    setOnFrameAvailableListener(surfaceTexture, (SurfaceTexture st) -> {
      if (hasPendingTexture) {
        Logging.d(TAG, "A frame is already pending, dropping frame.");
      }

      hasPendingTexture = true;
      tryDeliverTextureFrame();
    }, handler);
  }

  @TargetApi(21)
  private static void setOnFrameAvailableListener(SurfaceTexture surfaceTexture,
                                                  SurfaceTexture.OnFrameAvailableListener listener, Handler handler) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      surfaceTexture.setOnFrameAvailableListener(listener, handler);
    } else {

      surfaceTexture.setOnFrameAvailableListener(listener);
    }
  }

  public void startListening(final VideoSink listener) {
    if (this.listener != null || this.pendingListener != null) {
      throw new IllegalStateException("SurfaceTextureHelper listener has already been set.");
    }
    this.pendingListener = listener;
    handler.post(setListenerRunnable);
  }

  public void stopListening() {
    Logging.d(TAG, "stopListening()");
    handler.removeCallbacks(setListenerRunnable);
    ThreadUtils.invokeAtFrontUninterruptibly(handler, () -> {
      listener = null;
      pendingListener = null;
    });
  }

  public void setTextureSize(int textureWidth, int textureHeight) {
    if (textureWidth <= 0) {
      throw new IllegalArgumentException("Texture width must be positive, but was " + textureWidth);
    }
    if (textureHeight <= 0) {
      throw new IllegalArgumentException(
          "Texture height must be positive, but was " + textureHeight);
    }
    surfaceTexture.setDefaultBufferSize(textureWidth, textureHeight);
    handler.post(() -> {
      this.textureWidth = textureWidth;
      this.textureHeight = textureHeight;
      tryDeliverTextureFrame();
    });
  }

  public void forceFrame() {
    handler.post(() -> {
      hasPendingTexture = true;
      tryDeliverTextureFrame();
    });
  }

  public void setFrameRotation(int rotation) {
    handler.post(() -> this.frameRotation = rotation);
  }

  public SurfaceTexture getSurfaceTexture() {
    return surfaceTexture;
  }

  public Handler getHandler() {
    return handler;
  }

  private void returnTextureFrame() {
    handler.post(() -> {
      isTextureInUse = false;
      if (isQuitting) {
        release();
      } else {
        tryDeliverTextureFrame();
      }
    });
  }

  public boolean isTextureInUse() {
    return isTextureInUse;
  }

  public void dispose() {
    Logging.d(TAG, "dispose()");
    ThreadUtils.invokeAtFrontUninterruptibly(handler, () -> {
      isQuitting = true;
      if (!isTextureInUse) {
        release();
      }
    });
  }

  @Deprecated
  public VideoFrame.I420Buffer textureToYuv(final TextureBuffer textureBuffer) {
    return textureBuffer.toI420();
  }

  private void updateTexImage() {

    synchronized (EglBase.lock) {
      try {
        surfaceTexture.updateTexImage();
      } catch (Throwable ignore) {

      }
    }
  }

  private void tryDeliverTextureFrame() {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("Wrong thread.");
    }
    if (isQuitting || !hasPendingTexture || isTextureInUse || listener == null) {
      return;
    }
    if (textureWidth == 0 || textureHeight == 0) {

      Logging.w(TAG, "Texture size has not been set.");
      return;
    }
    isTextureInUse = true;
    hasPendingTexture = false;

    updateTexImage();

    final float[] transformMatrix = new float[16];
    surfaceTexture.getTransformMatrix(transformMatrix);
    long timestampNs = surfaceTexture.getTimestamp();
    if (timestampAligner != null) {
      timestampNs = timestampAligner.translateTimestamp(timestampNs);
    }
    final VideoFrame.TextureBuffer buffer =
        new TextureBufferImpl(textureWidth, textureHeight, TextureBuffer.Type.OES, oesTextureId,
            RendererCommon.convertMatrixToAndroidGraphicsMatrix(transformMatrix), handler,
            yuvConverter, textureRefCountMonitor);
    if (frameRefMonitor != null) {
      frameRefMonitor.onNewBuffer(buffer);
    }
    final VideoFrame frame = new VideoFrame(buffer, frameRotation, timestampNs);
    listener.onFrame(frame);
    frame.release();
  }

  private void release() {
    if (handler.getLooper().getThread() != Thread.currentThread()) {
      throw new IllegalStateException("Wrong thread.");
    }
    if (isTextureInUse || !isQuitting) {
      throw new IllegalStateException("Unexpected release.");
    }
    yuvConverter.release();
    GLES20.glDeleteTextures(1, new int[] {oesTextureId}, 0);
    surfaceTexture.release();
    eglBase.release();
    handler.getLooper().quit();
    if (timestampAligner != null) {
      timestampAligner.dispose();
    }
  }
}