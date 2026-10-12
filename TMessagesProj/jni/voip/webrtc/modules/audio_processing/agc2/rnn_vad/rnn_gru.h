/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_GRU_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_RNN_GRU_H_

#include <array>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/rnn_vad/vector_math.h"

namespace webrtc {
namespace rnn_vad {

constexpr int kGruLayerMaxUnits = 24;

class GatedRecurrentLayer {
 public:

  GatedRecurrentLayer(int input_size,
                      int output_size,
                      rtc::ArrayView<const int8_t> bias,
                      rtc::ArrayView<const int8_t> weights,
                      rtc::ArrayView<const int8_t> recurrent_weights,
                      const AvailableCpuFeatures& cpu_features,
                      absl::string_view layer_name);
  GatedRecurrentLayer(const GatedRecurrentLayer&) = delete;
  GatedRecurrentLayer& operator=(const GatedRecurrentLayer&) = delete;
  ~GatedRecurrentLayer();

  int input_size() const { return input_size_; }

  const float* data() const { return state_.data(); }

  int size() const { return output_size_; }

  void Reset();

  void ComputeOutput(rtc::ArrayView<const float> input);

 private:
  const int input_size_;
  const int output_size_;
  const std::vector<float> bias_;
  const std::vector<float> weights_;
  const std::vector<float> recurrent_weights_;
  const VectorMath vector_math_;

  std::array<float, kGruLayerMaxUnits> state_;
};

}
}

#endif
