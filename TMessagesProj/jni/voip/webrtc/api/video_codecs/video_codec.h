/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_CODEC_H_
#define API_VIDEO_CODECS_VIDEO_CODEC_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "absl/strings/string_view.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_codec_type.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/simulcast_stream.h"
#include "api/video_codecs/spatial_layer.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class VideoCodecComplexity {
  kComplexityLow = -1,
  kComplexityNormal = 0,
  kComplexityHigh = 1,
  kComplexityHigher = 2,
  kComplexityMax = 3
};

struct VideoCodecVP8 {
  bool operator==(const VideoCodecVP8& other) const;
  bool operator!=(const VideoCodecVP8& other) const {
    return !(*this == other);
  }

  void SetNumberOfTemporalLayers(unsigned char n) {
    numberOfTemporalLayers = n;
  }
  unsigned char numberOfTemporalLayers;
  bool denoisingOn;
  bool automaticResizeOn;
  int keyFrameInterval;
};

enum class InterLayerPredMode : int {
  kOff = 0,
  kOn = 1,
  kOnKeyPic = 2
};

struct VideoCodecVP9 {
  bool operator==(const VideoCodecVP9& other) const;
  bool operator!=(const VideoCodecVP9& other) const {
    return !(*this == other);
  }

  void SetNumberOfTemporalLayers(unsigned char n) {
    numberOfTemporalLayers = n;
  }
  unsigned char numberOfTemporalLayers;
  bool denoisingOn;
  int keyFrameInterval;
  bool adaptiveQpMode;
  bool automaticResizeOn;
  unsigned char numberOfSpatialLayers;
  bool flexibleMode;
  InterLayerPredMode interLayerPred;
};

struct VideoCodecH264 {
  bool operator==(const VideoCodecH264& other) const;
  bool operator!=(const VideoCodecH264& other) const {
    return !(*this == other);
  }

  void SetNumberOfTemporalLayers(unsigned char n) {
    numberOfTemporalLayers = n;
  }
  int keyFrameInterval;
  uint8_t numberOfTemporalLayers;
};

struct VideoCodecH265 {
  bool operator==(const VideoCodecH265& other) const;
  bool operator!=(const VideoCodecH265& other) const {
    return !(*this == other);
  }

  bool frameDroppingOn;
  int keyFrameInterval;
  const uint8_t* vpsData;
  size_t vpsLen;
  const uint8_t* spsData;
  size_t spsLen;
  const uint8_t* ppsData;
  size_t ppsLen;
};

struct VideoCodecAV1 {
  bool operator==(const VideoCodecAV1& other) const {
    return automatic_resize_on == other.automatic_resize_on;
  }
  bool operator!=(const VideoCodecAV1& other) const {
    return !(*this == other);
  }
  bool automatic_resize_on;
};

RTC_EXPORT const char* CodecTypeToPayloadString(VideoCodecType type);
RTC_EXPORT VideoCodecType PayloadStringToCodecType(const std::string& name);

union VideoCodecUnion {
  VideoCodecVP8 VP8;
  VideoCodecVP9 VP9;
  VideoCodecH264 H264;
  VideoCodecH265 H265;
  VideoCodecAV1 AV1;
};

enum class VideoCodecMode { kRealtimeVideo, kScreensharing };

class RTC_EXPORT VideoCodec {
 public:
  VideoCodec();

  absl::optional<ScalabilityMode> GetScalabilityMode() const {
    return scalability_mode_;
  }
  void SetScalabilityMode(ScalabilityMode scalability_mode) {
    scalability_mode_ = scalability_mode;
  }
  void UnsetScalabilityMode() { scalability_mode_ = absl::nullopt; }

  VideoCodecComplexity GetVideoEncoderComplexity() const;
  void SetVideoEncoderComplexity(VideoCodecComplexity complexity_setting);

  bool GetFrameDropEnabled() const;
  void SetFrameDropEnabled(bool enabled);

  bool IsSinglecast() const { return numberOfSimulcastStreams <= 1; }
  bool IsSimulcast() const { return !IsSinglecast(); }

  VideoCodecType codecType;

  uint16_t width;
  uint16_t height;

  unsigned int startBitrate;
  unsigned int maxBitrate;
  unsigned int minBitrate;

  uint32_t maxFramerate;

  bool active;

  unsigned int qpMax;

  unsigned char numberOfSimulcastStreams;
  SimulcastStream simulcastStream[kMaxSimulcastStreams];
  SpatialLayer spatialLayers[kMaxSpatialLayers];

  VideoCodecMode mode;
  bool expect_encode_from_texture;

  struct TimingFrameTriggerThresholds {
    int64_t delay_ms;
    uint16_t outlier_ratio_percent;
  } timing_frame_thresholds;

  bool legacy_conference_mode;

  bool operator==(const VideoCodec& other) const = delete;
  bool operator!=(const VideoCodec& other) const = delete;
  std::string ToString() const;

  VideoCodecVP8* VP8();
  const VideoCodecVP8& VP8() const;
  VideoCodecVP9* VP9();
  const VideoCodecVP9& VP9() const;
  VideoCodecH264* H264();
  const VideoCodecH264& H264() const;
  VideoCodecH265* H265();
  const VideoCodecH265& H265() const;
  VideoCodecAV1* AV1();
  const VideoCodecAV1& AV1() const;

 private:

  VideoCodecUnion codec_specific_;
  absl::optional<ScalabilityMode> scalability_mode_;

  VideoCodecComplexity complexity_;
  bool frame_drop_enabled_ = false;
};

}
#endif
