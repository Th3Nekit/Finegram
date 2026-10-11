/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BITRATE_TRACKER_H_
#define RTC_BASE_BITRATE_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT BitrateTracker {
 public:

  explicit BitrateTracker(TimeDelta max_window_size);

  BitrateTracker(const BitrateTracker&) = default;
  BitrateTracker(BitrateTracker&&) = default;
  BitrateTracker& operator=(const BitrateTracker&) = delete;
  BitrateTracker& operator=(BitrateTracker&&) = delete;

  ~BitrateTracker() = default;

  void Reset() { impl_.Reset(); }

  void Update(int64_t bytes, Timestamp now);
  void Update(DataSize size, Timestamp now) { Update(size.bytes(), now); }

  absl::optional<DataRate> Rate(Timestamp now) const;

  bool SetWindowSize(TimeDelta window_size, Timestamp now);

 private:
  RateStatistics impl_;
};
}

#endif
