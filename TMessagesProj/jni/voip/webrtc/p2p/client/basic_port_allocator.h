/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_CLIENT_BASIC_PORT_ALLOCATOR_H_
#define P2P_CLIENT_BASIC_PORT_ALLOCATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/field_trials_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/turn_customizer.h"
#include "p2p/base/port_allocator.h"
#include "p2p/client/relay_port_factory_interface.h"
#include "p2p/client/turn_port_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/memory/always_valid_pointer.h"
#include "rtc_base/network.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace cricket {

class RTC_EXPORT BasicPortAllocator : public PortAllocator {
 public:

  BasicPortAllocator(rtc::NetworkManager* network_manager,
                     rtc::PacketSocketFactory* socket_factory,
                     webrtc::TurnCustomizer* customizer = nullptr,
                     RelayPortFactoryInterface* relay_port_factory = nullptr,
                     const webrtc::FieldTrialsView* field_trials = nullptr);
  BasicPortAllocator(rtc::NetworkManager* network_manager,
                     rtc::PacketSocketFactory* socket_factory,
                     const ServerAddresses& stun_servers,
                     const webrtc::FieldTrialsView* field_trials = nullptr);
  ~BasicPortAllocator() override;

  void SetNetworkIgnoreMask(int network_ignore_mask) override;
  int GetNetworkIgnoreMask() const;

  rtc::NetworkManager* network_manager() const {
    CheckRunOnValidThreadIfInitialized();
    return network_manager_;
  }

  rtc::PacketSocketFactory* socket_factory() {
    CheckRunOnValidThreadIfInitialized();
    return socket_factory_;
  }

  PortAllocatorSession* CreateSessionInternal(
      absl::string_view content_name,
      int component,
      absl::string_view ice_ufrag,
      absl::string_view ice_pwd) override;

  void AddTurnServerForTesting(const RelayServerConfig& turn_server);

  RelayPortFactoryInterface* relay_port_factory() {
    CheckRunOnValidThreadIfInitialized();
    return relay_port_factory_;
  }

  void SetVpnList(const std::vector<rtc::NetworkMask>& vpn_list) override;

  const webrtc::FieldTrialsView* field_trials() const {
    return field_trials_.get();
  }

 private:
  bool MdnsObfuscationEnabled() const override;

  webrtc::AlwaysValidPointer<const webrtc::FieldTrialsView,
                             webrtc::FieldTrialBasedConfig>
      field_trials_;
  rtc::NetworkManager* network_manager_;

  rtc::PacketSocketFactory* const socket_factory_;
  int network_ignore_mask_ = rtc::kDefaultNetworkIgnoreMask;

  const std::unique_ptr<RelayPortFactoryInterface> default_relay_port_factory_;

  RelayPortFactoryInterface* const relay_port_factory_;
};

struct PortConfiguration;
class AllocationSequence;

enum class SessionState {
  GATHERING,
  CLEARED,

  STOPPED

};

class RTC_EXPORT BasicPortAllocatorSession : public PortAllocatorSession {
 public:
  BasicPortAllocatorSession(BasicPortAllocator* allocator,
                            absl::string_view content_name,
                            int component,
                            absl::string_view ice_ufrag,
                            absl::string_view ice_pwd);
  ~BasicPortAllocatorSession() override;

  virtual BasicPortAllocator* allocator();
  rtc::Thread* network_thread() { return network_thread_; }
  rtc::PacketSocketFactory* socket_factory() { return socket_factory_; }

  void SetCandidateFilter(uint32_t filter) override;
  void StartGettingPorts() override;
  void StopGettingPorts() override;
  void ClearGettingPorts() override;
  bool IsGettingPorts() override;
  bool IsCleared() const override;
  bool IsStopped() const override;

  std::vector<PortInterface*> ReadyPorts() const override;
  std::vector<Candidate> ReadyCandidates() const override;
  bool CandidatesAllocationDone() const override;
  void RegatherOnFailedNetworks() override;
  void GetCandidateStatsFromReadyPorts(
      CandidateStatsList* candidate_stats_list) const override;
  void SetStunKeepaliveIntervalForReadyPorts(
      const absl::optional<int>& stun_keepalive_interval) override;
  void PruneAllPorts() override;
  static std::vector<const rtc::Network*> SelectIPv6Networks(
      std::vector<const rtc::Network*>& all_ipv6_networks,
      int max_ipv6_networks);

 protected:
  void UpdateIceParametersInternal() override;

  virtual void GetPortConfigurations();

  void ConfigReady(std::unique_ptr<PortConfiguration> config);

  ABSL_DEPRECATED(
      "Use ConfigReady(std::unique_ptr<PortConfiguration>) instead!")
  void ConfigReady(PortConfiguration* config);

 private:
  class PortData {
   public:
    enum State {
      STATE_INPROGRESS,
      STATE_COMPLETE,
      STATE_ERROR,
      STATE_PRUNED

    };

    PortData() {}
    PortData(Port* port, AllocationSequence* seq)
        : port_(port), sequence_(seq) {}

    Port* port() const { return port_; }
    AllocationSequence* sequence() const { return sequence_; }
    bool has_pairable_candidate() const { return has_pairable_candidate_; }
    State state() const { return state_; }
    bool complete() const { return state_ == STATE_COMPLETE; }
    bool error() const { return state_ == STATE_ERROR; }
    bool pruned() const { return state_ == STATE_PRUNED; }
    bool inprogress() const { return state_ == STATE_INPROGRESS; }

    bool ready() const {
      return has_pairable_candidate_ && state_ != STATE_ERROR &&
             state_ != STATE_PRUNED;
    }

    void Prune() {
      state_ = STATE_PRUNED;
      if (port()) {
        port()->Prune();
      }
    }
    void set_has_pairable_candidate(bool has_pairable_candidate) {
      if (has_pairable_candidate) {
        RTC_DCHECK(state_ == STATE_INPROGRESS);
      }
      has_pairable_candidate_ = has_pairable_candidate;
    }
    void set_state(State state) {
      RTC_DCHECK(state != STATE_ERROR || state_ == STATE_INPROGRESS);
      state_ = state;
    }

   private:
    Port* port_ = nullptr;
    AllocationSequence* sequence_ = nullptr;
    bool has_pairable_candidate_ = false;
    State state_ = STATE_INPROGRESS;
  };

  void OnConfigReady(std::unique_ptr<PortConfiguration> config);
  void OnConfigStop();
  void AllocatePorts();
  void OnAllocate(int allocation_epoch);
  void DoAllocate(bool disable_equivalent_phases);
  void OnNetworksChanged();
  void OnAllocationSequenceObjectsCreated();
  void DisableEquivalentPhases(const rtc::Network* network,
                               PortConfiguration* config,
                               uint32_t* flags);
  void AddAllocatedPort(Port* port, AllocationSequence* seq);
  void OnCandidateReady(Port* port, const Candidate& c);
  void OnCandidateError(Port* port, const IceCandidateErrorEvent& event);
  void OnPortComplete(Port* port);
  void OnPortError(Port* port);
  void OnProtocolEnabled(AllocationSequence* seq, ProtocolType proto);
  void OnPortDestroyed(PortInterface* port);
  void MaybeSignalCandidatesAllocationDone();
  void OnPortAllocationComplete();
  PortData* FindPort(Port* port);
  std::vector<const rtc::Network*> GetNetworks();
  std::vector<const rtc::Network*> GetFailedNetworks();
  void Regather(const std::vector<const rtc::Network*>& networks,
                bool disable_equivalent_phases,
                IceRegatheringReason reason);

  bool CheckCandidateFilter(const Candidate& c) const;
  bool CandidatePairable(const Candidate& c, const Port* port) const;

  std::vector<PortData*> GetUnprunedPorts(
      const std::vector<const rtc::Network*>& networks);

  void PrunePortsAndRemoveCandidates(
      const std::vector<PortData*>& port_data_list);

  void GetCandidatesFromPort(const PortData& data,
                             std::vector<Candidate>* candidates) const;
  Port* GetBestTurnPortForNetwork(absl::string_view network_name) const;

  bool PruneTurnPorts(Port* newly_pairable_turn_port);
  bool PruneNewlyPairableTurnPort(PortData* newly_pairable_turn_port);

  BasicPortAllocator* allocator_;
  rtc::Thread* network_thread_;
  rtc::PacketSocketFactory* socket_factory_;
  bool allocation_started_;
  bool network_manager_started_;
  bool allocation_sequences_created_;
  std::vector<std::unique_ptr<PortConfiguration>> configs_;
  std::vector<AllocationSequence*> sequences_;
  std::vector<PortData> ports_;
  std::vector<IceCandidateErrorEvent> candidate_error_events_;
  uint32_t candidate_filter_ = CF_ALL;

  webrtc::PortPrunePolicy turn_port_prune_policy_;
  SessionState state_ = SessionState::CLEARED;
  int allocation_epoch_ RTC_GUARDED_BY(network_thread_) = 0;
  webrtc::ScopedTaskSafety network_safety_;

  friend class AllocationSequence;
};

struct RTC_EXPORT PortConfiguration {

  rtc::SocketAddress stun_address;
  ServerAddresses stun_servers;
  std::string username;
  std::string password;
  bool use_turn_server_as_stun_server_disabled = false;

  typedef std::vector<RelayServerConfig> RelayList;
  RelayList relays;

  PortConfiguration(const ServerAddresses& stun_servers,
                    absl::string_view username,
                    absl::string_view password,
                    const webrtc::FieldTrialsView* field_trials = nullptr);

  ServerAddresses StunServers();

  void AddRelay(const RelayServerConfig& config);

  bool SupportsProtocol(const RelayServerConfig& relay,
                        ProtocolType type) const;
  bool SupportsProtocol(ProtocolType type) const;

  ServerAddresses GetRelayServerAddresses(ProtocolType type) const;
};

class UDPPort;
class TurnPort;

class AllocationSequence {
 public:
  enum State {
    kInit,
    kRunning,
    kStopped,
    kCompleted,

  };

  AllocationSequence(BasicPortAllocatorSession* session,
                     const rtc::Network* network,
                     PortConfiguration* config,
                     uint32_t flags,
                     std::function<void()> port_allocation_complete_callback);
  void Init();
  void Clear();
  void OnNetworkFailed();

  State state() const { return state_; }
  const rtc::Network* network() const { return network_; }

  bool network_failed() const { return network_failed_; }
  void set_network_failed() { network_failed_ = true; }

  void DisableEquivalentPhases(const rtc::Network* network,
                               PortConfiguration* config,
                               uint32_t* flags);

  void Start();
  void Stop();

 private:
  void CreateTurnPort(const RelayServerConfig& config, int relative_priority);

  typedef std::vector<ProtocolType> ProtocolList;

  void Process(int epoch);
  bool IsFlagSet(uint32_t flag) { return ((flags_ & flag) != 0); }
  void CreateUDPPorts();
  void CreateTCPPorts();
  void CreateStunPorts();
  void CreateRelayPorts();

  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::ReceivedPacket& packet);

  void OnPortDestroyed(PortInterface* port);

  BasicPortAllocatorSession* session_;
  bool network_failed_ = false;
  const rtc::Network* network_;

  rtc::IPAddress previous_best_ip_;
  PortConfiguration* config_;
  State state_;
  uint32_t flags_;
  ProtocolList protocols_;
  std::unique_ptr<rtc::AsyncPacketSocket> udp_socket_;

  UDPPort* udp_port_;
  std::vector<Port*> relay_ports_;
  int phase_;
  std::function<void()> port_allocation_complete_callback_;

  int epoch_ = 0;
  webrtc::ScopedTaskSafety safety_;
};

}

#endif
