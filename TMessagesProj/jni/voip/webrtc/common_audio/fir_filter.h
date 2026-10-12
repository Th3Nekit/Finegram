/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_FIR_FILTER_H_
#define COMMON_AUDIO_FIR_FILTER_H_

#include <string.h>

namespace webrtc {

class FIRFilter {
 public:
  virtual ~FIRFilter() {}

  virtual void Filter(const float* in, size_t length, float* out) = 0;
};

}

#endif
