/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_LP_RESIDUAL_H_
#define MODULES_AUDIO_PROCESSING_AGC2_RNN_VAD_LP_RESIDUAL_H_

#include <stddef.h>

#include "api/array_view.h"

namespace webrtc {
namespace rnn_vad {

constexpr int kNumLpcCoefficients = 5;

void ComputeAndPostProcessLpcCoefficients(
    rtc::ArrayView<const float> x,
    rtc::ArrayView<float, kNumLpcCoefficients> lpc_coeffs);

void ComputeLpResidual(
    rtc::ArrayView<const float, kNumLpcCoefficients> lpc_coeffs,
    rtc::ArrayView<const float> x,
    rtc::ArrayView<float> y);

}
}

#endif
