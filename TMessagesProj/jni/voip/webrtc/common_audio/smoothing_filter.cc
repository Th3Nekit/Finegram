/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_audio/smoothing_filter.h"

#include <math.h>

#include <cmath>

#include "rtc_base/checks.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

SmoothingFilterImpl::SmoothingFilterImpl(int init_time_ms)
    : init_time_ms_(init_time_ms),

      init_factor_(init_time_ms_ == 0
                       ? 0.0f
                       : powf(init_time_ms_, -1.0f / init_time_ms_)),

      init_const_(init_time_ms_ == 0
                      ? 0.0f
                      : init_time_ms_ -
                            powf(init_time_ms_, 1.0f - 1.0f / init_time_ms_)) {
  UpdateAlpha(init_time_ms_);
}

SmoothingFilterImpl::~SmoothingFilterImpl() = default;

void SmoothingFilterImpl::AddSample(float sample) {
  const int64_t now_ms = rtc::TimeMillis();

  if (!init_end_time_ms_) {

    state_ = last_sample_ = sample;
    init_end_time_ms_ = now_ms + init_time_ms_;
    last_state_time_ms_ = now_ms;
    return;
  }

  ExtrapolateLastSample(now_ms);
  last_sample_ = sample;
}

absl::optional<float> SmoothingFilterImpl::GetAverage() {
  if (!init_end_time_ms_) {

    return absl::nullopt;
  }
  ExtrapolateLastSample(rtc::TimeMillis());
  return state_;
}

bool SmoothingFilterImpl::SetTimeConstantMs(int time_constant_ms) {
  if (!init_end_time_ms_ || last_state_time_ms_ < *init_end_time_ms_) {
    return false;
  }
  UpdateAlpha(time_constant_ms);
  return true;
}

void SmoothingFilterImpl::UpdateAlpha(int time_constant_ms) {
  alpha_ = time_constant_ms == 0 ? 0.0f : std::exp(-1.0f / time_constant_ms);
}

void SmoothingFilterImpl::ExtrapolateLastSample(int64_t time_ms) {
  RTC_DCHECK_GE(time_ms, last_state_time_ms_);
  RTC_DCHECK(init_end_time_ms_);

  float multiplier = 0.0f;

  if (time_ms <= *init_end_time_ms_) {

    if (init_time_ms_ == 0) {

      multiplier = 0.0f;
    } else if (init_time_ms_ == 1) {

      multiplier = std::exp(last_state_time_ms_ - time_ms);
    } else {
      multiplier = std::exp(
          -(powf(init_factor_, last_state_time_ms_ - *init_end_time_ms_) -
            powf(init_factor_, time_ms - *init_end_time_ms_)) /
          init_const_);
    }
  } else {
    if (last_state_time_ms_ < *init_end_time_ms_) {

      ExtrapolateLastSample(*init_end_time_ms_);

    }
    multiplier = powf(alpha_, time_ms - last_state_time_ms_);
  }

  state_ = multiplier * state_ + (1.0f - multiplier) * last_sample_;
  last_state_time_ms_ = time_ms;
}

}
