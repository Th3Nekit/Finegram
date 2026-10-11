/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_TRANSCEIVER_IMPL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "modules/rtp_rtcp/source/rtcp_packet/remb.h"
#include "modules/rtp_rtcp/source/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/source/rtcp_packet/target_bitrate.h"
#include "modules/rtp_rtcp/source/rtcp_transceiver_config.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

class RtcpTransceiverImpl {
 public:
  explicit RtcpTransceiverImpl(const RtcpTransceiverConfig& config);
  RtcpTransceiverImpl(const RtcpTransceiverImpl&) = delete;
  RtcpTransceiverImpl& operator=(const RtcpTransceiverImpl&) = delete;
  ~RtcpTransceiverImpl();

  void StopPeriodicTask() { periodic_task_handle_.Stop(); }

  void AddMediaReceiverRtcpObserver(uint32_t remote_ssrc,
                                    MediaReceiverRtcpObserver* observer);
  void RemoveMediaReceiverRtcpObserver(uint32_t remote_ssrc,
                                       MediaReceiverRtcpObserver* observer);

  bool AddMediaSender(uint32_t local_ssrc, RtpStreamRtcpHandler* handler);
  bool RemoveMediaSender(uint32_t local_ssrc);

  void SetReadyToSend(bool ready);

  void ReceivePacket(rtc::ArrayView<const uint8_t> packet, Timestamp now);

  void SendCompoundPacket();

  void SetRemb(int64_t bitrate_bps, std::vector<uint32_t> ssrcs);
  void UnsetRemb();

  void SendNack(uint32_t ssrc, std::vector<uint16_t> sequence_numbers);

  void SendPictureLossIndication(uint32_t ssrc);

  void SendFullIntraRequest(rtc::ArrayView<const uint32_t> ssrcs,
                            bool new_request);

  void SendCombinedRtcpPacket(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets);

 private:
  class PacketSender;
  struct RemoteSenderState;
  struct LocalSenderState;
  struct RrtrTimes {

    uint32_t received_remote_mid_ntp_time;

    uint32_t local_receive_mid_ntp_time;
  };

  void HandleReceivedPacket(const rtcp::CommonHeader& rtcp_packet_header,
                            Timestamp now,
                            std::vector<ReportBlockData>& report_blocks);

  void HandleBye(const rtcp::CommonHeader& rtcp_packet_header);
  void HandleSenderReport(const rtcp::CommonHeader& rtcp_packet_header,
                          Timestamp now,
                          std::vector<ReportBlockData>& report_blocks);
  void HandleReceiverReport(const rtcp::CommonHeader& rtcp_packet_header,
                            Timestamp now,
                            std::vector<ReportBlockData>& report_blocks);
  void HandleReportBlocks(
      uint32_t sender_ssrc,
      Timestamp now,
      rtc::ArrayView<const rtcp::ReportBlock> rtcp_report_blocks,
      std::vector<ReportBlockData>& report_blocks);
  void HandlePayloadSpecificFeedback(
      const rtcp::CommonHeader& rtcp_packet_header,
      Timestamp now);
  void HandleRtpFeedback(const rtcp::CommonHeader& rtcp_packet_header,
                         Timestamp now);
  void HandleFir(const rtcp::CommonHeader& rtcp_packet_header);
  void HandlePli(const rtcp::CommonHeader& rtcp_packet_header);
  void HandleRemb(const rtcp::CommonHeader& rtcp_packet_header, Timestamp now);
  void HandleNack(const rtcp::CommonHeader& rtcp_packet_header);
  void HandleTransportFeedback(const rtcp::CommonHeader& rtcp_packet_header,
                               Timestamp now);
  void HandleExtendedReports(const rtcp::CommonHeader& rtcp_packet_header,
                             Timestamp now);

  void HandleDlrr(const rtcp::Dlrr& dlrr, Timestamp now);
  void HandleTargetBitrate(const rtcp::TargetBitrate& target_bitrate,
                           uint32_t remote_ssrc);
  void ProcessReportBlocks(Timestamp now,
                           rtc::ArrayView<const ReportBlockData> report_blocks);

  void ReschedulePeriodicCompoundPackets();
  void SchedulePeriodicCompoundPackets(TimeDelta delay);

  struct ReservedBytes {
    size_t per_packet = 0;
    size_t per_sender = 0;
  };
  std::vector<uint32_t> FillReports(Timestamp now,
                                    ReservedBytes reserved_bytes,
                                    PacketSender& rtcp_sender);

  void CreateCompoundPacket(Timestamp now,
                            size_t reserved_bytes,
                            PacketSender& rtcp_sender);

  void SendPeriodicCompoundPacket();
  void SendImmediateFeedback(const rtcp::RtcpPacket& rtcp_packet);

  std::vector<rtcp::ReportBlock> CreateReportBlocks(Timestamp now,
                                                    size_t num_max_blocks);

  const RtcpTransceiverConfig config_;
  std::function<void(rtc::ArrayView<const uint8_t>)> rtcp_transport_;

  bool ready_to_send_;
  absl::optional<rtcp::Remb> remb_;

  flat_map<uint32_t, RemoteSenderState> remote_senders_;
  std::list<LocalSenderState> local_senders_;
  flat_map<uint32_t, std::list<LocalSenderState>::iterator>
      local_senders_by_ssrc_;
  flat_map<uint32_t, RrtrTimes> received_rrtrs_;
  RepeatingTaskHandle periodic_task_handle_;
};

}

#endif
