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

import android.graphics.Matrix;
import android.opengl.GLES20;
import java.nio.ByteBuffer;

import org.telegram.messenger.FileLog;
import org.webrtc.VideoFrame.I420Buffer;
import org.webrtc.VideoFrame.TextureBuffer;

public class YuvConverter {
  private static final String FRAGMENT_SHADER =

      "uniform vec2 xUnit;\n"

      + "uniform vec4 coeffs;\n"
      + "\n"
      + "void main() {\n"

      + "  gl_FragColor.r = coeffs.a + dot(coeffs.rgb,\n"
      + "      sample(tc - 1.5 * xUnit).rgb);\n"
      + "  gl_FragColor.g = coeffs.a + dot(coeffs.rgb,\n"
      + "      sample(tc - 0.5 * xUnit).rgb);\n"
      + "  gl_FragColor.b = coeffs.a + dot(coeffs.rgb,\n"
      + "      sample(tc + 0.5 * xUnit).rgb);\n"
      + "  gl_FragColor.a = coeffs.a + dot(coeffs.rgb,\n"
      + "      sample(tc + 1.5 * xUnit).rgb);\n"
      + "}\n";

  private static class ShaderCallbacks implements GlGenericDrawer.ShaderCallbacks {

    private static final float[] yCoeffs =
        new float[] {0.256788f, 0.504129f, 0.0979059f, 0.0627451f};
    private static final float[] uCoeffs =
        new float[] {-0.148223f, -0.290993f, 0.439216f, 0.501961f};
    private static final float[] vCoeffs =
        new float[] {0.439216f, -0.367788f, -0.0714274f, 0.501961f};

    private int xUnitLoc;
    private int coeffsLoc;

    private float[] coeffs;
    private float stepSize;

    public void setPlaneY() {
      coeffs = yCoeffs;
      stepSize = 1.0f;
    }

    public void setPlaneU() {
      coeffs = uCoeffs;
      stepSize = 2.0f;
    }

    public void setPlaneV() {
      coeffs = vCoeffs;
      stepSize = 2.0f;
    }

    @Override
    public void onNewShader(GlShader shader) {
      xUnitLoc = shader.getUniformLocation("xUnit");
      coeffsLoc = shader.getUniformLocation("coeffs");
    }

    @Override
    public void onPrepareShader(GlShader shader, float[] texMatrix, int frameWidth, int frameHeight,
        int viewportWidth, int viewportHeight) {
      GLES20.glUniform4fv(coeffsLoc,              1, coeffs,               0);

      GLES20.glUniform2f(
          xUnitLoc, stepSize * texMatrix[0] / frameWidth, stepSize * texMatrix[1] / frameWidth);
    }
  }

  private final ThreadUtils.ThreadChecker threadChecker = new ThreadUtils.ThreadChecker();
  private final GlTextureFrameBuffer i420TextureFrameBuffer =
      new GlTextureFrameBuffer(GLES20.GL_RGBA);
  private final ShaderCallbacks shaderCallbacks = new ShaderCallbacks();
  private final GlGenericDrawer drawer = new GlGenericDrawer(FRAGMENT_SHADER, shaderCallbacks);
  private final VideoFrameDrawer videoFrameDrawer;

  public YuvConverter() {
    this(new VideoFrameDrawer());
  }

  public YuvConverter(VideoFrameDrawer videoFrameDrawer) {
    this.videoFrameDrawer = videoFrameDrawer;
    threadChecker.detachThread();
  }

  public I420Buffer convert(TextureBuffer inputTextureBuffer) {
    threadChecker.checkIsOnValidThread();

    TextureBuffer preparedBuffer = (TextureBuffer) videoFrameDrawer.prepareBufferForViewportSize(
        inputTextureBuffer, inputTextureBuffer.getWidth(), inputTextureBuffer.getHeight());

    final int frameWidth = preparedBuffer.getWidth();
    final int frameHeight = preparedBuffer.getHeight();
    final int stride = ((frameWidth + 7) / 8) * 8;
    final int uvHeight = (frameHeight + 1) / 2;

    final int totalHeight = frameHeight + uvHeight;
    final ByteBuffer i420ByteBuffer = JniCommon.nativeAllocateByteBuffer(stride * totalHeight);

    final int viewportWidth = stride / 4;

    final Matrix renderMatrix = new Matrix();
    renderMatrix.preTranslate(0.5f, 0.5f);
    renderMatrix.preScale(1f, -1f);
    renderMatrix.preTranslate(-0.5f, -0.5f);

    try {
      i420TextureFrameBuffer.setSize(viewportWidth, totalHeight);

      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, i420TextureFrameBuffer.getFrameBufferId());
      GlUtil.checkNoGLES2Error("glBindFramebuffer");

      shaderCallbacks.setPlaneY();
      VideoFrameDrawer.drawTexture(drawer, preparedBuffer, renderMatrix, frameWidth, frameHeight, frameWidth, frameHeight,
                               0,                  0, viewportWidth,
                                    frameHeight, false);

      shaderCallbacks.setPlaneU();
      VideoFrameDrawer.drawTexture(drawer, preparedBuffer, renderMatrix, frameWidth, frameHeight, frameWidth, frameHeight,
                               0,                  frameHeight, viewportWidth / 2,
                                    uvHeight, false);

      shaderCallbacks.setPlaneV();
      VideoFrameDrawer.drawTexture(drawer, preparedBuffer, renderMatrix, frameWidth, frameHeight, frameWidth, frameHeight,
                               viewportWidth / 2,                  frameHeight, viewportWidth / 2,
                                    uvHeight, false);

      GLES20.glReadPixels(0, 0, i420TextureFrameBuffer.getWidth(), i420TextureFrameBuffer.getHeight(),
              GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, i420ByteBuffer);

      GlUtil.checkNoGLES2Error("YuvConverter.convert");

      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
    } catch (Exception e) {
      FileLog.e(e);
    }

    final int yPos = 0;
    final int uPos = yPos + stride * frameHeight;

    final int vPos = uPos + stride / 2;

    i420ByteBuffer.position(yPos);
    i420ByteBuffer.limit(yPos + stride * frameHeight);
    final ByteBuffer dataY = i420ByteBuffer.slice();

    i420ByteBuffer.position(uPos);

    final int uvSize = stride * (uvHeight - 1) + stride / 2;
    i420ByteBuffer.limit(uPos + uvSize);
    final ByteBuffer dataU = i420ByteBuffer.slice();

    i420ByteBuffer.position(vPos);
    i420ByteBuffer.limit(vPos + uvSize);
    final ByteBuffer dataV = i420ByteBuffer.slice();

    preparedBuffer.release();

    return JavaI420Buffer.wrap(frameWidth, frameHeight, dataY, stride, dataU, stride, dataV, stride,
        () -> { JniCommon.nativeFreeByteBuffer(i420ByteBuffer); });
  }

  public void release() {
    threadChecker.checkIsOnValidThread();
    drawer.release();
    i420TextureFrameBuffer.release();
    videoFrameDrawer.release();

    threadChecker.detachThread();
  }
}
