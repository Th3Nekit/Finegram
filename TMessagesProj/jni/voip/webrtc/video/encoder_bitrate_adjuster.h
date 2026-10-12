/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ENCODER_BITRATE_ADJUSTER_H_
#define VIDEO_ENCODER_BITRATE_ADJUSTER_H_

#include <memory>

#include "api/field_trials_view.h"
#include "api/video/encoded_image.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video_codecs/video_encoder.h"
#include "video/encoder_overshoot_detector.h"

namespace webrtc {

class EncoderBitrateAdjuster {
 public:

  static constexpr int64_t kWindowSizeMs = 3000;

  static constexpr size_t kMinFramesSinceLayoutChange = 30;

  static constexpr double kDefaultUtilizationFactor = 1.2;

  EncoderBitrateAdjuster(const VideoCodec& codec_settings,
                         const FieldTrialsView& field_trials);
  ~EncoderBitrateAdjuster();

  VideoBitrateAllocation AdjustRateAllocation(
      const VideoEncoder::RateControlParameters& rates);

  void OnEncoderInfo(const VideoEncoder::EncoderInfo& encoder_info);

  void OnEncodedFrame(DataSize size, int stream_index, int temporal_index);

  void Reset();

 private:
  const bool utilize_bandwidth_headroom_;

  VideoEncoder::RateControlParameters current_rate_control_parameters_;

  absl::InlinedVector<uint8_t, kMaxTemporalStreams>
      current_fps_allocation_[kMaxSpatialLayers];

  size_t frames_since_layout_change_;
  std::unique_ptr<EncoderOvershootDetector>
      overshoot_detectors_[kMaxSpatialLayers][kMaxTemporalStreams];

  uint32_t min_bitrates_bps_[kMaxSpatialLayers];

  uint32_t frame_size_pixels_[kMaxSpatialLayers];

  VideoCodecType codec_;

  VideoCodecMode codec_mode_;
};

}

#endif
