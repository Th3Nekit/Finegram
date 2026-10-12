/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_PREEMPTIVE_EXPAND_H_
#define MODULES_AUDIO_CODING_NETEQ_PREEMPTIVE_EXPAND_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/audio_coding/neteq/time_stretch.h"

namespace webrtc {

class AudioMultiVector;
class BackgroundNoise;

class PreemptiveExpand : public TimeStretch {
 public:
  PreemptiveExpand(int sample_rate_hz,
                   size_t num_channels,
                   const BackgroundNoise& background_noise,
                   size_t overlap_samples)
      : TimeStretch(sample_rate_hz, num_channels, background_noise),
        old_data_length_per_channel_(0),
        overlap_samples_(overlap_samples) {}

  PreemptiveExpand(const PreemptiveExpand&) = delete;
  PreemptiveExpand& operator=(const PreemptiveExpand&) = delete;

  ReturnCodes Process(const int16_t* pw16_decoded,
                      size_t len,
                      size_t old_data_len,
                      AudioMultiVector* output,
                      size_t* length_change_samples);

 protected:

  void SetParametersForPassiveSpeech(size_t input_length,
                                     int16_t* best_correlation,
                                     size_t* peak_index) const override;

  ReturnCodes CheckCriteriaAndStretch(const int16_t* input,
                                      size_t input_length,
                                      size_t peak_index,
                                      int16_t best_correlation,
                                      bool active_speech,
                                      bool              ,
                                      AudioMultiVector* output) const override;

 private:
  size_t old_data_length_per_channel_;
  size_t overlap_samples_;
};

struct PreemptiveExpandFactory {
  PreemptiveExpandFactory() {}
  virtual ~PreemptiveExpandFactory() {}

  virtual PreemptiveExpand* Create(int sample_rate_hz,
                                   size_t num_channels,
                                   const BackgroundNoise& background_noise,
                                   size_t overlap_samples) const;
};

}
#endif
