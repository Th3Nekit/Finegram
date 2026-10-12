/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FREQUENCY_TRACKER_H_
#define RTC_BASE_FREQUENCY_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT FrequencyTracker {
 public:
  explicit FrequencyTracker(TimeDelta window_size);

  FrequencyTracker(const FrequencyTracker&) = default;
  FrequencyTracker(FrequencyTracker&&) = default;
  FrequencyTracker& operator=(const FrequencyTracker&) = delete;
  FrequencyTracker& operator=(FrequencyTracker&&) = delete;

  ~FrequencyTracker() = default;

  void Reset() { impl_.Reset(); }

  void Update(int64_t count, Timestamp now);
  void Update(Timestamp now) { Update(1, now); }

  absl::optional<Frequency> Rate(Timestamp now) const;

 private:
  RateStatistics impl_;
};
}

#endif
