/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TRANSIENT_VOICE_PROBABILITY_DELAY_UNIT_H_
#define MODULES_AUDIO_PROCESSING_TRANSIENT_VOICE_PROBABILITY_DELAY_UNIT_H_

#include <array>

namespace webrtc {

class VoiceProbabilityDelayUnit {
 public:

  VoiceProbabilityDelayUnit(int delay_num_samples, int sample_rate_hz);

  void Initialize(int delay_num_samples, int sample_rate_hz);

  float Delay(float voice_probability);

 private:
  std::array<float, 3> weights_;
  std::array<float, 2> last_probabilities_;
};

}

#endif
