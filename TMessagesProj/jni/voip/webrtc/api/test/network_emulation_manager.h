/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETWORK_EMULATION_MANAGER_H_
#define API_TEST_NETWORK_EMULATION_MANAGER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/packet_socket_factory.h"
#include "api/test/network_emulation/cross_traffic.h"
#include "api/test/network_emulation/network_emulation_interfaces.h"
#include "api/test/peer_network_dependencies.h"
#include "api/test/simulated_network.h"
#include "api/test/time_controller.h"
#include "api/units/timestamp.h"
#include "rtc_base/network.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/thread.h"

namespace webrtc {

class EmulatedNetworkNode;

class EmulatedRoute;

enum class EmulatedNetworkStatsGatheringMode {

  kDefault,

  kDebug
};

struct EmulatedEndpointConfig {
  enum class IpAddressFamily { kIpv4, kIpv6 };

  absl::optional<std::string> name = absl::nullopt;
  IpAddressFamily generated_ip_family = IpAddressFamily::kIpv4;

  absl::optional<rtc::IPAddress> ip;

  bool start_as_enabled = true;

  rtc::AdapterType type = rtc::AdapterType::ADAPTER_TYPE_UNKNOWN;

  bool allow_send_packet_with_different_source_ip = false;

  bool allow_receive_packets_with_different_dest_ip = false;
};

struct EmulatedTURNServerConfig {
  EmulatedEndpointConfig client_config;
  EmulatedEndpointConfig peer_config;
};

class EmulatedTURNServerInterface {
 public:
  struct IceServerConfig {
    std::string username;
    std::string password;
    std::string url;
  };

  virtual ~EmulatedTURNServerInterface() {}

  virtual IceServerConfig GetIceServerConfig() const = 0;

  virtual EmulatedEndpoint* GetClientEndpoint() const = 0;

  virtual rtc::SocketAddress GetClientEndpointAddress() const = 0;

  virtual EmulatedEndpoint* GetPeerEndpoint() const = 0;
};

class EmulatedNetworkManagerInterface {
 public:
  virtual ~EmulatedNetworkManagerInterface() = default;

  virtual rtc::Thread* network_thread() = 0;

  virtual rtc::NetworkManager* network_manager() = 0;

  virtual rtc::PacketSocketFactory* packet_socket_factory() = 0;
  webrtc::webrtc_pc_e2e::PeerNetworkDependencies network_dependencies() {
    return {network_thread(), network_manager(), packet_socket_factory()};
  }

  virtual std::vector<EmulatedEndpoint*> endpoints() const = 0;

  virtual void GetStats(
      std::function<void(EmulatedNetworkStats)> stats_callback) const = 0;
};

enum class TimeMode { kRealTime, kSimulated };

bool AbslParseFlag(absl::string_view text, TimeMode* mode, std::string* error);

std::string AbslUnparseFlag(TimeMode mode);

class NetworkEmulationManager {
 public:

  struct SimulatedNetworkNode {
    SimulatedNetworkInterface* simulation;
    EmulatedNetworkNode* node;

    class Builder {
     public:
      explicit Builder(NetworkEmulationManager* net) : net_(net) {}
      Builder() : net_(nullptr) {}
      Builder(const Builder&) = default;

      Builder& config(BuiltInNetworkBehaviorConfig config);
      Builder& delay_ms(int queue_delay_ms);
      Builder& capacity_kbps(int link_capacity_kbps);
      Builder& capacity_Mbps(int link_capacity_Mbps);
      Builder& loss(double loss_rate);
      Builder& packet_queue_length(int max_queue_length_in_packets);
      Builder& delay_standard_deviation_ms(int delay_standard_deviation_ms);
      Builder& allow_reordering();
      Builder& avg_burst_loss_length(int avg_burst_loss_length);
      Builder& packet_overhead(int packet_overhead);
      SimulatedNetworkNode Build(uint64_t random_seed = 1) const;
      SimulatedNetworkNode Build(NetworkEmulationManager* net,
                                 uint64_t random_seed = 1) const;

     private:
      NetworkEmulationManager* const net_;
      BuiltInNetworkBehaviorConfig config_;
    };
  };
  virtual ~NetworkEmulationManager() = default;

  virtual TimeController* time_controller() = 0;

  virtual TimeMode time_mode() const = 0;

  EmulatedNetworkNode* CreateUnconstrainedEmulatedNode() {
    return CreateEmulatedNode(BuiltInNetworkBehaviorConfig());
  }

  virtual EmulatedNetworkNode* CreateEmulatedNode(
      BuiltInNetworkBehaviorConfig config,
      uint64_t random_seed = 1) = 0;

  virtual EmulatedNetworkNode* CreateEmulatedNode(
      std::unique_ptr<NetworkBehaviorInterface> network_behavior) = 0;

  virtual SimulatedNetworkNode::Builder NodeBuilder() = 0;

  virtual EmulatedEndpoint* CreateEndpoint(EmulatedEndpointConfig config) = 0;

  virtual void EnableEndpoint(EmulatedEndpoint* endpoint) = 0;

  virtual void DisableEndpoint(EmulatedEndpoint* endpoint) = 0;

  virtual EmulatedRoute* CreateRoute(
      EmulatedEndpoint* from,
      const std::vector<EmulatedNetworkNode*>& via_nodes,
      EmulatedEndpoint* to) = 0;

  virtual EmulatedRoute* CreateRoute(
      const std::vector<EmulatedNetworkNode*>& via_nodes) = 0;

  virtual EmulatedRoute* CreateDefaultRoute(
      EmulatedEndpoint* from,
      const std::vector<EmulatedNetworkNode*>& via_nodes,
      EmulatedEndpoint* to) = 0;

  virtual void ClearRoute(EmulatedRoute* route) = 0;

  virtual TcpMessageRoute* CreateTcpRoute(EmulatedRoute* send_route,
                                          EmulatedRoute* ret_route) = 0;

  virtual CrossTrafficRoute* CreateCrossTrafficRoute(
      const std::vector<EmulatedNetworkNode*>& via_nodes) = 0;

  virtual CrossTrafficGenerator* StartCrossTraffic(
      std::unique_ptr<CrossTrafficGenerator> generator) = 0;

  virtual void StopCrossTraffic(CrossTrafficGenerator* generator) = 0;

  virtual EmulatedNetworkManagerInterface*
  CreateEmulatedNetworkManagerInterface(
      const std::vector<EmulatedEndpoint*>& endpoints) = 0;

  virtual void GetStats(
      rtc::ArrayView<EmulatedEndpoint* const> endpoints,
      std::function<void(EmulatedNetworkStats)> stats_callback) = 0;

  virtual void GetStats(
      rtc::ArrayView<EmulatedNetworkNode* const> nodes,
      std::function<void(EmulatedNetworkNodeStats)> stats_callback) = 0;

  virtual EmulatedTURNServerInterface* CreateTURNServer(
      EmulatedTURNServerConfig config) = 0;

  std::pair<EmulatedNetworkManagerInterface*, EmulatedNetworkManagerInterface*>
  CreateEndpointPairWithTwoWayRoutes(
      const BuiltInNetworkBehaviorConfig& config);
};

}

#endif
