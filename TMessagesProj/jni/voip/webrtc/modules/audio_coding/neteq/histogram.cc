/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/histogram.h"

#include <algorithm>
#include <cstdlib>
#include <numeric>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

Histogram::Histogram(size_t num_buckets,
                     int forget_factor,
                     absl::optional<double> start_forget_weight)
    : buckets_(num_buckets, 0),
      forget_factor_(0),
      base_forget_factor_(forget_factor),
      add_count_(0),
      start_forget_weight_(start_forget_weight) {
  RTC_DCHECK_LT(base_forget_factor_, 1 << 15);
}

Histogram::~Histogram() {}

void Histogram::Add(int value) {
  RTC_DCHECK(value >= 0);
  RTC_DCHECK(value < static_cast<int>(buckets_.size()));
  int vector_sum = 0;

  for (int& bucket : buckets_) {
    bucket = (static_cast<int64_t>(bucket) * forget_factor_) >> 15;
    vector_sum += bucket;
  }

  buckets_[value] += (32768 - forget_factor_) << 15;
  vector_sum += (32768 - forget_factor_) << 15;

  vector_sum -= 1 << 30;
  if (vector_sum != 0) {

    int flip_sign = vector_sum > 0 ? -1 : 1;
    for (int& bucket : buckets_) {

      int correction = flip_sign * std::min(std::abs(vector_sum), bucket >> 4);
      bucket += correction;
      vector_sum += correction;
      if (std::abs(vector_sum) == 0) {
        break;
      }
    }
  }
  RTC_DCHECK(vector_sum == 0);

  ++add_count_;

  if (start_forget_weight_) {
    if (forget_factor_ != base_forget_factor_) {
      int old_forget_factor = forget_factor_;
      int forget_factor =
          (1 << 15) * (1 - start_forget_weight_.value() / (add_count_ + 1));
      forget_factor_ =
          std::max(0, std::min(base_forget_factor_, forget_factor));

      RTC_DCHECK_GE((1 << 15) - forget_factor_,
                    ((1 << 15) - old_forget_factor) * forget_factor_ >> 15);
    }
  } else {
    forget_factor_ += (base_forget_factor_ - forget_factor_ + 3) >> 2;
  }
}

int Histogram::Quantile(int probability) {

  int inverse_probability = (1 << 30) - probability;
  size_t index = 0;
  int sum = 1 << 30;
  sum -= buckets_[index];

  while ((sum > inverse_probability) && (index < buckets_.size() - 1)) {

    ++index;
    sum -= buckets_[index];
  }
  return static_cast<int>(index);
}

void Histogram::Reset() {

  uint16_t temp_prob = 0x4002;
  for (int& bucket : buckets_) {
    temp_prob >>= 1;
    bucket = temp_prob << 16;
  }
  forget_factor_ = 0;
  add_count_ = 0;
}

int Histogram::NumBuckets() const {
  return buckets_.size();
}

}
