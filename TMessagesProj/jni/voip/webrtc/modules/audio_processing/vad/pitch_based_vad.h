/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_VAD_PITCH_BASED_VAD_H_
#define MODULES_AUDIO_PROCESSING_VAD_PITCH_BASED_VAD_H_

#include <memory>

#include "modules/audio_processing/vad/common.h"
#include "modules/audio_processing/vad/gmm.h"

namespace webrtc {

class VadCircularBuffer;

class PitchBasedVad {
 public:
  PitchBasedVad();
  ~PitchBasedVad();

  int VoicingProbability(const AudioFeatures& features, double* p_combined);

 private:
  int UpdatePrior(double p);

  static const int kNoError = 0;

  GmmParameters noise_gmm_;
  GmmParameters voice_gmm_;

  double p_prior_;

  std::unique_ptr<VadCircularBuffer> circular_buffer_;
};

}

#endif
