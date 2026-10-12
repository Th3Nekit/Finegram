/*
 *  Copyright 2021 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/framerate_controller.h"

#include <limits>

#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {
constexpr double kMinFramerate = 0.5;
}

FramerateController::FramerateController()
    : FramerateController(std::numeric_limits<double>::max()) {}

FramerateController::FramerateController(double max_framerate)
    : max_framerate_(max_framerate) {}

FramerateController::~FramerateController() {}

void FramerateController::SetMaxFramerate(double max_framerate) {
  max_framerate_ = max_framerate;
}

double FramerateController::GetMaxFramerate() const {
  return max_framerate_;
}

bool FramerateController::ShouldDropFrame(int64_t in_timestamp_ns) {
  if (max_framerate_ < kMinFramerate)
    return true;

  int64_t frame_interval_ns = rtc::kNumNanosecsPerSec / max_framerate_;
  if (frame_interval_ns <= 0) {

    return false;
  }

  if (next_frame_timestamp_ns_) {

    const int64_t time_until_next_frame_ns =
        (*next_frame_timestamp_ns_ - in_timestamp_ns);

    if (std::abs(time_until_next_frame_ns) < 2 * frame_interval_ns) {

      if (time_until_next_frame_ns > 0)
        return true;

      *next_frame_timestamp_ns_ += frame_interval_ns;
      return false;
    }
  }

  next_frame_timestamp_ns_ = in_timestamp_ns + frame_interval_ns / 2;
  return false;
}

void FramerateController::Reset() {
  max_framerate_ = std::numeric_limits<double>::max();
  next_frame_timestamp_ns_ = absl::nullopt;
}

void FramerateController::KeepFrame(int64_t in_timestamp_ns) {
  if (ShouldDropFrame(in_timestamp_ns)) {
    if (max_framerate_ < kMinFramerate)
      return;

    int64_t frame_interval_ns = rtc::kNumNanosecsPerSec / max_framerate_;
    if (next_frame_timestamp_ns_)
      *next_frame_timestamp_ns_ += frame_interval_ns;
  }
}

}
