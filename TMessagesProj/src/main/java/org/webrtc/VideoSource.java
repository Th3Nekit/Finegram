/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;

public class VideoSource extends MediaSource {

  public static class AspectRatio {
    public static final AspectRatio UNDEFINED = new AspectRatio(             0,               0);

    public final int width;
    public final int height;

    public AspectRatio(int width, int height) {
      this.width = width;
      this.height = height;
    }
  }

  private final NativeAndroidVideoTrackSource nativeAndroidVideoTrackSource;
  private final Object videoProcessorLock = new Object();
  @Nullable private VideoProcessor videoProcessor;
  private boolean isCapturerRunning;

  private final CapturerObserver capturerObserver = new CapturerObserver() {
    @Override
    public void onCapturerStarted(boolean success) {
      nativeAndroidVideoTrackSource.setState(success);
      synchronized (videoProcessorLock) {
        isCapturerRunning = success;
        if (videoProcessor != null) {
          videoProcessor.onCapturerStarted(success);
        }
      }
    }

    @Override
    public void onCapturerStopped() {
      nativeAndroidVideoTrackSource.setState(              false);
      synchronized (videoProcessorLock) {
        isCapturerRunning = false;
        if (videoProcessor != null) {
          videoProcessor.onCapturerStopped();
        }
      }
    }

    @Override
    public void onFrameCaptured(VideoFrame frame) {
      final VideoProcessor.FrameAdaptationParameters parameters =
          nativeAndroidVideoTrackSource.adaptFrame(frame);
      synchronized (videoProcessorLock) {
        if (videoProcessor != null) {
          videoProcessor.onFrameCaptured(frame, parameters);
          return;
        }
      }

      VideoFrame adaptedFrame = VideoProcessor.applyFrameAdaptationParameters(frame, parameters);
      if (adaptedFrame != null) {
        nativeAndroidVideoTrackSource.onFrameCaptured(adaptedFrame);
        adaptedFrame.release();
      }
    }
  };

  public VideoSource(long nativeSource) {
    super(nativeSource);
    this.nativeAndroidVideoTrackSource = new NativeAndroidVideoTrackSource(nativeSource);
  }

  public void adaptOutputFormat(int width, int height, int fps) {
    final int maxSide = Math.max(width, height);
    final int minSide = Math.min(width, height);
    adaptOutputFormat(maxSide, minSide, minSide, maxSide, fps);
  }

  public void adaptOutputFormat(
      int landscapeWidth, int landscapeHeight, int portraitWidth, int portraitHeight, int fps) {
    adaptOutputFormat(new AspectRatio(landscapeWidth, landscapeHeight),
                                      landscapeWidth * landscapeHeight,
        new AspectRatio(portraitWidth, portraitHeight),
                                     portraitWidth * portraitHeight, fps);
  }

  public void adaptOutputFormat(AspectRatio targetLandscapeAspectRatio,
      @Nullable Integer maxLandscapePixelCount, AspectRatio targetPortraitAspectRatio,
      @Nullable Integer maxPortraitPixelCount, @Nullable Integer maxFps) {
    nativeAndroidVideoTrackSource.adaptOutputFormat(targetLandscapeAspectRatio,
        maxLandscapePixelCount, targetPortraitAspectRatio, maxPortraitPixelCount, maxFps);
  }

  public void setIsScreencast(boolean isScreencast) {
    nativeAndroidVideoTrackSource.setIsScreencast(isScreencast);
  }

  public void setVideoProcessor(@Nullable VideoProcessor newVideoProcessor) {
    synchronized (videoProcessorLock) {
      if (videoProcessor != null) {
        videoProcessor.setSink(            null);
        if (isCapturerRunning) {
          videoProcessor.onCapturerStopped();
        }
      }
      videoProcessor = newVideoProcessor;
      if (newVideoProcessor != null) {
        newVideoProcessor.setSink(
            (frame)
                -> runWithReference(() -> nativeAndroidVideoTrackSource.onFrameCaptured(frame)));
        if (isCapturerRunning) {
          newVideoProcessor.onCapturerStarted(               true);
        }
      }
    }
  }

  public CapturerObserver getCapturerObserver() {
    return capturerObserver;
  }

  long getNativeVideoTrackSource() {
    return getNativeMediaSource();
  }

  @Override
  public void dispose() {
    setVideoProcessor(                         null);
    super.dispose();
  }
}
