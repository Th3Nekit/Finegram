/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_DTMF_TONE_GENERATOR_H_
#define MODULES_AUDIO_CODING_NETEQ_DTMF_TONE_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/audio_coding/neteq/audio_multi_vector.h"

namespace webrtc {

class DtmfToneGenerator {
 public:
  enum ReturnCodes {
    kNotInitialized = -1,
    kParameterError = -2,
  };

  DtmfToneGenerator();
  virtual ~DtmfToneGenerator() {}

  DtmfToneGenerator(const DtmfToneGenerator&) = delete;
  DtmfToneGenerator& operator=(const DtmfToneGenerator&) = delete;

  virtual int Init(int fs, int event, int attenuation);
  virtual void Reset();
  virtual int Generate(size_t num_samples, AudioMultiVector* output);
  virtual bool initialized() const;

 private:
  static const int kCoeff1[4][16];
  static const int kCoeff2[4][16];
  static const int kInitValue1[4][16];
  static const int kInitValue2[4][16];
  static const int kAmplitude[64];
  static const int16_t kAmpMultiplier = 23171;

  bool initialized_;
  int coeff1_;
  int coeff2_;
  int amplitude_;
  int16_t sample_history1_[2];
  int16_t sample_history2_[2];
};

}
#endif
