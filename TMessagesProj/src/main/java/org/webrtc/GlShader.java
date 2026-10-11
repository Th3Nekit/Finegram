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

import android.opengl.GLES20;

import java.nio.FloatBuffer;

public class GlShader {
  private static final String TAG = "GlShader";

  private static int compileShader(int shaderType, String source) {
    final int shader = GLES20.glCreateShader(shaderType);
    if (shader == 0) {
      throw new RuntimeException("glCreateShader() failed. GLES20 error: " + GLES20.glGetError());
    }
    GLES20.glShaderSource(shader, source);
    GLES20.glCompileShader(shader);
    int[] compileStatus = new int[] {GLES20.GL_FALSE};
    GLES20.glGetShaderiv(shader, GLES20.GL_COMPILE_STATUS, compileStatus, 0);
    if (compileStatus[0] != GLES20.GL_TRUE) {
      Logging.e(
          TAG, "Compile error " + GLES20.glGetShaderInfoLog(shader) + " in shader:\n" + source);
      throw new RuntimeException(GLES20.glGetShaderInfoLog(shader));
    }
    GlUtil.checkNoGLES2Error("compileShader");
    return shader;
  }

  private int program;

  public GlShader(String vertexSource, String fragmentSource) {
    final int vertexShader = compileShader(GLES20.GL_VERTEX_SHADER, vertexSource);
    final int fragmentShader = compileShader(GLES20.GL_FRAGMENT_SHADER, fragmentSource);
    program = GLES20.glCreateProgram();
    if (program == 0) {
      throw new RuntimeException("glCreateProgram() failed. GLES20 error: " + GLES20.glGetError());
    }
    GLES20.glAttachShader(program, vertexShader);
    GLES20.glAttachShader(program, fragmentShader);
    GLES20.glLinkProgram(program);
    int[] linkStatus = new int[] {GLES20.GL_FALSE};
    GLES20.glGetProgramiv(program, GLES20.GL_LINK_STATUS, linkStatus, 0);
    if (linkStatus[0] != GLES20.GL_TRUE) {
      Logging.e(TAG, "Could not link program: " + GLES20.glGetProgramInfoLog(program));
      throw new RuntimeException(GLES20.glGetProgramInfoLog(program));
    }

    GLES20.glDeleteShader(vertexShader);
    GLES20.glDeleteShader(fragmentShader);
    GlUtil.checkNoGLES2Error("Creating GlShader");
  }

  public int getAttribLocation(String label) {
    if (program == -1) {
      throw new RuntimeException("The program has been released");
    }
    int location = GLES20.glGetAttribLocation(program, label);
    if (location < 0) {
      throw new RuntimeException("Could not locate '" + label + "' in program");
    }
    return location;
  }

  public void setVertexAttribArray(String label, int dimension, FloatBuffer buffer) {
    setVertexAttribArray(label, dimension, 0             , buffer);
  }

  public void setVertexAttribArray(String label, int dimension, int stride, FloatBuffer buffer) {
    if (program == -1) {
      throw new RuntimeException("The program has been released");
    }
    int location = getAttribLocation(label);
    GLES20.glEnableVertexAttribArray(location);
    GLES20.glVertexAttribPointer(location, dimension, GLES20.GL_FLOAT, false, stride, buffer);
    GlUtil.checkNoGLES2Error("setVertexAttribArray");
  }

  public int getUniformLocation(String label) {
    if (program == -1) {
      throw new RuntimeException("The program has been released");
    }
    int location = GLES20.glGetUniformLocation(program, label);
    if (location < 0) {
      throw new RuntimeException("Could not locate uniform '" + label + "' in program");
    }
    return location;
  }

  public void useProgram() {
    if (program == -1) {
      throw new RuntimeException("The program has been released");
    }
    synchronized (EglBase.lock) {
      GLES20.glUseProgram(program);
    }
    GlUtil.checkNoGLES2Error("glUseProgram");
  }

  public void release() {
    Logging.d(TAG, "Deleting shader.");

    if (program != -1) {
      GLES20.glDeleteProgram(program);
      program = -1;
    }
  }
}
