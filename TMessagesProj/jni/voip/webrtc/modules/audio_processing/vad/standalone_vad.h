/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_STANDALONE_VAD_H_
#define MODULES_AUDIO_PROCESSING_AGC_STANDALONE_VAD_H_

#include <stddef.h>
#include <stdint.h>

#include "common_audio/vad/include/webrtc_vad.h"
#include "modules/audio_processing/vad/common.h"

namespace webrtc {

class StandaloneVad {
 public:
  static StandaloneVad* Create();
  ~StandaloneVad();

  int GetActivity(double* p, size_t length_p);

  int AddAudio(const int16_t* data, size_t length);

  int set_mode(int mode);

  int mode() const { return mode_; }

 private:
  explicit StandaloneVad(VadInst* vad);

  static const size_t kMaxNum10msFrames = 3;

  VadInst* vad_;
  int16_t buffer_[kMaxNum10msFrames * kLength10Ms];
  size_t index_;
  int mode_;
};

}

#endif
