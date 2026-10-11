/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// P2PTransportChannel wraps up the state management of the connection between
// two P2P clients.  Clients have candidate ports for connecting, and
// connections which are combinations of candidates from each end (Alice and
// Bob each have candidates, one candidate from Alice and one candidate from
// Bob are used to make a connection, repeat to make many connections).
//
// When all of the available connections become invalid (non-writable), we
// kick off a process of determining more candidates and more connections.
//
#ifndef P2P_BASE_P2P_TRANSPORT_CHANNEL_H_
#define P2P_BASE_P2P_TRANSPORT_CHANNEL_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/async_dns_resolver.h"
#include "api/candidate.h"
#include "api/ice_transport_interface.h"
#include "api/rtc_error.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/enums.h"
#include "api/transport/stun.h"
#include "logging/rtc_event_log/events/rtc_event_ice_candidate_pair_config.h"
#include "logging/rtc_event_log/ice_logger.h"
#include "p2p/base/active_ice_controller_factory_interface.h"
#include "p2p/base/candidate_pair_interface.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_agent_interface.h"
#include "p2p/base/ice_controller_factory_interface.h"
#include "p2p/base/ice_controller_interface.h"
#include "p2p/base/ice_switch_reason.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/p2p_transport_channel_ice_field_trials.h"
#include "p2p/base/port.h"
#include "p2p/base/port_allocator.h"
#include "p2p/base/port_interface.h"
#include "p2p/base/regathering_controller.h"
#include "p2p/base/stun_dictionary.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/dscp.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/network_route.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {
class RtcEventLog;
}

namespace cricket {

bool IceCredentialsChanged(absl::string_view old_ufrag,
                           absl::string_view old_pwd,
                           absl::string_view new_ufrag,
                           absl::string_view new_pwd);

class RemoteCandidate : public Candidate {
 public:
  RemoteCandidate(const Candidate& c, PortInterface* origin_port)
      : Candidate(c), origin_port_(origin_port) {}

  PortInterface* origin_port() { return origin_port_; }

 private:
  PortInterface* origin_port_;
};

class RTC_EXPORT P2PTransportChannel : public IceTransportInternal,
                                       public IceAgentInterface {
 public:
  static std::unique_ptr<P2PTransportChannel> Create(
      absl::string_view transport_name,
      int component,
      webrtc::IceTransportInit init);

  P2PTransportChannel(absl::string_view transport_name,
                      int component,
                      PortAllocator* allocator,
                      const webrtc::FieldTrialsView* field_trials = nullptr);

  ~P2PTransportChannel() override;

  P2PTransportChannel(const P2PTransportChannel&) = delete;
  P2PTransportChannel& operator=(const P2PTransportChannel&) = delete;

  IceTransportState GetState() const override;
  webrtc::IceTransportState GetIceTransportState() const override;

  const std::string& transport_name() const override;
  int component() const override;
  bool writable() const override;
  bool receiving() const override;
  void SetIceRole(IceRole role) override;
  IceRole GetIceRole() const override;
  void SetIceTiebreaker(uint64_t tiebreaker) override;
  void SetIceParameters(const IceParameters& ice_params) override;
  void SetRemoteIceParameters(const IceParameters& ice_params) override;
  void SetRemoteIceMode(IceMode mode) override;

  void Connect() {}
  void MaybeStartGathering() override;
  IceGatheringState gathering_state() const override;
  void ResolveHostnameCandidate(const Candidate& candidate);
  void AddRemoteCandidate(const Candidate& candidate) override;
  void RemoveRemoteCandidate(const Candidate& candidate) override;
  void RemoveAllRemoteCandidates() override;

  void SetIceConfig(const IceConfig& config) override;
  const IceConfig& config() const;
  static webrtc::RTCError ValidateIceConfig(const IceConfig& config);

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  bool GetOption(rtc::Socket::Option opt, int* value) override;
  int GetError() override;
  bool GetStats(IceTransportStats* ice_transport_stats) override;
  absl::optional<int> GetRttEstimate() override;
  const Connection* selected_connection() const override;
  absl::optional<const CandidatePair> GetSelectedCandidatePair() const override;

  void OnStartedPinging() override;
  int64_t GetLastPingSentMs() const override;
  void UpdateConnectionStates() override;
  void UpdateState() override;
  void SendPingRequest(const Connection* connection) override;
  void SwitchSelectedConnection(const Connection* connection,
                                IceSwitchReason reason) override;
  void ForgetLearnedStateForConnections(
      rtc::ArrayView<const Connection* const> connections) override;
  bool PruneConnections(
      rtc::ArrayView<const Connection* const> connections) override;

  const Connection* best_connection() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return selected_connection_;
  }

  void set_incoming_only(bool value) {
    RTC_DCHECK_RUN_ON(network_thread_);
    incoming_only_ = value;
  }

  const std::vector<PortInterface*>& ports() {
    RTC_DCHECK_RUN_ON(network_thread_);
    return ports_;
  }
  const std::vector<PortInterface*>& pruned_ports() {
    RTC_DCHECK_RUN_ON(network_thread_);
    return pruned_ports_;
  }

  IceMode remote_ice_mode() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return remote_ice_mode_;
  }

  void PruneAllPorts();
  int check_receiving_interval() const;
  absl::optional<rtc::NetworkRoute> network_route() const override;

  void RemoveConnection(Connection* connection);

  rtc::DiffServCodePoint DefaultDscpValue() const;

  Connection* FindNextPingableConnection();
  void MarkConnectionPinged(Connection* conn);

  rtc::ArrayView<Connection* const> connections() const;
  void RemoveConnectionForTest(Connection* connection);

  PortAllocatorSession* allocator_session() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    if (allocator_sessions_.empty()) {
      return nullptr;
    }
    return allocator_sessions_.back().get();
  }

  const std::vector<RemoteCandidate>& remote_candidates() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    return remote_candidates_;
  }

  std::string ToString() const {
    RTC_DCHECK_RUN_ON(network_thread_);
    const std::string RECEIVING_ABBREV[2] = {"_", "R"};
    const std::string WRITABLE_ABBREV[2] = {"_", "W"};
    rtc::StringBuilder ss;
    ss << "Channel[" << transport_name_ << "|" << component_ << "|"
       << RECEIVING_ABBREV[receiving_] << WRITABLE_ABBREV[writable_] << "]";
    return ss.Release();
  }

  absl::optional<std::reference_wrapper<StunDictionaryWriter>>
  GetDictionaryWriter() override {
    return stun_dict_writer_;
  }

 private:
  P2PTransportChannel(
      absl::string_view transport_name,
      int component,
      PortAllocator* allocator,

      webrtc::AsyncDnsResolverFactoryInterface* async_dns_resolver_factory,

      std::unique_ptr<webrtc::AsyncDnsResolverFactoryInterface>
          owned_dns_resolver_factory,
      webrtc::RtcEventLog* event_log,
      IceControllerFactoryInterface* ice_controller_factory,
      ActiveIceControllerFactoryInterface* active_ice_controller_factory,
      const webrtc::FieldTrialsView* field_trials);

  bool IsGettingPorts() {
    RTC_DCHECK_RUN_ON(network_thread_);
    return allocator_session()->IsGettingPorts();
  }

  bool ReadyToSend(const Connection* connection) const;
  bool PresumedWritable(const Connection* conn) const;
  void SendPingRequestInternal(Connection* connection);

  rtc::NetworkRoute ConfigureNetworkRoute(const Connection* conn);
  void SwitchSelectedConnectionInternal(Connection* conn,
                                        IceSwitchReason reason);
  void UpdateTransportState();
  void HandleAllTimedOut();
  void MaybeStopPortAllocatorSessions();
  void OnSelectedConnectionDestroyed() RTC_RUN_ON(network_thread_);

  IceTransportState ComputeState() const;
  webrtc::IceTransportState ComputeIceTransportState() const;

  bool CreateConnections(const Candidate& remote_candidate,
                         PortInterface* origin_port);
  bool CreateConnection(PortInterface* port,
                        const Candidate& remote_candidate,
                        PortInterface* origin_port);
  bool FindConnection(const Connection* connection) const;

  uint32_t GetRemoteCandidateGeneration(const Candidate& candidate);
  bool IsDuplicateRemoteCandidate(const Candidate& candidate);
  void RememberRemoteCandidate(const Candidate& remote_candidate,
                               PortInterface* origin_port);
  void PingConnection(Connection* conn);
  void AddAllocatorSession(std::unique_ptr<PortAllocatorSession> session);
  void AddConnection(Connection* connection);

  void OnPortReady(PortAllocatorSession* session, PortInterface* port);
  void OnPortsPruned(PortAllocatorSession* session,
                     const std::vector<PortInterface*>& ports);
  void OnCandidatesReady(PortAllocatorSession* session,
                         const std::vector<Candidate>& candidates);
  void OnCandidateError(PortAllocatorSession* session,
                        const IceCandidateErrorEvent& event);
  void OnCandidatesRemoved(PortAllocatorSession* session,
                           const std::vector<Candidate>& candidates);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);
  void OnUnknownAddress(PortInterface* port,
                        const rtc::SocketAddress& addr,
                        ProtocolType proto,
                        IceMessage* stun_msg,
                        const std::string& remote_username,
                        bool port_muxed);
  void OnCandidateFilterChanged(uint32_t prev_filter, uint32_t cur_filter);

  void OnPortDestroyed(PortInterface* port);

  bool PrunePort(PortInterface* port);
  void OnRoleConflict(PortInterface* port);

  void OnConnectionStateChange(Connection* connection);
  void OnReadPacket(Connection* connection, const rtc::ReceivedPacket& packet);
  void OnSentPacket(const rtc::SentPacket& sent_packet);
  void OnReadyToSend(Connection* connection);
  void OnConnectionDestroyed(Connection* connection);

  void OnNominated(Connection* conn);

  void LogCandidatePairConfig(Connection* conn,
                              webrtc::IceCandidatePairConfigType type);

  uint32_t GetNominationAttr(Connection* conn) const;
  bool GetUseCandidateAttr(Connection* conn) const;

  bool AllowedToPruneConnections() const;

  IceParameters* remote_ice() {
    RTC_DCHECK_RUN_ON(network_thread_);
    return remote_ice_parameters_.empty() ? nullptr
                                          : &remote_ice_parameters_.back();
  }

  const IceParameters* FindRemoteIceFromUfrag(absl::string_view ufrag,
                                              uint32_t* generation);

  uint32_t remote_ice_generation() {
    RTC_DCHECK_RUN_ON(network_thread_);
    return remote_ice_parameters_.empty()
               ? 0
               : static_cast<uint32_t>(remote_ice_parameters_.size() - 1);
  }

  bool IsPortPruned(const PortInterface* port) const;

  bool IsRemoteCandidatePruned(const Candidate& cand) const;

  void SetWritable(bool writable);

  void SetReceiving(bool receiving);

  Candidate SanitizeLocalCandidate(const Candidate& c) const;

  Candidate SanitizeRemoteCandidate(const Candidate& c) const;

  Connection* FromIceController(const Connection* conn) {

    RTC_DCHECK(FindConnection(conn));
    return const_cast<Connection*>(conn);
  }

  int64_t ComputeEstimatedDisconnectedTimeMs(int64_t now,
                                             Connection* old_connection);

  void ParseFieldTrials(const webrtc::FieldTrialsView* field_trials);

  std::string transport_name_ RTC_GUARDED_BY(network_thread_);
  int component_ RTC_GUARDED_BY(network_thread_);
  PortAllocator* allocator_ RTC_GUARDED_BY(network_thread_);
  webrtc::AsyncDnsResolverFactoryInterface* const async_dns_resolver_factory_
      RTC_GUARDED_BY(network_thread_);
  const std::unique_ptr<webrtc::AsyncDnsResolverFactoryInterface>
      owned_dns_resolver_factory_;
  rtc::Thread* const network_thread_;
  bool incoming_only_ RTC_GUARDED_BY(network_thread_);
  int error_ RTC_GUARDED_BY(network_thread_);
  std::vector<std::unique_ptr<PortAllocatorSession>> allocator_sessions_
      RTC_GUARDED_BY(network_thread_);

  std::vector<PortInterface*> ports_ RTC_GUARDED_BY(network_thread_);

  std::vector<PortInterface*> pruned_ports_ RTC_GUARDED_BY(network_thread_);

  Connection* selected_connection_ RTC_GUARDED_BY(network_thread_) = nullptr;
  std::vector<Connection*> connections_ RTC_GUARDED_BY(network_thread_);

  std::vector<RemoteCandidate> remote_candidates_
      RTC_GUARDED_BY(network_thread_);
  bool had_connection_ RTC_GUARDED_BY(network_thread_) =
      false;
  typedef std::map<rtc::Socket::Option, int> OptionMap;
  OptionMap options_ RTC_GUARDED_BY(network_thread_);
  IceParameters ice_parameters_ RTC_GUARDED_BY(network_thread_);
  std::vector<IceParameters> remote_ice_parameters_
      RTC_GUARDED_BY(network_thread_);
  IceMode remote_ice_mode_ RTC_GUARDED_BY(network_thread_);
  IceRole ice_role_ RTC_GUARDED_BY(network_thread_);
  uint64_t ice_tiebreaker_ RTC_GUARDED_BY(network_thread_);
  IceGatheringState gathering_state_ RTC_GUARDED_BY(network_thread_);
  std::unique_ptr<webrtc::BasicRegatheringController> regathering_controller_
      RTC_GUARDED_BY(network_thread_);
  int64_t last_ping_sent_ms_ RTC_GUARDED_BY(network_thread_) = 0;
  int weak_ping_interval_ RTC_GUARDED_BY(network_thread_) = WEAK_PING_INTERVAL;

  IceTransportState state_ RTC_GUARDED_BY(network_thread_) =
      IceTransportState::STATE_INIT;
  webrtc::IceTransportState standardized_state_
      RTC_GUARDED_BY(network_thread_) = webrtc::IceTransportState::kNew;
  IceConfig config_ RTC_GUARDED_BY(network_thread_);
  int last_sent_packet_id_ RTC_GUARDED_BY(network_thread_) =
      -1;

  uint32_t nomination_ RTC_GUARDED_BY(network_thread_) = 0;
  bool receiving_ RTC_GUARDED_BY(network_thread_) = false;
  bool writable_ RTC_GUARDED_BY(network_thread_) = false;
  bool has_been_writable_ RTC_GUARDED_BY(network_thread_) =
      false;

  absl::optional<rtc::NetworkRoute> network_route_
      RTC_GUARDED_BY(network_thread_);
  webrtc::IceEventLog ice_event_log_ RTC_GUARDED_BY(network_thread_);

  std::unique_ptr<ActiveIceControllerInterface> ice_controller_
      RTC_GUARDED_BY(network_thread_);

  struct CandidateAndResolver final {
    CandidateAndResolver(
        const Candidate& candidate,
        std::unique_ptr<webrtc::AsyncDnsResolverInterface>&& resolver);
    ~CandidateAndResolver();

    CandidateAndResolver(CandidateAndResolver&&) = default;
    CandidateAndResolver& operator=(CandidateAndResolver&&) = default;

    Candidate candidate_;
    std::unique_ptr<webrtc::AsyncDnsResolverInterface> resolver_;
  };
  std::vector<CandidateAndResolver> resolvers_ RTC_GUARDED_BY(network_thread_);
  void FinishAddingRemoteCandidate(const Candidate& new_remote_candidate);
  void OnCandidateResolved(webrtc::AsyncDnsResolverInterface* resolver);
  void AddRemoteCandidateWithResult(
      Candidate candidate,
      const webrtc::AsyncDnsResolverResult& result);

  std::unique_ptr<StunAttribute> GoogDeltaReceived(
      const StunByteStringAttribute*);
  void GoogDeltaAckReceived(webrtc::RTCErrorOr<const StunUInt64Attribute*>);

  uint64_t bytes_sent_ = 0;
  uint64_t bytes_received_ = 0;
  uint64_t packets_sent_ = 0;
  uint64_t packets_received_ = 0;

  uint32_t selected_candidate_pair_changes_ = 0;

  int64_t last_data_received_ms_ = 0;

  IceFieldTrials ice_field_trials_;

  StunDictionaryWriter stun_dict_writer_;

  StunDictionaryView stun_dict_view_;
};

}

#endif
