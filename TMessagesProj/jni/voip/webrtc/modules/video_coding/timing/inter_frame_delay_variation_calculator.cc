/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/timing/inter_frame_delay_variation_calculator.h"

#include "absl/types/optional.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "modules/include/module_common_types_public.h"

namespace webrtc {

namespace {
constexpr Frequency k90kHz = Frequency::KiloHertz(90);
}

InterFrameDelayVariationCalculator::InterFrameDelayVariationCalculator() {
  Reset();
}

void InterFrameDelayVariationCalculator::Reset() {
  prev_wall_clock_ = absl::nullopt;
  prev_rtp_timestamp_unwrapped_ = 0;
}

absl::optional<TimeDelta> InterFrameDelayVariationCalculator::Calculate(
    uint32_t rtp_timestamp,
    Timestamp now) {
  int64_t rtp_timestamp_unwrapped = unwrapper_.Unwrap(rtp_timestamp);

  if (!prev_wall_clock_) {
    prev_wall_clock_ = now;
    prev_rtp_timestamp_unwrapped_ = rtp_timestamp_unwrapped;

    return TimeDelta::Zero();
  }

  uint32_t cropped_prev = static_cast<uint32_t>(prev_rtp_timestamp_unwrapped_);
  if (rtp_timestamp_unwrapped < prev_rtp_timestamp_unwrapped_ ||
      !IsNewerTimestamp(rtp_timestamp, cropped_prev)) {
    return absl::nullopt;
  }

  TimeDelta delta_wall = now - *prev_wall_clock_;
  int64_t d_rtp_ticks = rtp_timestamp_unwrapped - prev_rtp_timestamp_unwrapped_;
  TimeDelta delta_rtp = d_rtp_ticks / k90kHz;

  TimeDelta inter_frame_delay_variation = delta_wall - delta_rtp;

  prev_wall_clock_ = now;
  prev_rtp_timestamp_unwrapped_ = rtp_timestamp_unwrapped;

  return inter_frame_delay_variation;
}

}
