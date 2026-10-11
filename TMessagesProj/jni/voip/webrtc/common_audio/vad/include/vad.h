/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_VAD_INCLUDE_VAD_H_
#define COMMON_AUDIO_VAD_INCLUDE_VAD_H_

#include <memory>

#include "common_audio/vad/include/webrtc_vad.h"
#include "rtc_base/checks.h"

namespace webrtc {

class Vad {
 public:
  enum Aggressiveness {
    kVadNormal = 0,
    kVadLowBitrate = 1,
    kVadAggressive = 2,
    kVadVeryAggressive = 3
  };

  enum Activity { kPassive = 0, kActive = 1, kError = -1 };

  virtual ~Vad() = default;

  virtual Activity VoiceActivity(const int16_t* audio,
                                 size_t num_samples,
                                 int sample_rate_hz) = 0;

  virtual void Reset() = 0;
};

std::unique_ptr<Vad> CreateVad(Vad::Aggressiveness aggressiveness);

}

#endif
