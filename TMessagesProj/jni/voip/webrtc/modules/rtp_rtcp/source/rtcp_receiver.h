/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_H_
#define MODULES_RTP_RTCP_SOURCE_RTCP_RECEIVER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtcp_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_nack_stats.h"
#include "modules/rtp_rtcp/source/rtcp_packet/dlrr.h"
#include "modules/rtp_rtcp/source/rtcp_packet/tmmb_item.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/ntp_time.h"

namespace webrtc {

class ModuleRtpRtcpImpl2;
class VideoBitrateAllocationObserver;

namespace rtcp {
class CommonHeader;
class ReportBlock;
class Rrtr;
class TargetBitrate;
class TmmbItem;
}

class RTCPReceiver final {
 public:
  class ModuleRtpRtcp {
   public:
    virtual void SetTmmbn(std::vector<rtcp::TmmbItem> bounding_set) = 0;
    virtual void OnRequestSendReport() = 0;
    virtual void OnReceivedNack(
        const std::vector<uint16_t>& nack_sequence_numbers) = 0;
    virtual void OnReceivedRtcpReportBlocks(
        rtc::ArrayView<const ReportBlockData> report_blocks) = 0;

   protected:
    virtual ~ModuleRtpRtcp() = default;
  };

  class NonSenderRttStats {
   public:
    NonSenderRttStats() = default;
    NonSenderRttStats(const NonSenderRttStats&) = default;
    NonSenderRttStats& operator=(const NonSenderRttStats&) = default;
    ~NonSenderRttStats() = default;
    void Update(TimeDelta non_sender_rtt_seconds) {
      round_trip_time_ = non_sender_rtt_seconds;
      total_round_trip_time_ += non_sender_rtt_seconds;
      round_trip_time_measurements_++;
    }
    void Invalidate() { round_trip_time_.reset(); }

    absl::optional<TimeDelta> round_trip_time() const {
      return round_trip_time_;
    }

    TimeDelta total_round_trip_time() const { return total_round_trip_time_; }

    int round_trip_time_measurements() const {
      return round_trip_time_measurements_;
    }

   private:
    absl::optional<TimeDelta> round_trip_time_;
    TimeDelta total_round_trip_time_ = TimeDelta::Zero();
    int round_trip_time_measurements_ = 0;
  };

  RTCPReceiver(const RtpRtcpInterface::Configuration& config,
               ModuleRtpRtcp* owner);

  RTCPReceiver(const RtpRtcpInterface::Configuration& config,
               ModuleRtpRtcpImpl2* owner);

  ~RTCPReceiver();

  void IncomingPacket(rtc::ArrayView<const uint8_t> packet);

  int64_t LastReceivedReportBlockMs() const;

  void set_local_media_ssrc(uint32_t ssrc);
  uint32_t local_media_ssrc() const;

  void SetRemoteSSRC(uint32_t ssrc);
  uint32_t RemoteSSRC() const;

  bool receiver_only() const { return receiver_only_; }

  absl::optional<RtpRtcpInterface::SenderReportStats> GetSenderReportStats()
      const;

  std::vector<rtcp::ReceiveTimeInfo> ConsumeReceivedXrReferenceTimeInfo();

  absl::optional<TimeDelta> AverageRtt() const;
  absl::optional<TimeDelta> LastRtt() const;

  NonSenderRttStats GetNonSenderRTT() const;

  void SetNonSenderRttMeasurement(bool enabled);
  absl::optional<TimeDelta> GetAndResetXrRrRtt();

  absl::optional<TimeDelta> OnPeriodicRttUpdate(Timestamp newer_than,
                                                bool sending);

  std::vector<ReportBlockData> GetLatestReportBlockData() const;

  bool RtcpRrTimeout();

  bool RtcpRrSequenceNumberTimeout();

  std::vector<rtcp::TmmbItem> TmmbrReceived();

  bool UpdateTmmbrTimers();
  std::vector<rtcp::TmmbItem> BoundingSet(bool* tmmbr_owner);

  void NotifyTmmbrUpdated();

 private:
#if RTC_DCHECK_IS_ON
  class CustomSequenceChecker : public SequenceChecker {
   public:
    explicit CustomSequenceChecker(bool disable_checks)
        : disable_checks_(disable_checks) {}
    bool IsCurrent() const {
      if (disable_checks_)
        return true;
      return SequenceChecker::IsCurrent();
    }

   private:
    const bool disable_checks_;
  };
#else
  class CustomSequenceChecker : public SequenceChecker {
   public:
    explicit CustomSequenceChecker(bool) {}
  };
#endif

  class RegisteredSsrcs {
   public:
    static constexpr size_t kMediaSsrcIndex = 0;
    static constexpr size_t kMaxSsrcs = 3;

    RegisteredSsrcs(bool disable_sequence_checker,
                    const RtpRtcpInterface::Configuration& config);

    bool contains(uint32_t ssrc) const;
    uint32_t media_ssrc() const;
    void set_media_ssrc(uint32_t ssrc);

   private:
    RTC_NO_UNIQUE_ADDRESS CustomSequenceChecker packet_sequence_checker_;
    absl::InlinedVector<uint32_t, kMaxSsrcs> ssrcs_
        RTC_GUARDED_BY(packet_sequence_checker_);
  };

  struct PacketInformation;

  struct TmmbrInformation {
    struct TimedTmmbrItem {
      rtcp::TmmbItem tmmbr_item;
      Timestamp last_updated = Timestamp::Zero();
    };

    Timestamp last_time_received = Timestamp::Zero();

    bool ready_for_delete = false;

    std::vector<rtcp::TmmbItem> tmmbn;
    std::map<uint32_t, TimedTmmbrItem> tmmbr;
  };

  struct RrtrInformation {
    RrtrInformation(uint32_t ssrc,
                    uint32_t received_remote_mid_ntp_time,
                    uint32_t local_receive_mid_ntp_time)
        : ssrc(ssrc),
          received_remote_mid_ntp_time(received_remote_mid_ntp_time),
          local_receive_mid_ntp_time(local_receive_mid_ntp_time) {}

    uint32_t ssrc;

    uint32_t received_remote_mid_ntp_time;

    uint32_t local_receive_mid_ntp_time;
  };

  struct LastFirStatus {
    LastFirStatus(Timestamp now, uint8_t sequence_number)
        : request(now), sequence_number(sequence_number) {}
    Timestamp request;
    uint8_t sequence_number;
  };

  class RttStats {
   public:
    RttStats() = default;
    RttStats(const RttStats&) = default;
    RttStats& operator=(const RttStats&) = default;

    void AddRtt(TimeDelta rtt);

    TimeDelta last_rtt() const { return last_rtt_; }
    TimeDelta average_rtt() const { return sum_rtt_ / num_rtts_; }

   private:
    TimeDelta last_rtt_ = TimeDelta::Zero();
    TimeDelta sum_rtt_ = TimeDelta::Zero();
    size_t num_rtts_ = 0;
  };

  bool ParseCompoundPacket(rtc::ArrayView<const uint8_t> packet,
                           PacketInformation* packet_information);

  void TriggerCallbacksFromRtcpPacket(
      const PacketInformation& packet_information);

  TmmbrInformation* FindOrCreateTmmbrInfo(uint32_t remote_ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void UpdateTmmbrRemoteIsAlive(uint32_t remote_ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);
  TmmbrInformation* GetTmmbrInformation(uint32_t remote_ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleSenderReport(const rtcp::CommonHeader& rtcp_block,
                          PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleReceiverReport(const rtcp::CommonHeader& rtcp_block,
                            PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleReportBlock(const rtcp::ReportBlock& report_block,
                         PacketInformation* packet_information,
                         uint32_t remote_ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleSdes(const rtcp::CommonHeader& rtcp_block,
                  PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleXr(const rtcp::CommonHeader& rtcp_block,
                PacketInformation* packet_information,
                bool& contains_dlrr,
                uint32_t& ssrc)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleXrReceiveReferenceTime(uint32_t sender_ssrc,
                                    const rtcp::Rrtr& rrtr)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleXrDlrrReportBlock(uint32_t ssrc, const rtcp::ReceiveTimeInfo& rti)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleXrTargetBitrate(uint32_t ssrc,
                             const rtcp::TargetBitrate& target_bitrate,
                             PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleNack(const rtcp::CommonHeader& rtcp_block,
                  PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleApp(const rtcp::CommonHeader& rtcp_block,
                 PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleBye(const rtcp::CommonHeader& rtcp_block)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandlePli(const rtcp::CommonHeader& rtcp_block,
                 PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandlePsfbApp(const rtcp::CommonHeader& rtcp_block,
                     PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleTmmbr(const rtcp::CommonHeader& rtcp_block,
                   PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleTmmbn(const rtcp::CommonHeader& rtcp_block,
                   PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleSrReq(const rtcp::CommonHeader& rtcp_block,
                   PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool HandleFir(const rtcp::CommonHeader& rtcp_block,
                 PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  void HandleTransportFeedback(const rtcp::CommonHeader& rtcp_block,
                               PacketInformation* packet_information)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool RtcpRrTimeoutLocked(Timestamp now)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  bool RtcpRrSequenceNumberTimeoutLocked(Timestamp now)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(rtcp_receiver_lock_);

  Clock* const clock_;
  const bool receiver_only_;
  ModuleRtpRtcp* const rtp_rtcp_;

  RegisteredSsrcs registered_ssrcs_;

  NetworkLinkRtcpObserver* const network_link_rtcp_observer_;
  RtcpIntraFrameObserver* const rtcp_intra_frame_observer_;
  RtcpLossNotificationObserver* const rtcp_loss_notification_observer_;
  NetworkStateEstimateObserver* const network_state_estimate_observer_;
  VideoBitrateAllocationObserver* const bitrate_allocation_observer_;
  const TimeDelta report_interval_;

  mutable Mutex rtcp_receiver_lock_;
  uint32_t remote_ssrc_ RTC_GUARDED_BY(rtcp_receiver_lock_);

  RtpRtcpInterface::SenderReportStats remote_sender_
      RTC_GUARDED_BY(rtcp_receiver_lock_);

  std::list<RrtrInformation> received_rrtrs_
      RTC_GUARDED_BY(rtcp_receiver_lock_);

  flat_map<uint32_t, std::list<RrtrInformation>::iterator>
      received_rrtrs_ssrc_it_ RTC_GUARDED_BY(rtcp_receiver_lock_);

  bool xr_rrtr_status_ RTC_GUARDED_BY(rtcp_receiver_lock_);
  absl::optional<TimeDelta> xr_rr_rtt_;

  Timestamp oldest_tmmbr_info_ RTC_GUARDED_BY(rtcp_receiver_lock_);

  flat_map<uint32_t, TmmbrInformation> tmmbr_infos_
      RTC_GUARDED_BY(rtcp_receiver_lock_);

  flat_map<uint32_t, RttStats> rtts_ RTC_GUARDED_BY(rtcp_receiver_lock_);

  flat_map<uint32_t, NonSenderRttStats> non_sender_rtts_
      RTC_GUARDED_BY(rtcp_receiver_lock_);

  flat_map<uint32_t, ReportBlockData> received_report_blocks_
      RTC_GUARDED_BY(rtcp_receiver_lock_);
  flat_map<uint32_t, LastFirStatus> last_fir_
      RTC_GUARDED_BY(rtcp_receiver_lock_);

  Timestamp last_received_rb_ RTC_GUARDED_BY(rtcp_receiver_lock_) =
      Timestamp::PlusInfinity();

  Timestamp last_increased_sequence_number_ = Timestamp::PlusInfinity();

  RtcpCnameCallback* const cname_callback_;
  ReportBlockDataObserver* const report_block_data_observer_;

  RtcpPacketTypeCounterObserver* const packet_type_counter_observer_;
  RtcpPacketTypeCounter packet_type_counter_;

  RtcpNackStats nack_stats_;

  size_t num_skipped_packets_;
  Timestamp last_skipped_packets_warning_;
};
}
#endif
