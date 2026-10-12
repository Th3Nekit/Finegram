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

import static org.webrtc.MediaCodecUtils.EXYNOS_PREFIX;
import static org.webrtc.MediaCodecUtils.INTEL_PREFIX;
import static org.webrtc.MediaCodecUtils.QCOM_PREFIX;

import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.os.Build;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@SuppressWarnings("deprecation")
public class HardwareVideoEncoderFactory implements VideoEncoderFactory {
  private static final String TAG = "HardwareVideoEncoderFactory";

  private static final int PERIODIC_KEY_FRAME_INTERVAL_S = 3600;

  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_L_MS = 15000;
  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_M_MS = 20000;
  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_N_MS = 15000;

  private static final List<String> H264_HW_EXCEPTION_MODELS =
      Arrays.asList("SAMSUNG-SGH-I337", "Nexus 7", "Nexus 4");

  @Nullable private final EglBase14.Context sharedContext;
  private final boolean enableIntelVp8Encoder;
  private final boolean enableH264HighProfile;
  @Nullable private final Predicate<MediaCodecInfo> codecAllowedPredicate;

  public HardwareVideoEncoderFactory(
      EglBase.Context sharedContext, boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this(sharedContext, enableIntelVp8Encoder, enableH264HighProfile,
                                     null);
  }

  public HardwareVideoEncoderFactory(EglBase.Context sharedContext, boolean enableIntelVp8Encoder,
      boolean enableH264HighProfile, @Nullable Predicate<MediaCodecInfo> codecAllowedPredicate) {

    if (sharedContext instanceof EglBase14.Context) {
      this.sharedContext = (EglBase14.Context) sharedContext;
    } else {
      Logging.w(TAG, "No shared EglBase.Context.  Encoders will not use texture mode.");
      this.sharedContext = null;
    }
    this.enableIntelVp8Encoder = enableIntelVp8Encoder;
    this.enableH264HighProfile = enableH264HighProfile;
    this.codecAllowedPredicate = codecAllowedPredicate;
  }

  @Deprecated
  public HardwareVideoEncoderFactory(boolean enableIntelVp8Encoder, boolean enableH264HighProfile) {
    this(null, enableIntelVp8Encoder, enableH264HighProfile);
  }

  @Nullable
  @Override
  public VideoEncoder createEncoder(VideoCodecInfo input) {
    VideoCodecMimeType type = VideoCodecMimeType.valueOf(input.getName());
    MediaCodecInfo info = findCodecForType(type);

    if (info == null) {
      return null;
    }

    String codecName = info.getName();
    String mime = type.mimeType();
    Integer surfaceColorFormat = MediaCodecUtils.selectColorFormat(
        MediaCodecUtils.TEXTURE_COLOR_FORMATS, info.getCapabilitiesForType(mime));
    Integer yuvColorFormat = MediaCodecUtils.selectColorFormat(
        MediaCodecUtils.ENCODER_COLOR_FORMATS, info.getCapabilitiesForType(mime));

    if (type == VideoCodecMimeType.H264) {
      boolean isHighProfile = H264Utils.isSameH264Profile(
          input.params, MediaCodecUtils.getCodecProperties(type,                    true));
      boolean isBaselineProfile = H264Utils.isSameH264Profile(
          input.params, MediaCodecUtils.getCodecProperties(type,                    false));

      if (!isHighProfile && !isBaselineProfile) {
        return null;
      }
      if (isHighProfile && !isH264HighProfileSupported(info)) {
        return null;
      }
    }

    return new HardwareVideoEncoder(new MediaCodecWrapperFactoryImpl(), codecName, type,
        surfaceColorFormat, yuvColorFormat, input.params, PERIODIC_KEY_FRAME_INTERVAL_S,
        getForcedKeyFrameIntervalMs(type, codecName), createBitrateAdjuster(type, codecName),
        sharedContext);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    List<VideoCodecInfo> supportedCodecInfos = new ArrayList<VideoCodecInfo>();

    for (VideoCodecMimeType type :
        new VideoCodecMimeType[] {VideoCodecMimeType.VP8, VideoCodecMimeType.VP9,
            VideoCodecMimeType.H264, VideoCodecMimeType.AV1, VideoCodecMimeType.H265}) {
      MediaCodecInfo codec = findCodecForType(type);
      if (codec != null) {
        String name = type.name();

        if (type == VideoCodecMimeType.H264 && isH264HighProfileSupported(codec)) {
          supportedCodecInfos.add(new VideoCodecInfo(
              name, MediaCodecUtils.getCodecProperties(type,                    true)));
        }

        supportedCodecInfos.add(new VideoCodecInfo(
            name, MediaCodecUtils.getCodecProperties(type,                    false)));
      }
    }

    return supportedCodecInfos.toArray(new VideoCodecInfo[supportedCodecInfos.size()]);
  }

  private @Nullable MediaCodecInfo findCodecForType(VideoCodecMimeType type) {
    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = null;
      try {
        info = MediaCodecList.getCodecInfoAt(i);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve encoder codec info", e);
      }

      if (info == null || !info.isEncoder()) {
        continue;
      }

      if (isSupportedCodec(info, type)) {
        return info;
      }
    }
    return null;
  }

  private boolean isSupportedCodec(MediaCodecInfo info, VideoCodecMimeType type) {
    if (!MediaCodecUtils.codecSupportsType(info, type)) {
      return false;
    }

    if (MediaCodecUtils.selectColorFormat(
            MediaCodecUtils.ENCODER_COLOR_FORMATS, info.getCapabilitiesForType(type.mimeType()))
        == null) {
      return false;
    }
    return isHardwareSupportedInCurrentSdk(info, type) && isMediaCodecAllowed(info);
  }

  private boolean isHardwareSupportedInCurrentSdk(MediaCodecInfo info, VideoCodecMimeType type) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      return info.isHardwareAccelerated();
    }

    switch (type) {
      case VP8:
        return isHardwareSupportedInCurrentSdkVp8(info);
      case VP9:
        return isHardwareSupportedInCurrentSdkVp9(info);
      case H264:
        return isHardwareSupportedInCurrentSdkH264(info);
      case H265:
      case AV1:
        return false;
    }
    return false;
  }

  private boolean isHardwareSupportedInCurrentSdkVp8(MediaCodecInfo info) {
    String name = info.getName();

    return name.startsWith(QCOM_PREFIX)

        || (name.startsWith(EXYNOS_PREFIX) && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M)

        || (name.startsWith(INTEL_PREFIX) && enableIntelVp8Encoder);
  }

  private boolean isHardwareSupportedInCurrentSdkVp9(MediaCodecInfo info) {
    String name = info.getName();
    return (name.startsWith(QCOM_PREFIX) || name.startsWith(EXYNOS_PREFIX))

        && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
  }

  private boolean isHardwareSupportedInCurrentSdkH264(MediaCodecInfo info) {

    if (H264_HW_EXCEPTION_MODELS.contains(Build.MODEL)) {
      return false;
    }
    String name = info.getName();

    return name.startsWith(QCOM_PREFIX) || name.startsWith(EXYNOS_PREFIX);
  }

  private boolean isMediaCodecAllowed(MediaCodecInfo info) {
    if (codecAllowedPredicate == null) {
      return true;
    }
    return codecAllowedPredicate.test(info);
  }

  private int getForcedKeyFrameIntervalMs(VideoCodecMimeType type, String codecName) {
    if (type == VideoCodecMimeType.VP8 && codecName.startsWith(QCOM_PREFIX)) {
      if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
        return QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_L_MS;
      }
      if (Build.VERSION.SDK_INT == Build.VERSION_CODES.M) {
        return QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_M_MS;
      }
      return QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_N_MS;
    }

    return 0;
  }

  private BitrateAdjuster createBitrateAdjuster(VideoCodecMimeType type, String codecName) {
    if (codecName.startsWith(EXYNOS_PREFIX)) {
      if (type == VideoCodecMimeType.VP8) {

        return new DynamicBitrateAdjuster();
      } else {

        return new FramerateBitrateAdjuster();
      }
    }

    return new BaseBitrateAdjuster();
  }

  private boolean isH264HighProfileSupported(MediaCodecInfo info) {
    return enableH264HighProfile && Build.VERSION.SDK_INT > Build.VERSION_CODES.M
        && info.getName().startsWith(EXYNOS_PREFIX);
  }
}
