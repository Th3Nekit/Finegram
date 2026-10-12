/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_EVENT_BASED_EXPONENTIAL_MOVING_AVERAGE_H_
#define RTC_BASE_NUMERICS_EVENT_BASED_EXPONENTIAL_MOVING_AVERAGE_H_

#include <cmath>
#include <cstdint>
#include <limits>

#include "absl/types/optional.h"

namespace rtc {

class EventBasedExponentialMovingAverage {
 public:

  explicit EventBasedExponentialMovingAverage(int half_time);

  void AddSample(int64_t now, int value);

  double GetAverage() const { return value_; }
  double GetVariance() const { return sample_variance_; }

  double GetConfidenceInterval() const;

  void Reset();

  void SetHalfTime(int half_time);

 private:
  double tau_;
  double value_ = std::nan("uninit");
  double sample_variance_ = std::numeric_limits<double>::infinity();

  double estimator_variance_ = 1;
  absl::optional<int64_t> last_observation_timestamp_;
};

}

#endif
