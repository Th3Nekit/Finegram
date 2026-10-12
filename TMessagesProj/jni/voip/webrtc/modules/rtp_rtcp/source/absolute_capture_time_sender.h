/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ABSOLUTE_CAPTURE_TIME_SENDER_H_
#define MODULES_RTP_RTCP_SOURCE_ABSOLUTE_CAPTURE_TIME_SENDER_H_

#include "api/array_view.h"
#include "api/rtp_headers.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

class AbsoluteCaptureTimeSender {
 public:
  static constexpr TimeDelta kInterpolationMaxInterval = TimeDelta::Seconds(1);
  static constexpr TimeDelta kInterpolationMaxError = TimeDelta::Millis(1);

  explicit AbsoluteCaptureTimeSender(Clock* clock);

  static uint32_t GetSource(uint32_t ssrc,
                            rtc::ArrayView<const uint32_t> csrcs);

  absl::optional<AbsoluteCaptureTime> OnSendPacket(
      uint32_t source,
      uint32_t rtp_timestamp,
      int rtp_clock_frequency_hz,
      NtpTime absolute_capture_time,
      absl::optional<int64_t> estimated_capture_clock_offset,
      bool force = false);

  [[deprecated]] absl::optional<AbsoluteCaptureTime> OnSendPacket(
      uint32_t source,
      uint32_t rtp_timestamp,
      uint32_t rtp_clock_frequency,
      uint64_t absolute_capture_timestamp,
      absl::optional<int64_t> estimated_capture_clock_offset);

 private:
  bool ShouldSendExtension(
      Timestamp send_time,
      uint32_t source,
      uint32_t rtp_timestamp,
      int rtp_clock_frequency_hz,
      NtpTime absolute_capture_time,
      absl::optional<int64_t> estimated_capture_clock_offset) const;

  Clock* const clock_;

  Timestamp last_send_time_ = Timestamp::MinusInfinity();

  uint32_t last_source_;
  uint32_t last_rtp_timestamp_;
  int last_rtp_clock_frequency_hz_;
  NtpTime last_absolute_capture_time_;
  absl::optional<int64_t> last_estimated_capture_clock_offset_;
};

}

#endif
