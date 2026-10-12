/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_SIMULATED_NETWORK_H_
#define API_TEST_SIMULATED_NETWORK_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <queue>
#include <vector>

#include "absl/types/optional.h"
#include "rtc_base/random.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

struct PacketInFlightInfo {
  PacketInFlightInfo(size_t size, int64_t send_time_us, uint64_t packet_id)
      : size(size), send_time_us(send_time_us), packet_id(packet_id) {}

  size_t size;
  int64_t send_time_us;

  uint64_t packet_id;
};

struct PacketDeliveryInfo {
  static constexpr int kNotReceived = -1;
  PacketDeliveryInfo(PacketInFlightInfo source, int64_t receive_time_us)
      : receive_time_us(receive_time_us), packet_id(source.packet_id) {}

  bool operator==(const PacketDeliveryInfo& other) const {
    return receive_time_us == other.receive_time_us &&
           packet_id == other.packet_id;
  }

  int64_t receive_time_us;
  uint64_t packet_id;
};

struct BuiltInNetworkBehaviorConfig {

  size_t queue_length_packets = 0;

  int queue_delay_ms = 0;

  int delay_standard_deviation_ms = 0;

  int link_capacity_kbps = 0;

  int loss_percent = 0;

  bool allow_reordering = false;

  int avg_burst_loss_length = -1;

  int packet_overhead = 0;
};

class NetworkBehaviorInterface {
 public:

  virtual bool EnqueuePacket(PacketInFlightInfo packet_info) = 0;

  virtual std::vector<PacketDeliveryInfo> DequeueDeliverablePackets(
      int64_t receive_time_us) = 0;

  virtual absl::optional<int64_t> NextDeliveryTimeUs() const = 0;
  virtual ~NetworkBehaviorInterface() = default;
};

class SimulatedNetworkInterface : public NetworkBehaviorInterface {
 public:

  virtual void SetConfig(const BuiltInNetworkBehaviorConfig& config) = 0;
  virtual void UpdateConfig(
      std::function<void(BuiltInNetworkBehaviorConfig*)> config_modifier) = 0;

  virtual void PauseTransmissionUntil(int64_t until_us) = 0;
};

}

#endif
