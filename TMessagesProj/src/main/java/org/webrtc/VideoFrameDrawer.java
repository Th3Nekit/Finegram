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

import android.graphics.Matrix;
import android.graphics.Point;
import android.opengl.GLES20;
import androidx.annotation.Nullable;
import java.nio.ByteBuffer;

public class VideoFrameDrawer {
  public static final String TAG = "VideoFrameDrawer";

  public static void drawTexture(RendererCommon.GlDrawer drawer, VideoFrame.TextureBuffer buffer,
      Matrix renderMatrix, int rotatedWidth, int rotatedHeight, int frameWidth, int frameHeight, int viewportX, int viewportY,
      int viewportWidth, int viewportHeight, boolean blur) {
    Matrix finalMatrix = new Matrix(buffer.getTransformMatrix());
    finalMatrix.preConcat(renderMatrix);
    float[] finalGlMatrix = RendererCommon.convertMatrixFromAndroidGraphicsMatrix(finalMatrix);
    switch (buffer.getType()) {
      case OES:
        drawer.drawOes(buffer.getTextureId(), buffer.getWidth(), buffer.getHeight(), rotatedWidth, rotatedHeight, finalGlMatrix, frameWidth, frameHeight, viewportX,
            viewportY, viewportWidth, viewportHeight, blur);
        break;
      case RGB:
        drawer.drawRgb(buffer.getTextureId(), buffer.getWidth(), buffer.getHeight(), rotatedWidth, rotatedHeight, finalGlMatrix, frameWidth, frameHeight, viewportX,
            viewportY, viewportWidth, viewportHeight, blur);
        break;
      default:
        throw new RuntimeException("Unknown texture type.");
    }
  }

  public void getRenderBufferBitmap(RendererCommon.GlDrawer drawer, int rotation, GlGenericDrawer.TextureCallback callback) {
    if (drawer instanceof GlGenericDrawer) {
      ((GlGenericDrawer) drawer).getRenderBufferBitmap(rotation, callback);
    }
  }

  private static class YuvUploader {

    @Nullable private ByteBuffer copyBuffer;
    @Nullable private int[] yuvTextures;

    @Nullable
    public int[] uploadYuvData(int width, int height, int[] strides, ByteBuffer[] planes) {
      final int[] planeWidths = new int[] {width, width / 2, width / 2};
      final int[] planeHeights = new int[] {height, height / 2, height / 2};

      int copyCapacityNeeded = 0;
      for (int i = 0; i < 3; ++i) {
        if (strides[i] > planeWidths[i]) {
          copyCapacityNeeded = Math.max(copyCapacityNeeded, planeWidths[i] * planeHeights[i]);
        }
      }

      if (copyCapacityNeeded > 0
          && (copyBuffer == null || copyBuffer.capacity() < copyCapacityNeeded)) {
        copyBuffer = ByteBuffer.allocateDirect(copyCapacityNeeded);
      }

      if (yuvTextures == null) {
        yuvTextures = new int[3];
        for (int i = 0; i < 3; i++) {
          yuvTextures[i] = GlUtil.generateTexture(GLES20.GL_TEXTURE_2D);
        }
      }

      for (int i = 0; i < 3; ++i) {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);

        final ByteBuffer packedByteBuffer;
        if (strides[i] == planeWidths[i]) {

          packedByteBuffer = planes[i];
        } else {
          YuvHelper.copyPlane(
              planes[i], strides[i], copyBuffer, planeWidths[i], planeWidths[i], planeHeights[i]);
          packedByteBuffer = copyBuffer;
        }
        GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_LUMINANCE, planeWidths[i],
            planeHeights[i], 0, GLES20.GL_LUMINANCE, GLES20.GL_UNSIGNED_BYTE, packedByteBuffer);
      }
      return yuvTextures;
    }

    @Nullable
    public int[] uploadFromBuffer(VideoFrame.I420Buffer buffer) {
      int[] strides = {buffer.getStrideY(), buffer.getStrideU(), buffer.getStrideV()};
      ByteBuffer[] planes = {buffer.getDataY(), buffer.getDataU(), buffer.getDataV()};
      return uploadYuvData(buffer.getWidth(), buffer.getHeight(), strides, planes);
    }

    @Nullable
    public int[] getYuvTextures() {
      return yuvTextures;
    }

    public void release() {
      copyBuffer = null;
      if (yuvTextures != null) {
        GLES20.glDeleteTextures(3, yuvTextures, 0);
        yuvTextures = null;
      }
    }
  }

  private static int distance(float x0, float y0, float x1, float y1) {
    return (int) Math.round(Math.hypot(x1 - x0, y1 - y0));
  }

  final static float[] srcPoints =
      new float[] {0f         , 0f         , 1f         , 0f         , 0f         , 1f         };
  private final float[] dstPoints = new float[6];
  private final Point renderSize = new Point();
  private int renderWidth;
  private int renderHeight;

  private void calculateTransformedRenderSize(
      int frameWidth, int frameHeight, @Nullable Matrix renderMatrix) {
    if (renderMatrix == null) {
      renderWidth = frameWidth;
      renderHeight = frameHeight;
      return;
    }

    renderMatrix.mapPoints(dstPoints, srcPoints);

    for (int i = 0; i < 3; ++i) {
      dstPoints[i * 2] *= frameWidth;
      dstPoints[i * 2 + 1] *= frameHeight;
    }

    renderWidth = distance(dstPoints[0], dstPoints[1], dstPoints[2], dstPoints[3]);
    renderHeight = distance(dstPoints[0], dstPoints[1], dstPoints[4], dstPoints[5]);
  }

  private final YuvUploader yuvUploader = new YuvUploader();

  @Nullable private VideoFrame lastI420Frame;
  private final Matrix renderMatrix = new Matrix();
  private final Matrix renderRotateMatrix = new Matrix();

  public void drawFrame(VideoFrame frame, RendererCommon.GlDrawer drawer) {
    drawFrame(frame, drawer, null                             );
  }

  public void drawFrame(
      VideoFrame frame, RendererCommon.GlDrawer drawer, Matrix additionalRenderMatrix) {
    drawFrame(frame, drawer, additionalRenderMatrix, 0                , 0                ,
        frame.getRotatedWidth(), frame.getRotatedHeight(), false, false);
  }

  public void drawFrame(VideoFrame frame, RendererCommon.GlDrawer drawer,
      @Nullable Matrix additionalRenderMatrix, int viewportX, int viewportY, int viewportWidth,
      int viewportHeight, boolean rotate, boolean blur) {
    final int width = rotate ? frame.getRotatedHeight() : frame.getRotatedWidth();
    final int height = rotate ? frame.getRotatedWidth() : frame.getRotatedHeight();
    calculateTransformedRenderSize(width, height, additionalRenderMatrix);
    if (renderWidth <= 0 || renderHeight <= 0) {
      Logging.w(TAG, "Illegal frame size: " + renderWidth + "x" + renderHeight);
      return;
    }

    final boolean isTextureFrame = frame.getBuffer() instanceof VideoFrame.TextureBuffer;
    renderMatrix.reset();
    renderMatrix.preTranslate(0.5f, 0.5f);
    if (!isTextureFrame) {
      renderMatrix.preScale(1f, -1f);
    }
    renderMatrix.preRotate(frame.getRotation());
    renderMatrix.preTranslate(-0.5f, -0.5f);
    if (additionalRenderMatrix != null) {
      renderMatrix.preConcat(additionalRenderMatrix);
    }

    if (isTextureFrame) {
      lastI420Frame = null;
      drawTexture(drawer, (VideoFrame.TextureBuffer) frame.getBuffer(), renderMatrix, frame.getRotatedWidth(), frame.getRotatedHeight(), renderWidth,
          renderHeight, viewportX, viewportY, viewportWidth, viewportHeight, blur);
    } else {

      if (frame != lastI420Frame) {
        lastI420Frame = frame;
        final VideoFrame.I420Buffer i420Buffer = frame.getBuffer().toI420();
        yuvUploader.uploadFromBuffer(i420Buffer);
        i420Buffer.release();
      }

      drawer.drawYuv(yuvUploader.getYuvTextures(), frame.getBuffer().getWidth(), frame.getBuffer().getHeight(), frame.getRotatedWidth(), frame.getRotatedHeight(),
          RendererCommon.convertMatrixFromAndroidGraphicsMatrix(renderMatrix), renderWidth,
          renderHeight, viewportX, viewportY, viewportWidth, viewportHeight, blur);
    }
  }

  public VideoFrame.Buffer prepareBufferForViewportSize(
      VideoFrame.Buffer buffer, int width, int height) {
    buffer.retain();
    return buffer;
  }

  public void release() {
    yuvUploader.release();
    lastI420Frame = null;
  }
}
