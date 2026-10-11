/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_RTT_FILTER_H_
#define MODULES_VIDEO_CODING_TIMING_RTT_FILTER_H_

#include <stdint.h>

#include "absl/container/inlined_vector.h"
#include "api/units/time_delta.h"

namespace webrtc {

class RttFilter {
 public:
  RttFilter();
  RttFilter(const RttFilter&) = delete;
  RttFilter& operator=(const RttFilter&) = delete;

  void Reset();

  void Update(TimeDelta rtt);

  TimeDelta Rtt() const;

 private:

  static constexpr int kMaxDriftJumpCount = 5;
  using BufferList = absl::InlinedVector<TimeDelta, kMaxDriftJumpCount>;

  bool JumpDetection(TimeDelta rtt);

  bool DriftDetection(TimeDelta rtt);

  void ShortRttFilter(const BufferList& buf);

  bool got_non_zero_update_;
  TimeDelta avg_rtt_;

  int64_t var_rtt_;
  TimeDelta max_rtt_;
  uint32_t filt_fact_count_;
  bool last_jump_positive_ = false;
  BufferList jump_buf_;
  BufferList drift_buf_;
};

}

#endif
