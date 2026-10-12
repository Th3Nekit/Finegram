/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_INTERNAL_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_PITCH_SEARCH_INTERNAL_H_

#include <stddef.h>

#include <array>
#include <utility>

#include "api/array_view.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/agc2/rnn_vad/common.h"

namespace webrtc {
namespace rnn_vad {

void Decimate2x(rtc::ArrayView<const float, kBufSize24kHz> src,
                rtc::ArrayView<float, kBufSize12kHz> dst);

void ComputeSlidingFrameSquareEnergies24kHz(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<float, kRefineNumLags24kHz> y_energy,
    AvailableCpuFeatures cpu_features);

struct CandidatePitchPeriods {
  int best;
  int second_best;
};

CandidatePitchPeriods ComputePitchPeriod12kHz(
    rtc::ArrayView<const float, kBufSize12kHz> pitch_buffer,
    rtc::ArrayView<const float, kNumLags12kHz> auto_correlation,
    AvailableCpuFeatures cpu_features);

int ComputePitchPeriod48kHz(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<const float, kRefineNumLags24kHz> y_energy,
    CandidatePitchPeriods pitch_candidates_24kHz,
    AvailableCpuFeatures cpu_features);

struct PitchInfo {
  int period;
  float strength;
};

PitchInfo ComputeExtendedPitchPeriod48kHz(
    rtc::ArrayView<const float, kBufSize24kHz> pitch_buffer,
    rtc::ArrayView<const float, kRefineNumLags24kHz> y_energy,
    int initial_pitch_period_48kHz,
    PitchInfo last_pitch_48kHz,
    AvailableCpuFeatures cpu_features);

}
}

#endif
