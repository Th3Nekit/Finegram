/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_VAD_VOICE_ACTIVITY_DETECTOR_H_
#define MODULES_AUDIO_PROCESSING_VAD_VOICE_ACTIVITY_DETECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "common_audio/resampler/include/resampler.h"
#include "modules/audio_processing/vad/common.h"
#include "modules/audio_processing/vad/pitch_based_vad.h"
#include "modules/audio_processing/vad/standalone_vad.h"
#include "modules/audio_processing/vad/vad_audio_proc.h"

namespace webrtc {

class VoiceActivityDetector {
 public:
  VoiceActivityDetector();
  ~VoiceActivityDetector();

  void ProcessChunk(const int16_t* audio, size_t length, int sample_rate_hz);

  const std::vector<double>& chunkwise_voice_probabilities() const {
    return chunkwise_voice_probabilities_;
  }

  const std::vector<double>& chunkwise_rms() const { return chunkwise_rms_; }

  float last_voice_probability() const { return last_voice_probability_; }

 private:

  std::vector<double> chunkwise_voice_probabilities_;
  std::vector<double> chunkwise_rms_;

  float last_voice_probability_;

  Resampler resampler_;
  VadAudioProc audio_processing_;

  std::unique_ptr<StandaloneVad> standalone_vad_;
  PitchBasedVad pitch_based_vad_;

  int16_t resampled_[kLength10Ms];
  AudioFeatures features_;
};

}

#endif
