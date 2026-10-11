/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_ACCELERATE_H_
#define MODULES_AUDIO_CODING_NETEQ_ACCELERATE_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/audio_coding/neteq/time_stretch.h"

namespace webrtc {

class AudioMultiVector;
class BackgroundNoise;

class Accelerate : public TimeStretch {
 public:
  Accelerate(int sample_rate_hz,
             size_t num_channels,
             const BackgroundNoise& background_noise)
      : TimeStretch(sample_rate_hz, num_channels, background_noise) {}

  Accelerate(const Accelerate&) = delete;
  Accelerate& operator=(const Accelerate&) = delete;

  ReturnCodes Process(const int16_t* input,
                      size_t input_length,
                      bool fast_accelerate,
                      AudioMultiVector* output,
                      size_t* length_change_samples);

 protected:

  void SetParametersForPassiveSpeech(size_t len,
                                     int16_t* best_correlation,
                                     size_t* peak_index) const override;

  ReturnCodes CheckCriteriaAndStretch(const int16_t* input,
                                      size_t input_length,
                                      size_t peak_index,
                                      int16_t best_correlation,
                                      bool active_speech,
                                      bool fast_mode,
                                      AudioMultiVector* output) const override;
};

struct AccelerateFactory {
  AccelerateFactory() {}
  virtual ~AccelerateFactory() {}

  virtual Accelerate* Create(int sample_rate_hz,
                             size_t num_channels,
                             const BackgroundNoise& background_noise) const;
};

}
#endif
