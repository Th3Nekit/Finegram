/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_TRANSIENT_DYADIC_DECIMATOR_H_
#define MODULES_AUDIO_PROCESSING_TRANSIENT_DYADIC_DECIMATOR_H_

#include <cstdlib>

namespace webrtc {

inline size_t GetOutLengthToDyadicDecimate(size_t in_length,
                                           bool odd_sequence) {
  size_t out_length = in_length / 2;

  if (in_length % 2 == 1 && !odd_sequence) {
    ++out_length;
  }

  return out_length;
}

template <typename T>
static size_t DyadicDecimate(const T* in,
                             size_t in_length,
                             bool odd_sequence,
                             T* out,
                             size_t out_length) {
  size_t half_length = GetOutLengthToDyadicDecimate(in_length, odd_sequence);

  if (!in || !out || in_length <= 0 || out_length < half_length) {
    return 0;
  }

  size_t output_samples = 0;
  size_t index_adjustment = odd_sequence ? 1 : 0;
  for (output_samples = 0; output_samples < half_length; ++output_samples) {
    out[output_samples] = in[output_samples * 2 + index_adjustment];
  }

  return output_samples;
}

}

#endif
