/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_FEATURES_EXTRACTION_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_FEATURES_EXTRACTION_H_

#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/biquad_filter.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"
#include "modules/audio_processing/agc2/rnn_vad/pitch_search.h"
#include "modules/audio_processing/agc2/rnn_vad/sequence_buffer.h"
#include "modules/audio_processing/agc2/rnn_vad/spectral_features.h"

namespace webrtc {
namespace rnn_vad {

class FeaturesExtractor {
 public:
  explicit FeaturesExtractor(const AvailableCpuFeatures& cpu_features);
  FeaturesExtractor(const FeaturesExtractor&) = delete;
  FeaturesExtractor& operator=(const FeaturesExtractor&) = delete;
  ~FeaturesExtractor();
  void Reset();

  bool CheckSilenceComputeFeatures(
      rtc::ArrayView<const float, kFrameSize10ms24kHz> samples,
      rtc::ArrayView<float, kFeatureVectorSize> feature_vector);

 private:
  const bool use_high_pass_filter_;

  BiQuadFilter hpf_;
  SequenceBuffer<float, kBufSize24kHz, kFrameSize10ms24kHz, kFrameSize20ms24kHz>
      pitch_buf_24kHz_;
  rtc::ArrayView<const float, kBufSize24kHz> pitch_buf_24kHz_view_;
  std::vector<float> lp_residual_;
  rtc::ArrayView<float, kBufSize24kHz> lp_residual_view_;
  PitchEstimator pitch_estimator_;
  rtc::ArrayView<const float, kFrameSize20ms24kHz> reference_frame_view_;
  SpectralFeaturesExtractor spectral_features_extractor_;
  int pitch_period_48kHz_;
};

}
}

#endif
