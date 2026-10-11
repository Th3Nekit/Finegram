/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_PACKET_HISTORY_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_PACKET_HISTORY_H_

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/function_view.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Clock;

class RtpPacketHistory {
 public:
  enum class StorageMode {
    kDisabled,
    kStoreAndCull

  };

  enum class PaddingMode {
    kDefault,

    kPriority,

    kRecentLargePacket

  };

  static constexpr size_t kMaxCapacity = 9600;

  static constexpr size_t kMaxPaddingHistory = 63;

  static constexpr TimeDelta kMinPacketDuration = TimeDelta::Seconds(1);
  static constexpr int kMinPacketDurationRtt = 3;

  static constexpr int kPacketCullingDelayFactor = 3;

  RtpPacketHistory(Clock* clock, bool enable_padding_prio)
      : RtpPacketHistory(clock,
                         enable_padding_prio ? PaddingMode::kPriority
                                             : PaddingMode::kDefault) {}
  RtpPacketHistory(Clock* clock, PaddingMode padding_mode);

  RtpPacketHistory() = delete;
  RtpPacketHistory(const RtpPacketHistory&) = delete;
  RtpPacketHistory& operator=(const RtpPacketHistory&) = delete;

  ~RtpPacketHistory();

  void SetStorePacketsStatus(StorageMode mode, size_t number_to_store);
  StorageMode GetStorageMode() const;

  void SetRtt(TimeDelta rtt);

  void PutRtpPacket(std::unique_ptr<RtpPacketToSend> packet,
                    Timestamp send_time);

  std::unique_ptr<RtpPacketToSend> GetPacketAndMarkAsPending(
      uint16_t sequence_number);

  std::unique_ptr<RtpPacketToSend> GetPacketAndMarkAsPending(
      uint16_t sequence_number,
      rtc::FunctionView<std::unique_ptr<RtpPacketToSend>(
          const RtpPacketToSend&)> encapsulate);

  void MarkPacketAsSent(uint16_t sequence_number);

  bool GetPacketState(uint16_t sequence_number) const;

  std::unique_ptr<RtpPacketToSend> GetPayloadPaddingPacket();

  std::unique_ptr<RtpPacketToSend> GetPayloadPaddingPacket(
      rtc::FunctionView<std::unique_ptr<RtpPacketToSend>(
          const RtpPacketToSend&)> encapsulate);

  void CullAcknowledgedPackets(rtc::ArrayView<const uint16_t> sequence_numbers);

  void Clear();

 private:
  struct MoreUseful;
  class StoredPacket;
  using PacketPrioritySet = std::set<StoredPacket*, MoreUseful>;

  class StoredPacket {
   public:
    StoredPacket() = default;
    StoredPacket(std::unique_ptr<RtpPacketToSend> packet,
                 Timestamp send_time,
                 uint64_t insert_order);
    StoredPacket(StoredPacket&&);
    StoredPacket& operator=(StoredPacket&&);
    ~StoredPacket();

    uint64_t insert_order() const { return insert_order_; }
    size_t times_retransmitted() const { return times_retransmitted_; }
    void IncrementTimesRetransmitted(PacketPrioritySet* priority_set);

    Timestamp send_time() const { return send_time_; }
    void set_send_time(Timestamp value) { send_time_ = value; }

    std::unique_ptr<RtpPacketToSend> packet_;

    bool pending_transmission_;

   private:
    Timestamp send_time_ = Timestamp::Zero();

    uint64_t insert_order_;

    size_t times_retransmitted_;
  };
  struct MoreUseful {
    bool operator()(StoredPacket* lhs, StoredPacket* rhs) const;
  };

  bool padding_priority_enabled() const;

  bool VerifyRtt(const StoredPacket& packet) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void Reset() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void CullOldPackets() RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  std::unique_ptr<RtpPacketToSend> RemovePacket(int packet_index)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  int GetPacketIndex(uint16_t sequence_number) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  StoredPacket* GetStoredPacket(uint16_t sequence_number)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  Clock* const clock_;
  const PaddingMode padding_mode_;
  mutable Mutex lock_;
  size_t number_to_store_ RTC_GUARDED_BY(lock_);
  StorageMode mode_ RTC_GUARDED_BY(lock_);
  TimeDelta rtt_ RTC_GUARDED_BY(lock_);

  std::deque<StoredPacket> packet_history_ RTC_GUARDED_BY(lock_);

  uint64_t packets_inserted_ RTC_GUARDED_BY(lock_);

  PacketPrioritySet padding_priority_ RTC_GUARDED_BY(lock_);

  absl::optional<RtpPacketToSend> large_payload_packet_ RTC_GUARDED_BY(lock_);
};
}
#endif
