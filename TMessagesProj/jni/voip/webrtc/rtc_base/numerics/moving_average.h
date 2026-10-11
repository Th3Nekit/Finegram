/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_MOVING_AVERAGE_H_
#define RTC_BASE_NUMERICS_MOVING_AVERAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "absl/types/optional.h"

namespace rtc {

class MovingAverage {
 public:

  explicit MovingAverage(size_t window_size);
  ~MovingAverage();

  MovingAverage(const MovingAverage&) = delete;
  MovingAverage& operator=(const MovingAverage&) = delete;

  void AddSample(int sample);

  absl::optional<int> GetAverageRoundedDown() const;

  absl::optional<int> GetAverageRoundedToClosest() const;

  absl::optional<double> GetUnroundedAverage() const;

  void Reset();

  size_t Size() const;

 private:

  size_t count_ = 0;

  int64_t sum_ = 0;

  std::vector<int> history_;
};

}
#endif
