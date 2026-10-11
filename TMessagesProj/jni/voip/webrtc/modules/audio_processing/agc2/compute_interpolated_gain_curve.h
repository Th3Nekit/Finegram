/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_COMPUTE_INTERPOLATED_GAIN_CURVE_H_
#define MODULES_AUDIO_PROCESSING_AGC2_COMPUTE_INTERPOLATED_GAIN_CURVE_H_

#include <array>

#include "modules/audio_processing/agc2/agc2_common.h"

namespace webrtc {

namespace test {

struct InterpolatedParameters {
  std::array<float, kInterpolatedGainCurveTotalPoints>
      computed_approximation_params_x;
  std::array<float, kInterpolatedGainCurveTotalPoints>
      computed_approximation_params_m;
  std::array<float, kInterpolatedGainCurveTotalPoints>
      computed_approximation_params_q;
};

InterpolatedParameters ComputeInterpolatedGainCurveApproximationParams();
}
}

#endif
