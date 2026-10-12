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
import android.opengl.GLES11Ext;
import android.opengl.GLES20;
import androidx.annotation.Nullable;
import java.nio.ByteBuffer;

public class VideoFrame implements RefCounted {

  public interface Buffer extends RefCounted {

    default int getBufferType() {
      return 0;
    }

    @CalledByNative("Buffer") int getWidth();
    @CalledByNative("Buffer") int getHeight();

    @Nullable @CalledByNative("Buffer") I420Buffer toI420();

    @Override @CalledByNative("Buffer") void retain();
    @Override @CalledByNative("Buffer") void release();

    @CalledByNative("Buffer")
    Buffer cropAndScale(
        int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight);
  }

  public interface I420Buffer extends Buffer {
    @Override
    default int getBufferType() {
      return 1;
    }

    @CalledByNative("I420Buffer") ByteBuffer getDataY();

    @CalledByNative("I420Buffer") ByteBuffer getDataU();

    @CalledByNative("I420Buffer") ByteBuffer getDataV();

    @CalledByNative("I420Buffer") int getStrideY();
    @CalledByNative("I420Buffer") int getStrideU();
    @CalledByNative("I420Buffer") int getStrideV();
  }

  public interface TextureBuffer extends Buffer {
    enum Type {
      OES(GLES11Ext.GL_TEXTURE_EXTERNAL_OES),
      RGB(GLES20.GL_TEXTURE_2D);

      private final int glTarget;

      private Type(final int glTarget) {
        this.glTarget = glTarget;
      }

      public int getGlTarget() {
        return glTarget;
      }
    }

    Type getType();
    int getTextureId();

    Matrix getTransformMatrix();

    default TextureBuffer applyTransformMatrix(
        Matrix transformMatrix, int newWidth, int newHeight) {
      throw new UnsupportedOperationException("Not implemented");
    }

    default public int getUnscaledWidth() {
      return getWidth();
    }

    default public int getUnscaledHeight() {
      return getHeight();
    }
  }

  private final Buffer buffer;
  private final int rotation;
  private final long timestampNs;

  @CalledByNative
  public VideoFrame(Buffer buffer, int rotation, long timestampNs) {
    if (buffer == null) {
      throw new IllegalArgumentException("buffer not allowed to be null");
    }
    if (rotation % 90 != 0) {
      throw new IllegalArgumentException("rotation must be a multiple of 90");
    }
    this.buffer = buffer;
    this.rotation = rotation;
    this.timestampNs = timestampNs;
  }

  @CalledByNative
  public Buffer getBuffer() {
    return buffer;
  }

  @CalledByNative
  public int getRotation() {
    return rotation;
  }

  @CalledByNative
  public long getTimestampNs() {
    return timestampNs;
  }

  public int getRotatedWidth() {
    if (rotation % 180 == 0) {
      return buffer.getWidth();
    }
    return buffer.getHeight();
  }

  public int getRotatedHeight() {
    if (rotation % 180 == 0) {
      return buffer.getHeight();
    }
    return buffer.getWidth();
  }

  @Override
  public void retain() {
    buffer.retain();
  }

  @Override
  @CalledByNative
  public void release() {
    buffer.release();
  }
}
