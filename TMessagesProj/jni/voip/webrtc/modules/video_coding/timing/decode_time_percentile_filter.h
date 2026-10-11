/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_TIMING_DECODE_TIME_PERCENTILE_FILTER_H_
#define MODULES_VIDEO_CODING_TIMING_DECODE_TIME_PERCENTILE_FILTER_H_

#include <queue>

#include "rtc_base/numerics/percentile_filter.h"

namespace webrtc {

class DecodeTimePercentileFilter {
 public:
  DecodeTimePercentileFilter();
  ~DecodeTimePercentileFilter();

  void AddTiming(int64_t new_decode_time_ms, int64_t now_ms);

  int64_t RequiredDecodeTimeMs() const;

 private:
  struct Sample {
    Sample(int64_t decode_time_ms, int64_t sample_time_ms);
    int64_t decode_time_ms;
    int64_t sample_time_ms;
  };

  int ignored_sample_count_;

  std::queue<Sample> history_;

  PercentileFilter<int64_t> filter_;
};

}

#endif
