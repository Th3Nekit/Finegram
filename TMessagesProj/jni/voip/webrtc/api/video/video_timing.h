/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_TIMING_H_
#define API_VIDEO_VIDEO_TIMING_H_

#include <stdint.h>

#include <limits>
#include <string>

#include "api/units/time_delta.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT VideoSendTiming {
  enum TimingFrameFlags : uint8_t {
    kNotTriggered = 0,

    kTriggeredByTimer = 1 << 0,
    kTriggeredBySize = 1 << 1,
    kInvalid = std::numeric_limits<uint8_t>::max()
  };

  static uint16_t GetDeltaCappedMs(int64_t base_ms, int64_t time_ms);
  static uint16_t GetDeltaCappedMs(TimeDelta delta);

  uint16_t encode_start_delta_ms;
  uint16_t encode_finish_delta_ms;
  uint16_t packetization_finish_delta_ms;
  uint16_t pacer_exit_delta_ms;
  uint16_t network_timestamp_delta_ms;
  uint16_t network2_timestamp_delta_ms;
  uint8_t flags = TimingFrameFlags::kInvalid;
};

struct RTC_EXPORT TimingFrameInfo {
  TimingFrameInfo();

  int64_t EndToEndDelay() const;

  bool IsLongerThan(const TimingFrameInfo& other) const;

  bool IsOutlier() const;

  bool IsTimerTriggered() const;

  bool IsInvalid() const;

  std::string ToString() const;

  bool operator<(const TimingFrameInfo& other) const;

  bool operator<=(const TimingFrameInfo& other) const;

  uint32_t rtp_timestamp;

  int64_t capture_time_ms;
  int64_t encode_start_ms;
  int64_t encode_finish_ms;
  int64_t packetization_finish_ms;
  int64_t pacer_exit_ms;

  int64_t network_timestamp_ms;
  int64_t network2_timestamp_ms;
  int64_t receive_start_ms;
  int64_t receive_finish_ms;
  int64_t decode_start_ms;
  int64_t decode_finish_ms;
  int64_t render_time_ms;

  uint8_t flags;
};

class RTC_EXPORT VideoPlayoutDelay {
 public:

  static constexpr TimeDelta kMax = TimeDelta::Millis(10) * 0xFFF;

  static VideoPlayoutDelay Minimal() {
    return VideoPlayoutDelay(TimeDelta::Zero(), TimeDelta::Zero());
  }

  VideoPlayoutDelay() = default;
  VideoPlayoutDelay(const VideoPlayoutDelay&) = default;
  VideoPlayoutDelay& operator=(const VideoPlayoutDelay&) = default;
  VideoPlayoutDelay(TimeDelta min, TimeDelta max);

  bool Set(TimeDelta min, TimeDelta max);

  TimeDelta min() const { return min_; }
  TimeDelta max() const { return max_; }

  friend bool operator==(const VideoPlayoutDelay& lhs,
                         const VideoPlayoutDelay& rhs) {
    return lhs.min_ == rhs.min_ && lhs.max_ == rhs.max_;
  }

 private:
  TimeDelta min_ = TimeDelta::Zero();
  TimeDelta max_ = kMax;
};

}

#endif
