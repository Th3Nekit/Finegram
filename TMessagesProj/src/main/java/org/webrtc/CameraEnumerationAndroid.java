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

import static java.lang.Math.abs;

import android.graphics.ImageFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

@SuppressWarnings("deprecation")
public class CameraEnumerationAndroid {
  private final static String TAG = "CameraEnumerationAndroid";

  static final ArrayList<Size> COMMON_RESOLUTIONS = new ArrayList<Size>(Arrays.asList(

      new Size(160, 120),
      new Size(240, 160),
      new Size(320, 240),
      new Size(400, 240),
      new Size(480, 320),
      new Size(640, 360),
      new Size(640, 480),
      new Size(768, 480),
      new Size(854, 480),
      new Size(800, 600),
      new Size(960, 540),
      new Size(960, 640),
      new Size(1024, 576),
      new Size(1024, 600),
      new Size(1280, 720),
      new Size(1280, 1024),
      new Size(1920, 1080),
      new Size(1920, 1440),
      new Size(2560, 1440),
      new Size(3840, 2160)
      ));

  public static class CaptureFormat {

    public static class FramerateRange {
      public int min;
      public int max;

      public FramerateRange(int min, int max) {
        this.min = min;
        this.max = max;
      }

      @Override
      public String toString() {
        return "[" + (min / 1000.0f) + ":" + (max / 1000.0f) + "]";
      }

      @Override
      public boolean equals(Object other) {
        if (!(other instanceof FramerateRange)) {
          return false;
        }
        final FramerateRange otherFramerate = (FramerateRange) other;
        return min == otherFramerate.min && max == otherFramerate.max;
      }

      @Override
      public int hashCode() {

        return 1 + 65537 * min + max;
      }
    }

    public final int width;
    public final int height;
    public final FramerateRange framerate;

    public final int imageFormat = ImageFormat.NV21;

    public CaptureFormat(int width, int height, int minFramerate, int maxFramerate) {
      this.width = width;
      this.height = height;
      this.framerate = new FramerateRange(minFramerate, maxFramerate);
    }

    public CaptureFormat(int width, int height, FramerateRange framerate) {
      this.width = width;
      this.height = height;
      this.framerate = framerate;
    }

    public int frameSize() {
      return frameSize(width, height, imageFormat);
    }

    public static int frameSize(int width, int height, int imageFormat) {
      if (imageFormat != ImageFormat.NV21) {
        throw new UnsupportedOperationException("Don't know how to calculate "
            + "the frame size of non-NV21 image formats.");
      }
      return (width * height * ImageFormat.getBitsPerPixel(imageFormat)) / 8;
    }

    @Override
    public String toString() {
      return width + "x" + height + "@" + framerate;
    }

    @Override
    public boolean equals(Object other) {
      if (!(other instanceof CaptureFormat)) {
        return false;
      }
      final CaptureFormat otherFormat = (CaptureFormat) other;
      return width == otherFormat.width && height == otherFormat.height
          && framerate.equals(otherFormat.framerate);
    }

    @Override
    public int hashCode() {
      return 1 + (width * 65497 + height) * 251 + framerate.hashCode();
    }
  }

  private static abstract class ClosestComparator<T> implements Comparator<T> {

    abstract int diff(T supportedParameter);

    @Override
    public int compare(T t1, T t2) {
      return diff(t1) - diff(t2);
    }
  }

  public static CaptureFormat.FramerateRange getClosestSupportedFramerateRange(
      List<CaptureFormat.FramerateRange> supportedFramerates, final int requestedFps) {
    return Collections.min(
        supportedFramerates, new ClosestComparator<CaptureFormat.FramerateRange>() {

          private static final int MAX_FPS_DIFF_THRESHOLD = 5000;
          private static final int MAX_FPS_LOW_DIFF_WEIGHT = 1;
          private static final int MAX_FPS_HIGH_DIFF_WEIGHT = 3;

          private static final int MIN_FPS_THRESHOLD = 8000;
          private static final int MIN_FPS_LOW_VALUE_WEIGHT = 1;
          private static final int MIN_FPS_HIGH_VALUE_WEIGHT = 4;

          private int progressivePenalty(int value, int threshold, int lowWeight, int highWeight) {
            return (value < threshold) ? value * lowWeight
                                       : threshold * lowWeight + (value - threshold) * highWeight;
          }

          @Override
          int diff(CaptureFormat.FramerateRange range) {
            final int minFpsError = progressivePenalty(
                range.min, MIN_FPS_THRESHOLD, MIN_FPS_LOW_VALUE_WEIGHT, MIN_FPS_HIGH_VALUE_WEIGHT);
            final int maxFpsError = progressivePenalty(Math.abs(requestedFps * 1000 - range.max),
                MAX_FPS_DIFF_THRESHOLD, MAX_FPS_LOW_DIFF_WEIGHT, MAX_FPS_HIGH_DIFF_WEIGHT);
            return minFpsError + maxFpsError;
          }
        });
  }

  public static Size getClosestSupportedSize(
      List<Size> supportedSizes, final int requestedWidth, final int requestedHeight) {
    return Collections.min(supportedSizes, new ClosestComparator<Size>() {
      @Override
      int diff(Size size) {
        return abs(requestedWidth - size.width) + abs(requestedHeight - size.height);
      }
    });
  }

  static void reportCameraResolution(Histogram histogram, Size resolution) {
    int index = COMMON_RESOLUTIONS.indexOf(resolution);

    histogram.addSample(index + 1);
  }
}
