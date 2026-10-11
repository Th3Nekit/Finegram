/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/p2p_transport_channel.h"

#include <errno.h>
#include <stdlib.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/async_dns_resolver.h"
#include "api/candidate.h"
#include "api/field_trials_view.h"
#include "api/units/time_delta.h"
#include "logging/rtc_event_log/ice_logger.h"
#include "p2p/base/basic_ice_controller.h"
#include "p2p/base/connection.h"
#include "p2p/base/connection_info.h"
#include "p2p/base/port.h"
#include "p2p/base/wrapping_active_ice_controller.h"
#include "rtc_base/checks.h"
#include "rtc_base/crc32.h"
#include "rtc_base/experiments/struct_parameters_parser.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/network.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/metrics.h"

namespace {

cricket::PortInterface::CandidateOrigin GetOrigin(
    cricket::PortInterface* port,
    cricket::PortInterface* origin_port) {
  if (!origin_port)
    return cricket::PortInterface::ORIGIN_MESSAGE;
  else if (port == origin_port)
    return cricket::PortInterface::ORIGIN_THIS_PORT;
  else
    return cricket::PortInterface::ORIGIN_OTHER_PORT;
}

uint32_t GetWeakPingIntervalInFieldTrial(
    const webrtc::FieldTrialsView* field_trials) {
  if (field_trials != nullptr) {
    uint32_t weak_ping_interval =
        ::strtoul(field_trials->Lookup("WebRTC-StunInterPacketDelay").c_str(),
                  nullptr, 10);
    if (weak_ping_interval) {
      return static_cast<int>(weak_ping_interval);
    }
  }
  return cricket::WEAK_PING_INTERVAL;
}

rtc::RouteEndpoint CreateRouteEndpointFromCandidate(
    bool local,
    const cricket::Candidate& candidate,
    bool uses_turn) {
  auto adapter_type = candidate.network_type();
  if (!local && adapter_type == rtc::ADAPTER_TYPE_UNKNOWN) {
    bool vpn;
    std::tie(adapter_type, vpn) =
        rtc::Network::GuessAdapterFromNetworkCost(candidate.network_cost());
  }

  uint16_t adapter_id = static_cast<int>(adapter_type);
  return rtc::RouteEndpoint(adapter_type, adapter_id, candidate.network_id(),
                            uses_turn);
}

using ::webrtc::RTCError;
using ::webrtc::RTCErrorType;
using ::webrtc::SafeTask;
using ::webrtc::TimeDelta;

}

namespace cricket {

bool IceCredentialsChanged(absl::string_view old_ufrag,
                           absl::string_view old_pwd,
                           absl::string_view new_ufrag,
                           absl::string_view new_pwd) {

  return (old_ufrag != new_ufrag) || (old_pwd != new_pwd);
}

std::unique_ptr<P2PTransportChannel> P2PTransportChannel::Create(
    absl::string_view transport_name,
    int component,
    webrtc::IceTransportInit init) {
  return absl::WrapUnique(new P2PTransportChannel(
      transport_name, component, init.port_allocator(),
      init.async_dns_resolver_factory(), nullptr, init.event_log(),
      init.ice_controller_factory(), init.active_ice_controller_factory(),
      init.field_trials()));
}

P2PTransportChannel::P2PTransportChannel(
    absl::string_view transport_name,
    int component,
    PortAllocator* allocator,
    const webrtc::FieldTrialsView* field_trials)
    : P2PTransportChannel(transport_name,
                          component,
                          allocator,
                                                            nullptr,
                                                            nullptr,
                                           nullptr,
                                                        nullptr,
                                                               nullptr,
                          field_trials) {}

P2PTransportChannel::P2PTransportChannel(
    absl::string_view transport_name,
    int component,
    PortAllocator* allocator,
    webrtc::AsyncDnsResolverFactoryInterface* async_dns_resolver_factory,
    std::unique_ptr<webrtc::AsyncDnsResolverFactoryInterface>
        owned_dns_resolver_factory,
    webrtc::RtcEventLog* event_log,
    IceControllerFactoryInterface* ice_controller_factory,
    ActiveIceControllerFactoryInterface* active_ice_controller_factory,
    const webrtc::FieldTrialsView* field_trials)
    : transport_name_(transport_name),
      component_(component),
      allocator_(allocator),

      async_dns_resolver_factory_(owned_dns_resolver_factory
                                      ? owned_dns_resolver_factory.get()
                                      : async_dns_resolver_factory),
      owned_dns_resolver_factory_(std::move(owned_dns_resolver_factory)),
      network_thread_(rtc::Thread::Current()),
      incoming_only_(false),
      error_(0),
      remote_ice_mode_(ICEMODE_FULL),
      ice_role_(ICEROLE_UNKNOWN),
      ice_tiebreaker_(0),
      gathering_state_(kIceGatheringNew),
      weak_ping_interval_(GetWeakPingIntervalInFieldTrial(field_trials)),
      config_(RECEIVING_TIMEOUT,
              BACKUP_CONNECTION_PING_INTERVAL,
              GATHER_ONCE                                 ,
              false                                             ,
              STRONG_AND_STABLE_WRITABLE_CONNECTION_PING_INTERVAL,
              true                                          ,
              REGATHER_ON_FAILED_NETWORKS_INTERVAL,
              RECEIVING_SWITCHING_DELAY) {
  TRACE_EVENT0("webrtc", "P2PTransportChannel::P2PTransportChannel");
  RTC_DCHECK(allocator_ != nullptr);

  RTC_DCHECK(ValidateIceConfig(config_).ok());
  webrtc::BasicRegatheringController::Config regathering_config;
  regathering_config.regather_on_failed_networks_interval =
      config_.regather_on_failed_networks_interval_or_default();
  regathering_controller_ =
      std::make_unique<webrtc::BasicRegatheringController>(
          regathering_config, this, network_thread_);

  allocator_->SignalCandidateFilterChanged.connect(
      this, &P2PTransportChannel::OnCandidateFilterChanged);
  ice_event_log_.set_event_log(event_log);

  ParseFieldTrials(field_trials);

  IceControllerFactoryArgs args{
      [this] { return GetState(); }, [this] { return GetIceRole(); },
      [this](const Connection* connection) {
        return IsPortPruned(connection->port()) ||
               IsRemoteCandidatePruned(connection->remote_candidate());
      },
      &ice_field_trials_,
      field_trials ? field_trials->Lookup("WebRTC-IceControllerFieldTrials")
                   : ""};

  if (active_ice_controller_factory) {
    ActiveIceControllerFactoryArgs active_args{args,
                                                                this};
    ice_controller_ = active_ice_controller_factory->Create(active_args);
  } else {
    ice_controller_ = std::make_unique<WrappingActiveIceController>(
                         this, ice_controller_factory, args);
  }
}

P2PTransportChannel::~P2PTransportChannel() {
  TRACE_EVENT0("webrtc", "P2PTransportChannel::~P2PTransportChannel");
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<Connection*> copy(connections_.begin(), connections_.end());
  for (Connection* connection : copy) {
    connection->SignalDestroyed.disconnect(this);
    RemoveConnection(connection);
    connection->Destroy();
  }
  resolvers_.clear();
}

void P2PTransportChannel::AddAllocatorSession(
    std::unique_ptr<PortAllocatorSession> session) {
  RTC_DCHECK_RUN_ON(network_thread_);

  session->set_generation(static_cast<uint32_t>(allocator_sessions_.size()));
  session->SignalPortReady.connect(this, &P2PTransportChannel::OnPortReady);
  session->SignalPortsPruned.connect(this, &P2PTransportChannel::OnPortsPruned);
  session->SignalCandidatesReady.connect(
      this, &P2PTransportChannel::OnCandidatesReady);
  session->SignalCandidateError.connect(this,
                                        &P2PTransportChannel::OnCandidateError);
  session->SignalCandidatesRemoved.connect(
      this, &P2PTransportChannel::OnCandidatesRemoved);
  session->SignalCandidatesAllocationDone.connect(
      this, &P2PTransportChannel::OnCandidatesAllocationDone);
  if (!allocator_sessions_.empty()) {
    allocator_session()->PruneAllPorts();
  }
  allocator_sessions_.push_back(std::move(session));
  regathering_controller_->set_allocator_session(allocator_session());

  PruneAllPorts();
}

void P2PTransportChannel::AddConnection(Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  connection->set_receiving_timeout(config_.receiving_timeout);
  connection->set_unwritable_timeout(config_.ice_unwritable_timeout);
  connection->set_unwritable_min_checks(config_.ice_unwritable_min_checks);
  connection->set_inactive_timeout(config_.ice_inactive_timeout);
  connection->RegisterReceivedPacketCallback(
      [&](Connection* connection, const rtc::ReceivedPacket& packet) {
        OnReadPacket(connection, packet);
      });
  connection->SignalReadyToSend.connect(this,
                                        &P2PTransportChannel::OnReadyToSend);
  connection->SignalStateChange.connect(
      this, &P2PTransportChannel::OnConnectionStateChange);
  connection->SignalDestroyed.connect(
      this, &P2PTransportChannel::OnConnectionDestroyed);
  connection->SignalNominated.connect(this, &P2PTransportChannel::OnNominated);

  had_connection_ = true;

  connection->set_ice_event_log(&ice_event_log_);
  connection->SetIceFieldTrials(&ice_field_trials_);
  connection->SetStunDictConsumer(
      [this](const StunByteStringAttribute* delta) {
        return GoogDeltaReceived(delta);
      },
      [this](webrtc::RTCErrorOr<const StunUInt64Attribute*> delta_ack) {
        GoogDeltaAckReceived(std::move(delta_ack));
      });
  LogCandidatePairConfig(connection,
                         webrtc::IceCandidatePairConfigType::kAdded);

  connections_.push_back(connection);
  ice_controller_->OnConnectionAdded(connection);
}

void P2PTransportChannel::ForgetLearnedStateForConnections(
    rtc::ArrayView<const Connection* const> connections) {
  for (const Connection* con : connections) {
    FromIceController(con)->ForgetLearnedState();
  }
}

void P2PTransportChannel::SetIceRole(IceRole ice_role) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (ice_role_ != ice_role) {
    ice_role_ = ice_role;
    for (PortInterface* port : ports_) {
      port->SetIceRole(ice_role);
    }

    for (PortInterface* port : pruned_ports_) {
      port->SetIceRole(ice_role);
    }
  }
}

IceRole P2PTransportChannel::GetIceRole() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return ice_role_;
}

void P2PTransportChannel::SetIceTiebreaker(uint64_t tiebreaker) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!ports_.empty() || !pruned_ports_.empty()) {
    RTC_LOG(LS_ERROR)
        << "Attempt to change tiebreaker after Port has been allocated.";
    return;
  }

  ice_tiebreaker_ = tiebreaker;
}

IceTransportState P2PTransportChannel::GetState() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return state_;
}

webrtc::IceTransportState P2PTransportChannel::GetIceTransportState() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return standardized_state_;
}

const std::string& P2PTransportChannel::transport_name() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return transport_name_;
}

int P2PTransportChannel::component() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return component_;
}

bool P2PTransportChannel::writable() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return writable_;
}

bool P2PTransportChannel::receiving() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return receiving_;
}

IceGatheringState P2PTransportChannel::gathering_state() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return gathering_state_;
}

absl::optional<int> P2PTransportChannel::GetRttEstimate() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (selected_connection_ != nullptr &&
      selected_connection_->rtt_samples() > 0) {
    return selected_connection_->rtt();
  } else {
    return absl::nullopt;
  }
}

absl::optional<const CandidatePair>
P2PTransportChannel::GetSelectedCandidatePair() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (selected_connection_ == nullptr) {
    return absl::nullopt;
  }

  CandidatePair pair;
  pair.local = SanitizeLocalCandidate(selected_connection_->local_candidate());
  pair.remote =
      SanitizeRemoteCandidate(selected_connection_->remote_candidate());
  return pair;
}

IceTransportState P2PTransportChannel::ComputeState() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!had_connection_) {
    return IceTransportState::STATE_INIT;
  }

  std::vector<Connection*> active_connections;
  for (Connection* connection : connections_) {
    if (connection->active()) {
      active_connections.push_back(connection);
    }
  }
  if (active_connections.empty()) {
    return IceTransportState::STATE_FAILED;
  }

  std::set<const rtc::Network*> networks;
  for (Connection* connection : active_connections) {
    const rtc::Network* network = connection->network();
    if (networks.find(network) == networks.end()) {
      networks.insert(network);
    } else {
      RTC_LOG(LS_VERBOSE) << ToString()
                          << ": Ice not completed yet for this channel as "
                          << network->ToString()
                          << " has more than 1 connection.";
      return IceTransportState::STATE_CONNECTING;
    }
  }

  ice_event_log_.DumpCandidatePairDescriptionToMemoryAsConfigEvents();
  return IceTransportState::STATE_COMPLETED;
}

webrtc::IceTransportState P2PTransportChannel::ComputeIceTransportState()
    const {
  RTC_DCHECK_RUN_ON(network_thread_);
  bool has_connection = false;
  for (Connection* connection : connections_) {
    if (connection->active()) {
      has_connection = true;
      break;
    }
  }

  if (had_connection_ && !has_connection) {
    return webrtc::IceTransportState::kFailed;
  }

  if (!writable() && has_been_writable_) {
    return webrtc::IceTransportState::kDisconnected;
  }

  if (!had_connection_ && !has_connection) {
    return webrtc::IceTransportState::kNew;
  }

  if (has_connection && !writable()) {

    return webrtc::IceTransportState::kChecking;
  }

  return webrtc::IceTransportState::kConnected;
}

void P2PTransportChannel::SetIceParameters(const IceParameters& ice_params) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << "Set ICE ufrag: " << ice_params.ufrag
                   << " pwd: " << ice_params.pwd << " on transport "
                   << transport_name();
  ice_parameters_ = ice_params;

}

void P2PTransportChannel::SetRemoteIceParameters(
    const IceParameters& ice_params) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << "Received remote ICE parameters: ufrag="
                   << ice_params.ufrag << ", renomination "
                   << (ice_params.renomination ? "enabled" : "disabled");
  IceParameters* current_ice = remote_ice();
  if (!current_ice || *current_ice != ice_params) {

    remote_ice_parameters_.push_back(ice_params);
  }

  for (RemoteCandidate& candidate : remote_candidates_) {
    if (candidate.username() == ice_params.ufrag &&
        candidate.password().empty()) {
      candidate.set_password(ice_params.pwd);
    }
  }

  for (Connection* conn : connections_) {
    conn->MaybeSetRemoteIceParametersAndGeneration(
        ice_params, static_cast<int>(remote_ice_parameters_.size() - 1));
  }

  ice_controller_->OnSortAndSwitchRequest(
      IceSwitchReason::REMOTE_CANDIDATE_GENERATION_CHANGE);
}

void P2PTransportChannel::SetRemoteIceMode(IceMode mode) {
  RTC_DCHECK_RUN_ON(network_thread_);
  remote_ice_mode_ = mode;
}

void P2PTransportChannel::SetIceConfig(const IceConfig& config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (config_.continual_gathering_policy != config.continual_gathering_policy) {
    if (!allocator_sessions_.empty()) {
      RTC_LOG(LS_ERROR) << "Trying to change continual gathering policy "
                           "when gathering has already started!";
    } else {
      config_.continual_gathering_policy = config.continual_gathering_policy;
      RTC_LOG(LS_INFO) << "Set continual_gathering_policy to "
                       << config_.continual_gathering_policy;
    }
  }

  if (config_.backup_connection_ping_interval !=
      config.backup_connection_ping_interval) {
    config_.backup_connection_ping_interval =
        config.backup_connection_ping_interval;
    RTC_LOG(LS_INFO) << "Set backup connection ping interval to "
                     << config_.backup_connection_ping_interval_or_default()
                     << " milliseconds.";
  }
  if (config_.receiving_timeout != config.receiving_timeout) {
    config_.receiving_timeout = config.receiving_timeout;
    for (Connection* connection : connections_) {
      connection->set_receiving_timeout(config_.receiving_timeout);
    }
    RTC_LOG(LS_INFO) << "Set ICE receiving timeout to "
                     << config_.receiving_timeout_or_default()
                     << " milliseconds";
  }

  config_.prioritize_most_likely_candidate_pairs =
      config.prioritize_most_likely_candidate_pairs;
  RTC_LOG(LS_INFO) << "Set ping most likely connection to "
                   << config_.prioritize_most_likely_candidate_pairs;

  if (config_.stable_writable_connection_ping_interval !=
      config.stable_writable_connection_ping_interval) {
    config_.stable_writable_connection_ping_interval =
        config.stable_writable_connection_ping_interval;
    RTC_LOG(LS_INFO)
        << "Set stable_writable_connection_ping_interval to "
        << config_.stable_writable_connection_ping_interval_or_default();
  }

  if (config_.presume_writable_when_fully_relayed !=
      config.presume_writable_when_fully_relayed) {
    if (!connections_.empty()) {
      RTC_LOG(LS_ERROR) << "Trying to change 'presume writable' "
                           "while connections already exist!";
    } else {
      config_.presume_writable_when_fully_relayed =
          config.presume_writable_when_fully_relayed;
      RTC_LOG(LS_INFO) << "Set presume writable when fully relayed to "
                       << config_.presume_writable_when_fully_relayed;
    }
  }

  config_.surface_ice_candidates_on_ice_transport_type_changed =
      config.surface_ice_candidates_on_ice_transport_type_changed;
  if (config_.surface_ice_candidates_on_ice_transport_type_changed &&
      config_.continual_gathering_policy != GATHER_CONTINUALLY) {
    RTC_LOG(LS_WARNING)
        << "surface_ice_candidates_on_ice_transport_type_changed is "
           "ineffective since we do not gather continually.";
  }

  if (config_.regather_on_failed_networks_interval !=
      config.regather_on_failed_networks_interval) {
    config_.regather_on_failed_networks_interval =
        config.regather_on_failed_networks_interval;
    RTC_LOG(LS_INFO)
        << "Set regather_on_failed_networks_interval to "
        << config_.regather_on_failed_networks_interval_or_default();
  }

  if (config_.receiving_switching_delay != config.receiving_switching_delay) {
    config_.receiving_switching_delay = config.receiving_switching_delay;
    RTC_LOG(LS_INFO) << "Set receiving_switching_delay to "
                     << config_.receiving_switching_delay_or_default();
  }

  if (config_.default_nomination_mode != config.default_nomination_mode) {
    config_.default_nomination_mode = config.default_nomination_mode;
    RTC_LOG(LS_INFO) << "Set default nomination mode to "
                     << static_cast<int>(config_.default_nomination_mode);
  }

  if (config_.ice_check_interval_strong_connectivity !=
      config.ice_check_interval_strong_connectivity) {
    config_.ice_check_interval_strong_connectivity =
        config.ice_check_interval_strong_connectivity;
    RTC_LOG(LS_INFO)
        << "Set strong ping interval to "
        << config_.ice_check_interval_strong_connectivity_or_default();
  }

  if (config_.ice_check_interval_weak_connectivity !=
      config.ice_check_interval_weak_connectivity) {
    config_.ice_check_interval_weak_connectivity =
        config.ice_check_interval_weak_connectivity;
    RTC_LOG(LS_INFO)
        << "Set weak ping interval to "
        << config_.ice_check_interval_weak_connectivity_or_default();
  }

  if (config_.ice_check_min_interval != config.ice_check_min_interval) {
    config_.ice_check_min_interval = config.ice_check_min_interval;
    RTC_LOG(LS_INFO) << "Set min ping interval to "
                     << config_.ice_check_min_interval_or_default();
  }

  if (config_.ice_unwritable_timeout != config.ice_unwritable_timeout) {
    config_.ice_unwritable_timeout = config.ice_unwritable_timeout;
    for (Connection* conn : connections_) {
      conn->set_unwritable_timeout(config_.ice_unwritable_timeout);
    }
    RTC_LOG(LS_INFO) << "Set unwritable timeout to "
                     << config_.ice_unwritable_timeout_or_default();
  }

  if (config_.ice_unwritable_min_checks != config.ice_unwritable_min_checks) {
    config_.ice_unwritable_min_checks = config.ice_unwritable_min_checks;
    for (Connection* conn : connections_) {
      conn->set_unwritable_min_checks(config_.ice_unwritable_min_checks);
    }
    RTC_LOG(LS_INFO) << "Set unwritable min checks to "
                     << config_.ice_unwritable_min_checks_or_default();
  }

  if (config_.ice_inactive_timeout != config.ice_inactive_timeout) {
    config_.ice_inactive_timeout = config.ice_inactive_timeout;
    for (Connection* conn : connections_) {
      conn->set_inactive_timeout(config_.ice_inactive_timeout);
    }
    RTC_LOG(LS_INFO) << "Set inactive timeout to "
                     << config_.ice_inactive_timeout_or_default();
  }

  if (config_.network_preference != config.network_preference) {
    config_.network_preference = config.network_preference;
    ice_controller_->OnSortAndSwitchRequest(
        IceSwitchReason::NETWORK_PREFERENCE_CHANGE);
    RTC_LOG(LS_INFO) << "Set network preference to "
                     << (config_.network_preference.has_value()
                             ? config_.network_preference.value()
                             : -1);

  }

  if (config_.stun_keepalive_interval != config.stun_keepalive_interval) {
    config_.stun_keepalive_interval = config.stun_keepalive_interval;
    allocator_session()->SetStunKeepaliveIntervalForReadyPorts(
        config_.stun_keepalive_interval);
    RTC_LOG(LS_INFO) << "Set STUN keepalive interval to "
                     << config.stun_keepalive_interval_or_default();
  }

  webrtc::BasicRegatheringController::Config regathering_config;
  regathering_config.regather_on_failed_networks_interval =
      config_.regather_on_failed_networks_interval_or_default();
  regathering_controller_->SetConfig(regathering_config);

  config_.vpn_preference = config.vpn_preference;
  allocator_->SetVpnPreference(config_.vpn_preference);

  ice_controller_->SetIceConfig(config_);

  RTC_DCHECK(ValidateIceConfig(config_).ok());
}

void P2PTransportChannel::ParseFieldTrials(
    const webrtc::FieldTrialsView* field_trials) {
  if (field_trials == nullptr) {
    return;
  }

  if (field_trials->IsEnabled("WebRTC-ExtraICEPing")) {
    RTC_LOG(LS_INFO) << "Set WebRTC-ExtraICEPing: Enabled";
  }

  webrtc::StructParametersParser::Create(

      "skip_relay_to_non_relay_connections",
      &ice_field_trials_.skip_relay_to_non_relay_connections,

      "max_outstanding_pings", &ice_field_trials_.max_outstanding_pings,

      "initial_select_dampening", &ice_field_trials_.initial_select_dampening,

      "initial_select_dampening_ping_received",
      &ice_field_trials_.initial_select_dampening_ping_received,

      "announce_goog_ping", &ice_field_trials_.announce_goog_ping,

      "enable_goog_ping", &ice_field_trials_.enable_goog_ping,

      "rtt_estimate_halftime_ms", &ice_field_trials_.rtt_estimate_halftime_ms,

      "send_ping_on_switch_ice_controlling",
      &ice_field_trials_.send_ping_on_switch_ice_controlling,

      "send_ping_on_selected_ice_controlling",
      &ice_field_trials_.send_ping_on_selected_ice_controlling,

      "send_ping_on_nomination_ice_controlled",
      &ice_field_trials_.send_ping_on_nomination_ice_controlled,

      "dead_connection_timeout_ms",
      &ice_field_trials_.dead_connection_timeout_ms,

      "stop_gather_on_strongly_connected",
      &ice_field_trials_.stop_gather_on_strongly_connected,

      "enable_goog_delta", &ice_field_trials_.enable_goog_delta,
      "answer_goog_delta", &ice_field_trials_.answer_goog_delta)
      ->Parse(field_trials->Lookup("WebRTC-IceFieldTrials"));

  if (ice_field_trials_.dead_connection_timeout_ms < 30000) {
    RTC_LOG(LS_WARNING) << "dead_connection_timeout_ms set to "
                        << ice_field_trials_.dead_connection_timeout_ms
                        << " increasing it to 30000";
    ice_field_trials_.dead_connection_timeout_ms = 30000;
  }

  if (ice_field_trials_.skip_relay_to_non_relay_connections) {
    RTC_LOG(LS_INFO) << "Set skip_relay_to_non_relay_connections";
  }

  if (ice_field_trials_.max_outstanding_pings.has_value()) {
    RTC_LOG(LS_INFO) << "Set max_outstanding_pings: "
                     << *ice_field_trials_.max_outstanding_pings;
  }

  if (ice_field_trials_.initial_select_dampening.has_value()) {
    RTC_LOG(LS_INFO) << "Set initial_select_dampening: "
                     << *ice_field_trials_.initial_select_dampening;
  }

  if (ice_field_trials_.initial_select_dampening_ping_received.has_value()) {
    RTC_LOG(LS_INFO)
        << "Set initial_select_dampening_ping_received: "
        << *ice_field_trials_.initial_select_dampening_ping_received;
  }

  webrtc::StructParametersParser::Create("override_dscp",
                                         &ice_field_trials_.override_dscp)
      ->Parse(field_trials->Lookup("WebRTC-DscpFieldTrial"));

  if (ice_field_trials_.override_dscp) {
    SetOption(rtc::Socket::OPT_DSCP, *ice_field_trials_.override_dscp);
  }

  std::string field_trial_string =
      field_trials->Lookup("WebRTC-SetSocketReceiveBuffer");
  int receive_buffer_size_kb = 0;
  sscanf(field_trial_string.c_str(), "Enabled-%d", &receive_buffer_size_kb);
  if (receive_buffer_size_kb > 0) {
    RTC_LOG(LS_INFO) << "Set WebRTC-SetSocketReceiveBuffer: Enabled and set to "
                     << receive_buffer_size_kb << "kb";
    SetOption(rtc::Socket::OPT_RCVBUF, receive_buffer_size_kb * 1024);
  }

  ice_field_trials_.piggyback_ice_check_acknowledgement =
      field_trials->IsEnabled("WebRTC-PiggybackIceCheckAcknowledgement");

  ice_field_trials_.extra_ice_ping =
      field_trials->IsEnabled("WebRTC-ExtraICEPing");

  if (!ice_field_trials_.enable_goog_delta) {
    stun_dict_writer_.Disable();
  }
}

const IceConfig& P2PTransportChannel::config() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return config_;
}

RTCError P2PTransportChannel::ValidateIceConfig(const IceConfig& config) {
  if (config.ice_check_interval_strong_connectivity_or_default() <
      config.ice_check_interval_weak_connectivity.value_or(
          GetWeakPingIntervalInFieldTrial(nullptr))) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of candidate pairs is shorter when ICE is "
                    "strongly connected than that when ICE is weakly "
                    "connected");
  }

  if (config.receiving_timeout_or_default() <
      std::max(config.ice_check_interval_strong_connectivity_or_default(),
               config.ice_check_min_interval_or_default())) {
    return RTCError(
        RTCErrorType::INVALID_PARAMETER,
        "Receiving timeout is shorter than the minimal ping interval.");
  }

  if (config.backup_connection_ping_interval_or_default() <
      config.ice_check_interval_strong_connectivity_or_default()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of backup candidate pairs is shorter than "
                    "that of general candidate pairs when ICE is strongly "
                    "connected");
  }

  if (config.stable_writable_connection_ping_interval_or_default() <
      config.ice_check_interval_strong_connectivity_or_default()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "Ping interval of stable and writable candidate pairs is "
                    "shorter than that of general candidate pairs when ICE is "
                    "strongly connected");
  }

  if (config.ice_unwritable_timeout_or_default() >
      config.ice_inactive_timeout_or_default()) {
    return RTCError(RTCErrorType::INVALID_PARAMETER,
                    "The timeout period for the writability state to become "
                    "UNRELIABLE is longer than that to become TIMEOUT.");
  }

  return RTCError::OK();
}

const Connection* P2PTransportChannel::selected_connection() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return selected_connection_;
}

int P2PTransportChannel::check_receiving_interval() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return std::max(MIN_CHECK_RECEIVING_INTERVAL,
                  config_.receiving_timeout_or_default() / 10);
}

void P2PTransportChannel::MaybeStartGathering() {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (ice_parameters_.ufrag.empty() || ice_parameters_.pwd.empty()) {
    RTC_LOG(LS_ERROR)
        << "Cannot gather candidates because ICE parameters are empty"
           " ufrag: "
        << ice_parameters_.ufrag << " pwd: " << ice_parameters_.pwd;
    return;
  }

  if (allocator_sessions_.empty() ||
      IceCredentialsChanged(allocator_sessions_.back()->ice_ufrag(),
                            allocator_sessions_.back()->ice_pwd(),
                            ice_parameters_.ufrag, ice_parameters_.pwd)) {
    if (gathering_state_ != kIceGatheringGathering) {
      gathering_state_ = kIceGatheringGathering;
      SendGatheringStateEvent();
    }

    for (const auto& session : allocator_sessions_) {
      if (session->IsStopped()) {
        continue;
      }
      session->StopGettingPorts();
    }

    std::unique_ptr<PortAllocatorSession> pooled_session =
        allocator_->TakePooledSession(transport_name(), component(),
                                      ice_parameters_.ufrag,
                                      ice_parameters_.pwd);
    if (pooled_session) {
      AddAllocatorSession(std::move(pooled_session));
      PortAllocatorSession* raw_pooled_session =
          allocator_sessions_.back().get();

      OnCandidatesReady(raw_pooled_session,
                        raw_pooled_session->ReadyCandidates());
      for (PortInterface* port : allocator_sessions_.back()->ReadyPorts()) {
        OnPortReady(raw_pooled_session, port);
      }
      if (allocator_sessions_.back()->CandidatesAllocationDone()) {
        OnCandidatesAllocationDone(raw_pooled_session);
      }
    } else {
      AddAllocatorSession(allocator_->CreateSession(
          transport_name(), component(), ice_parameters_.ufrag,
          ice_parameters_.pwd));
      allocator_sessions_.back()->StartGettingPorts();
    }
  }
}

void P2PTransportChannel::OnPortReady(PortAllocatorSession* session,
                                      PortInterface* port) {
  RTC_DCHECK_RUN_ON(network_thread_);

  for (OptionMap::const_iterator it = options_.begin(); it != options_.end();
       ++it) {
    int val = port->SetOption(it->first, it->second);
    if (val < 0) {

      RTC_LOG(LS_INFO) << port->ToString() << ": SetOption(" << it->first
                       << ", " << it->second
                       << ") failed: " << port->GetError();
    }
  }

  port->SetIceRole(ice_role_);
  port->SetIceTiebreaker(ice_tiebreaker_);
  ports_.push_back(port);
  port->SignalUnknownAddress.connect(this,
                                     &P2PTransportChannel::OnUnknownAddress);
  port->SubscribePortDestroyed(
      [this](PortInterface* port) { OnPortDestroyed(port); });

  port->SignalRoleConflict.connect(this, &P2PTransportChannel::OnRoleConflict);
  port->SignalSentPacket.connect(this, &P2PTransportChannel::OnSentPacket);

  std::vector<RemoteCandidate>::iterator iter;
  for (iter = remote_candidates_.begin(); iter != remote_candidates_.end();
       ++iter) {
    CreateConnection(port, *iter, iter->origin_port());
  }

  ice_controller_->OnImmediateSortAndSwitchRequest(
      IceSwitchReason::NEW_CONNECTION_FROM_LOCAL_CANDIDATE);
}

void P2PTransportChannel::OnCandidatesReady(
    PortAllocatorSession* session,
    const std::vector<Candidate>& candidates) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (size_t i = 0; i < candidates.size(); ++i) {
    SignalCandidateGathered(this, candidates[i]);
  }
}

void P2PTransportChannel::OnCandidateError(
    PortAllocatorSession* session,
    const IceCandidateErrorEvent& event) {
  RTC_DCHECK(network_thread_ == rtc::Thread::Current());
  if (candidate_error_callback_) {
    candidate_error_callback_(this, event);
  }
}

void P2PTransportChannel::OnCandidatesAllocationDone(
    PortAllocatorSession* session) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (config_.gather_continually()) {
    RTC_LOG(LS_INFO) << "P2PTransportChannel: " << transport_name()
                     << ", component " << component()
                     << " gathering complete, but using continual "
                        "gathering so not changing gathering state.";
    return;
  }
  gathering_state_ = kIceGatheringComplete;
  RTC_LOG(LS_INFO) << "P2PTransportChannel: " << transport_name()
                   << ", component " << component() << " gathering complete";
  SendGatheringStateEvent();
}

void P2PTransportChannel::OnUnknownAddress(PortInterface* port,
                                           const rtc::SocketAddress& address,
                                           ProtocolType proto,
                                           IceMessage* stun_msg,
                                           const std::string& remote_username,
                                           bool port_muxed) {
  RTC_DCHECK_RUN_ON(network_thread_);

  const Candidate* candidate = nullptr;
  for (const Candidate& c : remote_candidates_) {
    if (c.username() == remote_username && c.address() == address &&
        c.protocol() == ProtoToString(proto)) {
      candidate = &c;
      break;
    }
  }

  uint32_t remote_generation = 0;
  std::string remote_password;

  const IceParameters* ice_param =
      FindRemoteIceFromUfrag(remote_username, &remote_generation);

  if (ice_param != nullptr) {
    remote_password = ice_param->pwd;
  }

  Candidate remote_candidate;
  bool remote_candidate_is_new = (candidate == nullptr);
  if (!remote_candidate_is_new) {
    remote_candidate = *candidate;
  } else {

    const StunUInt32Attribute* priority_attr =
        stun_msg->GetUInt32(STUN_ATTR_PRIORITY);
    if (!priority_attr) {
      RTC_LOG(LS_WARNING) << "P2PTransportChannel::OnUnknownAddress - "
                             "No STUN_ATTR_PRIORITY found in the "
                             "stun request message";
      port->SendBindingErrorResponse(stun_msg, address, STUN_ERROR_BAD_REQUEST,
                                     STUN_ERROR_REASON_BAD_REQUEST);
      return;
    }
    int remote_candidate_priority = priority_attr->value();

    uint16_t network_id = 0;
    uint16_t network_cost = 0;
    const StunUInt32Attribute* network_attr =
        stun_msg->GetUInt32(STUN_ATTR_GOOG_NETWORK_INFO);
    if (network_attr) {
      uint32_t network_info = network_attr->value();
      network_id = static_cast<uint16_t>(network_info >> 16);
      network_cost = static_cast<uint16_t>(network_info);
    }

    remote_candidate = Candidate(
        component(), ProtoToString(proto), address, remote_candidate_priority,
        remote_username, remote_password, PRFLX_PORT_TYPE, remote_generation,
        "", network_id, network_cost);
    if (proto == PROTO_TCP) {
      remote_candidate.set_tcptype(TCPTYPE_ACTIVE_STR);
    }

    remote_candidate.set_foundation(
        rtc::ToString(rtc::ComputeCrc32(remote_candidate.id())));
  }

  if (port->GetConnection(remote_candidate.address())) {
    if (port_muxed) {
      RTC_LOG(LS_INFO) << "Connection already exists for peer reflexive "
                          "candidate: "
                       << remote_candidate.ToSensitiveString();
      return;
    } else {
      RTC_DCHECK_NOTREACHED();
      port->SendBindingErrorResponse(stun_msg, address, STUN_ERROR_SERVER_ERROR,
                                     STUN_ERROR_REASON_SERVER_ERROR);
      return;
    }
  }

  Connection* connection =
      port->CreateConnection(remote_candidate, PortInterface::ORIGIN_THIS_PORT);
  if (!connection) {

    port->SendBindingErrorResponse(stun_msg, address, STUN_ERROR_SERVER_ERROR,
                                   STUN_ERROR_REASON_SERVER_ERROR);
    return;
  }

  RTC_LOG(LS_INFO) << "Adding connection from "
                   << (remote_candidate_is_new ? "peer reflexive"
                                               : "resurrected")
                   << " candidate: " << remote_candidate.ToSensitiveString();
  AddConnection(connection);
  connection->HandleStunBindingOrGoogPingRequest(stun_msg);

  ice_controller_->OnImmediateSortAndSwitchRequest(
      IceSwitchReason::NEW_CONNECTION_FROM_UNKNOWN_REMOTE_ADDRESS);
}

void P2PTransportChannel::OnCandidateFilterChanged(uint32_t prev_filter,
                                                   uint32_t cur_filter) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (prev_filter == cur_filter || allocator_session() == nullptr) {
    return;
  }
  if (config_.surface_ice_candidates_on_ice_transport_type_changed) {
    allocator_session()->SetCandidateFilter(cur_filter);
  }
}

void P2PTransportChannel::OnRoleConflict(PortInterface* port) {
  SignalRoleConflict(this);

}

const IceParameters* P2PTransportChannel::FindRemoteIceFromUfrag(
    absl::string_view ufrag,
    uint32_t* generation) {
  RTC_DCHECK_RUN_ON(network_thread_);
  const auto& params = remote_ice_parameters_;
  auto it = std::find_if(
      params.rbegin(), params.rend(),
      [ufrag](const IceParameters& param) { return param.ufrag == ufrag; });
  if (it == params.rend()) {

    return nullptr;
  }
  *generation = params.rend() - it - 1;
  return &(*it);
}

void P2PTransportChannel::OnNominated(Connection* conn) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(ice_role_ == ICEROLE_CONTROLLED);

  if (selected_connection_ == conn) {
    return;
  }

  if (ice_field_trials_.send_ping_on_nomination_ice_controlled &&
      conn != nullptr) {
    SendPingRequestInternal(conn);
  }

  if (ice_controller_->OnImmediateSwitchRequest(
          IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE, conn)) {

    ice_controller_->OnSortAndSwitchRequest(
        IceSwitchReason::NOMINATION_ON_CONTROLLED_SIDE);
  } else {
    RTC_LOG(LS_INFO)
        << "Not switching the selected connection on controlled side yet: "
        << conn->ToString();
  }
}

void P2PTransportChannel::ResolveHostnameCandidate(const Candidate& candidate) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!async_dns_resolver_factory_) {
    RTC_LOG(LS_WARNING) << "Dropping ICE candidate with hostname address "
                           "(no AsyncResolverFactory)";
    return;
  }

  auto resolver = async_dns_resolver_factory_->Create();
  auto resptr = resolver.get();
  resolvers_.emplace_back(candidate, std::move(resolver));
  resptr->Start(candidate.address(),
                [this, resptr]() { OnCandidateResolved(resptr); });
  RTC_LOG(LS_INFO) << "Asynchronously resolving ICE candidate hostname "
                   << candidate.address().HostAsSensitiveURIString();
}

void P2PTransportChannel::AddRemoteCandidate(const Candidate& candidate) {
  RTC_DCHECK_RUN_ON(network_thread_);

  uint32_t generation = GetRemoteCandidateGeneration(candidate);

  if (generation < remote_ice_generation()) {
    RTC_LOG(LS_WARNING) << "Dropping a remote candidate because its ufrag "
                        << candidate.username()
                        << " indicates it was for a previous generation.";
    return;
  }

  Candidate new_remote_candidate(candidate);
  new_remote_candidate.set_generation(generation);

  if (remote_ice()) {
    if (candidate.username().empty()) {
      new_remote_candidate.set_username(remote_ice()->ufrag);
    }
    if (new_remote_candidate.username() == remote_ice()->ufrag) {
      if (candidate.password().empty()) {
        new_remote_candidate.set_password(remote_ice()->pwd);
      }
    } else {

      RTC_LOG(LS_WARNING)
          << "A remote candidate arrives with an unknown ufrag: "
          << candidate.username();
    }
  }

  if (new_remote_candidate.address().IsUnresolvedIP() && !absl::EndsWith(new_remote_candidate.address().hostname(), ".reflector")) {

    bool sharing_host = ((allocator_->candidate_filter() & CF_HOST) != 0);
    bool sharing_stun = ((allocator_->candidate_filter() & CF_REFLEXIVE) != 0);
    if (sharing_host || sharing_stun) {
      ResolveHostnameCandidate(new_remote_candidate);
    }
    return;
  }

  FinishAddingRemoteCandidate(new_remote_candidate);
}

P2PTransportChannel::CandidateAndResolver::CandidateAndResolver(
    const Candidate& candidate,
    std::unique_ptr<webrtc::AsyncDnsResolverInterface>&& resolver)
    : candidate_(candidate), resolver_(std::move(resolver)) {}

P2PTransportChannel::CandidateAndResolver::~CandidateAndResolver() {}

void P2PTransportChannel::OnCandidateResolved(
    webrtc::AsyncDnsResolverInterface* resolver) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto p =
      absl::c_find_if(resolvers_, [resolver](const CandidateAndResolver& cr) {
        return cr.resolver_.get() == resolver;
      });
  if (p == resolvers_.end()) {
    RTC_LOG(LS_ERROR) << "Unexpected AsyncDnsResolver return";
    RTC_DCHECK_NOTREACHED();
    return;
  }
  Candidate candidate = p->candidate_;
  AddRemoteCandidateWithResult(candidate, resolver->result());

  std::unique_ptr<webrtc::AsyncDnsResolverInterface> to_delete =
      std::move(p->resolver_);

  network_thread_->PostTask([to_delete = std::move(to_delete)] {});
  resolvers_.erase(p);
}

void P2PTransportChannel::AddRemoteCandidateWithResult(
    Candidate candidate,
    const webrtc::AsyncDnsResolverResult& result) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (result.GetError()) {
    RTC_LOG(LS_WARNING) << "Failed to resolve ICE candidate hostname "
                        << candidate.address().HostAsSensitiveURIString()
                        << " with error " << result.GetError();
    return;
  }

  rtc::SocketAddress resolved_address;

  bool have_address = result.GetResolvedAddress(AF_INET6, &resolved_address) ||
                      result.GetResolvedAddress(AF_INET, &resolved_address);
  if (!have_address) {
    RTC_LOG(LS_INFO) << "ICE candidate hostname "
                     << candidate.address().HostAsSensitiveURIString()
                     << " could not be resolved";
    return;
  }

  RTC_LOG(LS_INFO) << "Resolved ICE candidate hostname "
                   << candidate.address().HostAsSensitiveURIString() << " to "
                   << resolved_address.ipaddr().ToSensitiveString();
  candidate.set_address(resolved_address);
  FinishAddingRemoteCandidate(candidate);
}

void P2PTransportChannel::FinishAddingRemoteCandidate(
    const Candidate& new_remote_candidate) {
  RTC_DCHECK_RUN_ON(network_thread_);

  for (Connection* conn : connections_) {
    conn->MaybeUpdatePeerReflexiveCandidate(new_remote_candidate);
  }

  CreateConnections(new_remote_candidate, NULL);

  ice_controller_->OnImmediateSortAndSwitchRequest(
      IceSwitchReason::NEW_CONNECTION_FROM_REMOTE_CANDIDATE);
}

void P2PTransportChannel::RemoveRemoteCandidate(
    const Candidate& cand_to_remove) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto iter =
      std::remove_if(remote_candidates_.begin(), remote_candidates_.end(),
                     [cand_to_remove](const Candidate& candidate) {
                       return cand_to_remove.MatchesForRemoval(candidate);
                     });
  if (iter != remote_candidates_.end()) {
    RTC_LOG(LS_VERBOSE) << "Removed remote candidate "
                        << cand_to_remove.ToSensitiveString();
    remote_candidates_.erase(iter, remote_candidates_.end());
  }
}

void P2PTransportChannel::RemoveAllRemoteCandidates() {
  RTC_DCHECK_RUN_ON(network_thread_);
  remote_candidates_.clear();
}

bool P2PTransportChannel::CreateConnections(const Candidate& remote_candidate,
                                            PortInterface* origin_port) {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (!origin_port && IsDuplicateRemoteCandidate(remote_candidate)) {

    return true;
  }

  bool created = false;
  std::vector<PortInterface*>::reverse_iterator it;
  for (it = ports_.rbegin(); it != ports_.rend(); ++it) {
    if (CreateConnection(*it, remote_candidate, origin_port)) {
      if (*it == origin_port)
        created = true;
    }
  }

  if ((origin_port != NULL) && !absl::c_linear_search(ports_, origin_port)) {
    if (CreateConnection(origin_port, remote_candidate, origin_port))
      created = true;
  }

  RememberRemoteCandidate(remote_candidate, origin_port);

  return created;
}

bool P2PTransportChannel::CreateConnection(PortInterface* port,
                                           const Candidate& remote_candidate,
                                           PortInterface* origin_port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!port->SupportsProtocol(remote_candidate.protocol())) {
    return false;
  }

  if (ice_field_trials_.skip_relay_to_non_relay_connections) {
    if ((port->Type() != remote_candidate.type()) &&
        (port->Type() == RELAY_PORT_TYPE || remote_candidate.is_relay())) {
      RTC_LOG(LS_INFO) << ToString() << ": skip creating connection "
                       << port->Type() << " to "
                       << remote_candidate.type_name();
      return false;
    }
  }

  Connection* connection = port->GetConnection(remote_candidate.address());
  if (connection == nullptr || connection->remote_candidate().generation() <
                                   remote_candidate.generation()) {

    PortInterface::CandidateOrigin origin = GetOrigin(port, origin_port);
    if (origin == PortInterface::ORIGIN_MESSAGE && incoming_only_) {
      return false;
    }
    Connection* connection = port->CreateConnection(remote_candidate, origin);
    if (!connection) {
      return false;
    }
    AddConnection(connection);
    RTC_LOG(LS_INFO) << ToString()
                     << ": Created connection with origin: " << origin
                     << ", total: " << connections_.size();
    return true;
  }

  if (!remote_candidate.IsEquivalent(connection->remote_candidate())) {
    RTC_LOG(LS_INFO) << "Attempt to change a remote candidate."
                        " Existing remote candidate: "
                     << connection->remote_candidate().ToSensitiveString()
                     << "New remote candidate: "
                     << remote_candidate.ToSensitiveString();
  }
  return false;
}

bool P2PTransportChannel::FindConnection(const Connection* connection) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return absl::c_linear_search(connections_, connection);
}

uint32_t P2PTransportChannel::GetRemoteCandidateGeneration(
    const Candidate& candidate) {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (!candidate.username().empty()) {
    uint32_t generation = 0;
    if (!FindRemoteIceFromUfrag(candidate.username(), &generation)) {

      generation = static_cast<uint32_t>(remote_ice_parameters_.size());
    }
    return generation;
  }

  if (candidate.generation() > 0) {
    return candidate.generation();
  }

  return remote_ice_generation();
}

bool P2PTransportChannel::IsDuplicateRemoteCandidate(
    const Candidate& candidate) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (size_t i = 0; i < remote_candidates_.size(); ++i) {
    if (remote_candidates_[i].IsEquivalent(candidate)) {
      return true;
    }
  }
  return false;
}

void P2PTransportChannel::RememberRemoteCandidate(
    const Candidate& remote_candidate,
    PortInterface* origin_port) {
  RTC_DCHECK_RUN_ON(network_thread_);

  size_t i = 0;
  while (i < remote_candidates_.size()) {
    if (remote_candidates_[i].generation() < remote_candidate.generation()) {
      RTC_LOG(LS_INFO) << "Pruning candidate from old generation: "
                       << remote_candidates_[i].address().ToSensitiveString();
      remote_candidates_.erase(remote_candidates_.begin() + i);
    } else {
      i += 1;
    }
  }

  if (IsDuplicateRemoteCandidate(remote_candidate)) {
    RTC_LOG(LS_INFO) << "Duplicate candidate: "
                     << remote_candidate.ToSensitiveString();
    return;
  }

  remote_candidates_.push_back(RemoteCandidate(remote_candidate, origin_port));
}

int P2PTransportChannel::SetOption(rtc::Socket::Option opt, int value) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (ice_field_trials_.override_dscp && opt == rtc::Socket::OPT_DSCP) {
    value = *ice_field_trials_.override_dscp;
  }

  OptionMap::iterator it = options_.find(opt);
  if (it == options_.end()) {
    options_.insert(std::make_pair(opt, value));
  } else if (it->second == value) {
    return 0;
  } else {
    it->second = value;
  }

  for (PortInterface* port : ports_) {
    int val = port->SetOption(opt, value);
    if (val < 0) {

      RTC_LOG(LS_WARNING) << "SetOption(" << opt << ", " << value
                          << ") failed: " << port->GetError();
    }
  }
  return 0;
}

bool P2PTransportChannel::GetOption(rtc::Socket::Option opt, int* value) {
  RTC_DCHECK_RUN_ON(network_thread_);

  const auto& found = options_.find(opt);
  if (found == options_.end()) {
    return false;
  }
  *value = found->second;
  return true;
}

int P2PTransportChannel::GetError() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return error_;
}

int P2PTransportChannel::SendPacket(const char* data,
                                    size_t len,
                                    const rtc::PacketOptions& options,
                                    int flags) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (flags != 0) {
    error_ = EINVAL;
    return -1;
  }

  if (!ReadyToSend(selected_connection_)) {
    error_ = ENOTCONN;
    return -1;
  }

  packets_sent_++;
  last_sent_packet_id_ = options.packet_id;
  rtc::PacketOptions modified_options(options);
  modified_options.info_signaled_after_sent.packet_type =
      rtc::PacketType::kData;
  int sent = selected_connection_->Send(data, len, modified_options);
  if (sent <= 0) {
    RTC_DCHECK(sent < 0);
    error_ = selected_connection_->GetError();
    return sent;
  }

  bytes_sent_ += sent;
  return sent;
}

bool P2PTransportChannel::GetStats(IceTransportStats* ice_transport_stats) {
  RTC_DCHECK_RUN_ON(network_thread_);

  ice_transport_stats->candidate_stats_list.clear();
  ice_transport_stats->connection_infos.clear();

  if (!allocator_sessions_.empty()) {
    allocator_session()->GetCandidateStatsFromReadyPorts(
        &ice_transport_stats->candidate_stats_list);
  }

  for (Connection* connection : connections_) {
    ConnectionInfo stats = connection->stats();
    stats.local_candidate = SanitizeLocalCandidate(stats.local_candidate);
    stats.remote_candidate = SanitizeRemoteCandidate(stats.remote_candidate);
    stats.best_connection = (selected_connection_ == connection);
    ice_transport_stats->connection_infos.push_back(std::move(stats));
  }

  ice_transport_stats->selected_candidate_pair_changes =
      selected_candidate_pair_changes_;

  ice_transport_stats->bytes_sent = bytes_sent_;
  ice_transport_stats->bytes_received = bytes_received_;
  ice_transport_stats->packets_sent = packets_sent_;
  ice_transport_stats->packets_received = packets_received_;

  ice_transport_stats->ice_role = GetIceRole();
  ice_transport_stats->ice_local_username_fragment = ice_parameters_.ufrag;
  ice_transport_stats->ice_state = ComputeIceTransportState();

  return true;
}

absl::optional<rtc::NetworkRoute> P2PTransportChannel::network_route() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return network_route_;
}

rtc::DiffServCodePoint P2PTransportChannel::DefaultDscpValue() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  OptionMap::const_iterator it = options_.find(rtc::Socket::OPT_DSCP);
  if (it == options_.end()) {
    return rtc::DSCP_NO_CHANGE;
  }
  return static_cast<rtc::DiffServCodePoint>(it->second);
}

rtc::ArrayView<Connection* const> P2PTransportChannel::connections() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return rtc::ArrayView<Connection* const>(connections_.data(),
                                           connections_.size());
}

void P2PTransportChannel::RemoveConnectionForTest(Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(FindConnection(connection));
  connection->SignalDestroyed.disconnect(this);
  RemoveConnection(connection);
  RTC_DCHECK(!FindConnection(connection));
  if (selected_connection_ == connection)
    selected_connection_ = nullptr;
  connection->Destroy();
}

void P2PTransportChannel::UpdateConnectionStates() {
  RTC_DCHECK_RUN_ON(network_thread_);
  int64_t now = rtc::TimeMillis();

  std::vector<Connection*> copy(connections_.begin(), connections_.end());
  for (Connection* c : copy) {
    c->UpdateState(now);
  }
}

void P2PTransportChannel::OnStartedPinging() {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << ToString()
                   << ": Have a pingable connection for the first time; "
                      "starting to ping.";
  regathering_controller_->Start();
}

bool P2PTransportChannel::IsPortPruned(const PortInterface* port) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return !absl::c_linear_search(ports_, port);
}

bool P2PTransportChannel::IsRemoteCandidatePruned(const Candidate& cand) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return !absl::c_linear_search(remote_candidates_, cand);
}

bool P2PTransportChannel::PresumedWritable(const Connection* conn) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return (conn->write_state() == Connection::STATE_WRITE_INIT &&
          config_.presume_writable_when_fully_relayed &&
          conn->local_candidate().is_relay() &&
          (conn->remote_candidate().is_relay() ||
           conn->remote_candidate().is_prflx()));
}

void P2PTransportChannel::UpdateState() {
  RTC_DCHECK_RUN_ON(network_thread_);

  bool all_connections_timedout = true;
  for (const Connection* conn : connections_) {
    if (conn->write_state() != Connection::STATE_WRITE_TIMEOUT) {
      all_connections_timedout = false;
      break;
    }
  }

  if (all_connections_timedout) {
    HandleAllTimedOut();
  }

  UpdateTransportState();
}

bool P2PTransportChannel::AllowedToPruneConnections() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return ice_role_ == ICEROLE_CONTROLLING ||
         (selected_connection_ && selected_connection_->nominated());
}

bool P2PTransportChannel::PruneConnections(
    rtc::ArrayView<const Connection* const> connections) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!AllowedToPruneConnections()) {
    RTC_LOG(LS_WARNING) << "Not allowed to prune connections";
    return false;
  }
  for (const Connection* conn : connections) {
    FromIceController(conn)->Prune();
  }
  return true;
}

rtc::NetworkRoute P2PTransportChannel::ConfigureNetworkRoute(
    const Connection* conn) {
  RTC_DCHECK_RUN_ON(network_thread_);
  return {.connected = ReadyToSend(conn),
          .local = CreateRouteEndpointFromCandidate(
                           true, conn->local_candidate(),

              conn->port()->Type() == RELAY_PORT_TYPE),
          .remote = CreateRouteEndpointFromCandidate(
                           false, conn->remote_candidate(),
                               conn->remote_candidate().is_relay()),
          .last_sent_packet_id = last_sent_packet_id_,
          .packet_overhead =
              conn->local_candidate().address().ipaddr().overhead() +
              GetProtocolOverhead(conn->local_candidate().protocol())};
}

void P2PTransportChannel::SwitchSelectedConnection(
    const Connection* new_connection,
    IceSwitchReason reason) {
  RTC_DCHECK_RUN_ON(network_thread_);
  SwitchSelectedConnectionInternal(FromIceController(new_connection), reason);
}

void P2PTransportChannel::SwitchSelectedConnectionInternal(
    Connection* conn,
    IceSwitchReason reason) {
  RTC_DCHECK_RUN_ON(network_thread_);

  Connection* old_selected_connection = selected_connection_;
  selected_connection_ = conn;
  LogCandidatePairConfig(conn, webrtc::IceCandidatePairConfigType::kSelected);
  network_route_.reset();
  if (old_selected_connection) {
    old_selected_connection->set_selected(false);
  }
  if (selected_connection_) {
    ++nomination_;
    selected_connection_->set_selected(true);
    if (old_selected_connection) {
      RTC_LOG(LS_INFO) << ToString() << ": Previous selected connection: "
                       << old_selected_connection->ToString();
    }
    RTC_LOG(LS_INFO) << ToString() << ": New selected connection: "
                     << selected_connection_->ToString();
    SignalRouteChange(this, selected_connection_->remote_candidate());

    if (selected_connection_->writable() ||
        PresumedWritable(selected_connection_)) {
      SignalReadyToSend(this);
    }

    network_route_.emplace(ConfigureNetworkRoute(selected_connection_));
  } else {
    RTC_LOG(LS_INFO) << ToString() << ": No selected connection";
  }

  if (conn != nullptr && ice_role_ == ICEROLE_CONTROLLING &&
      ((ice_field_trials_.send_ping_on_switch_ice_controlling &&
        old_selected_connection != nullptr) ||
       ice_field_trials_.send_ping_on_selected_ice_controlling)) {
    SendPingRequestInternal(conn);
  }

  SignalNetworkRouteChanged(network_route_);

  if (selected_connection_) {
    CandidatePairChangeEvent pair_change;
    pair_change.reason = IceSwitchReasonToString(reason);
    pair_change.selected_candidate_pair = *GetSelectedCandidatePair();
    pair_change.last_data_received_ms =
        selected_connection_->last_data_received();

    if (old_selected_connection) {
      pair_change.estimated_disconnected_time_ms =
          ComputeEstimatedDisconnectedTimeMs(rtc::TimeMillis(),
                                             old_selected_connection);
    } else {
      pair_change.estimated_disconnected_time_ms = 0;
    }
    if (candidate_pair_change_callback_) {
      candidate_pair_change_callback_(pair_change);
    }
  }

  ++selected_candidate_pair_changes_;

  ice_controller_->OnConnectionSwitched(selected_connection_);
}

int64_t P2PTransportChannel::ComputeEstimatedDisconnectedTimeMs(
    int64_t now_ms,
    Connection* old_connection) {

  int64_t last_data_or_old_ping =
      std::max(old_connection->last_received(), last_data_received_ms_);
  return (now_ms - last_data_or_old_ping);
}

void P2PTransportChannel::UpdateTransportState() {
  RTC_DCHECK_RUN_ON(network_thread_);

  bool writable =
      selected_connection_ && (selected_connection_->writable() ||
                               PresumedWritable(selected_connection_));
  SetWritable(writable);

  bool receiving = false;
  for (const Connection* connection : connections_) {
    if (connection->receiving()) {
      receiving = true;
      break;
    }
  }
  SetReceiving(receiving);

  IceTransportState state = ComputeState();
  webrtc::IceTransportState current_standardized_state =
      ComputeIceTransportState();

  if (state_ != state) {
    RTC_LOG(LS_INFO) << ToString() << ": Transport channel state changed from "
                     << static_cast<int>(state_) << " to "
                     << static_cast<int>(state);

    switch (state_) {
      case IceTransportState::STATE_INIT:

        RTC_DCHECK(state == IceTransportState::STATE_CONNECTING ||
                   state == IceTransportState::STATE_COMPLETED ||
                   state == IceTransportState::STATE_FAILED);
        break;
      case IceTransportState::STATE_CONNECTING:
        RTC_DCHECK(state == IceTransportState::STATE_COMPLETED ||
                   state == IceTransportState::STATE_FAILED);
        break;
      case IceTransportState::STATE_COMPLETED:

        RTC_DCHECK(state == IceTransportState::STATE_CONNECTING ||
                   state == IceTransportState::STATE_FAILED);
        break;
      case IceTransportState::STATE_FAILED:

        RTC_DCHECK(state == IceTransportState::STATE_CONNECTING ||
                   state == IceTransportState::STATE_COMPLETED);
        break;
      default:
        RTC_DCHECK_NOTREACHED();
        break;
    }
    state_ = state;
    SignalStateChanged(this);
  }

  if (standardized_state_ != current_standardized_state) {
    standardized_state_ = current_standardized_state;
    SignalIceTransportStateChanged(this);
  }
}

void P2PTransportChannel::MaybeStopPortAllocatorSessions() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!IsGettingPorts()) {
    return;
  }

  for (const auto& session : allocator_sessions_) {
    if (session->IsStopped()) {
      continue;
    }

    if (config_.gather_continually() && session == allocator_sessions_.back()) {
      session->ClearGettingPorts();
    } else {
      session->StopGettingPorts();
    }
  }
}

void P2PTransportChannel::OnSelectedConnectionDestroyed() {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << "Selected connection destroyed. Will choose a new one.";
  IceSwitchReason reason = IceSwitchReason::SELECTED_CONNECTION_DESTROYED;
  SwitchSelectedConnectionInternal(nullptr, reason);
  ice_controller_->OnSortAndSwitchRequest(reason);
}

void P2PTransportChannel::HandleAllTimedOut() {
  RTC_DCHECK_RUN_ON(network_thread_);
  bool update_selected_connection = false;
  std::vector<Connection*> copy(connections_.begin(), connections_.end());
  for (Connection* connection : copy) {
    if (selected_connection_ == connection) {
      selected_connection_ = nullptr;
      update_selected_connection = true;
    }
    connection->SignalDestroyed.disconnect(this);
    RemoveConnection(connection);
    connection->Destroy();
  }

  if (update_selected_connection)
    OnSelectedConnectionDestroyed();
}

bool P2PTransportChannel::ReadyToSend(const Connection* connection) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  return connection != nullptr &&
         (connection->writable() ||
          connection->write_state() == Connection::STATE_WRITE_UNRELIABLE ||
          PresumedWritable(connection));
}

Connection* P2PTransportChannel::FindNextPingableConnection() {
  RTC_DCHECK_RUN_ON(network_thread_);
  const Connection* conn = ice_controller_->FindNextPingableConnection();
  if (conn) {
    return FromIceController(conn);
  } else {
    return nullptr;
  }
}

int64_t P2PTransportChannel::GetLastPingSentMs() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return last_ping_sent_ms_;
}

void P2PTransportChannel::SendPingRequest(const Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  SendPingRequestInternal(FromIceController(connection));
}

void P2PTransportChannel::SendPingRequestInternal(Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  PingConnection(connection);
  MarkConnectionPinged(connection);
}

void P2PTransportChannel::MarkConnectionPinged(Connection* conn) {
  RTC_DCHECK_RUN_ON(network_thread_);
  ice_controller_->OnConnectionPinged(conn);
}

void P2PTransportChannel::PingConnection(Connection* conn) {
  RTC_DCHECK_RUN_ON(network_thread_);
  bool use_candidate_attr = false;
  uint32_t nomination = 0;
  if (ice_role_ == ICEROLE_CONTROLLING) {
    bool renomination_supported = ice_parameters_.renomination &&
                                  !remote_ice_parameters_.empty() &&
                                  remote_ice_parameters_.back().renomination;
    if (renomination_supported) {
      nomination = GetNominationAttr(conn);
    } else {
      use_candidate_attr = GetUseCandidateAttr(conn);
    }
  }
  conn->set_nomination(nomination);
  conn->set_use_candidate_attr(use_candidate_attr);
  last_ping_sent_ms_ = rtc::TimeMillis();
  conn->Ping(last_ping_sent_ms_, stun_dict_writer_.CreateDelta());
}

uint32_t P2PTransportChannel::GetNominationAttr(Connection* conn) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return (conn == selected_connection_) ? nomination_ : 0;
}

bool P2PTransportChannel::GetUseCandidateAttr(Connection* conn) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return ice_controller_->GetUseCandidateAttribute(
      conn, config_.default_nomination_mode, remote_ice_mode_);
}

void P2PTransportChannel::OnConnectionStateChange(Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (ice_field_trials_.stop_gather_on_strongly_connected) {
    bool strongly_connected = !connection->weak();
    bool latest_generation = connection->local_candidate().generation() >=
                             allocator_session()->generation();
    if (strongly_connected && latest_generation) {
      MaybeStopPortAllocatorSessions();
    }
  }

  ice_controller_->OnSortAndSwitchRequest(
      IceSwitchReason::CONNECT_STATE_CHANGE);

}

void P2PTransportChannel::OnConnectionDestroyed(Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);

  RemoveConnection(connection);

  RTC_LOG(LS_INFO) << ToString() << ": Removed connection " << connection
                   << " (" << connections_.size() << " remaining)";

  if (selected_connection_ == connection) {
    OnSelectedConnectionDestroyed();
  } else {

    UpdateTransportState();
  }
}

void P2PTransportChannel::RemoveConnection(Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto it = absl::c_find(connections_, connection);
  RTC_DCHECK(it != connections_.end());
  connection->DeregisterReceivedPacketCallback();
  connections_.erase(it);
  connection->ClearStunDictConsumer();
  ice_controller_->OnConnectionDestroyed(connection);
}

void P2PTransportChannel::OnPortDestroyed(PortInterface* port) {
  RTC_DCHECK_RUN_ON(network_thread_);

  ports_.erase(std::remove(ports_.begin(), ports_.end(), port), ports_.end());
  pruned_ports_.erase(
      std::remove(pruned_ports_.begin(), pruned_ports_.end(), port),
      pruned_ports_.end());
  RTC_LOG(LS_INFO) << "Removed port because it is destroyed: " << ports_.size()
                   << " remaining";
}

void P2PTransportChannel::OnPortsPruned(
    PortAllocatorSession* session,
    const std::vector<PortInterface*>& ports) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (PortInterface* port : ports) {
    if (PrunePort(port)) {
      RTC_LOG(LS_INFO) << "Removed port: " << port->ToString() << " "
                       << ports_.size() << " remaining";
    }
  }
}

void P2PTransportChannel::OnCandidatesRemoved(
    PortAllocatorSession* session,
    const std::vector<Candidate>& candidates) {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (!config_.gather_continually() || session != allocator_session()) {
    return;
  }

  std::vector<Candidate> candidates_to_remove;
  for (Candidate candidate : candidates) {
    candidate.set_transport_name(transport_name());
    candidates_to_remove.push_back(candidate);
  }
  if (candidates_removed_callback_) {
    candidates_removed_callback_(this, candidates_to_remove);
  }
}

void P2PTransportChannel::PruneAllPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  pruned_ports_.insert(pruned_ports_.end(), ports_.begin(), ports_.end());
  ports_.clear();
}

bool P2PTransportChannel::PrunePort(PortInterface* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto it = absl::c_find(ports_, port);

  if (it == ports_.end()) {
    return false;
  }
  ports_.erase(it);
  pruned_ports_.push_back(port);
  return true;
}

void P2PTransportChannel::OnReadPacket(Connection* connection,
                                       const rtc::ReceivedPacket& packet) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (connection != selected_connection_ && !FindConnection(connection)) {

    RTC_DCHECK_NOTREACHED();
    return;
  }

    packets_received_++;
    bytes_received_ += packet.payload().size();
    RTC_DCHECK(connection->last_data_received() >= last_data_received_ms_);
    last_data_received_ms_ =
        std::max(last_data_received_ms_, connection->last_data_received());

    SignalReadPacket(
        this, reinterpret_cast<const char*>(packet.payload().data()),
        packet.payload().size(),
        packet.arrival_time() ? packet.arrival_time()->us() : -1, 0);

  if (ice_role_ == ICEROLE_CONTROLLED && connection != selected_connection_) {
    ice_controller_->OnImmediateSwitchRequest(IceSwitchReason::DATA_RECEIVED,
                                              connection);
  }
}

void P2PTransportChannel::OnSentPacket(const rtc::SentPacket& sent_packet) {
  RTC_DCHECK_RUN_ON(network_thread_);

  SignalSentPacket(this, sent_packet);
}

void P2PTransportChannel::OnReadyToSend(Connection* connection) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (connection == selected_connection_ && writable()) {
    SignalReadyToSend(this);
  }
}

void P2PTransportChannel::SetWritable(bool writable) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (writable_ == writable) {
    return;
  }
  RTC_LOG(LS_VERBOSE) << ToString() << ": Changed writable_ to " << writable;
  writable_ = writable;
  if (writable_) {
    has_been_writable_ = true;
    SignalReadyToSend(this);
  }
  SignalWritableState(this);
}

void P2PTransportChannel::SetReceiving(bool receiving) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (receiving_ == receiving) {
    return;
  }
  receiving_ = receiving;
  SignalReceivingState(this);
}

Candidate P2PTransportChannel::SanitizeLocalCandidate(
    const Candidate& c) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  return allocator_->SanitizeCandidate(c);
}

Candidate P2PTransportChannel::SanitizeRemoteCandidate(
    const Candidate& c) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  bool use_hostname_address = absl::EndsWith(c.address().hostname(), LOCAL_TLD);

  use_hostname_address |= c.is_prflx();
  return c.ToSanitizedCopy(use_hostname_address,
                           false                             );
}

void P2PTransportChannel::LogCandidatePairConfig(
    Connection* conn,
    webrtc::IceCandidatePairConfigType type) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (conn == nullptr) {
    return;
  }
  ice_event_log_.LogCandidatePairConfig(type, conn->id(),
                                        conn->ToLogDescription());
}

std::unique_ptr<StunAttribute> P2PTransportChannel::GoogDeltaReceived(
    const StunByteStringAttribute* delta) {
  auto error = stun_dict_view_.ApplyDelta(*delta);
  if (error.ok()) {
    auto& result = error.value();
    RTC_LOG(LS_INFO) << "Applied GOOG_DELTA";
    dictionary_view_updated_callback_list_.Send(this, stun_dict_view_,
                                                result.second);
    return std::move(result.first);
  } else {
    RTC_LOG(LS_ERROR) << "Failed to apply GOOG_DELTA: "
                      << error.error().message();
  }
  return nullptr;
}

void P2PTransportChannel::GoogDeltaAckReceived(
    webrtc::RTCErrorOr<const StunUInt64Attribute*> error_or_ack) {
  if (error_or_ack.ok()) {
    RTC_LOG(LS_ERROR) << "Applied GOOG_DELTA_ACK";
    auto ack = error_or_ack.value();
    stun_dict_writer_.ApplyDeltaAck(*ack);
    dictionary_writer_synced_callback_list_.Send(this, stun_dict_writer_);
  } else {
    stun_dict_writer_.Disable();
    RTC_LOG(LS_ERROR) << "Failed GOOG_DELTA_ACK: "
                      << error_or_ack.error().message();
  }
}

}
