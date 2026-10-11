/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SPECTRAL_FEATURES_INTERNAL_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_SPECTRAL_FEATURES_INTERNAL_H_

#include <stddef.h>

#include <array>
#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"

namespace webrtc {
namespace rnn_vad {

constexpr int kOpusBands24kHz = 20;
static_assert(kOpusBands24kHz < kNumBands,
              "The number of bands at 24 kHz must be less than those defined "
              "in the Opus scale at 48 kHz.");

constexpr std::array<int, kOpusBands24kHz - 1> GetOpusScaleNumBins24kHz20ms() {
  return {4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 8, 8, 16, 16, 16, 24, 24, 32, 48};
}

class SpectralCorrelator {
 public:

  SpectralCorrelator();
  SpectralCorrelator(const SpectralCorrelator&) = delete;
  SpectralCorrelator& operator=(const SpectralCorrelator&) = delete;
  ~SpectralCorrelator();

  void ComputeAutoCorrelation(
      rtc::ArrayView<const float> x,
      rtc::ArrayView<float, kOpusBands24kHz> auto_corr) const;

  void ComputeCrossCorrelation(
      rtc::ArrayView<const float> x,
      rtc::ArrayView<const float> y,
      rtc::ArrayView<float, kOpusBands24kHz> cross_corr) const;

 private:
  const std::vector<float> weights_;
};

void ComputeSmoothedLogMagnitudeSpectrum(
    rtc::ArrayView<const float> bands_energy,
    rtc::ArrayView<float, kNumBands> log_bands_energy);

std::array<float, kNumBands * kNumBands> ComputeDctTable();

void ComputeDct(rtc::ArrayView<const float> in,
                rtc::ArrayView<const float, kNumBands * kNumBands> dct_table,
                rtc::ArrayView<float> out);

}
}

#endif
