/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_CNG_AUDIO_ENCODER_CNG_H_
#define MODULES_AUDIO_CODING_CODECS_CNG_AUDIO_ENCODER_CNG_H_

#include <stddef.h>

#include <memory>

#include "api/audio_codecs/audio_encoder.h"
#include "common_audio/vad/include/vad.h"

namespace webrtc {

struct AudioEncoderCngConfig {

  AudioEncoderCngConfig();
  AudioEncoderCngConfig(AudioEncoderCngConfig&&);
  ~AudioEncoderCngConfig();

  bool IsOk() const;

  size_t num_channels = 1;
  int payload_type = 13;
  std::unique_ptr<AudioEncoder> speech_encoder;
  Vad::Aggressiveness vad_mode = Vad::kVadNormal;
  int sid_frame_interval_ms = 100;
  int num_cng_coefficients = 8;

  Vad* vad = nullptr;
};

std::unique_ptr<AudioEncoder> CreateComfortNoiseEncoder(
    AudioEncoderCngConfig&& config);

}

#endif
