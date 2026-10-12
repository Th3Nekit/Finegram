/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_RTCP_INTERFACE_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_RTCP_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/field_trials_view.h"
#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "api/video/video_bitrate_allocation.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtp_packet_sender.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/source/rtp_sequence_number_map.h"
#include "modules/rtp_rtcp/source/video_fec_generator.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

class FrameEncryptorInterface;
class RateLimiter;
class RtcEventLog;
class RTPSender;
class Transport;
class VideoBitrateAllocationObserver;

class RtpRtcpInterface : public RtcpFeedbackSenderInterface {
 public:
  struct Configuration {
    Configuration() = default;
    Configuration(Configuration&& rhs) = default;

    Configuration(const Configuration&) = delete;
    Configuration& operator=(const Configuration&) = delete;

    bool audio = false;
    bool receiver_only = false;

    Clock* clock = nullptr;

    ReceiveStatisticsProvider* receive_statistics = nullptr;

    Transport* outgoing_transport = nullptr;

    RtcpIntraFrameObserver* intra_frame_callback = nullptr;

    RtcpLossNotificationObserver* rtcp_loss_notification_observer = nullptr;

    NetworkLinkRtcpObserver* network_link_rtcp_observer = nullptr;

    NetworkStateEstimateObserver* network_state_estimate_observer = nullptr;
    TransportFeedbackObserver* transport_feedback_callback = nullptr;
    VideoBitrateAllocationObserver* bitrate_allocation_observer = nullptr;
    RtcpRttStats* rtt_stats = nullptr;
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer = nullptr;

    RtcpCnameCallback* rtcp_cname_callback = nullptr;
    ReportBlockDataObserver* report_block_data_observer = nullptr;

    RtpPacketSender* paced_sender = nullptr;

    VideoFecGenerator* fec_generator = nullptr;

    BitrateStatisticsObserver* send_bitrate_observer = nullptr;
    RtcEventLog* event_log = nullptr;
    SendPacketObserver* send_packet_observer = nullptr;
    RateLimiter* retransmission_rate_limiter = nullptr;
    StreamDataCountersCallback* rtp_stats_callback = nullptr;

    int rtcp_report_interval_ms = 0;

    bool populate_network2_timestamp = false;

    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer;

    FrameEncryptorInterface* frame_encryptor = nullptr;

    bool require_frame_encryption = false;

    bool extmap_allow_mixed = false;

    bool always_send_mid_and_rid = false;

    const FieldTrialsView* field_trials = nullptr;

    uint32_t local_media_ssrc = 0;
    absl::optional<uint32_t> rtx_send_ssrc;

    bool need_rtp_packet_infos = false;

    bool non_sender_rtt_measurement = false;

    std::string rid;

    bool enable_send_packet_batching = false;
  };

  struct SenderReportStats {

    NtpTime last_arrival_timestamp;

    NtpTime last_remote_timestamp;

    uint32_t last_remote_rtp_timestamp = 0;

    uint32_t packets_sent = 0;

    uint64_t bytes_sent = 0;

    uint64_t reports_count = 0;
  };

  struct NonSenderRttStats {

    absl::optional<TimeDelta> round_trip_time;

    TimeDelta total_round_trip_time = TimeDelta::Zero();

    int round_trip_time_measurements = 0;
  };

  virtual void IncomingRtcpPacket(
      rtc::ArrayView<const uint8_t> incoming_packet) = 0;

  virtual void SetRemoteSSRC(uint32_t ssrc) = 0;

  virtual void SetLocalSsrc(uint32_t ssrc) = 0;

  virtual void SetMaxRtpPacketSize(size_t size) = 0;

  virtual size_t MaxRtpPacketSize() const = 0;

  virtual void RegisterSendPayloadFrequency(int payload_type,
                                            int payload_frequency) = 0;

  virtual int32_t DeRegisterSendPayload(int8_t payload_type) = 0;

  virtual void SetExtmapAllowMixed(bool extmap_allow_mixed) = 0;

  virtual void RegisterRtpHeaderExtension(absl::string_view uri, int id) = 0;

  virtual void DeregisterSendRtpHeaderExtension(absl::string_view uri) = 0;

  virtual bool SupportsPadding() const = 0;

  virtual bool SupportsRtxPayloadPadding() const = 0;

  virtual uint32_t StartTimestamp() const = 0;

  virtual void SetStartTimestamp(uint32_t timestamp) = 0;

  virtual uint16_t SequenceNumber() const = 0;

  virtual void SetSequenceNumber(uint16_t seq) = 0;

  virtual void SetRtpState(const RtpState& rtp_state) = 0;
  virtual void SetRtxState(const RtpState& rtp_state) = 0;
  virtual RtpState GetRtpState() const = 0;
  virtual RtpState GetRtxState() const = 0;

  virtual void SetNonSenderRttMeasurement(bool enabled) = 0;

  virtual uint32_t SSRC() const = 0;

  virtual void SetMid(absl::string_view mid) = 0;

  virtual void SetRtxSendStatus(int modes) = 0;

  virtual int RtxSendStatus() const = 0;

  virtual absl::optional<uint32_t> RtxSsrc() const = 0;

  virtual void SetRtxSendPayloadType(int payload_type,
                                     int associated_payload_type) = 0;

  virtual absl::optional<uint32_t> FlexfecSsrc() const = 0;

  virtual int32_t SetSendingStatus(bool sending) = 0;

  virtual bool Sending() const = 0;

  virtual void SetSendingMediaStatus(bool sending) = 0;

  virtual bool SendingMedia() const = 0;

  virtual bool IsAudioConfigured() const = 0;

  virtual void SetAsPartOfAllocation(bool part_of_allocation) = 0;

  virtual RtpSendRates GetSendRates() const = 0;

  virtual RTPSender* RtpSender() = 0;
  virtual const RTPSender* RtpSender() const = 0;

  virtual bool OnSendingRtpFrame(uint32_t timestamp,
                                 int64_t capture_time_ms,
                                 int payload_type,
                                 bool force_sender_report) = 0;

  virtual bool TrySendPacket(std::unique_ptr<RtpPacketToSend> packet,
                             const PacedPacketInfo& pacing_info) = 0;

  virtual void OnBatchComplete() = 0;

  virtual void SetFecProtectionParams(
      const FecProtectionParams& delta_params,
      const FecProtectionParams& key_params) = 0;

  virtual std::vector<std::unique_ptr<RtpPacketToSend>> FetchFecPackets() = 0;

  virtual void OnAbortedRetransmissions(
      rtc::ArrayView<const uint16_t> sequence_numbers) = 0;

  virtual void OnPacketsAcknowledged(
      rtc::ArrayView<const uint16_t> sequence_numbers) = 0;

  virtual std::vector<std::unique_ptr<RtpPacketToSend>> GeneratePadding(
      size_t target_size_bytes) = 0;

  virtual std::vector<RtpSequenceNumberMap::Info> GetSentRtpPacketInfos(
      rtc::ArrayView<const uint16_t> sequence_numbers) const = 0;

  virtual size_t ExpectedPerPacketOverhead() const = 0;

  virtual void OnPacketSendingThreadSwitched() = 0;

  virtual RtcpMode RTCP() const = 0;

  virtual void SetRTCPStatus(RtcpMode method) = 0;

  virtual int32_t SetCNAME(absl::string_view cname) = 0;

  virtual absl::optional<TimeDelta> LastRtt() const = 0;

  virtual TimeDelta ExpectedRetransmissionTime() const = 0;

  virtual int32_t SendRTCP(RTCPPacketType rtcp_packet_type) = 0;

  virtual void GetSendStreamDataCounters(
      StreamDataCounters* rtp_counters,
      StreamDataCounters* rtx_counters) const = 0;

  virtual std::vector<ReportBlockData> GetLatestReportBlockData() const = 0;

  virtual absl::optional<SenderReportStats> GetSenderReportStats() const = 0;

  virtual absl::optional<NonSenderRttStats> GetNonSenderRttStats() const = 0;

  void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs) override = 0;

  void UnsetRemb() override = 0;

  virtual int32_t SendNACK(const uint16_t* nack_list, uint16_t size) = 0;

  virtual void SendNack(const std::vector<uint16_t>& sequence_numbers) = 0;

  virtual void SetStorePacketsStatus(bool enable, uint16_t numberToStore) = 0;

  virtual void SetVideoBitrateAllocation(
      const VideoBitrateAllocation& bitrate) = 0;

  void SendPictureLossIndication() { SendRTCP(kRtcpPli); }

  void SendFullIntraRequest() { SendRTCP(kRtcpFir); }

  virtual int32_t SendLossNotification(uint16_t last_decoded_seq_num,
                                       uint16_t last_received_seq_num,
                                       bool decodability_flag,
                                       bool buffering_allowed) = 0;
};

}

#endif
