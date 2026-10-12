/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_REVERB_DECAY_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_AEC3_REVERB_DECAY_ESTIMATOR_H_

#include <array>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class ApmDataDumper;
struct EchoCanceller3Config;

class ReverbDecayEstimator {
 public:
  explicit ReverbDecayEstimator(const EchoCanceller3Config& config);
  ~ReverbDecayEstimator();

  void Update(rtc::ArrayView<const float> filter,
              const absl::optional<float>& filter_quality,
              int filter_delay_blocks,
              bool usable_linear_filter,
              bool stationary_signal);

  float Decay(bool mild) const {
    if (use_adaptive_echo_decay_) {
      return decay_;
    } else {
      return mild ? mild_decay_ : decay_;
    }
  }

  void Dump(ApmDataDumper* data_dumper) const;

 private:
  void EstimateDecay(rtc::ArrayView<const float> filter, int peak_block);
  void AnalyzeFilter(rtc::ArrayView<const float> filter);

  void ResetDecayEstimation();

  class LateReverbLinearRegressor {
   public:

    void Reset(int num_data_points);

    void Accumulate(float z);

    float Estimate();

    bool EstimateAvailable() const { return n_ == N_ && N_ != 0; }

   public:
    float nz_ = 0.f;
    float nn_ = 0.f;
    float count_ = 0.f;
    int N_ = 0;
    int n_ = 0;
  };

  class EarlyReverbLengthEstimator {
   public:
    explicit EarlyReverbLengthEstimator(int max_blocks);
    ~EarlyReverbLengthEstimator();

    void Reset();

    void Accumulate(float value, float smoothing);

    int Estimate();

    void Dump(ApmDataDumper* data_dumper) const;

   private:
    std::vector<float> numerators_smooth_;
    std::vector<float> numerators_;
    int coefficients_counter_;
    int block_counter_ = 0;
    int n_sections_ = 0;
  };

  const int filter_length_blocks_;
  const int filter_length_coefficients_;
  const bool use_adaptive_echo_decay_;
  LateReverbLinearRegressor late_reverb_decay_estimator_;
  EarlyReverbLengthEstimator early_reverb_estimator_;
  int late_reverb_start_;
  int late_reverb_end_;
  int block_to_analyze_ = 0;
  int estimation_region_candidate_size_ = 0;
  bool estimation_region_identified_ = false;
  std::vector<float> previous_gains_;
  float decay_;
  float mild_decay_;
  float tail_gain_ = 0.f;
  float smoothing_constant_ = 0.f;
};

}

#endif
