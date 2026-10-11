/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_FIXED_DIGITAL_LEVEL_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AGC2_FIXED_DIGITAL_LEVEL_ESTIMATOR_H_

#include <array>
#include <vector>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/include/audio_frame_view.h"

namespace webrtc {

class ApmDataDumper;

class FixedDigitalLevelEstimator {
 public:

  FixedDigitalLevelEstimator(int sample_rate_hz,
                             ApmDataDumper* apm_data_dumper);

  FixedDigitalLevelEstimator(const FixedDigitalLevelEstimator&) = delete;
  FixedDigitalLevelEstimator& operator=(const FixedDigitalLevelEstimator&) =
      delete;

  std::array<float, kSubFramesInFrame> ComputeLevel(
      const AudioFrameView<const float>& float_frame);

  void SetSampleRate(int sample_rate_hz);

  void Reset();

  float LastAudioLevel() const { return filter_state_level_; }

 private:
  void CheckParameterCombination();

  ApmDataDumper* const apm_data_dumper_ = nullptr;
  float filter_state_level_;
  int samples_in_frame_;
  int samples_in_sub_frame_;
};
}

#endif
