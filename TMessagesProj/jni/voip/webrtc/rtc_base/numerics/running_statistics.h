/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NUMERICS_RUNNING_STATISTICS_H_
#define API_NUMERICS_RUNNING_STATISTICS_H_

#include <algorithm>
#include <cmath>
#include <limits>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/math_utils.h"

namespace webrtc {
namespace webrtc_impl {

template <typename T>
class RunningStatistics {
 public:

  void AddSample(T sample) {
    max_ = std::max(max_, sample);
    min_ = std::min(min_, sample);
    sum_ += sample;
    ++size_;

    const double delta = sample - mean_;
    mean_ += delta / size_;
    const double delta2 = sample - mean_;
    cumul_ += delta * delta2;
  }

  void RemoveSample(T sample) {
    RTC_DCHECK_GT(Size(), 0);

    if (Size() == 0) {
      return;
    }

    --size_;
    const double delta = sample - mean_;
    mean_ -= delta / size_;
    const double delta2 = sample - mean_;
    cumul_ -= delta * delta2;
  }

  void MergeStatistics(const RunningStatistics<T>& other) {
    if (other.size_ == 0) {
      return;
    }
    max_ = std::max(max_, other.max_);
    min_ = std::min(min_, other.min_);
    const int64_t new_size = size_ + other.size_;
    const double new_mean =
        (mean_ * size_ + other.mean_ * other.size_) / new_size;

    auto delta = [new_mean](const RunningStatistics<T>& stats) {
      return stats.size_ * (new_mean * (new_mean - 2 * stats.mean_) +
                            stats.mean_ * stats.mean_);
    };
    cumul_ = cumul_ + delta(*this) + other.cumul_ + delta(other);
    mean_ = new_mean;
    size_ = new_size;
  }

  int64_t Size() const { return size_; }

  absl::optional<T> GetMin() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return min_;
  }

  absl::optional<T> GetMax() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return max_;
  }

  absl::optional<double> GetSum() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return sum_;
  }

  absl::optional<double> GetMean() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return mean_;
  }

  absl::optional<double> GetVariance() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return cumul_ / size_;
  }

  absl::optional<double> GetStandardDeviation() const {
    if (size_ == 0) {
      return absl::nullopt;
    }
    return std::sqrt(*GetVariance());
  }

 private:
  int64_t size_ = 0;
  T min_ = infinity_or_max<T>();
  T max_ = minus_infinity_or_min<T>();
  double mean_ = 0;
  double cumul_ = 0;
  double sum_ = 0;
};

}
}

#endif
