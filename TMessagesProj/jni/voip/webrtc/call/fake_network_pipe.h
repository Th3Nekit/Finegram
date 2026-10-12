/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_FAKE_NETWORK_PIPE_H_
#define CALL_FAKE_NETWORK_PIPE_H_

#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "api/call/transport.h"
#include "api/test/simulated_network.h"
#include "call/simulated_packet_receiver.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class Clock;
class PacketReceiver;
enum class MediaType;

class NetworkPacket {
 public:
  NetworkPacket(rtc::CopyOnWriteBuffer packet,
                int64_t send_time,
                int64_t arrival_time,
                absl::optional<PacketOptions> packet_options,
                bool is_rtcp,
                MediaType media_type,
                absl::optional<int64_t> packet_time_us,
                Transport* transport);

  NetworkPacket(RtpPacketReceived packet,
                MediaType media_type,
                int64_t send_time,
                int64_t arrival_time);

  NetworkPacket(const NetworkPacket&) = delete;
  ~NetworkPacket();
  NetworkPacket& operator=(const NetworkPacket&) = delete;

  NetworkPacket(NetworkPacket&&);
  NetworkPacket& operator=(NetworkPacket&&);

  const uint8_t* data() const { return packet_.data(); }
  size_t data_length() const { return packet_.size(); }
  rtc::CopyOnWriteBuffer* raw_packet() { return &packet_; }
  int64_t send_time() const { return send_time_; }
  int64_t arrival_time() const { return arrival_time_; }
  void IncrementArrivalTime(int64_t extra_delay) {
    arrival_time_ += extra_delay;
  }
  PacketOptions packet_options() const {
    return packet_options_.value_or(PacketOptions());
  }
  bool is_rtcp() const { return is_rtcp_; }
  MediaType media_type() const { return media_type_; }
  absl::optional<int64_t> packet_time_us() const { return packet_time_us_; }
  RtpPacketReceived* packet_received() {
    return packet_received_ ? &packet_received_.value() : nullptr;
  }
  absl::optional<RtpPacketReceived> packet_received() const {
    return packet_received_;
  }
  Transport* transport() const { return transport_; }

 private:
  rtc::CopyOnWriteBuffer packet_;

  int64_t send_time_;

  int64_t arrival_time_;

  absl::optional<PacketOptions> packet_options_;
  bool is_rtcp_;

  MediaType media_type_;
  absl::optional<int64_t> packet_time_us_;
  absl::optional<RtpPacketReceived> packet_received_;
  Transport* transport_;
};

class FakeNetworkPipe : public SimulatedPacketReceiverInterface {
 public:

  FakeNetworkPipe(Clock* clock,
                  std::unique_ptr<NetworkBehaviorInterface> network_behavior);
  FakeNetworkPipe(Clock* clock,
                  std::unique_ptr<NetworkBehaviorInterface> network_behavior,
                  PacketReceiver* receiver);
  FakeNetworkPipe(Clock* clock,
                  std::unique_ptr<NetworkBehaviorInterface> network_behavior,
                  PacketReceiver* receiver,
                  uint64_t seed);

  ~FakeNetworkPipe() override;

  FakeNetworkPipe(const FakeNetworkPipe&) = delete;
  FakeNetworkPipe& operator=(const FakeNetworkPipe&) = delete;

  void SetClockOffset(int64_t offset_ms);

  void SetReceiver(PacketReceiver* receiver) override;

  void AddActiveTransport(Transport* transport);
  void RemoveActiveTransport(Transport* transport);

  bool SendRtp(rtc::ArrayView<const uint8_t> packet,
               const PacketOptions& options,
               Transport* transport);
  bool SendRtcp(rtc::ArrayView<const uint8_t> packet, Transport* transport);

  void DeliverRtpPacket(
      MediaType media_type,
      RtpPacketReceived packet,
      OnUndemuxablePacketHandler undemuxable_packet_handler) override;
  void DeliverRtcpPacket(rtc::CopyOnWriteBuffer packet) override;

  void Process() override;
  absl::optional<int64_t> TimeUntilNextProcess() override;

  float PercentageLoss();
  int AverageDelay() override;
  size_t DroppedPackets();
  size_t SentPackets();
  void ResetStats();

 protected:
  void DeliverPacketWithLock(NetworkPacket* packet);
  int64_t GetTimeInMicroseconds() const;
  bool ShouldProcess(int64_t time_now_us) const;
  void SetTimeToNextProcess(int64_t skip_us);

 private:
  struct StoredPacket {
    NetworkPacket packet;
    bool removed = false;
    explicit StoredPacket(NetworkPacket&& packet);
    StoredPacket(StoredPacket&&) = default;
    StoredPacket(const StoredPacket&) = delete;
    StoredPacket& operator=(const StoredPacket&) = delete;
    StoredPacket() = delete;
  };

  bool EnqueuePacket(rtc::CopyOnWriteBuffer packet,
                     absl::optional<PacketOptions> options,
                     bool is_rtcp,
                     MediaType media_type,
                     absl::optional<int64_t> packet_time_us);

  bool EnqueuePacket(rtc::CopyOnWriteBuffer packet,
                     absl::optional<PacketOptions> options,
                     bool is_rtcp,
                     Transport* transport);

  bool EnqueuePacket(NetworkPacket&& net_packet)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(process_lock_);

  void DeliverNetworkPacket(NetworkPacket* packet)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(config_lock_);
  bool HasReceiver() const;

  Clock* const clock_;

  mutable Mutex config_lock_;
  const std::unique_ptr<NetworkBehaviorInterface> network_behavior_;
  PacketReceiver* receiver_ RTC_GUARDED_BY(config_lock_);

  Mutex process_lock_;

  std::deque<StoredPacket> packets_in_flight_ RTC_GUARDED_BY(process_lock_);

  int64_t clock_offset_ms_ RTC_GUARDED_BY(config_lock_);

  size_t dropped_packets_ RTC_GUARDED_BY(process_lock_);
  size_t sent_packets_ RTC_GUARDED_BY(process_lock_);
  int64_t total_packet_delay_us_ RTC_GUARDED_BY(process_lock_);
  int64_t last_log_time_us_;

  std::map<Transport*, size_t> active_transports_ RTC_GUARDED_BY(config_lock_);
};

}

#endif
