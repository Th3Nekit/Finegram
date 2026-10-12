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

import androidx.annotation.Nullable;
import org.webrtc.EncodedImage;

public interface VideoEncoder {

  public class Settings {
    public final int numberOfCores;
    public final int width;
    public final int height;
    public final int startBitrate;
    public final int maxFramerate;
    public final int numberOfSimulcastStreams;
    public final boolean automaticResizeOn;
    public final Capabilities capabilities;

    @Deprecated
    public Settings(int numberOfCores, int width, int height, int startBitrate, int maxFramerate,
        int numberOfSimulcastStreams, boolean automaticResizeOn) {
      this(numberOfCores, width, height, startBitrate, maxFramerate, numberOfSimulcastStreams,
          automaticResizeOn, new VideoEncoder.Capabilities(false                       ));
    }

    @CalledByNative("Settings")
    public Settings(int numberOfCores, int width, int height, int startBitrate, int maxFramerate,
        int numberOfSimulcastStreams, boolean automaticResizeOn, Capabilities capabilities) {
      this.numberOfCores = numberOfCores;
      this.width = width;
      this.height = height;
      this.startBitrate = startBitrate;
      this.maxFramerate = maxFramerate;
      this.numberOfSimulcastStreams = numberOfSimulcastStreams;
      this.automaticResizeOn = automaticResizeOn;
      this.capabilities = capabilities;
    }
  }

  public class Capabilities {

    public final boolean lossNotification;

    @CalledByNative("Capabilities")
    public Capabilities(boolean lossNotification) {
      this.lossNotification = lossNotification;
    }
  }

  public class EncodeInfo {
    public final EncodedImage.FrameType[] frameTypes;

    @CalledByNative("EncodeInfo")
    public EncodeInfo(EncodedImage.FrameType[] frameTypes) {
      this.frameTypes = frameTypes;
    }
  }

  public class CodecSpecificInfo {}

  public class CodecSpecificInfoVP8 extends CodecSpecificInfo {}

  public class CodecSpecificInfoVP9 extends CodecSpecificInfo {}

  public class CodecSpecificInfoH264 extends CodecSpecificInfo {}

  public class CodecSpecificInfoAV1 extends CodecSpecificInfo {}

  public class BitrateAllocation {

    public final int[][] bitratesBbs;

    @CalledByNative("BitrateAllocation")
    public BitrateAllocation(int[][] bitratesBbs) {
      this.bitratesBbs = bitratesBbs;
    }

    public int getSum() {
      int sum = 0;
      for (int[] spatialLayer : bitratesBbs) {
        for (int bitrate : spatialLayer) {
          sum += bitrate;
        }
      }
      return sum;
    }
  }

  public class ScalingSettings {
    public final boolean on;
    @Nullable public final Integer low;
    @Nullable public final Integer high;

    public static final ScalingSettings OFF = new ScalingSettings();

    public ScalingSettings(int low, int high) {
      this.on = true;
      this.low = low;
      this.high = high;
    }

    private ScalingSettings() {
      this.on = false;
      this.low = null;
      this.high = null;
    }

    @Deprecated
    public ScalingSettings(boolean on) {
      this.on = on;
      this.low = null;
      this.high = null;
    }

    @Deprecated
    public ScalingSettings(boolean on, int low, int high) {
      this.on = on;
      this.low = low;
      this.high = high;
    }

    @Override
    public String toString() {
      return on ? "[ " + low + ", " + high + " ]" : "OFF";
    }
  }

  public class ResolutionBitrateLimits {

    public final int frameSizePixels;

    public final int minStartBitrateBps;

    public final int minBitrateBps;

    public final int maxBitrateBps;

    public ResolutionBitrateLimits(
        int frameSizePixels, int minStartBitrateBps, int minBitrateBps, int maxBitrateBps) {
      this.frameSizePixels = frameSizePixels;
      this.minStartBitrateBps = minStartBitrateBps;
      this.minBitrateBps = minBitrateBps;
      this.maxBitrateBps = maxBitrateBps;
    }

    @CalledByNative("ResolutionBitrateLimits")
    public int getFrameSizePixels() {
      return frameSizePixels;
    }

    @CalledByNative("ResolutionBitrateLimits")
    public int getMinStartBitrateBps() {
      return minStartBitrateBps;
    }

    @CalledByNative("ResolutionBitrateLimits")
    public int getMinBitrateBps() {
      return minBitrateBps;
    }

    @CalledByNative("ResolutionBitrateLimits")
    public int getMaxBitrateBps() {
      return maxBitrateBps;
    }
  }

  public class RateControlParameters {

    public final BitrateAllocation bitrate;

    public final double framerateFps;

    @CalledByNative("RateControlParameters")
    public RateControlParameters(BitrateAllocation bitrate, double framerateFps) {
      this.bitrate = bitrate;
      this.framerateFps = framerateFps;
    }
  }

  public class EncoderInfo {

    public final int requestedResolutionAlignment;

    public final boolean applyAlignmentToAllSimulcastLayers;

    public EncoderInfo(
        int requestedResolutionAlignment, boolean applyAlignmentToAllSimulcastLayers) {
      this.requestedResolutionAlignment = requestedResolutionAlignment;
      this.applyAlignmentToAllSimulcastLayers = applyAlignmentToAllSimulcastLayers;
    }

    @CalledByNative("EncoderInfo")
    public int getRequestedResolutionAlignment() {
      return requestedResolutionAlignment;
    }

    @CalledByNative("EncoderInfo")
    public boolean getApplyAlignmentToAllSimulcastLayers() {
      return applyAlignmentToAllSimulcastLayers;
    }
  }

  public interface Callback {

    void onEncodedFrame(EncodedImage frame, CodecSpecificInfo info);
  }

  @CalledByNative
  default long createNativeVideoEncoder() {
    return 0;
  }

  @CalledByNative
  default boolean isHardwareEncoder() {
    return true;
  }

  @CalledByNative VideoCodecStatus initEncode(Settings settings, Callback encodeCallback);

  @CalledByNative VideoCodecStatus release();

  @CalledByNative VideoCodecStatus encode(VideoFrame frame, EncodeInfo info);

  VideoCodecStatus setRateAllocation(BitrateAllocation allocation, int framerate);

  default @CalledByNative VideoCodecStatus setRates(RateControlParameters rcParameters) {

    int framerateFps = (int) Math.ceil(rcParameters.framerateFps);
    return setRateAllocation(rcParameters.bitrate, framerateFps);
  }

  @CalledByNative ScalingSettings getScalingSettings();

  @CalledByNative
  default ResolutionBitrateLimits[] getResolutionBitrateLimits() {

    ResolutionBitrateLimits bitrate_limits[] = {};
    return bitrate_limits;
  }

  @CalledByNative String getImplementationName();

  @CalledByNative
  default EncoderInfo getEncoderInfo() {
    return new EncoderInfo(
                                            1,                                           false);
  }
}
