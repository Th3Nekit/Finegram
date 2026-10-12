/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_TEST_NETWORK_EMULATION_NETWORK_EMULATION_INTERFACES_H_
#define API_TEST_NETWORK_EMULATION_NETWORK_EMULATION_INTERFACES_H_

#include <map>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

struct EmulatedIpPacket {
 public:
  EmulatedIpPacket(const rtc::SocketAddress& from,
                   const rtc::SocketAddress& to,
                   rtc::CopyOnWriteBuffer data,
                   Timestamp arrival_time,
                   uint16_t application_overhead = 0);
  ~EmulatedIpPacket() = default;

  EmulatedIpPacket(const EmulatedIpPacket&) = delete;
  EmulatedIpPacket& operator=(const EmulatedIpPacket&) = delete;

  EmulatedIpPacket(EmulatedIpPacket&&) = default;
  EmulatedIpPacket& operator=(EmulatedIpPacket&&) = default;

  size_t size() const { return data.size(); }
  const uint8_t* cdata() const { return data.cdata(); }

  size_t ip_packet_size() const { return size() + headers_size; }
  rtc::SocketAddress from;
  rtc::SocketAddress to;

  rtc::CopyOnWriteBuffer data;
  uint16_t headers_size;
  Timestamp arrival_time;
};

class EmulatedNetworkReceiverInterface {
 public:
  virtual ~EmulatedNetworkReceiverInterface() = default;

  virtual void OnPacketReceived(EmulatedIpPacket packet) = 0;
};

struct EmulatedNetworkOutgoingStats {
  int64_t packets_sent = 0;

  DataSize bytes_sent = DataSize::Zero();

  SamplesStatsCounter sent_packets_size;

  DataSize first_sent_packet_size = DataSize::Zero();

  Timestamp first_packet_sent_time = Timestamp::PlusInfinity();

  Timestamp last_packet_sent_time = Timestamp::MinusInfinity();

  DataRate AverageSendRate() const;
};

struct EmulatedNetworkIncomingStats {

  int64_t packets_received = 0;

  DataSize bytes_received = DataSize::Zero();

  SamplesStatsCounter received_packets_size;

  int64_t packets_discarded_no_receiver = 0;

  DataSize bytes_discarded_no_receiver = DataSize::Zero();

  SamplesStatsCounter packets_discarded_no_receiver_size;

  DataSize first_received_packet_size = DataSize::Zero();

  Timestamp first_packet_received_time = Timestamp::PlusInfinity();

  Timestamp last_packet_received_time = Timestamp::MinusInfinity();

  DataRate AverageReceiveRate() const;
};

struct EmulatedNetworkStats {
  int64_t PacketsSent() const { return overall_outgoing_stats.packets_sent; }

  DataSize BytesSent() const { return overall_outgoing_stats.bytes_sent; }

  const SamplesStatsCounter& SentPacketsSizeCounter() const {
    return overall_outgoing_stats.sent_packets_size;
  }

  DataSize FirstSentPacketSize() const {
    return overall_outgoing_stats.first_sent_packet_size;
  }

  Timestamp FirstPacketSentTime() const {
    return overall_outgoing_stats.first_packet_sent_time;
  }

  Timestamp LastPacketSentTime() const {
    return overall_outgoing_stats.last_packet_sent_time;
  }

  DataRate AverageSendRate() const {
    return overall_outgoing_stats.AverageSendRate();
  }

  int64_t PacketsReceived() const {
    return overall_incoming_stats.packets_received;
  }

  DataSize BytesReceived() const {
    return overall_incoming_stats.bytes_received;
  }

  const SamplesStatsCounter& ReceivedPacketsSizeCounter() const {
    return overall_incoming_stats.received_packets_size;
  }

  int64_t PacketsDiscardedNoReceiver() const {
    return overall_incoming_stats.packets_discarded_no_receiver;
  }

  DataSize BytesDiscardedNoReceiver() const {
    return overall_incoming_stats.bytes_discarded_no_receiver;
  }

  const SamplesStatsCounter& PacketsDiscardedNoReceiverSizeCounter() const {
    return overall_incoming_stats.packets_discarded_no_receiver_size;
  }

  DataSize FirstReceivedPacketSize() const {
    return overall_incoming_stats.first_received_packet_size;
  }

  Timestamp FirstPacketReceivedTime() const {
    return overall_incoming_stats.first_packet_received_time;
  }

  Timestamp LastPacketReceivedTime() const {
    return overall_incoming_stats.last_packet_received_time;
  }

  DataRate AverageReceiveRate() const {
    return overall_incoming_stats.AverageReceiveRate();
  }

  std::vector<rtc::IPAddress> local_addresses;

  EmulatedNetworkOutgoingStats overall_outgoing_stats;

  EmulatedNetworkIncomingStats overall_incoming_stats;

  std::map<rtc::IPAddress, EmulatedNetworkOutgoingStats>
      outgoing_stats_per_destination;
  std::map<rtc::IPAddress, EmulatedNetworkIncomingStats>
      incoming_stats_per_source;

  SamplesStatsCounter sent_packets_queue_wait_time_us;
};

struct EmulatedNetworkNodeStats {

  SamplesStatsCounter packet_transport_time;

  SamplesStatsCounter size_to_packet_transport_time;
};

class EmulatedEndpoint : public EmulatedNetworkReceiverInterface {
 public:

  virtual void SendPacket(const rtc::SocketAddress& from,
                          const rtc::SocketAddress& to,
                          rtc::CopyOnWriteBuffer packet_data,
                          uint16_t application_overhead = 0) = 0;

  virtual absl::optional<uint16_t> BindReceiver(
      uint16_t desired_port,
      EmulatedNetworkReceiverInterface* receiver) = 0;

  virtual void UnbindReceiver(uint16_t port) = 0;

  virtual void BindDefaultReceiver(
      EmulatedNetworkReceiverInterface* receiver) = 0;

  virtual void UnbindDefaultReceiver() = 0;
  virtual rtc::IPAddress GetPeerLocalAddress() const = 0;

 private:

  friend class EmulatedEndpointImpl;
  EmulatedEndpoint() = default;
};

class TcpMessageRoute {
 public:

  virtual void SendMessage(size_t size, std::function<void()> on_received) = 0;

 protected:
  ~TcpMessageRoute() = default;
};
}

#endif
