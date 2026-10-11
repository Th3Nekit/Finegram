/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;
import org.webrtc.VideoFrame;
import org.webrtc.VideoProcessor;

class NativeAndroidVideoTrackSource {

  private final long nativeAndroidVideoTrackSource;

  public NativeAndroidVideoTrackSource(long nativeAndroidVideoTrackSource) {
    this.nativeAndroidVideoTrackSource = nativeAndroidVideoTrackSource;
  }

  public void setState(boolean isLive) {
    nativeSetState(nativeAndroidVideoTrackSource, isLive);
  }

  @Nullable
  public VideoProcessor.FrameAdaptationParameters adaptFrame(VideoFrame frame) {
    return nativeAdaptFrame(nativeAndroidVideoTrackSource, frame.getBuffer().getWidth(),
        frame.getBuffer().getHeight(), frame.getRotation(), frame.getTimestampNs());
  }

  public void onFrameCaptured(VideoFrame frame) {
    nativeOnFrameCaptured(nativeAndroidVideoTrackSource, frame.getRotation(),
        frame.getTimestampNs(), frame.getBuffer());
  }

  public void adaptOutputFormat(VideoSource.AspectRatio targetLandscapeAspectRatio,
      @Nullable Integer maxLandscapePixelCount, VideoSource.AspectRatio targetPortraitAspectRatio,
      @Nullable Integer maxPortraitPixelCount, @Nullable Integer maxFps) {
    nativeAdaptOutputFormat(nativeAndroidVideoTrackSource, targetLandscapeAspectRatio.width,
        targetLandscapeAspectRatio.height, maxLandscapePixelCount, targetPortraitAspectRatio.width,
        targetPortraitAspectRatio.height, maxPortraitPixelCount, maxFps);
  }

  public void setIsScreencast(boolean isScreencast) {
    nativeSetIsScreencast(nativeAndroidVideoTrackSource, isScreencast);
  }

  @CalledByNative
  static VideoProcessor.FrameAdaptationParameters createFrameAdaptationParameters(int cropX,
      int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight, long timestampNs,
      boolean drop) {
    return new VideoProcessor.FrameAdaptationParameters(
        cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight, timestampNs, drop);
  }

  private static native void nativeSetIsScreencast(
      long nativeAndroidVideoTrackSource, boolean isScreencast);
  private static native void nativeSetState(long nativeAndroidVideoTrackSource, boolean isLive);
  private static native void nativeAdaptOutputFormat(long nativeAndroidVideoTrackSource,
      int landscapeWidth, int landscapeHeight, @Nullable Integer maxLandscapePixelCount,
      int portraitWidth, int portraitHeight, @Nullable Integer maxPortraitPixelCount,
      @Nullable Integer maxFps);
  @Nullable
  private static native VideoProcessor.FrameAdaptationParameters nativeAdaptFrame(
      long nativeAndroidVideoTrackSource, int width, int height, int rotation, long timestampNs);
  private static native void nativeOnFrameCaptured(
      long nativeAndroidVideoTrackSource, int rotation, long timestampNs, VideoFrame.Buffer buffer);
}
