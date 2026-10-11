/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/include/bitrate_adjuster.h"

#include <algorithm>
#include <cmath>

#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

const uint32_t BitrateAdjuster::kBitrateUpdateIntervalMs = 1000;

const uint32_t BitrateAdjuster::kBitrateUpdateFrameInterval = 30;

const float BitrateAdjuster::kBitrateTolerancePct = .1f;

const float BitrateAdjuster::kBytesPerMsToBitsPerSecond = 8 * 1000;

BitrateAdjuster::BitrateAdjuster(float min_adjusted_bitrate_pct,
                                 float max_adjusted_bitrate_pct)
    : min_adjusted_bitrate_pct_(min_adjusted_bitrate_pct),
      max_adjusted_bitrate_pct_(max_adjusted_bitrate_pct),
      bitrate_tracker_(1.5 * kBitrateUpdateIntervalMs,
                       kBytesPerMsToBitsPerSecond) {
  Reset();
}

void BitrateAdjuster::SetTargetBitrateBps(uint32_t bitrate_bps) {
  MutexLock lock(&mutex_);

  if (!IsWithinTolerance(bitrate_bps, target_bitrate_bps_) ||
      !IsWithinTolerance(bitrate_bps, last_adjusted_target_bitrate_bps_)) {
    adjusted_bitrate_bps_ = bitrate_bps;
    last_adjusted_target_bitrate_bps_ = bitrate_bps;
  }
  target_bitrate_bps_ = bitrate_bps;
}

uint32_t BitrateAdjuster::GetTargetBitrateBps() const {
  MutexLock lock(&mutex_);
  return target_bitrate_bps_;
}

uint32_t BitrateAdjuster::GetAdjustedBitrateBps() const {
  MutexLock lock(&mutex_);
  return adjusted_bitrate_bps_;
}

absl::optional<uint32_t> BitrateAdjuster::GetEstimatedBitrateBps() {
  MutexLock lock(&mutex_);
  return bitrate_tracker_.Rate(rtc::TimeMillis());
}

void BitrateAdjuster::Update(size_t frame_size) {
  MutexLock lock(&mutex_);
  uint32_t current_time_ms = rtc::TimeMillis();
  bitrate_tracker_.Update(frame_size, current_time_ms);
  UpdateBitrate(current_time_ms);
}

bool BitrateAdjuster::IsWithinTolerance(uint32_t bitrate_bps,
                                        uint32_t target_bitrate_bps) {
  if (target_bitrate_bps == 0) {
    return false;
  }
  float delta = std::abs(static_cast<float>(bitrate_bps) -
                         static_cast<float>(target_bitrate_bps));
  float delta_pct = delta / target_bitrate_bps;
  return delta_pct < kBitrateTolerancePct;
}

uint32_t BitrateAdjuster::GetMinAdjustedBitrateBps() const {
  return min_adjusted_bitrate_pct_ * target_bitrate_bps_;
}

uint32_t BitrateAdjuster::GetMaxAdjustedBitrateBps() const {
  return max_adjusted_bitrate_pct_ * target_bitrate_bps_;
}

void BitrateAdjuster::Reset() {
  MutexLock lock(&mutex_);
  target_bitrate_bps_ = 0;
  adjusted_bitrate_bps_ = 0;
  last_adjusted_target_bitrate_bps_ = 0;
  last_bitrate_update_time_ms_ = 0;
  frames_since_last_update_ = 0;
  bitrate_tracker_.Reset();
}

void BitrateAdjuster::UpdateBitrate(uint32_t current_time_ms) {
  uint32_t time_since_last_update_ms =
      current_time_ms - last_bitrate_update_time_ms_;

  ++frames_since_last_update_;
  if (time_since_last_update_ms < kBitrateUpdateIntervalMs ||
      frames_since_last_update_ < kBitrateUpdateFrameInterval) {
    return;
  }
  float target_bitrate_bps = target_bitrate_bps_;
  float estimated_bitrate_bps =
      bitrate_tracker_.Rate(current_time_ms).value_or(target_bitrate_bps);
  float error = target_bitrate_bps - estimated_bitrate_bps;

  if (estimated_bitrate_bps > target_bitrate_bps ||
      error > kBitrateTolerancePct * target_bitrate_bps) {

    float adjustment = .5 * error;
    float adjusted_bitrate_bps = target_bitrate_bps + adjustment;

    float min_bitrate_bps = GetMinAdjustedBitrateBps();
    float max_bitrate_bps = GetMaxAdjustedBitrateBps();
    adjusted_bitrate_bps = std::max(adjusted_bitrate_bps, min_bitrate_bps);
    adjusted_bitrate_bps = std::min(adjusted_bitrate_bps, max_bitrate_bps);

    float last_adjusted_bitrate_bps = adjusted_bitrate_bps_;
    if (adjusted_bitrate_bps != last_adjusted_bitrate_bps) {
      RTC_LOG(LS_VERBOSE) << "Adjusting encoder bitrate:"
                             "\n  target_bitrate:"
                          << static_cast<uint32_t>(target_bitrate_bps)
                          << "\n  estimated_bitrate:"
                          << static_cast<uint32_t>(estimated_bitrate_bps)
                          << "\n  last_adjusted_bitrate:"
                          << static_cast<uint32_t>(last_adjusted_bitrate_bps)
                          << "\n  adjusted_bitrate:"
                          << static_cast<uint32_t>(adjusted_bitrate_bps);
      adjusted_bitrate_bps_ = adjusted_bitrate_bps;
    }
  }
  last_bitrate_update_time_ms_ = current_time_ms;
  frames_since_last_update_ = 0;
  last_adjusted_target_bitrate_bps_ = target_bitrate_bps_;
}

}
