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

import android.graphics.Point;
import android.opengl.Matrix;
import android.view.View;

public class RendererCommon {

  public static interface RendererEvents {

    public void onFirstFrameRendered();

    public void onFrameResolutionChanged(int videoWidth, int videoHeight, int rotation);
  }

  public interface GlDrawer {

    void drawOes(int oesTextureId, int originalWidth, int originalHeight, int rotatedWidth, int rotatedHeight, float[] texMatrix, int frameWidth, int frameHeight,
        int viewportX, int viewportY, int viewportWidth, int viewportHeight, boolean blur);
    void drawRgb(int textureId, int originalWidth, int originalHeight, int rotatedWidth, int rotatedHeight, float[] texMatrix, int frameWidth, int frameHeight, int viewportX,
        int viewportY, int viewportWidth, int viewportHeight, boolean blur);
    void drawYuv(int[] yuvTextures, int originalWidth, int originalHeight, int rotatedWidth, int rotatedHeight, float[] texMatrix, int frameWidth, int frameHeight,
        int viewportX, int viewportY, int viewportWidth, int viewportHeight, boolean blur);

    void release();
  }

  public static class VideoLayoutMeasure {

    private float visibleFractionMatchOrientation =
        convertScalingTypeToVisibleFraction(ScalingType.SCALE_ASPECT_BALANCED);
    private float visibleFractionMismatchOrientation =
        convertScalingTypeToVisibleFraction(ScalingType.SCALE_ASPECT_BALANCED);

    public void setScalingType(ScalingType scalingType) {
      setScalingType(                                   scalingType,
                                                scalingType);
    }

    public void setScalingType(
        ScalingType scalingTypeMatchOrientation, ScalingType scalingTypeMismatchOrientation) {
      this.visibleFractionMatchOrientation =
          convertScalingTypeToVisibleFraction(scalingTypeMatchOrientation);
      this.visibleFractionMismatchOrientation =
          convertScalingTypeToVisibleFraction(scalingTypeMismatchOrientation);
    }

    public void setVisibleFraction(
        float visibleFractionMatchOrientation, float visibleFractionMismatchOrientation) {
      this.visibleFractionMatchOrientation = visibleFractionMatchOrientation;
      this.visibleFractionMismatchOrientation = visibleFractionMismatchOrientation;
    }

    public Point measure(boolean applyRotation, int widthSpec, int heightSpec, int frameWidth, int frameHeight) {

      final int maxWidth = View.getDefaultSize(Integer.MAX_VALUE, widthSpec);
      final int maxHeight = View.getDefaultSize(Integer.MAX_VALUE, heightSpec);
      if (frameWidth == 0 || frameHeight == 0 || maxWidth == 0 || maxHeight == 0) {
        return new Point(maxWidth, maxHeight);
      }

      final float frameAspect = frameWidth / (float) frameHeight;
      final float displayAspect = maxWidth / (float) maxHeight;
      final float visibleFraction = (frameAspect > 1.0f) == (displayAspect > 1.0f)
          ? visibleFractionMatchOrientation
          : visibleFractionMismatchOrientation;
      final Point layoutSize = getDisplaySize(visibleFraction, frameAspect, maxWidth, maxHeight);

      if (!applyRotation) {
        if (View.MeasureSpec.getMode(widthSpec) == View.MeasureSpec.EXACTLY) {
          layoutSize.x = maxWidth;
        }
        if (View.MeasureSpec.getMode(heightSpec) == View.MeasureSpec.EXACTLY || frameAspect > 1.0f == displayAspect > 1.0f) {
          layoutSize.y = maxHeight;
        }
      }
      return layoutSize;
    }
  }

  public static enum ScalingType { SCALE_ASPECT_FIT, SCALE_ASPECT_FILL, SCALE_ASPECT_BALANCED }

  private static float BALANCED_VISIBLE_FRACTION = 0.5625f;

  public static float[] getLayoutMatrix(
      boolean mirror, float videoAspectRatio, float displayAspectRatio) {
    float scaleX = 1;
    float scaleY = 1;

    if (displayAspectRatio > videoAspectRatio) {
      scaleY = videoAspectRatio / displayAspectRatio;
    } else {
      scaleX = displayAspectRatio / videoAspectRatio;
    }

    if (mirror) {
      scaleX *= -1;
    }
    final float matrix[] = new float[16];
    Matrix.setIdentityM(matrix, 0);
    Matrix.scaleM(matrix, 0, scaleX, scaleY, 1);
    adjustOrigin(matrix);
    return matrix;
  }

  public static android.graphics.Matrix convertMatrixToAndroidGraphicsMatrix(float[] matrix4x4) {

    float[] values = {
        matrix4x4[0 * 4 + 0], matrix4x4[1 * 4 + 0], matrix4x4[3 * 4 + 0],
        matrix4x4[0 * 4 + 1], matrix4x4[1 * 4 + 1], matrix4x4[3 * 4 + 1],
        matrix4x4[0 * 4 + 3], matrix4x4[1 * 4 + 3], matrix4x4[3 * 4 + 3],
    };

    android.graphics.Matrix matrix = new android.graphics.Matrix();
    matrix.setValues(values);
    return matrix;
  }

  public static float[] convertMatrixFromAndroidGraphicsMatrix(android.graphics.Matrix matrix) {
    float[] values = new float[9];
    matrix.getValues(values);

    float[] matrix4x4 = {
        values[0 * 3 + 0],  values[1 * 3 + 0], 0,  values[2 * 3 + 0],
        values[0 * 3 + 1],  values[1 * 3 + 1], 0,  values[2 * 3 + 1],
        0,                  0,                 1,  0,
        values[0 * 3 + 2],  values[1 * 3 + 2], 0,  values[2 * 3 + 2],
    };

    return matrix4x4;
  }

  public static Point getDisplaySize(
      ScalingType scalingType, float videoAspectRatio, int maxDisplayWidth, int maxDisplayHeight) {
    return getDisplaySize(convertScalingTypeToVisibleFraction(scalingType), videoAspectRatio,
        maxDisplayWidth, maxDisplayHeight);
  }

  private static void adjustOrigin(float[] matrix) {

    matrix[12] -= 0.5f * (matrix[0] + matrix[4]);
    matrix[13] -= 0.5f * (matrix[1] + matrix[5]);

    matrix[12] += 0.5f;
    matrix[13] += 0.5f;
  }

  private static float convertScalingTypeToVisibleFraction(ScalingType scalingType) {
    switch (scalingType) {
      case SCALE_ASPECT_FIT:
        return 1.0f;
      case SCALE_ASPECT_FILL:
        return 0.0f;
      case SCALE_ASPECT_BALANCED:
        return BALANCED_VISIBLE_FRACTION;
      default:
        throw new IllegalArgumentException();
    }
  }

  public static Point getDisplaySize(
      float minVisibleFraction, float videoAspectRatio, int maxDisplayWidth, int maxDisplayHeight) {

    if (minVisibleFraction == 0 || videoAspectRatio == 0) {
      return new Point(maxDisplayWidth, maxDisplayHeight);
    }

    final int width = Math.min(
        maxDisplayWidth, Math.round(maxDisplayHeight / minVisibleFraction * videoAspectRatio));
    final int height = Math.min(
        maxDisplayHeight, Math.round(maxDisplayWidth / minVisibleFraction / videoAspectRatio));
    return new Point(width, height);
  }
}
