/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TRANSIENT_TRANSIENT_SUPPRESSOR_H_
#define MODULES_AUDIO_PROCESSING_TRANSIENT_TRANSIENT_SUPPRESSOR_H_

#include <cstddef>

namespace webrtc {

class TransientSuppressor {
 public:

  enum class VadMode {

    kDefault = 0,

    kRnnVad,

    kNoVad,
  };

  virtual ~TransientSuppressor() {}

  virtual void Initialize(int sample_rate_hz,
                          int detector_rate_hz,
                          int num_channels) = 0;

  virtual float Suppress(float* data,
                         size_t data_length,
                         int num_channels,
                         const float* detection_data,
                         size_t detection_length,
                         const float* reference_data,
                         size_t reference_length,
                         float voice_probability,
                         bool key_pressed) = 0;
};

}

#endif
