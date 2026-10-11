/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_ORDER_H_
#define LOGGING_RTC_EVENT_LOG_RTC_EVENT_PROCESSOR_ORDER_H_

#include <stdint.h>

#include "absl/types/optional.h"
#include "api/function_view.h"
#include "logging/rtc_event_log/events/logged_rtp_rtcp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"

namespace webrtc {

enum class TypeOrder {
  Start,

  StreamConfig,
  IceCondidateConfig,
  IceCandidateEvent,
  DtlsTransportState,
  DtlsWritable,
  RouteChange,

  RtpIn,
  RtcpIn,
  GenericPacketIn,
  GenericAckIn,

  BweRemoteEstimate,
  BweProbeFailure,
  BweProbeSuccess,
  BweDelayBased,
  BweLossBased,
  BweProbeCreated,

  AudioNetworkAdaptation,
  NetEqSetMinDelay,
  AudioPlayout,
  FrameDecoded,

  RtpOut,
  RtcpOut,
  GenericPacketOut,

  AlrState,
  Stop,
};

template <typename T>
class TieBreaker {
  static_assert(sizeof(T) != sizeof(T),
                "Specialize TieBreaker to define an order for the event type.");
};

template <>
class TieBreaker<LoggedStartEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::Start);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedStartEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedStopEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::Stop);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedStopEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAudioRecvConfig> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::StreamConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAudioRecvConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAudioSendConfig> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::StreamConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAudioSendConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedVideoRecvConfig> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::StreamConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedVideoRecvConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedVideoSendConfig> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::StreamConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedVideoSendConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedIceCandidatePairConfig> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::IceCondidateConfig);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedIceCandidatePairConfig&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedIceCandidatePairEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::IceCandidateEvent);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedIceCandidatePairEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedDtlsTransportState> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::DtlsTransportState);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedDtlsTransportState&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedDtlsWritableState> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::DtlsWritable);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedDtlsWritableState&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRouteChangeEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RouteChange);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRouteChangeEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRemoteEstimateEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::BweRemoteEstimate);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRemoteEstimateEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweProbeFailureEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::BweProbeFailure);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweProbeFailureEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweProbeSuccessEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::BweProbeSuccess);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweProbeSuccessEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweDelayBasedUpdate> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::BweDelayBased);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweDelayBasedUpdate&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweLossBasedUpdate> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::BweLossBased);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweLossBasedUpdate&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedBweProbeClusterCreatedEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::BweProbeCreated);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedBweProbeClusterCreatedEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAudioNetworkAdaptationEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::AudioNetworkAdaptation);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAudioNetworkAdaptationEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedNetEqSetMinimumDelayEvent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::NetEqSetMinDelay);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedNetEqSetMinimumDelayEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAudioPlayoutEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::AudioPlayout);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAudioPlayoutEvent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedFrameDecoded> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::FrameDecoded);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedFrameDecoded&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedGenericPacketReceived> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::GenericPacketIn);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedGenericPacketReceived&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedGenericAckReceived> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::GenericAckIn);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedGenericAckReceived&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedGenericPacketSent> {
 public:
  static constexpr int type_order =
      static_cast<int>(TypeOrder::GenericPacketOut);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedGenericPacketSent&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtpPacket> {
 public:
  static constexpr int type_order(PacketDirection direction) {
    return static_cast<int>(direction == PacketDirection::kIncomingPacket
                                ? TypeOrder::RtpIn
                                : TypeOrder::RtpOut);
  }
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtpPacket& p) {
    return p.header.extension.hasTransportSequenceNumber
               ? p.header.extension.transportSequenceNumber
               : absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedPacketInfo> {
 public:
  static constexpr int type_order(PacketDirection direction) {
    return static_cast<int>(direction == PacketDirection::kIncomingPacket
                                ? TypeOrder::RtpIn
                                : TypeOrder::RtpOut);
  }
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedPacketInfo& p) {
    return p.has_transport_seq_no ? p.transport_seq_no
                                  : absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtpPacketIncoming> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RtpIn);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtpPacketIncoming& p) {
    return p.rtp.header.extension.hasTransportSequenceNumber
               ? p.rtp.header.extension.transportSequenceNumber
               : absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtpPacketOutgoing> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RtpOut);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtpPacketOutgoing& p) {
    return p.rtp.header.extension.hasTransportSequenceNumber
               ? p.rtp.header.extension.transportSequenceNumber
               : absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtcpPacketIncoming> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RtcpIn);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtcpPacketIncoming&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtcpPacketOutgoing> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::RtcpOut);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtcpPacketOutgoing&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtcpPacketTransportFeedback> {
 public:
  static constexpr int type_order(PacketDirection direction) {
    return static_cast<int>(direction == PacketDirection::kIncomingPacket
                                ? TypeOrder::RtcpIn
                                : TypeOrder::RtcpOut);
  }
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtcpPacketTransportFeedback&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedRtcpPacketReceiverReport> {
 public:
  static constexpr int type_order(PacketDirection direction) {
    return static_cast<int>(direction == PacketDirection::kIncomingPacket
                                ? TypeOrder::RtcpIn
                                : TypeOrder::RtcpOut);
  }
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedRtcpPacketReceiverReport&) {
    return absl::optional<uint16_t>();
  }
};

template <>
class TieBreaker<LoggedAlrStateEvent> {
 public:
  static constexpr int type_order = static_cast<int>(TypeOrder::AlrState);
  static absl::optional<uint16_t> transport_seq_num_accessor(
      const LoggedAlrStateEvent&) {
    return absl::optional<uint16_t>();
  }
};

}

#endif
