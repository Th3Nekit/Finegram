/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_GAIN_APPLIER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_GAIN_APPLIER_H_

#include <stddef.h>

#include "modules/audio_processing/include/audio_frame_view.h"

namespace webrtc {
class GainApplier {
 public:
  GainApplier(bool hard_clip_samples, float initial_gain_factor);

  void ApplyGain(AudioFrameView<float> signal);
  void SetGainFactor(float gain_factor);
  float GetGainFactor() const { return current_gain_factor_; }

 private:
  void Initialize(int samples_per_channel);

  const bool hard_clip_samples_;
  float last_gain_factor_;

  float current_gain_factor_;
  int samples_per_channel_ = -1;
  float inverse_samples_per_channel_ = -1.f;
};
}

#endif
