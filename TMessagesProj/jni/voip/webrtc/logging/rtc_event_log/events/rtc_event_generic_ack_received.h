/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_GENERIC_ACK_RECEIVED_H_
#define LOGGING_RTC_EVENT_LOG_EVENTS_RTC_EVENT_GENERIC_ACK_RECEIVED_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/rtc_event_log/rtc_event.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/events/rtc_event_field_encoding_parser.h"

namespace webrtc {

struct LoggedGenericAckReceived {
  LoggedGenericAckReceived() = default;
  LoggedGenericAckReceived(Timestamp timestamp,
                           int64_t packet_number,
                           int64_t acked_packet_number,
                           absl::optional<int64_t> receive_acked_packet_time_ms)
      : timestamp(timestamp),
        packet_number(packet_number),
        acked_packet_number(acked_packet_number),
        receive_acked_packet_time_ms(receive_acked_packet_time_ms) {}

  int64_t log_time_us() const { return timestamp.us(); }
  int64_t log_time_ms() const { return timestamp.ms(); }
  Timestamp log_time() const { return timestamp; }

  Timestamp timestamp = Timestamp::MinusInfinity();
  int64_t packet_number;
  int64_t acked_packet_number;
  absl::optional<int64_t> receive_acked_packet_time_ms;
};

struct AckedPacket {

  int64_t packet_number;

  absl::optional<int64_t> receive_acked_packet_time_ms;
};

class RtcEventGenericAckReceived final : public RtcEvent {
 public:
  static constexpr Type kType = Type::GenericAckReceived;

  static std::vector<std::unique_ptr<RtcEventGenericAckReceived>> CreateLogs(
      int64_t packet_number,
      const std::vector<AckedPacket>& acked_packets);

  ~RtcEventGenericAckReceived() override;

  std::unique_ptr<RtcEventGenericAckReceived> Copy() const;

  Type GetType() const override { return kType; }
  bool IsConfigEvent() const override { return false; }

  int64_t packet_number() const { return packet_number_; }

  int64_t acked_packet_number() const { return acked_packet_number_; }

  absl::optional<int64_t> receive_acked_packet_time_ms() const {
    return receive_acked_packet_time_ms_;
  }

  static std::string Encode(rtc::ArrayView<const RtcEvent*> batch) {

    return "";
  }

  static RtcEventLogParseStatus Parse(
      absl::string_view encoded_bytes,
      bool batched,
      std::vector<LoggedGenericAckReceived>& output) {

    return RtcEventLogParseStatus::Error("Not Implemented", __FILE__, __LINE__);
  }

 private:
  RtcEventGenericAckReceived(const RtcEventGenericAckReceived& packet);

  RtcEventGenericAckReceived(
      int64_t timestamp_us,
      int64_t packet_number,
      int64_t acked_packet_number,
      absl::optional<int64_t> receive_acked_packet_time_ms);

  const int64_t packet_number_;
  const int64_t acked_packet_number_;
  const absl::optional<int64_t> receive_acked_packet_time_ms_;
};

}

#endif
