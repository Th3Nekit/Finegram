/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_RATE_TRACKER_H_
#define RTC_BASE_RATE_TRACKER_H_

#include <stdint.h>
#include <stdlib.h>

namespace rtc {

class RateTracker {
 public:
  RateTracker(int64_t bucket_milliseconds, size_t bucket_count);
  virtual ~RateTracker();

  double ComputeRateForInterval(int64_t interval_milliseconds) const;

  double ComputeRate() const {
    return ComputeRateForInterval(bucket_milliseconds_ *
                                  static_cast<int64_t>(bucket_count_));
  }

  double ComputeTotalRate() const;

  int64_t TotalSampleCount() const;

  void AddSamples(int64_t sample_count);

  void AddSamplesAtTime(int64_t current_time_ms, int64_t sample_count);

 protected:

  virtual int64_t Time() const;

 private:
  void EnsureInitialized();
  size_t NextBucketIndex(size_t bucket_index) const;

  const int64_t bucket_milliseconds_;
  const size_t bucket_count_;
  int64_t* sample_buckets_;
  size_t total_sample_count_;
  size_t current_bucket_;
  int64_t bucket_start_time_milliseconds_;
  int64_t initialization_time_milliseconds_;
};

}

#endif
