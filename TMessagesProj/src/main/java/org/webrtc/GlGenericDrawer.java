/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.graphics.Bitmap;
import android.opengl.GLES11Ext;
import android.opengl.GLES20;

import org.telegram.messenger.FileLog;

import androidx.annotation.Nullable;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

import javax.microedition.khronos.opengles.GL10;

public class GlGenericDrawer implements RendererCommon.GlDrawer {

  private static final int OES = 0;
  private static final int RGB = 1;
  private static final int YUV = 2;

  public interface ShaderCallbacks {

    void onNewShader(GlShader shader);

    void onPrepareShader(GlShader shader, float[] texMatrix, int frameWidth, int frameHeight,
        int viewportWidth, int viewportHeight);
  }

  private static final String INPUT_VERTEX_COORDINATE_NAME = "in_pos";
  private static final String INPUT_TEXTURE_COORDINATE_NAME = "in_tc";
  private static final String TEXTURE_MATRIX_NAME = "tex_mat";
  private static final String DEFAULT_VERTEX_SHADER_STRING = "varying vec2 tc;\n"
      + "attribute vec4 in_pos;\n"
      + "attribute vec4 in_tc;\n"
      + "uniform mat4 tex_mat;\n"
      + "void main() {\n"
      + "  gl_Position = in_pos;\n"
      + "  tc = (tex_mat * in_tc).xy;\n"
      + "}\n";

  private static final FloatBuffer FULL_RECTANGLE_BUFFER = GlUtil.createFloatBuffer(new float[] {
      -1.0f, -1.0f,
      1.0f, -1.0f,
      -1.0f, 1.0f,
      1.0f, 1.0f,
  });

  private static final FloatBuffer FULL_RECTANGLE_TEXTURE_BUFFER =
      GlUtil.createFloatBuffer(new float[] {
          0.0f, 0.0f,
          1.0f, 0.0f,
          0.0f, 1.0f,
          1.0f, 1.0f,
      });

  static String createFragmentShaderString(String genericFragmentSource, int shaderType, boolean blur) {
    final StringBuilder stringBuilder = new StringBuilder();
    if (shaderType == OES) {
      stringBuilder.append("#extension GL_OES_EGL_image_external : require\n");
    }
    stringBuilder.append("precision highp float;\n");
    if (!blur) {
      stringBuilder.append("varying vec2 tc;\n");
    }

    if (shaderType == YUV) {
      stringBuilder.append("uniform sampler2D y_tex;\n");
      stringBuilder.append("uniform sampler2D u_tex;\n");
      stringBuilder.append("uniform sampler2D v_tex;\n");

      stringBuilder.append("vec4 sample(vec2 p) {\n");
      stringBuilder.append("  float y = texture2D(y_tex, p).r * 1.16438;\n");
      stringBuilder.append("  float u = texture2D(u_tex, p).r;\n");
      stringBuilder.append("  float v = texture2D(v_tex, p).r;\n");
      stringBuilder.append("  return vec4(y + 1.59603 * v - 0.874202,\n");
      stringBuilder.append("    y - 0.391762 * u - 0.812968 * v + 0.531668,\n");
      stringBuilder.append("    y + 2.01723 * u - 1.08563, 1);\n");
      stringBuilder.append("}\n");
      stringBuilder.append(genericFragmentSource);
    } else {
      final String samplerName = shaderType == OES ? "samplerExternalOES" : "sampler2D";
      stringBuilder.append("uniform ").append(samplerName).append(" tex;\n");
      if (blur) {
        stringBuilder.append("precision mediump float;\n")
                .append("varying vec2 tc;\n")
                .append("const mediump vec3 satLuminanceWeighting = vec3(0.2126, 0.7152, 0.0722);\n")
                .append("uniform float texelWidthOffset;\n")
                .append("uniform float texelHeightOffset;\n")
                .append("void main(){\n")
                .append("int rad = 3;\n")
                .append("int diameter = 2 * rad + 1;\n")
                .append("vec4 sampleTex = vec4(0, 0, 0, 0);\n")
                .append("vec3 col = vec3(0, 0, 0);\n")
                .append("float weightSum = 0.0;\n")
                .append("for(int i = 0; i < diameter; i++) {\n")
                .append("vec2 offset = vec2(float(i - rad) * texelWidthOffset, float(i - rad) * texelHeightOffset);\n")
                .append("sampleTex = vec4(texture2D(tex, tc.st+offset));\n")
                .append("float index = float(i);\n")
                .append("float boxWeight = float(rad) + 1.0 - abs(index - float(rad));\n")
                .append("col += sampleTex.rgb * boxWeight;\n")
                .append("weightSum += boxWeight;\n")
                .append("}\n")
                .append("vec3 result = col / weightSum;\n")
                .append("lowp float satLuminance = dot(result.rgb, satLuminanceWeighting);\n")
                .append("lowp vec3 greyScaleColor = vec3(satLuminance);\n")
                .append("gl_FragColor = vec4(clamp(mix(greyScaleColor, result.rgb, 1.1), 0.0, 1.0), 1.0);\n")
                .append("}\n");
      } else {

        stringBuilder.append(genericFragmentSource.replace("sample(", "texture2D(tex, "));
      }
    }

    return stringBuilder.toString();
  }

  private final String genericFragmentSource;
  private final String vertexShader;
  private final ShaderCallbacks shaderCallbacks;
  @Nullable private GlShader[][] currentShader = new GlShader[3][3];
  private int[][] inPosLocation = new int[3][3];
  private int[][] inTcLocation = new int[3][3];
  private int[][] texMatrixLocation = new int[3][3];
  private int[][] texelLocation = new int[3][3];

  public GlGenericDrawer(String genericFragmentSource, ShaderCallbacks shaderCallbacks) {
    this(DEFAULT_VERTEX_SHADER_STRING, genericFragmentSource, shaderCallbacks);
  }

  public GlGenericDrawer(
      String vertexShader, String genericFragmentSource, ShaderCallbacks shaderCallbacks) {
    this.vertexShader = vertexShader;
    this.genericFragmentSource = genericFragmentSource;
    this.shaderCallbacks = shaderCallbacks;
  }

  GlShader createShader(int shaderType, boolean blur) {
    return new GlShader(vertexShader, createFragmentShaderString(genericFragmentSource, shaderType, blur));
  }

  private int[] renderTexture = new int[2];
  private int[] renderFrameBuffer;
  private float[] renderMatrix;

  private int[] renderTextureWidth = new int[2];
  private int[] renderTextureHeight = new int[2];
  private float[] textureMatrix;
  private float renderTextureDownscale;

  private void ensureRenderTargetCreated(int originalWidth, int originalHeight, int texIndex) {
    if (renderFrameBuffer == null) {
      renderFrameBuffer = new int[2];
      GLES20.glGenFramebuffers(2, renderFrameBuffer, 0);
      GLES20.glGenTextures(2, renderTexture, 0);
      for (int a = 0; a < renderTexture.length; a++) {
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, renderTexture[a]);
        GLES20.glTexParameteri(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MIN_FILTER, GL10.GL_LINEAR);
        GLES20.glTexParameteri(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_MAG_FILTER, GL10.GL_LINEAR);
        GLES20.glTexParameteri(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_S, GL10.GL_CLAMP_TO_EDGE);
        GLES20.glTexParameteri(GL10.GL_TEXTURE_2D, GL10.GL_TEXTURE_WRAP_T, GL10.GL_CLAMP_TO_EDGE);
      }
      renderMatrix = new float[16];
      android.opengl.Matrix.setIdentityM(renderMatrix, 0);
    }
    if (renderTextureWidth[texIndex] != originalWidth) {
      renderTextureDownscale = Math.max(1.0f, Math.max(originalWidth, originalHeight) / 50f);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, renderTexture[texIndex]);
      GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D, 0, GLES20.GL_RGBA, (int) (originalWidth / renderTextureDownscale), (int) (originalHeight / renderTextureDownscale), 0, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, null);
      renderTextureWidth[texIndex] = originalWidth;
      renderTextureHeight[texIndex] = originalHeight;
    }
  }
  public interface TextureCallback {
    void run(Bitmap bitmap, int rotation);
  }

  public void getRenderBufferBitmap(int baseRotation, TextureCallback callback) {
    if (renderFrameBuffer == null || textureMatrix == null) {
      callback.run(null, 0);
      return;
    }

    int rotation;
    double Ry = Math.asin(textureMatrix[2]);
    if (Ry < Math.PI / 2 && Ry > -Math.PI / 2) {
      rotation = (int) (-Math.atan(-textureMatrix[1] / textureMatrix[0]) / (Math.PI / 180));
    } else {
      rotation = baseRotation;
    }

    int viewportW = (int) (renderTextureWidth[0] / renderTextureDownscale);
    int viewportH = (int) (renderTextureHeight[0] / renderTextureDownscale);
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, renderFrameBuffer[0]);
    GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, renderTexture[0], 0);
    ByteBuffer buffer = ByteBuffer.allocateDirect(viewportW * viewportH * 4);
    GLES20.glReadPixels(0, 0, viewportW, viewportH, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, buffer);
    Bitmap bitmap = Bitmap.createBitmap(viewportW, viewportH, Bitmap.Config.ARGB_8888);
    bitmap.copyPixelsFromBuffer(buffer);
    GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);
    callback.run(bitmap, rotation);
  }

  @Override
  public void drawOes(int oesTextureId, int originalWidth, int originalHeight, int rotatedWidth, int rotatedHeight, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight, boolean blur) {
    if (blur) {
      ensureRenderTargetCreated(originalWidth, originalHeight, 1);

      textureMatrix = texMatrix;
      int viewportW = (int) (originalWidth / renderTextureDownscale);
      int viewportH = (int) (originalHeight / renderTextureDownscale);
      GLES20.glViewport(0, 0, viewportW, viewportH);
      prepareShader(OES, renderMatrix, rotatedWidth, rotatedHeight, frameWidth, frameHeight, viewportWidth, viewportHeight, 0);
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
      GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, oesTextureId);
      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, renderFrameBuffer[1]);
      GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, renderTexture[1], 0);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
      GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);

      if (rotatedWidth != originalWidth) {
        int temp = viewportW;
        viewportW = viewportH;
        viewportH = temp;
      }

      ensureRenderTargetCreated(originalWidth, originalHeight, 0);
      prepareShader(RGB, renderMatrix, rotatedWidth != originalWidth ? viewportH : viewportW, rotatedWidth != originalWidth ? viewportW : viewportH, frameWidth, frameHeight, viewportWidth, viewportHeight, 1);
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, renderTexture[1]);
      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, renderFrameBuffer[0]);
      GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, renderTexture[0], 0);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);

      GLES20.glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
      prepareShader(RGB, texMatrix, rotatedWidth != originalWidth ? viewportH : viewportW, rotatedWidth != originalWidth ? viewportW : viewportH, frameWidth, frameHeight, viewportWidth, viewportHeight, 2);
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, renderTexture[0]);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    } else {
      prepareShader(OES, texMatrix, rotatedWidth, rotatedHeight, frameWidth, frameHeight, viewportWidth, viewportHeight, 0);
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
      GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, oesTextureId);
      GLES20.glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
      GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
    }
  }

  @Override
  public void drawRgb(int textureId, int originalWidth, int originalHeight, int rotatedWidth, int rotatedHeight, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight, boolean blur) {
      prepareShader(RGB, texMatrix, rotatedWidth, rotatedHeight, frameWidth, frameHeight, viewportWidth, viewportHeight, 0);
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textureId);
      GLES20.glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
  }

  @Override
  public void drawYuv(int[] yuvTextures, int originalWidth, int originalHeight, int rotatedWidth, int rotatedHeight, float[] texMatrix, int frameWidth, int frameHeight,
      int viewportX, int viewportY, int viewportWidth, int viewportHeight, boolean blur) {
    if (blur && originalWidth > 0 && originalHeight > 0) {
      textureMatrix = texMatrix;
      ensureRenderTargetCreated(originalWidth, originalHeight, 1);

      int viewportW = (int) (originalWidth / renderTextureDownscale);
      int viewportH = (int) (originalHeight / renderTextureDownscale);

      GLES20.glViewport(0, 0, viewportW, viewportH);
      prepareShader(YUV, renderMatrix, rotatedWidth, rotatedHeight, frameWidth, frameHeight, viewportWidth, viewportHeight, 0);
      for (int i = 0; i < 3; ++i) {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
      }
      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, renderFrameBuffer[1]);
      GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, renderTexture[1], 0);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
      for (int i = 0; i < 3; ++i) {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
      }

      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);

      if (rotatedWidth != originalWidth) {
        int temp = viewportW;
        viewportW = viewportH;
        viewportH = temp;
      }

      ensureRenderTargetCreated(originalWidth, originalHeight, 0);
      prepareShader(RGB, renderMatrix, rotatedWidth != originalWidth ? viewportH : viewportW, rotatedWidth != originalWidth ? viewportW : viewportH, frameWidth, frameHeight, viewportWidth, viewportHeight, 1);
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, renderTexture[1]);
      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, renderFrameBuffer[0]);
      GLES20.glFramebufferTexture2D(GLES20.GL_FRAMEBUFFER, GLES20.GL_COLOR_ATTACHMENT0, GLES20.GL_TEXTURE_2D, renderTexture[0], 0);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
      GLES20.glBindFramebuffer(GLES20.GL_FRAMEBUFFER, 0);

      GLES20.glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
      prepareShader(RGB, texMatrix, rotatedWidth != originalWidth ? viewportH : viewportW, rotatedWidth != originalWidth ? viewportW : viewportH, frameWidth, frameHeight, viewportWidth, viewportHeight, 2);
      GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
      GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, renderTexture[0]);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
    } else {
      prepareShader(YUV, texMatrix, rotatedWidth, rotatedHeight, frameWidth, frameHeight, viewportWidth, viewportHeight, 0);
      for (int i = 0; i < 3; ++i) {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, yuvTextures[i]);
      }
      GLES20.glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
      GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
      for (int i = 0; i < 3; ++i) {
        GLES20.glActiveTexture(GLES20.GL_TEXTURE0 + i);
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, 0);
      }
    }
  }

  private void prepareShader(int shaderType, float[] texMatrix, int texWidth, int texHeight, int frameWidth,
      int frameHeight, int viewportWidth, int viewportHeight, int blurPass) {
    final GlShader shader;

    boolean blur = blurPass != 0;
    if (currentShader[shaderType][blurPass] != null) {
      shader = currentShader[shaderType][blurPass];
    } else {
      try {
        shader = createShader(shaderType, blur);
      } catch (Exception e) {
        FileLog.e(e);
        return;
      }
      currentShader[shaderType][blurPass] = shader;

      shader.useProgram();

      if (shaderType == YUV) {
        GLES20.glUniform1i(shader.getUniformLocation("y_tex"), 0);
        GLES20.glUniform1i(shader.getUniformLocation("u_tex"), 1);
        GLES20.glUniform1i(shader.getUniformLocation("v_tex"), 2);
      } else {
        GLES20.glUniform1i(shader.getUniformLocation("tex"), 0);
      }

      GlUtil.checkNoGLES2Error("Create shader");
      shaderCallbacks.onNewShader(shader);
      if (blur) {
        texelLocation[shaderType][0] = shader.getUniformLocation("texelWidthOffset");
        texelLocation[shaderType][1] = shader.getUniformLocation("texelHeightOffset");
      }
      texMatrixLocation[shaderType][blurPass] = shader.getUniformLocation(TEXTURE_MATRIX_NAME);
      inPosLocation[shaderType][blurPass] = shader.getAttribLocation(INPUT_VERTEX_COORDINATE_NAME);
      inTcLocation[shaderType][blurPass] = shader.getAttribLocation(INPUT_TEXTURE_COORDINATE_NAME);
    }

    shader.useProgram();

    if (blur) {
      GLES20.glUniform1f(texelLocation[shaderType][0], blurPass == 1 ? 1.0f / texWidth : 0);
      GLES20.glUniform1f(texelLocation[shaderType][1], blurPass == 2 ? 1.0f / texHeight : 0);
    }

    GLES20.glEnableVertexAttribArray(inPosLocation[shaderType][blurPass]);
    GLES20.glVertexAttribPointer(inPosLocation[shaderType][blurPass],             2,
                        GLES20.GL_FLOAT,                   false,               0,
            FULL_RECTANGLE_BUFFER);

    GLES20.glEnableVertexAttribArray(inTcLocation[shaderType][blurPass]);
    GLES20.glVertexAttribPointer(inTcLocation[shaderType][blurPass],             2,
                        GLES20.GL_FLOAT,                   false,               0,
            FULL_RECTANGLE_TEXTURE_BUFFER);

    GLES20.glUniformMatrix4fv(texMatrixLocation[shaderType][blurPass], 1             , false                 , texMatrix, 0              );

    shaderCallbacks.onPrepareShader(shader, texMatrix, frameWidth, frameHeight, viewportWidth, viewportHeight);
    GlUtil.checkNoGLES2Error("Prepare shader");
  }

  @Override
  public void release() {
    for (int a = 0; a < currentShader.length; a++) {
      for (int b = 0; b < currentShader[a].length; b++) {
        if (currentShader[a][b] != null) {
          currentShader[a][b].release();
          currentShader[a][b] = null;
        }
      }
    }
    if (renderFrameBuffer != null) {
      GLES20.glDeleteFramebuffers(2, renderFrameBuffer, 0);
      GLES20.glDeleteTextures(2, renderTexture, 0);
    }
  }
}
