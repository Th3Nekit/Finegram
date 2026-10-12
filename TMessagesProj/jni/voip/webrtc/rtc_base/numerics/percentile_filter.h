/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NUMERICS_PERCENTILE_FILTER_H_
#define RTC_BASE_NUMERICS_PERCENTILE_FILTER_H_

#include <stdint.h>

#include <iterator>
#include <set>

#include "rtc_base/checks.h"

namespace webrtc {

template <typename T>
class PercentileFilter {
 public:

  explicit PercentileFilter(float percentile);

  void Insert(const T& value);

  bool Erase(const T& value);

  T GetPercentileValue() const;

  void Reset();

 private:

  void UpdatePercentileIterator();

  const float percentile_;
  std::multiset<T> set_;

  typename std::multiset<T>::iterator percentile_it_;
  int64_t percentile_index_;
};

template <typename T>
PercentileFilter<T>::PercentileFilter(float percentile)
    : percentile_(percentile),
      percentile_it_(set_.begin()),
      percentile_index_(0) {
  RTC_CHECK_GE(percentile, 0.0f);
  RTC_CHECK_LE(percentile, 1.0f);
}

template <typename T>
void PercentileFilter<T>::Insert(const T& value) {

  set_.insert(value);
  if (set_.size() == 1u) {

    percentile_it_ = set_.begin();
    percentile_index_ = 0;
  } else if (value < *percentile_it_) {

    ++percentile_index_;
  }
  UpdatePercentileIterator();
}

template <typename T>
bool PercentileFilter<T>::Erase(const T& value) {
  typename std::multiset<T>::const_iterator it = set_.lower_bound(value);

  if (it == set_.end() || *it != value)
    return false;
  if (it == percentile_it_) {

    percentile_it_ = set_.erase(it);
  } else {
    set_.erase(it);

    if (value <= *percentile_it_)
      --percentile_index_;
  }
  UpdatePercentileIterator();
  return true;
}

template <typename T>
void PercentileFilter<T>::UpdatePercentileIterator() {
  if (set_.empty())
    return;
  const int64_t index = static_cast<int64_t>(percentile_ * (set_.size() - 1));
  std::advance(percentile_it_, index - percentile_index_);
  percentile_index_ = index;
}

template <typename T>
T PercentileFilter<T>::GetPercentileValue() const {
  return set_.empty() ? 0 : *percentile_it_;
}

template <typename T>
void PercentileFilter<T>::Reset() {
  set_.clear();
  percentile_it_ = set_.begin();
  percentile_index_ = 0;
}
}

#endif
