/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_AUDIO_PROCESSING_CAPTURE_LEVELS_ADJUSTER_CAPTURE_LEVELS_ADJUSTER_H_
#define MODULES_AUDIO_PROCESSING_CAPTURE_LEVELS_ADJUSTER_CAPTURE_LEVELS_ADJUSTER_H_

#include <stddef.h>

#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/capture_levels_adjuster/audio_samples_scaler.h"

namespace webrtc {

class CaptureLevelsAdjuster {
 public:

  CaptureLevelsAdjuster(bool emulated_analog_mic_gain_enabled,
                        int emulated_analog_mic_gain_level,
                        float pre_gain,
                        float post_gain);
  CaptureLevelsAdjuster(const CaptureLevelsAdjuster&) = delete;
  CaptureLevelsAdjuster& operator=(const CaptureLevelsAdjuster&) = delete;

  void ApplyPreLevelAdjustment(AudioBuffer& audio_buffer);

  void ApplyPostLevelAdjustment(AudioBuffer& audio_buffer);

  void SetPreGain(float pre_gain);

  float GetPreAdjustmentGain() const { return pre_adjustment_gain_; }

  void SetPostGain(float post_gain);

  void SetAnalogMicGainLevel(int level);

  int GetAnalogMicGainLevel() const { return emulated_analog_mic_gain_level_; }

 private:

  void UpdatePreAdjustmentGain();

  const bool emulated_analog_mic_gain_enabled_;
  int emulated_analog_mic_gain_level_;
  float pre_gain_;
  float pre_adjustment_gain_;
  AudioSamplesScaler pre_scaler_;
  AudioSamplesScaler post_scaler_;
};
}

#endif
