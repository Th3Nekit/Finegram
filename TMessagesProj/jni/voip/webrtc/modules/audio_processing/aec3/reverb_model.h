/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_REVERB_MODEL_H_
#define MODULES_AUDIO_PROCESSING_AEC3_REVERB_MODEL_H_

#include <array>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"

namespace webrtc {

class ReverbModel {
 public:
  ReverbModel();
  ~ReverbModel();

  void Reset();

  rtc::ArrayView<const float, kFftLengthBy2Plus1> reverb() const {
    return reverb_;
  }

  void UpdateReverbNoFreqShaping(rtc::ArrayView<const float> power_spectrum,
                                 float power_spectrum_scaling,
                                 float reverb_decay);

  void UpdateReverb(rtc::ArrayView<const float> power_spectrum,
                    rtc::ArrayView<const float> power_spectrum_scaling,
                    float reverb_decay);

 private:
  std::array<float, kFftLengthBy2Plus1> reverb_;
};

}

#endif
