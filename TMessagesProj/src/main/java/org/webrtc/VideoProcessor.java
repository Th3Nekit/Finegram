/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;

public interface VideoProcessor extends CapturerObserver {
  public static class FrameAdaptationParameters {
    public final int cropX;
    public final int cropY;
    public final int cropWidth;
    public final int cropHeight;
    public final int scaleWidth;
    public final int scaleHeight;
    public final long timestampNs;
    public final boolean drop;

    public FrameAdaptationParameters(int cropX, int cropY, int cropWidth, int cropHeight,
        int scaleWidth, int scaleHeight, long timestampNs, boolean drop) {
      this.cropX = cropX;
      this.cropY = cropY;
      this.cropWidth = cropWidth;
      this.cropHeight = cropHeight;
      this.scaleWidth = scaleWidth;
      this.scaleHeight = scaleHeight;
      this.timestampNs = timestampNs;
      this.drop = drop;
    }
  }

  default void onFrameCaptured(VideoFrame frame, FrameAdaptationParameters parameters) {
    VideoFrame adaptedFrame = applyFrameAdaptationParameters(frame, parameters);
    if (adaptedFrame != null) {
      onFrameCaptured(adaptedFrame);
      adaptedFrame.release();
    }
  }

  void setSink(@Nullable VideoSink sink);

  public static @Nullable VideoFrame applyFrameAdaptationParameters(
      VideoFrame frame, FrameAdaptationParameters parameters) {
    if (parameters.drop) {
      return null;
    }

    final VideoFrame.Buffer adaptedBuffer =
        frame.getBuffer().cropAndScale(parameters.cropX, parameters.cropY, parameters.cropWidth,
            parameters.cropHeight, parameters.scaleWidth, parameters.scaleHeight);
    return new VideoFrame(adaptedBuffer, frame.getRotation(), parameters.timestampNs);
  }
}
