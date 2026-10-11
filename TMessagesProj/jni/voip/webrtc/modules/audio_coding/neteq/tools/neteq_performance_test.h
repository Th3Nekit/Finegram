/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_PERFORMANCE_TEST_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_PERFORMANCE_TEST_H_

#include <stdint.h>

namespace webrtc {
namespace test {

class NetEqPerformanceTest {
 public:

  static int64_t Run(int runtime_ms, int lossrate, double drift_factor);
};

}
}

#endif
