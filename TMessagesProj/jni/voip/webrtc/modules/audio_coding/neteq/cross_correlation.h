/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_CROSS_CORRELATION_H_
#define MODULES_AUDIO_CODING_NETEQ_CROSS_CORRELATION_H_

#include <stddef.h>
#include <stdint.h>

namespace webrtc {

int CrossCorrelationWithAutoShift(const int16_t* sequence_1,
                                  const int16_t* sequence_2,
                                  size_t sequence_1_length,
                                  size_t cross_correlation_length,
                                  int cross_correlation_step,
                                  int32_t* cross_correlation);

}

#endif
