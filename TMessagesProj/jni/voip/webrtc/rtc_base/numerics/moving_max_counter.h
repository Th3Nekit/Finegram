/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_MOVING_MAX_COUNTER_H_
#define RTC_BASE_NUMERICS_MOVING_MAX_COUNTER_H_

#include <stdint.h>

#include <deque>
#include <limits>
#include <utility>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"

namespace rtc {

template <class T>
class MovingMaxCounter {
 public:
  explicit MovingMaxCounter(int64_t window_length_ms);

  MovingMaxCounter(const MovingMaxCounter&) = delete;
  MovingMaxCounter& operator=(const MovingMaxCounter&) = delete;

  void Add(const T& sample, int64_t current_time_ms);

  absl::optional<T> Max(int64_t current_time_ms);
  void Reset();

 private:

  void RollWindow(int64_t new_time_ms);
  const int64_t window_length_ms_;

  std::deque<std::pair<int64_t, T>> samples_;
#if RTC_DCHECK_IS_ON
  int64_t last_call_time_ms_ = std::numeric_limits<int64_t>::min();
#endif
};

template <class T>
MovingMaxCounter<T>::MovingMaxCounter(int64_t window_length_ms)
    : window_length_ms_(window_length_ms) {}

template <class T>
void MovingMaxCounter<T>::Add(const T& sample, int64_t current_time_ms) {
  RollWindow(current_time_ms);

  while (!samples_.empty() && samples_.back().second <= sample) {
    samples_.pop_back();
  }

  if (samples_.empty() || samples_.back().first < current_time_ms) {
    samples_.emplace_back(std::make_pair(current_time_ms, sample));
  }
}

template <class T>
absl::optional<T> MovingMaxCounter<T>::Max(int64_t current_time_ms) {
  RollWindow(current_time_ms);
  absl::optional<T> res;
  if (!samples_.empty()) {
    res.emplace(samples_.front().second);
  }
  return res;
}

template <class T>
void MovingMaxCounter<T>::Reset() {
  samples_.clear();
}

template <class T>
void MovingMaxCounter<T>::RollWindow(int64_t new_time_ms) {
#if RTC_DCHECK_IS_ON
  RTC_DCHECK_GE(new_time_ms, last_call_time_ms_);
  last_call_time_ms_ = new_time_ms;
#endif
  const int64_t window_begin_ms = new_time_ms - window_length_ms_;
  auto it = samples_.begin();
  while (it != samples_.end() && it->first < window_begin_ms) {
    ++it;
  }
  samples_.erase(samples_.begin(), it);
}

}

#endif
