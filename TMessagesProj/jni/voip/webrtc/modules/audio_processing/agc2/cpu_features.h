/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_CPU_FEATURES_H_
#define MODULES_AUDIO_PROCESSING_AGC2_CPU_FEATURES_H_

#include <string>

namespace webrtc {

struct AvailableCpuFeatures {
  AvailableCpuFeatures(bool sse2, bool avx2, bool neon)
      : sse2(sse2), avx2(avx2), neon(neon) {}

  bool sse2;
  bool avx2;

  bool neon;
  std::string ToString() const;
};

AvailableCpuFeatures GetAvailableCpuFeatures();

AvailableCpuFeatures NoAvailableCpuFeatures();

}

#endif
