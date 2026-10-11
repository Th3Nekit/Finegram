/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SYSTEM_WRAPPERS_INCLUDE_CPU_FEATURES_WRAPPER_H_
#define SYSTEM_WRAPPERS_INCLUDE_CPU_FEATURES_WRAPPER_H_

#include <stdint.h>

namespace webrtc {

typedef enum { kSSE2, kSSE3, kAVX2, kFMA3 } CPUFeature;

enum {
  kCPUFeatureARMv7 = (1 << 0),
  kCPUFeatureVFPv3 = (1 << 1),
  kCPUFeatureNEON = (1 << 2),
  kCPUFeatureLDREXSTREX = (1 << 3)
};

int GetCPUInfo(CPUFeature feature);

int GetCPUInfoNoASM(CPUFeature feature);

uint64_t GetCPUFeaturesARM(void);

}

#endif
