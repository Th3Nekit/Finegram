/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_INCLUDE_BITRATE_ADJUSTER_H_
#define COMMON_VIDEO_INCLUDE_BITRATE_ADJUSTER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_EXPORT BitrateAdjuster {
 public:

  BitrateAdjuster(float min_adjusted_bitrate_pct,
                  float max_adjusted_bitrate_pct);
  virtual ~BitrateAdjuster() {}

  static const uint32_t kBitrateUpdateIntervalMs;
  static const uint32_t kBitrateUpdateFrameInterval;
  static const float kBitrateTolerancePct;
  static const float kBytesPerMsToBitsPerSecond;

  void SetTargetBitrateBps(uint32_t bitrate_bps);
  uint32_t GetTargetBitrateBps() const;

  uint32_t GetAdjustedBitrateBps() const;

  absl::optional<uint32_t> GetEstimatedBitrateBps();

  void Update(size_t frame_size);

 private:

  bool IsWithinTolerance(uint32_t bitrate_bps, uint32_t target_bitrate_bps);

  uint32_t GetMinAdjustedBitrateBps() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  uint32_t GetMaxAdjustedBitrateBps() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void Reset();
  void UpdateBitrate(uint32_t current_time_ms)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  const float min_adjusted_bitrate_pct_;
  const float max_adjusted_bitrate_pct_;

  volatile uint32_t target_bitrate_bps_ RTC_GUARDED_BY(mutex_);

  volatile uint32_t adjusted_bitrate_bps_ RTC_GUARDED_BY(mutex_);

  volatile uint32_t last_adjusted_target_bitrate_bps_ RTC_GUARDED_BY(mutex_);

  RateStatistics bitrate_tracker_ RTC_GUARDED_BY(mutex_);

  uint32_t last_bitrate_update_time_ms_ RTC_GUARDED_BY(mutex_);

  uint32_t frames_since_last_update_ RTC_GUARDED_BY(mutex_);
};

}

#endif
