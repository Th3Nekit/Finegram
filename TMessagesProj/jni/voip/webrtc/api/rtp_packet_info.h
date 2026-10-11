/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_PACKET_INFO_H_
#define API_RTP_PACKET_INFO_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT RtpPacketInfo {
 public:
  RtpPacketInfo();

  RtpPacketInfo(uint32_t ssrc,
                std::vector<uint32_t> csrcs,
                uint32_t rtp_timestamp,
                Timestamp receive_time);

  RtpPacketInfo(const RTPHeader& rtp_header, Timestamp receive_time);

  RtpPacketInfo(const RtpPacketInfo& other) = default;
  RtpPacketInfo(RtpPacketInfo&& other) = default;
  RtpPacketInfo& operator=(const RtpPacketInfo& other) = default;
  RtpPacketInfo& operator=(RtpPacketInfo&& other) = default;

  uint32_t ssrc() const { return ssrc_; }
  void set_ssrc(uint32_t value) { ssrc_ = value; }

  const std::vector<uint32_t>& csrcs() const { return csrcs_; }
  void set_csrcs(std::vector<uint32_t> value) { csrcs_ = std::move(value); }

  uint32_t rtp_timestamp() const { return rtp_timestamp_; }
  void set_rtp_timestamp(uint32_t value) { rtp_timestamp_ = value; }

  Timestamp receive_time() const { return receive_time_; }
  void set_receive_time(Timestamp value) { receive_time_ = value; }

  absl::optional<uint8_t> audio_level() const { return audio_level_; }
  RtpPacketInfo& set_audio_level(absl::optional<uint8_t> value) {
    audio_level_ = value;
    return *this;
  }

  const absl::optional<AbsoluteCaptureTime>& absolute_capture_time() const {
    return absolute_capture_time_;
  }
  RtpPacketInfo& set_absolute_capture_time(
      const absl::optional<AbsoluteCaptureTime>& value) {
    absolute_capture_time_ = value;
    return *this;
  }

  const absl::optional<TimeDelta>& local_capture_clock_offset() const {
    return local_capture_clock_offset_;
  }
  RtpPacketInfo& set_local_capture_clock_offset(
      absl::optional<TimeDelta> value) {
    local_capture_clock_offset_ = value;
    return *this;
  }

 private:

  uint32_t ssrc_;
  std::vector<uint32_t> csrcs_;
  uint32_t rtp_timestamp_;

  Timestamp receive_time_;

  absl::optional<uint8_t> audio_level_;

  absl::optional<AbsoluteCaptureTime> absolute_capture_time_;

  absl::optional<TimeDelta> local_capture_clock_offset_;
};

bool operator==(const RtpPacketInfo& lhs, const RtpPacketInfo& rhs);

inline bool operator!=(const RtpPacketInfo& lhs, const RtpPacketInfo& rhs) {
  return !(lhs == rhs);
}

}

#endif
