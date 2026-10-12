/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RATE_STATISTICS_H_
#define RTC_BASE_RATE_STATISTICS_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <memory>

#include "absl/types/optional.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT RateStatistics {
 public:
  static constexpr float kBpsScale = 8000.0f;

  RateStatistics(int64_t max_window_size_ms, float scale);

  RateStatistics(const RateStatistics& other);

  RateStatistics(RateStatistics&& other);

  ~RateStatistics();

  void Reset();

  void Update(int64_t count, int64_t now_ms);

  absl::optional<int64_t> Rate(int64_t now_ms) const;

  bool SetWindowSize(int64_t window_size_ms, int64_t now_ms);

 private:
  void EraseOld(int64_t now_ms);

  struct Bucket {
    explicit Bucket(int64_t timestamp);
    int64_t sum;
    int num_samples;
    const int64_t timestamp;
  };

  std::deque<Bucket> buckets_;

  int64_t accumulated_count_;

  int64_t first_timestamp_;

  bool overflow_ = false;

  int num_samples_;

  const float scale_;

  const int64_t max_window_size_ms_;
  int64_t current_window_size_ms_;
};
}

#endif
