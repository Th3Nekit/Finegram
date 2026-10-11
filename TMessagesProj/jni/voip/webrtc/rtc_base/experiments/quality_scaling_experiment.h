/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef RTC_BASE_EXPERIMENTS_QUALITY_SCALING_EXPERIMENT_H_
#define RTC_BASE_EXPERIMENTS_QUALITY_SCALING_EXPERIMENT_H_

#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/video_codecs/video_encoder.h"

namespace webrtc {
class QualityScalingExperiment {
 public:
  struct Settings {
    int vp8_low;
    int vp8_high;
    int vp9_low;
    int vp9_high;
    int h264_low;
    int h264_high;
    int generic_low;
    int generic_high;
    float alpha_high;
    float alpha_low;
    int drop;
  };

  struct Config {
    float alpha_high = 0.9995f;
    float alpha_low = 0.9999f;

    bool use_all_drop_reasons = false;
  };

  static bool Enabled(const FieldTrialsView& field_trials);

  static absl::optional<Settings> ParseSettings(
      const FieldTrialsView& field_trials);

  static absl::optional<VideoEncoder::QpThresholds> GetQpThresholds(
      VideoCodecType codec_type,
      const FieldTrialsView& field_trials);

  static Config GetConfig(const FieldTrialsView& field_trials);
};

}

#endif
