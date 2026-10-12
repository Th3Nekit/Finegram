/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_DEFINES_H_
#define MODULES_RTP_RTCP_INCLUDE_RTP_RTCP_DEFINES_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/array_view.h"
#include "api/audio_codecs/audio_format.h"
#include "api/rtp_headers.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remote_estimate.h"
#include "system_wrappers/include/clock.h"

#define RTCP_CNAME_SIZE 256  // RFC 3550 page 44, including null termination
#define IP_PACKET_SIZE 1500  // we assume ethernet

namespace webrtc {
class RtpPacket;
class RtpPacketToSend;
namespace rtcp {
class TransportFeedback;
}

const int kVideoPayloadTypeFrequency = 90000;

const int kBogusRtpRateForAudioRtcp = 8000;

const uint8_t kRtpHeaderSize = 12;

bool IsLegalMidName(absl::string_view name);
bool IsLegalRsidName(absl::string_view name);

enum RTPExtensionType : int {
  kRtpExtensionNone,
  kRtpExtensionTransmissionTimeOffset,
  kRtpExtensionAudioLevel,
  kRtpExtensionCsrcAudioLevel,
  kRtpExtensionInbandComfortNoise,
  kRtpExtensionAbsoluteSendTime,
  kRtpExtensionAbsoluteCaptureTime,
  kRtpExtensionVideoRotation,
  kRtpExtensionTransportSequenceNumber,
  kRtpExtensionTransportSequenceNumber02,
  kRtpExtensionPlayoutDelay,
  kRtpExtensionVideoContentType,
  kRtpExtensionVideoLayersAllocation,
  kRtpExtensionVideoTiming,
  kRtpExtensionRtpStreamId,
  kRtpExtensionRepairedRtpStreamId,
  kRtpExtensionMid,
  kRtpExtensionGenericFrameDescriptor,
  kRtpExtensionGenericFrameDescriptor00 [[deprecated]] =
      kRtpExtensionGenericFrameDescriptor,
  kRtpExtensionDependencyDescriptor,
  kRtpExtensionGenericFrameDescriptor02 [[deprecated]] =
      kRtpExtensionDependencyDescriptor,
  kRtpExtensionColorSpace,
  kRtpExtensionVideoFrameTrackingId,
  kRtpExtensionNumberOfExtensions
};

enum RTCPAppSubTypes { kAppSubtypeBwe = 0x00 };

enum RTCPPacketType : uint32_t {
  kRtcpReport = 0x0001,
  kRtcpSr = 0x0002,
  kRtcpRr = 0x0004,
  kRtcpSdes = 0x0008,
  kRtcpBye = 0x0010,
  kRtcpPli = 0x0020,
  kRtcpNack = 0x0040,
  kRtcpFir = 0x0080,
  kRtcpTmmbr = 0x0100,
  kRtcpTmmbn = 0x0200,
  kRtcpSrReq = 0x0400,
  kRtcpLossNotification = 0x2000,
  kRtcpRemb = 0x10000,
  kRtcpTransmissionTimeOffset = 0x20000,
  kRtcpXrReceiverReferenceTime = 0x40000,
  kRtcpXrDlrrReportBlock = 0x80000,
  kRtcpTransportFeedback = 0x100000,
  kRtcpXrTargetBitrate = 0x200000
};

enum class KeyFrameReqMethod : uint8_t {
  kNone,
  kPliRtcp,
  kFirRtcp
};

enum RtxMode {
  kRtxOff = 0x0,
  kRtxRetransmitted = 0x1,
  kRtxRedundantPayloads = 0x2

};

const size_t kRtxHeaderSize = 2;

struct RtpState {
  uint16_t sequence_number = 0;
  uint32_t start_timestamp = 0;
  uint32_t timestamp = 0;
  Timestamp capture_time = Timestamp::MinusInfinity();
  Timestamp last_timestamp_time = Timestamp::MinusInfinity();
  bool ssrc_has_acked = false;
};

class RtcpIntraFrameObserver {
 public:
  virtual ~RtcpIntraFrameObserver() {}

  virtual void OnReceivedIntraFrameRequest(uint32_t ssrc) = 0;
};

class RtcpLossNotificationObserver {
 public:
  virtual ~RtcpLossNotificationObserver() = default;

  virtual void OnReceivedLossNotification(uint32_t ssrc,
                                          uint16_t seq_num_of_last_decodable,
                                          uint16_t seq_num_of_last_received,
                                          bool decodability_flag) = 0;
};

class NetworkLinkRtcpObserver {
 public:
  virtual ~NetworkLinkRtcpObserver() = default;

  virtual void OnTransportFeedback(Timestamp receive_time,
                                   const rtcp::TransportFeedback& feedback) {}
  virtual void OnReceiverEstimatedMaxBitrate(Timestamp receive_time,
                                             DataRate bitrate) {}

  virtual void OnReport(Timestamp receive_time,
                        rtc::ArrayView<const ReportBlockData> report_blocks) {}
  virtual void OnRttUpdate(Timestamp receive_time, TimeDelta rtt) {}
};

static constexpr size_t kNumMediaTypes = 5;
enum class RtpPacketMediaType : size_t {
  kAudio,
  kVideo,
  kRetransmission,
  kForwardErrorCorrection,
  kPadding = kNumMediaTypes - 1,

};

struct RtpPacketSendInfo {
  uint16_t transport_sequence_number = 0;
  absl::optional<uint32_t> media_ssrc;
  uint16_t rtp_sequence_number = 0;
  uint32_t rtp_timestamp = 0;
  size_t length = 0;
  absl::optional<RtpPacketMediaType> packet_type;
  PacedPacketInfo pacing_info;
};

class NetworkStateEstimateObserver {
 public:
  virtual void OnRemoteNetworkEstimate(NetworkStateEstimate estimate) = 0;
  virtual ~NetworkStateEstimateObserver() = default;
};

class TransportFeedbackObserver {
 public:
  virtual ~TransportFeedbackObserver() = default;

  virtual void OnAddPacket(const RtpPacketSendInfo& packet_info) = 0;
};

class RtcpFeedbackSenderInterface {
 public:
  virtual ~RtcpFeedbackSenderInterface() = default;
  virtual void SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets) = 0;
  virtual void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) = 0;
  virtual void UnsetRemb() = 0;
};

class StreamFeedbackObserver {
 public:
  struct StreamPacketInfo {
    bool received;

    absl::optional<uint32_t> ssrc;
    uint16_t rtp_sequence_number;
    bool is_retransmission;
  };
  virtual ~StreamFeedbackObserver() = default;

  virtual void OnPacketFeedbackVector(
      std::vector<StreamPacketInfo> packet_feedback_vector) = 0;
};

class StreamFeedbackProvider {
 public:
  virtual void RegisterStreamFeedbackObserver(
      std::vector<uint32_t> ssrcs,
      StreamFeedbackObserver* observer) = 0;
  virtual void DeRegisterStreamFeedbackObserver(
      StreamFeedbackObserver* observer) = 0;
  virtual ~StreamFeedbackProvider() = default;
};

class RtcpRttStats {
 public:
  virtual void OnRttUpdate(int64_t rtt) = 0;

  virtual int64_t LastProcessedRtt() const = 0;

  virtual ~RtcpRttStats() {}
};

struct RtpPacketCounter {
  RtpPacketCounter()
      : header_bytes(0), payload_bytes(0), padding_bytes(0), packets(0) {}

  explicit RtpPacketCounter(const RtpPacket& packet);
  explicit RtpPacketCounter(const RtpPacketToSend& packet_to_send);

  void Add(const RtpPacketCounter& other) {
    header_bytes += other.header_bytes;
    payload_bytes += other.payload_bytes;
    padding_bytes += other.padding_bytes;
    packets += other.packets;
    total_packet_delay += other.total_packet_delay;
  }

  bool operator==(const RtpPacketCounter& other) const {
    return header_bytes == other.header_bytes &&
           payload_bytes == other.payload_bytes &&
           padding_bytes == other.padding_bytes && packets == other.packets &&
           total_packet_delay == other.total_packet_delay;
  }

  void AddPacket(const RtpPacket& packet);
  void AddPacket(const RtpPacketToSend& packet_to_send);

  size_t TotalBytes() const {
    return header_bytes + payload_bytes + padding_bytes;
  }

  size_t header_bytes;
  size_t payload_bytes;
  size_t padding_bytes;
  size_t packets;

  webrtc::TimeDelta total_packet_delay = webrtc::TimeDelta::Zero();
};

struct StreamDataCounters {
  StreamDataCounters();

  void Add(const StreamDataCounters& other) {
    transmitted.Add(other.transmitted);
    retransmitted.Add(other.retransmitted);
    fec.Add(other.fec);
    if (other.first_packet_time < first_packet_time) {

      first_packet_time = other.first_packet_time;
    }
  }

  void MaybeSetFirstPacketTime(Timestamp now) {
    if (first_packet_time == Timestamp::PlusInfinity()) {
      first_packet_time = now;
    }
  }

  TimeDelta TimeSinceFirstPacket(Timestamp now) const {
    return first_packet_time == Timestamp::PlusInfinity()
               ? TimeDelta::Zero()
               : now - first_packet_time;
  }

  size_t MediaPayloadBytes() const {
    return transmitted.payload_bytes - retransmitted.payload_bytes -
           fec.payload_bytes;
  }

  Timestamp first_packet_time = Timestamp::PlusInfinity();

  RtpPacketCounter transmitted;
  RtpPacketCounter retransmitted;
  RtpPacketCounter fec;
};

class RtpSendRates {
  template <std::size_t... Is>
  constexpr std::array<DataRate, sizeof...(Is)> make_zero_array(
      std::index_sequence<Is...>) {
    return {{(static_cast<void>(Is), DataRate::Zero())...}};
  }

 public:
  RtpSendRates()
      : send_rates_(
            make_zero_array(std::make_index_sequence<kNumMediaTypes>())) {}
  RtpSendRates(const RtpSendRates& rhs) = default;
  RtpSendRates& operator=(const RtpSendRates&) = default;

  DataRate& operator[](RtpPacketMediaType type) {
    return send_rates_[static_cast<size_t>(type)];
  }
  const DataRate& operator[](RtpPacketMediaType type) const {
    return send_rates_[static_cast<size_t>(type)];
  }
  DataRate Sum() const {
    return absl::c_accumulate(send_rates_, DataRate::Zero());
  }

 private:
  std::array<DataRate, kNumMediaTypes> send_rates_;
};

class StreamDataCountersCallback {
 public:
  virtual ~StreamDataCountersCallback() {}

  virtual void DataCountersUpdated(const StreamDataCounters& counters,
                                   uint32_t ssrc) = 0;
};

struct RtpReceiveStats {

  int32_t packets_lost = 0;

  uint32_t jitter = 0;

  webrtc::TimeDelta interarrival_jitter = webrtc::TimeDelta::Zero();

  absl::optional<Timestamp> last_packet_received;

  RtpPacketCounter packet_counter;
};

class BitrateStatisticsObserver {
 public:
  virtual ~BitrateStatisticsObserver() {}

  virtual void Notify(uint32_t total_bitrate_bps,
                      uint32_t retransmit_bitrate_bps,
                      uint32_t ssrc) = 0;
};

class SendPacketObserver {
 public:
  virtual ~SendPacketObserver() = default;
  virtual void OnSendPacket(absl::optional<uint16_t> packet_id,
                            Timestamp capture_time,
                            uint32_t ssrc) = 0;
};

}
#endif
