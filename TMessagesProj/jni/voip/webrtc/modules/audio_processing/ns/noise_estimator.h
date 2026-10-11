/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_NS_NOISE_ESTIMATOR_H_
#define MODULES_AUDIO_PROCESSING_NS_NOISE_ESTIMATOR_H_

#include <array>

#include "api/array_view.h"
#include "modules/audio_processing/ns/ns_common.h"
#include "modules/audio_processing/ns/quantile_noise_estimator.h"
#include "modules/audio_processing/ns/suppression_params.h"

namespace webrtc {

class NoiseEstimator {
 public:
  explicit NoiseEstimator(const SuppressionParams& suppression_params);

  void PrepareAnalysis();

  void PreUpdate(int32_t num_analyzed_frames,
                 rtc::ArrayView<const float, kFftSizeBy2Plus1> signal_spectrum,
                 float signal_spectral_sum);

  void PostUpdate(
      rtc::ArrayView<const float> speech_probability,
      rtc::ArrayView<const float, kFftSizeBy2Plus1> signal_spectrum);

  rtc::ArrayView<const float, kFftSizeBy2Plus1> get_noise_spectrum() const {
    return noise_spectrum_;
  }

  rtc::ArrayView<const float, kFftSizeBy2Plus1> get_prev_noise_spectrum()
      const {
    return prev_noise_spectrum_;
  }

  rtc::ArrayView<const float, kFftSizeBy2Plus1> get_parametric_noise_spectrum()
      const {
    return parametric_noise_spectrum_;
  }
  rtc::ArrayView<const float, kFftSizeBy2Plus1>
  get_conservative_noise_spectrum() const {
    return conservative_noise_spectrum_;
  }

 private:
  const SuppressionParams& suppression_params_;
  float white_noise_level_ = 0.f;
  float pink_noise_numerator_ = 0.f;
  float pink_noise_exp_ = 0.f;
  std::array<float, kFftSizeBy2Plus1> prev_noise_spectrum_;
  std::array<float, kFftSizeBy2Plus1> conservative_noise_spectrum_;
  std::array<float, kFftSizeBy2Plus1> parametric_noise_spectrum_;
  std::array<float, kFftSizeBy2Plus1> noise_spectrum_;
  QuantileNoiseEstimator quantile_noise_estimator_;
};

}

#endif
