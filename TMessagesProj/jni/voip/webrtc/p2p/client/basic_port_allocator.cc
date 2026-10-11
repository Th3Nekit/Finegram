/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/client/basic_port_allocator.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/transport/field_trial_based_config.h"
#include "api/units/time_delta.h"
#include "p2p/base/basic_packet_socket_factory.h"
#include "p2p/base/port.h"
#include "p2p/base/stun_port.h"
#include "p2p/base/tcp_port.h"
#include "p2p/base/turn_port.h"
#include "p2p/base/udp_port.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/metrics.h"

namespace cricket {
namespace {
using ::rtc::CreateRandomId;
using ::webrtc::SafeTask;
using ::webrtc::TimeDelta;

const int PHASE_UDP = 0;
const int PHASE_RELAY = 1;
const int PHASE_TCP = 2;

const int kNumPhases = 3;

int GetProtocolPriority(cricket::ProtocolType protocol) {
  switch (protocol) {
    case cricket::PROTO_UDP:
      return 2;
    case cricket::PROTO_TCP:
      return 1;
    case cricket::PROTO_SSLTCP:
    case cricket::PROTO_TLS:
      return 0;
    default:
      RTC_DCHECK_NOTREACHED();
      return 0;
  }
}

int GetAddressFamilyPriority(int ip_family) {
  switch (ip_family) {
    case AF_INET6:
      return 2;
    case AF_INET:
      return 1;
    default:
      RTC_DCHECK_NOTREACHED();
      return 0;
  }
}

int ComparePort(const cricket::Port* a, const cricket::Port* b) {
  int a_protocol = GetProtocolPriority(a->GetProtocol());
  int b_protocol = GetProtocolPriority(b->GetProtocol());
  int cmp_protocol = a_protocol - b_protocol;
  if (cmp_protocol != 0) {
    return cmp_protocol;
  }

  int a_family = GetAddressFamilyPriority(a->Network()->GetBestIP().family());
  int b_family = GetAddressFamilyPriority(b->Network()->GetBestIP().family());
  return a_family - b_family;
}

struct NetworkFilter {
  using Predicate = std::function<bool(const rtc::Network*)>;
  NetworkFilter(Predicate pred, absl::string_view description)
      : predRemain(
            [pred](const rtc::Network* network) { return !pred(network); }),
        description(description) {}
  Predicate predRemain;
  const std::string description;
};

void FilterNetworks(std::vector<const rtc::Network*>* networks,
                    NetworkFilter filter) {
  auto start_to_remove =
      std::partition(networks->begin(), networks->end(), filter.predRemain);
  if (start_to_remove == networks->end()) {
    return;
  }
  RTC_LOG(LS_INFO) << "Filtered out " << filter.description << " networks:";
  for (auto it = start_to_remove; it != networks->end(); ++it) {
    RTC_LOG(LS_INFO) << (*it)->ToString();
  }
  networks->erase(start_to_remove, networks->end());
}

bool IsAllowedByCandidateFilter(const Candidate& c, uint32_t filter) {

  if (c.address().IsAnyIP()) {
    return false;
  }

  if (c.is_relay()) {
    return ((filter & CF_RELAY) != 0);
  }

  if (c.is_stun()) {
    return ((filter & CF_REFLEXIVE) != 0);
  }

  if (c.is_local()) {
    if ((filter & CF_REFLEXIVE) && !c.address().IsPrivateIP()) {

      return true;
    }

    return ((filter & CF_HOST) != 0);
  }

  return false;
}

std::string NetworksToString(const std::vector<const rtc::Network*>& networks) {
  rtc::StringBuilder ost;
  for (auto n : networks) {
    ost << n->name() << " ";
  }
  return ost.Release();
}

}

const uint32_t DISABLE_ALL_PHASES =
    PORTALLOCATOR_DISABLE_UDP | PORTALLOCATOR_DISABLE_TCP |
    PORTALLOCATOR_DISABLE_STUN | PORTALLOCATOR_DISABLE_RELAY;

BasicPortAllocator::BasicPortAllocator(
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    webrtc::TurnCustomizer* customizer,
    RelayPortFactoryInterface* relay_port_factory,
    const webrtc::FieldTrialsView* field_trials)
    : field_trials_(field_trials),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      default_relay_port_factory_(relay_port_factory ? nullptr
                                                     : new TurnPortFactory()),
      relay_port_factory_(relay_port_factory
                              ? relay_port_factory
                              : default_relay_port_factory_.get()) {
  RTC_CHECK(socket_factory_);
  RTC_DCHECK(relay_port_factory_);
  RTC_DCHECK(network_manager_);
  SetConfiguration(ServerAddresses(), std::vector<RelayServerConfig>(), 0,
                   webrtc::NO_PRUNE, customizer);
}

BasicPortAllocator::BasicPortAllocator(
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    const ServerAddresses& stun_servers,
    const webrtc::FieldTrialsView* field_trials)
    : field_trials_(field_trials),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      default_relay_port_factory_(new TurnPortFactory()),
      relay_port_factory_(default_relay_port_factory_.get()) {
  RTC_CHECK(socket_factory_);
  RTC_DCHECK(relay_port_factory_);
  RTC_DCHECK(network_manager_);
  SetConfiguration(stun_servers, std::vector<RelayServerConfig>(), 0,
                   webrtc::NO_PRUNE, nullptr);
}

BasicPortAllocator::~BasicPortAllocator() {
  CheckRunOnValidThreadIfInitialized();

  DiscardCandidatePool();
}

void BasicPortAllocator::SetNetworkIgnoreMask(int network_ignore_mask) {

  CheckRunOnValidThreadIfInitialized();
  network_ignore_mask_ = network_ignore_mask;
}

int BasicPortAllocator::GetNetworkIgnoreMask() const {
  CheckRunOnValidThreadIfInitialized();
  int mask = network_ignore_mask_;
  switch (vpn_preference_) {
    case webrtc::VpnPreference::kOnlyUseVpn:
      mask |= ~static_cast<int>(rtc::ADAPTER_TYPE_VPN);
      break;
    case webrtc::VpnPreference::kNeverUseVpn:
      mask |= static_cast<int>(rtc::ADAPTER_TYPE_VPN);
      break;
    default:
      break;
  }
  return mask;
}

PortAllocatorSession* BasicPortAllocator::CreateSessionInternal(
    absl::string_view content_name,
    int component,
    absl::string_view ice_ufrag,
    absl::string_view ice_pwd) {
  CheckRunOnValidThreadAndInitialized();
  return new BasicPortAllocatorSession(this, std::string(content_name),
                                       component, std::string(ice_ufrag),
                                       std::string(ice_pwd));
}

void BasicPortAllocator::AddTurnServerForTesting(
    const RelayServerConfig& turn_server) {
  CheckRunOnValidThreadAndInitialized();
  std::vector<RelayServerConfig> new_turn_servers = turn_servers();
  new_turn_servers.push_back(turn_server);
  SetConfiguration(stun_servers(), new_turn_servers, candidate_pool_size(),
                   turn_port_prune_policy(), turn_customizer());
}

BasicPortAllocatorSession::BasicPortAllocatorSession(
    BasicPortAllocator* allocator,
    absl::string_view content_name,
    int component,
    absl::string_view ice_ufrag,
    absl::string_view ice_pwd)
    : PortAllocatorSession(content_name,
                           component,
                           ice_ufrag,
                           ice_pwd,
                           allocator->flags()),
      allocator_(allocator),
      network_thread_(rtc::Thread::Current()),
      socket_factory_(allocator->socket_factory()),
      allocation_started_(false),
      network_manager_started_(false),
      allocation_sequences_created_(false),
      turn_port_prune_policy_(allocator->turn_port_prune_policy()) {
  TRACE_EVENT0("webrtc",
               "BasicPortAllocatorSession::BasicPortAllocatorSession");
  allocator_->network_manager()->SignalNetworksChanged.connect(
      this, &BasicPortAllocatorSession::OnNetworksChanged);
  allocator_->network_manager()->StartUpdating();
}

BasicPortAllocatorSession::~BasicPortAllocatorSession() {
  TRACE_EVENT0("webrtc",
               "BasicPortAllocatorSession::~BasicPortAllocatorSession");
  RTC_DCHECK_RUN_ON(network_thread_);
  allocator_->network_manager()->StopUpdating();

  for (uint32_t i = 0; i < sequences_.size(); ++i) {

    sequences_[i]->Clear();
  }

  std::vector<PortData>::iterator it;
  for (it = ports_.begin(); it != ports_.end(); it++)
    delete it->port();

  configs_.clear();

  for (uint32_t i = 0; i < sequences_.size(); ++i)
    delete sequences_[i];
}

BasicPortAllocator* BasicPortAllocatorSession::allocator() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return allocator_;
}

void BasicPortAllocatorSession::SetCandidateFilter(uint32_t filter) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (filter == candidate_filter_) {
    return;
  }
  uint32_t prev_filter = candidate_filter_;
  candidate_filter_ = filter;
  for (PortData& port_data : ports_) {
    if (port_data.error() || port_data.pruned()) {
      continue;
    }
    PortData::State cur_state = port_data.state();
    bool found_signalable_candidate = false;
    bool found_pairable_candidate = false;
    cricket::Port* port = port_data.port();
    for (const auto& c : port->Candidates()) {
      if (!IsStopped() && !IsAllowedByCandidateFilter(c, prev_filter) &&
          IsAllowedByCandidateFilter(c, filter)) {

        if (!found_signalable_candidate) {
          found_signalable_candidate = true;
          port_data.set_state(PortData::STATE_INPROGRESS);
        }
        port->SignalCandidateReady(port, c);
      }

      if (CandidatePairable(c, port)) {
        found_pairable_candidate = true;
      }
    }

    port_data.set_state(cur_state);

    if (!found_pairable_candidate) {
      port_data.set_has_pairable_candidate(false);
    }
  }
}

void BasicPortAllocatorSession::StartGettingPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  state_ = SessionState::GATHERING;

  network_thread_->PostTask(
      SafeTask(network_safety_.flag(), [this] { GetPortConfigurations(); }));

  RTC_LOG(LS_INFO) << "Start getting ports with turn_port_prune_policy "
                   << turn_port_prune_policy_;
}

void BasicPortAllocatorSession::StopGettingPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  ClearGettingPorts();

  state_ = SessionState::STOPPED;
}

void BasicPortAllocatorSession::ClearGettingPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  ++allocation_epoch_;
  for (uint32_t i = 0; i < sequences_.size(); ++i) {
    sequences_[i]->Stop();
  }
  network_thread_->PostTask(
      SafeTask(network_safety_.flag(), [this] { OnConfigStop(); }));
  state_ = SessionState::CLEARED;
}

bool BasicPortAllocatorSession::IsGettingPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  return state_ == SessionState::GATHERING;
}

bool BasicPortAllocatorSession::IsCleared() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return state_ == SessionState::CLEARED;
}

bool BasicPortAllocatorSession::IsStopped() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  return state_ == SessionState::STOPPED;
}

std::vector<const rtc::Network*>
BasicPortAllocatorSession::GetFailedNetworks() {
  RTC_DCHECK_RUN_ON(network_thread_);

  std::vector<const rtc::Network*> networks = GetNetworks();

  std::set<std::string> networks_with_connection;
  for (const PortData& data : ports_) {
    Port* port = data.port();
    if (!port->connections().empty()) {
      networks_with_connection.insert(port->Network()->name());
    }
  }

  networks.erase(
      std::remove_if(networks.begin(), networks.end(),
                     [networks_with_connection](const rtc::Network* network) {

                       return networks_with_connection.find(network->name()) !=
                              networks_with_connection.end();
                     }),
      networks.end());
  return networks;
}

void BasicPortAllocatorSession::RegatherOnFailedNetworks() {
  RTC_DCHECK_RUN_ON(network_thread_);

  std::vector<const rtc::Network*> failed_networks = GetFailedNetworks();
  if (failed_networks.empty()) {
    return;
  }

  RTC_LOG(LS_INFO) << "Regather candidates on failed networks";

  for (AllocationSequence* sequence : sequences_) {
    if (!sequence->network_failed() &&
        absl::c_linear_search(failed_networks, sequence->network())) {
      sequence->set_network_failed();
    }
  }

  bool disable_equivalent_phases = true;
  Regather(failed_networks, disable_equivalent_phases,
           IceRegatheringReason::NETWORK_FAILURE);
}

void BasicPortAllocatorSession::Regather(
    const std::vector<const rtc::Network*>& networks,
    bool disable_equivalent_phases,
    IceRegatheringReason reason) {
  RTC_DCHECK_RUN_ON(network_thread_);

  std::vector<PortData*> ports_to_prune = GetUnprunedPorts(networks);
  if (!ports_to_prune.empty()) {
    RTC_LOG(LS_INFO) << "Prune " << ports_to_prune.size() << " ports";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }

  if (allocation_started_ && network_manager_started_ && !IsStopped()) {
    SignalIceRegathering(this, reason);

    DoAllocate(disable_equivalent_phases);
  }
}

void BasicPortAllocatorSession::GetCandidateStatsFromReadyPorts(
    CandidateStatsList* candidate_stats_list) const {
  auto ports = ReadyPorts();
  for (auto* port : ports) {
    auto candidates = port->Candidates();
    for (const auto& candidate : candidates) {
      absl::optional<StunStats> stun_stats;
      port->GetStunStats(&stun_stats);
      CandidateStats candidate_stats(allocator_->SanitizeCandidate(candidate),
                                     std::move(stun_stats));
      candidate_stats_list->push_back(std::move(candidate_stats));
    }
  }
}

void BasicPortAllocatorSession::SetStunKeepaliveIntervalForReadyPorts(
    const absl::optional<int>& stun_keepalive_interval) {
  RTC_DCHECK_RUN_ON(network_thread_);
  auto ports = ReadyPorts();
  for (PortInterface* port : ports) {

    if (port->Type() == STUN_PORT_TYPE ||
        (port->Type() == LOCAL_PORT_TYPE && port->GetProtocol() == PROTO_UDP)) {
      static_cast<UDPPort*>(port)->set_stun_keepalive_delay(
          stun_keepalive_interval);
    }
  }
}

std::vector<PortInterface*> BasicPortAllocatorSession::ReadyPorts() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<PortInterface*> ret;
  for (const PortData& data : ports_) {
    if (data.ready()) {
      ret.push_back(data.port());
    }
  }
  return ret;
}

std::vector<Candidate> BasicPortAllocatorSession::ReadyCandidates() const {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<Candidate> candidates;
  for (const PortData& data : ports_) {
    if (!data.ready()) {
      continue;
    }
    GetCandidatesFromPort(data, &candidates);
  }
  return candidates;
}

void BasicPortAllocatorSession::GetCandidatesFromPort(
    const PortData& data,
    std::vector<Candidate>* candidates) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_CHECK(candidates != nullptr);
  for (const Candidate& candidate : data.port()->Candidates()) {
    if (!CheckCandidateFilter(candidate)) {
      continue;
    }
    candidates->push_back(allocator_->SanitizeCandidate(candidate));
  }
}

bool BasicPortAllocator::MdnsObfuscationEnabled() const {
  return network_manager()->GetMdnsResponder() != nullptr;
}

bool BasicPortAllocatorSession::CandidatesAllocationDone() const {
  RTC_DCHECK_RUN_ON(network_thread_);

  if (!allocation_sequences_created_) {
    return false;
  }

  if (absl::c_any_of(sequences_, [](const AllocationSequence* sequence) {
        return sequence->state() == AllocationSequence::kRunning;
      })) {
    return false;
  }

  return absl::c_none_of(
      ports_, [](const PortData& port) { return port.inprogress(); });
}

void BasicPortAllocatorSession::UpdateIceParametersInternal() {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (PortData& port : ports_) {
    port.port()->set_content_name(content_name());
    port.port()->SetIceParameters(component(), ice_ufrag(), ice_pwd());
  }
}

void BasicPortAllocatorSession::GetPortConfigurations() {
  RTC_DCHECK_RUN_ON(network_thread_);

  auto config = std::make_unique<PortConfiguration>(
      allocator_->stun_servers(), username(), password(),
      allocator()->field_trials());

  for (const RelayServerConfig& turn_server : allocator_->turn_servers()) {
    config->AddRelay(turn_server);
  }
  ConfigReady(std::move(config));
}

void BasicPortAllocatorSession::ConfigReady(PortConfiguration* config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  ConfigReady(absl::WrapUnique(config));
}

void BasicPortAllocatorSession::ConfigReady(
    std::unique_ptr<PortConfiguration> config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  network_thread_->PostTask(SafeTask(
      network_safety_.flag(), [this, config = std::move(config)]() mutable {
        OnConfigReady(std::move(config));
      }));
}

void BasicPortAllocatorSession::OnConfigReady(
    std::unique_ptr<PortConfiguration> config) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (config)
    configs_.push_back(std::move(config));

  AllocatePorts();
}

void BasicPortAllocatorSession::OnConfigStop() {
  RTC_DCHECK_RUN_ON(network_thread_);

  bool send_signal = false;
  for (std::vector<PortData>::iterator it = ports_.begin(); it != ports_.end();
       ++it) {
    if (it->inprogress()) {

      it->set_state(PortData::STATE_ERROR);
      send_signal = true;
    }
  }

  for (std::vector<AllocationSequence*>::iterator it = sequences_.begin();
       it != sequences_.end() && !send_signal; ++it) {
    if ((*it)->state() == AllocationSequence::kStopped) {
      send_signal = true;
    }
  }

  if (send_signal) {
    MaybeSignalCandidatesAllocationDone();
  }
}

void BasicPortAllocatorSession::AllocatePorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  network_thread_->PostTask(SafeTask(
      network_safety_.flag(), [this, allocation_epoch = allocation_epoch_] {
        OnAllocate(allocation_epoch);
      }));
}

void BasicPortAllocatorSession::OnAllocate(int allocation_epoch) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (allocation_epoch != allocation_epoch_)
    return;

  if (network_manager_started_ && !IsStopped()) {
    bool disable_equivalent_phases = true;
    DoAllocate(disable_equivalent_phases);
  }

  allocation_started_ = true;
}

std::vector<const rtc::Network*> BasicPortAllocatorSession::GetNetworks() {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<const rtc::Network*> networks;
  rtc::NetworkManager* network_manager = allocator_->network_manager();
  RTC_DCHECK(network_manager != nullptr);

  if (network_manager->enumeration_permission() ==
      rtc::NetworkManager::ENUMERATION_BLOCKED) {
    set_flags(flags() | PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION);
  }

  if (flags() & PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION) {
    networks = network_manager->GetAnyAddressNetworks();
  } else {
    networks = network_manager->GetNetworks();

    if (networks.empty() ||
        (flags() & PORTALLOCATOR_ENABLE_ANY_ADDRESS_PORTS)) {
      std::vector<const rtc::Network*> any_address_networks =
          network_manager->GetAnyAddressNetworks();
      networks.insert(networks.end(), any_address_networks.begin(),
                      any_address_networks.end());
    }
    RTC_LOG(LS_INFO) << "Count of networks: " << networks.size();
    for (const rtc::Network* network : networks) {
      RTC_LOG(LS_INFO) << network->ToString();
    }
  }

  if (flags() & PORTALLOCATOR_DISABLE_LINK_LOCAL_NETWORKS) {
    NetworkFilter link_local_filter(
        [](const rtc::Network* network) {
          return IPIsLinkLocal(network->prefix());
        },
        "link-local");
    FilterNetworks(&networks, link_local_filter);
  }

  NetworkFilter ignored_filter(
      [this](const rtc::Network* network) {
        return allocator_->GetNetworkIgnoreMask() & network->type();
      },
      "ignored");
  FilterNetworks(&networks, ignored_filter);
  if (flags() & PORTALLOCATOR_DISABLE_COSTLY_NETWORKS) {
    uint16_t lowest_cost = rtc::kNetworkCostMax;
    for (const rtc::Network* network : networks) {

      if (rtc::IPIsLinkLocal(network->GetBestIP())) {
        continue;
      }
      lowest_cost = std::min<uint16_t>(
          lowest_cost, network->GetCost(*allocator()->field_trials()));
    }
    NetworkFilter costly_filter(
        [lowest_cost, this](const rtc::Network* network) {
          return network->GetCost(*allocator()->field_trials()) >
                 lowest_cost + rtc::kNetworkCostLow;
        },
        "costly");
    FilterNetworks(&networks, costly_filter);
  }

  std::vector<const rtc::Network*> ipv6_networks;
  for (auto it = networks.begin(); it != networks.end();) {
    if ((*it)->prefix().family() == AF_INET6) {
      ipv6_networks.push_back(*it);
      it = networks.erase(it);
      continue;
    }
    ++it;
  }
  ipv6_networks =
      SelectIPv6Networks(ipv6_networks, allocator_->max_ipv6_networks());
  networks.insert(networks.end(), ipv6_networks.begin(), ipv6_networks.end());
  return networks;
}

std::vector<const rtc::Network*> BasicPortAllocatorSession::SelectIPv6Networks(
    std::vector<const rtc::Network*>& all_ipv6_networks,
    int max_ipv6_networks) {
  if (static_cast<int>(all_ipv6_networks.size()) <= max_ipv6_networks) {
    return all_ipv6_networks;
  }

  std::vector<rtc::AdapterType> adapter_types = {
      rtc::ADAPTER_TYPE_ETHERNET, rtc::ADAPTER_TYPE_LOOPBACK,
      rtc::ADAPTER_TYPE_WIFI,     rtc::ADAPTER_TYPE_CELLULAR,
      rtc::ADAPTER_TYPE_VPN,      rtc::ADAPTER_TYPE_UNKNOWN,
      rtc::ADAPTER_TYPE_ANY};
  int adapter_types_cnt = adapter_types.size();
  std::vector<const rtc::Network*> selected_networks;
  int adapter_types_pos = 0;

  while (static_cast<int>(selected_networks.size()) < max_ipv6_networks &&
         adapter_types_pos < adapter_types_cnt * max_ipv6_networks) {
    int network_pos = 0;
    while (network_pos < static_cast<int>(all_ipv6_networks.size())) {
      if (adapter_types[adapter_types_pos % adapter_types_cnt] ==
              all_ipv6_networks[network_pos]->type() ||
          (adapter_types[adapter_types_pos % adapter_types_cnt] ==
               rtc::ADAPTER_TYPE_CELLULAR &&
           all_ipv6_networks[network_pos]->IsCellular())) {
        selected_networks.push_back(all_ipv6_networks[network_pos]);
        all_ipv6_networks.erase(all_ipv6_networks.begin() + network_pos);
        break;
      }
      network_pos++;
    }
    adapter_types_pos++;
  }

  return selected_networks;
}

void BasicPortAllocatorSession::DoAllocate(bool disable_equivalent) {
  RTC_DCHECK_RUN_ON(network_thread_);
  bool done_signal_needed = false;
  std::vector<const rtc::Network*> networks = GetNetworks();
  if (networks.empty()) {
    RTC_LOG(LS_WARNING)
        << "Machine has no networks; no ports will be allocated";
    done_signal_needed = true;
  } else {
    RTC_LOG(LS_INFO) << "Allocate ports on " << NetworksToString(networks);
    PortConfiguration* config =
        configs_.empty() ? nullptr : configs_.back().get();
    for (uint32_t i = 0; i < networks.size(); ++i) {
      uint32_t sequence_flags = flags();
      if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {

        done_signal_needed = true;
        break;
      }

      if (!config || config->relays.empty()) {

        sequence_flags |= PORTALLOCATOR_DISABLE_RELAY;
      }

      if (!(sequence_flags & PORTALLOCATOR_ENABLE_IPV6) &&
          networks[i]->GetBestIP().family() == AF_INET6) {

        continue;
      }

      if (!(sequence_flags & PORTALLOCATOR_ENABLE_IPV6_ON_WIFI) &&
          networks[i]->GetBestIP().family() == AF_INET6 &&
          networks[i]->type() == rtc::ADAPTER_TYPE_WIFI) {

        continue;
      }

      if (disable_equivalent) {

        DisableEquivalentPhases(networks[i], config, &sequence_flags);

        if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {

          continue;
        }
      }

      AllocationSequence* sequence =
          new AllocationSequence(this, networks[i], config, sequence_flags,
                                 [this, safety_flag = network_safety_.flag()] {
                                   if (safety_flag->alive())
                                     OnPortAllocationComplete();
                                 });
      sequence->Init();
      sequence->Start();
      sequences_.push_back(sequence);
      done_signal_needed = true;
    }
  }
  if (done_signal_needed) {
    network_thread_->PostTask(SafeTask(network_safety_.flag(), [this] {
      OnAllocationSequenceObjectsCreated();
    }));
  }
}

void BasicPortAllocatorSession::OnNetworksChanged() {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<const rtc::Network*> networks = GetNetworks();
  std::vector<const rtc::Network*> failed_networks;
  for (AllocationSequence* sequence : sequences_) {

    if (!sequence->network_failed() &&
        !absl::c_linear_search(networks, sequence->network())) {
      sequence->OnNetworkFailed();
      failed_networks.push_back(sequence->network());
    }
  }
  std::vector<PortData*> ports_to_prune = GetUnprunedPorts(failed_networks);
  if (!ports_to_prune.empty()) {
    RTC_LOG(LS_INFO) << "Prune " << ports_to_prune.size()
                     << " ports because their networks were gone";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }

  if (allocation_started_ && !IsStopped()) {
    if (network_manager_started_) {

      SignalIceRegathering(this, IceRegatheringReason::NETWORK_CHANGE);
    }
    bool disable_equivalent_phases = true;
    DoAllocate(disable_equivalent_phases);
  }

  if (!network_manager_started_) {
    RTC_LOG(LS_INFO) << "Network manager has started";
    network_manager_started_ = true;
  }
}

void BasicPortAllocatorSession::DisableEquivalentPhases(
    const rtc::Network* network,
    PortConfiguration* config,
    uint32_t* flags) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (uint32_t i = 0; i < sequences_.size() &&
                       (*flags & DISABLE_ALL_PHASES) != DISABLE_ALL_PHASES;
       ++i) {
    sequences_[i]->DisableEquivalentPhases(network, config, flags);
  }
}

void BasicPortAllocatorSession::AddAllocatedPort(Port* port,
                                                 AllocationSequence* seq) {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (!port)
    return;

  RTC_LOG(LS_INFO) << "Adding allocated port for " << content_name();
  port->set_content_name(content_name());
  port->set_component(component());
  port->set_generation(generation());
  if (allocator_->proxy().type != rtc::PROXY_NONE)
    port->set_proxy(allocator_->user_agent(), allocator_->proxy());
  port->set_send_retransmit_count_attribute(
      (flags() & PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE) != 0);

  PortData data(port, seq);
  ports_.push_back(data);

  port->SignalCandidateReady.connect(
      this, &BasicPortAllocatorSession::OnCandidateReady);
  port->SignalCandidateError.connect(
      this, &BasicPortAllocatorSession::OnCandidateError);
  port->SignalPortComplete.connect(this,
                                   &BasicPortAllocatorSession::OnPortComplete);
  port->SubscribePortDestroyed(
      [this](PortInterface* port) { OnPortDestroyed(port); });

  port->SignalPortError.connect(this, &BasicPortAllocatorSession::OnPortError);
  RTC_LOG(LS_INFO) << port->ToString() << ": Added port to allocator";

  port->PrepareAddress();
}

void BasicPortAllocatorSession::OnAllocationSequenceObjectsCreated() {
  RTC_DCHECK_RUN_ON(network_thread_);
  allocation_sequences_created_ = true;

  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnCandidateReady(Port* port,
                                                 const Candidate& c) {
  RTC_DCHECK_RUN_ON(network_thread_);
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);
  RTC_LOG(LS_INFO) << port->ToString()
                   << ": Gathered candidate: " << c.ToSensitiveString();

  if (!data->inprogress()) {
    RTC_LOG(LS_WARNING)
        << "Discarding candidate because port is already done gathering.";
    return;
  }

  bool pruned = false;
  if (CandidatePairable(c, port) && !data->has_pairable_candidate()) {
    data->set_has_pairable_candidate(true);

    if (port->Type() == RELAY_PORT_TYPE) {
      if (turn_port_prune_policy_ == webrtc::KEEP_FIRST_READY) {
        pruned = PruneNewlyPairableTurnPort(data);
      } else if (turn_port_prune_policy_ == webrtc::PRUNE_BASED_ON_PRIORITY) {
        pruned = PruneTurnPorts(port);
      }
    }

    if (!data->pruned()) {
      RTC_LOG(LS_INFO) << port->ToString() << ": Port ready.";
      SignalPortReady(this, port);
      port->KeepAliveUntilPruned();
    }
  }

  if (data->ready() && CheckCandidateFilter(c)) {
    std::vector<Candidate> candidates;
    candidates.push_back(allocator_->SanitizeCandidate(c));
    SignalCandidatesReady(this, candidates);
  } else {
    RTC_LOG(LS_INFO) << "Discarding candidate because it doesn't match filter.";
  }

  if (pruned) {
    MaybeSignalCandidatesAllocationDone();
  }
}

void BasicPortAllocatorSession::OnCandidateError(
    Port* port,
    const IceCandidateErrorEvent& event) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(FindPort(port));
  if (event.address.empty()) {
    candidate_error_events_.push_back(event);
  } else {
    SignalCandidateError(this, event);
  }
}

Port* BasicPortAllocatorSession::GetBestTurnPortForNetwork(
    absl::string_view network_name) const {
  RTC_DCHECK_RUN_ON(network_thread_);
  Port* best_turn_port = nullptr;
  for (const PortData& data : ports_) {
    if (data.port()->Network()->name() == network_name &&
        data.port()->Type() == RELAY_PORT_TYPE && data.ready() &&
        (!best_turn_port || ComparePort(data.port(), best_turn_port) > 0)) {
      best_turn_port = data.port();
    }
  }
  return best_turn_port;
}

bool BasicPortAllocatorSession::PruneNewlyPairableTurnPort(
    PortData* newly_pairable_port_data) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK(newly_pairable_port_data->port()->Type() == RELAY_PORT_TYPE);

  const std::string& network_name =
      newly_pairable_port_data->port()->Network()->name();

  for (PortData& data : ports_) {
    if (data.port()->Network()->name() == network_name &&
        data.port()->Type() == RELAY_PORT_TYPE && data.ready() &&
        &data != newly_pairable_port_data) {
      RTC_LOG(LS_INFO) << "Port pruned: "
                       << newly_pairable_port_data->port()->ToString();
      newly_pairable_port_data->Prune();
      return true;
    }
  }
  return false;
}

bool BasicPortAllocatorSession::PruneTurnPorts(Port* newly_pairable_turn_port) {
  RTC_DCHECK_RUN_ON(network_thread_);

  const std::string& network_name = newly_pairable_turn_port->Network()->name();
  Port* best_turn_port = GetBestTurnPortForNetwork(network_name);

  RTC_CHECK(best_turn_port != nullptr);

  bool pruned = false;
  std::vector<PortData*> ports_to_prune;
  for (PortData& data : ports_) {
    if (data.port()->Network()->name() == network_name &&
        data.port()->Type() == RELAY_PORT_TYPE && !data.pruned() &&
        ComparePort(data.port(), best_turn_port) < 0) {
      pruned = true;
      if (data.port() != newly_pairable_turn_port) {

        ports_to_prune.push_back(&data);
      } else {
        data.Prune();
      }
    }
  }

  if (!ports_to_prune.empty()) {
    RTC_LOG(LS_INFO) << "Prune " << ports_to_prune.size()
                     << " low-priority TURN ports";
    PrunePortsAndRemoveCandidates(ports_to_prune);
  }
  return pruned;
}

void BasicPortAllocatorSession::PruneAllPorts() {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (PortData& data : ports_) {
    data.Prune();
  }
}

void BasicPortAllocatorSession::OnPortComplete(Port* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << port->ToString()
                   << ": Port completed gathering candidates.";
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);

  if (!data->inprogress()) {
    return;
  }

  data->set_state(PortData::STATE_COMPLETE);

  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnPortError(Port* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_LOG(LS_INFO) << port->ToString()
                   << ": Port encountered error while gathering candidates.";
  PortData* data = FindPort(port);
  RTC_DCHECK(data != NULL);

  if (!data->inprogress()) {
    return;
  }

  data->set_state(PortData::STATE_ERROR);

  MaybeSignalCandidatesAllocationDone();
}

bool BasicPortAllocatorSession::CheckCandidateFilter(const Candidate& c) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  return IsAllowedByCandidateFilter(c, candidate_filter_);
}

bool BasicPortAllocatorSession::CandidatePairable(const Candidate& c,
                                                  const Port* port) const {
  RTC_DCHECK_RUN_ON(network_thread_);

  bool candidate_signalable = CheckCandidateFilter(c);

  bool network_enumeration_disabled = c.address().IsAnyIP();
  bool can_ping_from_candidate =
      (port->SharedSocket() || c.protocol() == TCP_PROTOCOL_NAME);
  bool host_candidates_disabled = !(candidate_filter_ & CF_HOST);

  return candidate_signalable ||
         (network_enumeration_disabled && can_ping_from_candidate &&
          !host_candidates_disabled);
}

void BasicPortAllocatorSession::OnPortAllocationComplete() {
  RTC_DCHECK_RUN_ON(network_thread_);

  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::MaybeSignalCandidatesAllocationDone() {
  RTC_DCHECK_RUN_ON(network_thread_);
  if (CandidatesAllocationDone()) {
    if (pooled()) {
      RTC_LOG(LS_INFO) << "All candidates gathered for pooled session.";
    } else {
      RTC_LOG(LS_INFO) << "All candidates gathered for " << content_name()
                       << ":" << component() << ":" << generation();
    }
    for (const auto& event : candidate_error_events_) {
      SignalCandidateError(this, event);
    }
    candidate_error_events_.clear();
    SignalCandidatesAllocationDone(this);
  }
}

void BasicPortAllocatorSession::OnPortDestroyed(PortInterface* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (std::vector<PortData>::iterator iter = ports_.begin();
       iter != ports_.end(); ++iter) {
    if (port == iter->port()) {
      ports_.erase(iter);
      RTC_LOG(LS_INFO) << port->ToString() << ": Removed port from allocator ("
                       << static_cast<int>(ports_.size()) << " remaining)";
      return;
    }
  }
  RTC_DCHECK_NOTREACHED();
}

BasicPortAllocatorSession::PortData* BasicPortAllocatorSession::FindPort(
    Port* port) {
  RTC_DCHECK_RUN_ON(network_thread_);
  for (std::vector<PortData>::iterator it = ports_.begin(); it != ports_.end();
       ++it) {
    if (it->port() == port) {
      return &*it;
    }
  }
  return NULL;
}

std::vector<BasicPortAllocatorSession::PortData*>
BasicPortAllocatorSession::GetUnprunedPorts(
    const std::vector<const rtc::Network*>& networks) {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<PortData*> unpruned_ports;
  for (PortData& port : ports_) {
    if (!port.pruned() &&
        absl::c_linear_search(networks, port.sequence()->network())) {
      unpruned_ports.push_back(&port);
    }
  }
  return unpruned_ports;
}

void BasicPortAllocatorSession::PrunePortsAndRemoveCandidates(
    const std::vector<PortData*>& port_data_list) {
  RTC_DCHECK_RUN_ON(network_thread_);
  std::vector<PortInterface*> pruned_ports;
  std::vector<Candidate> removed_candidates;
  for (PortData* data : port_data_list) {

    data->Prune();
    pruned_ports.push_back(data->port());
    if (data->has_pairable_candidate()) {
      GetCandidatesFromPort(*data, &removed_candidates);

      data->set_has_pairable_candidate(false);
    }
  }
  if (!pruned_ports.empty()) {
    SignalPortsPruned(this, pruned_ports);
  }
  if (!removed_candidates.empty()) {
    RTC_LOG(LS_INFO) << "Removed " << removed_candidates.size()
                     << " candidates";
    SignalCandidatesRemoved(this, removed_candidates);
  }
}

void BasicPortAllocator::SetVpnList(
    const std::vector<rtc::NetworkMask>& vpn_list) {
  network_manager_->set_vpn_list(vpn_list);
}

AllocationSequence::AllocationSequence(
    BasicPortAllocatorSession* session,
    const rtc::Network* network,
    PortConfiguration* config,
    uint32_t flags,
    std::function<void()> port_allocation_complete_callback)
    : session_(session),
      network_(network),
      config_(config),
      state_(kInit),
      flags_(flags),
      udp_socket_(),
      udp_port_(NULL),
      phase_(0),
      port_allocation_complete_callback_(
          std::move(port_allocation_complete_callback)) {}

void AllocationSequence::Init() {
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    udp_socket_.reset(session_->socket_factory()->CreateUdpSocket(
        rtc::SocketAddress(network_->GetBestIP(), 0),
        session_->allocator()->min_port(), session_->allocator()->max_port()));
    if (udp_socket_) {
      udp_socket_->RegisterReceivedPacketCallback(
          [&](rtc::AsyncPacketSocket* socket,
              const rtc::ReceivedPacket& packet) {
            OnReadPacket(socket, packet);
          });
    }

  }
}

void AllocationSequence::Clear() {
  TRACE_EVENT0("webrtc", "AllocationSequence::Clear");
  udp_port_ = NULL;
  relay_ports_.clear();
}

void AllocationSequence::OnNetworkFailed() {
  RTC_DCHECK(!network_failed_);
  network_failed_ = true;

  Stop();
}

void AllocationSequence::DisableEquivalentPhases(const rtc::Network* network,
                                                 PortConfiguration* config,
                                                 uint32_t* flags) {
  if (network_failed_) {

    return;
  }

  if (!((network == network_) && (previous_best_ip_ == network->GetBestIP()))) {

    return;
  }

  if (absl::c_any_of(session_->ports_,
                     [this](const BasicPortAllocatorSession::PortData& p) {
                       return !p.pruned() && p.port()->Network() == network_ &&
                              p.port()->GetProtocol() == PROTO_UDP &&
                              p.port()->Type() == LOCAL_PORT_TYPE && !p.error();
                     })) {
    *flags |= PORTALLOCATOR_DISABLE_UDP;
  }

  if (absl::c_any_of(session_->ports_,
                     [this](const BasicPortAllocatorSession::PortData& p) {
                       return !p.pruned() && p.port()->Network() == network_ &&
                              p.port()->GetProtocol() == PROTO_TCP &&
                              p.port()->Type() == LOCAL_PORT_TYPE && !p.error();
                     })) {
    *flags |= PORTALLOCATOR_DISABLE_TCP;
  }

  if (config_ && config) {

    if (config_->StunServers() == config->StunServers() &&
        (*flags & PORTALLOCATOR_DISABLE_UDP)) {

      *flags |= PORTALLOCATOR_DISABLE_STUN;
    }
    if (!config_->relays.empty()) {

      *flags |= PORTALLOCATOR_DISABLE_RELAY;
    }
  }
}

void AllocationSequence::Start() {
  state_ = kRunning;

  session_->network_thread()->PostTask(
      SafeTask(safety_.flag(), [this, epoch = epoch_] { Process(epoch); }));

  previous_best_ip_ = network_->GetBestIP();
}

void AllocationSequence::Stop() {

  if (state_ == kRunning) {
    state_ = kStopped;

    ++epoch_;
  }
}

void AllocationSequence::Process(int epoch) {
  RTC_DCHECK(rtc::Thread::Current() == session_->network_thread());
  const char* const PHASE_NAMES[kNumPhases] = {"Udp", "Relay", "Tcp"};

  if (epoch != epoch_)
    return;

  RTC_LOG(LS_INFO) << network_->ToString()
                   << ": Allocation Phase=" << PHASE_NAMES[phase_];

  switch (phase_) {
    case PHASE_UDP:
      CreateUDPPorts();
      CreateStunPorts();
      break;

    case PHASE_RELAY:
      CreateRelayPorts();
      break;

    case PHASE_TCP:
      CreateTCPPorts();
      state_ = kCompleted;
      break;

    default:
      RTC_DCHECK_NOTREACHED();
  }

  if (state() == kRunning) {
    ++phase_;
    session_->network_thread()->PostDelayedTask(
        SafeTask(safety_.flag(), [this, epoch = epoch_] { Process(epoch); }),
        TimeDelta::Millis(session_->allocator()->step_delay()));
  } else {

    ++epoch_;
    port_allocation_complete_callback_();
  }
}

void AllocationSequence::CreateUDPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_UDP)) {
    RTC_LOG(LS_VERBOSE) << "AllocationSequence: UDP ports disabled, skipping.";
    return;
  }

  std::unique_ptr<UDPPort> port;
  bool emit_local_candidate_for_anyaddress =
      !IsFlagSet(PORTALLOCATOR_DISABLE_DEFAULT_LOCAL_CANDIDATE);
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) && udp_socket_) {
    port = UDPPort::Create(
        session_->network_thread(), session_->socket_factory(), network_,
        udp_socket_.get(), session_->username(), session_->password(),
        emit_local_candidate_for_anyaddress,
        session_->allocator()->stun_candidate_keepalive_interval(),
        session_->allocator()->field_trials());
  } else {
    port = UDPPort::Create(
        session_->network_thread(), session_->socket_factory(), network_,
        session_->allocator()->min_port(), session_->allocator()->max_port(),
        session_->username(), session_->password(),
        emit_local_candidate_for_anyaddress,
        session_->allocator()->stun_candidate_keepalive_interval(),
        session_->allocator()->field_trials());
  }

  if (port) {
    port->SetIceTiebreaker(session_->allocator()->ice_tiebreaker());

    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
      udp_port_ = port.get();
      port->SubscribePortDestroyed(
          [this](PortInterface* port) { OnPortDestroyed(port); });

      if (!IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
        if (config_ && !config_->StunServers().empty()) {
          RTC_LOG(LS_INFO)
              << "AllocationSequence: UDPPort will be handling the "
                 "STUN candidate generation.";
          port->set_server_addresses(config_->StunServers());
        }
      }
    }

    session_->AddAllocatedPort(port.release(), this);
  }
}

void AllocationSequence::CreateTCPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_TCP)) {
    RTC_LOG(LS_VERBOSE) << "AllocationSequence: TCP ports disabled, skipping.";
    return;
  }

  std::unique_ptr<Port> port = TCPPort::Create(
      session_->network_thread(), session_->socket_factory(), network_,
      session_->allocator()->min_port(), session_->allocator()->max_port(),
      session_->username(), session_->password(),
      session_->allocator()->allow_tcp_listen(),
      session_->allocator()->field_trials());
  if (port) {
    port->SetIceTiebreaker(session_->allocator()->ice_tiebreaker());
    session_->AddAllocatedPort(port.release(), this);

  }
}

void AllocationSequence::CreateStunPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
    RTC_LOG(LS_VERBOSE) << "AllocationSequence: STUN ports disabled, skipping.";
    return;
  }

  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    return;
  }

  if (!(config_ && !config_->StunServers().empty())) {
    RTC_LOG(LS_WARNING)
        << "AllocationSequence: No STUN server configured, skipping.";
    return;
  }

  std::unique_ptr<StunPort> port = StunPort::Create(
      session_->network_thread(), session_->socket_factory(), network_,
      session_->allocator()->min_port(), session_->allocator()->max_port(),
      session_->username(), session_->password(), config_->StunServers(),
      session_->allocator()->stun_candidate_keepalive_interval(),
      session_->allocator()->field_trials());
  if (port) {
    port->SetIceTiebreaker(session_->allocator()->ice_tiebreaker());
    session_->AddAllocatedPort(port.release(), this);

  }
}

void AllocationSequence::CreateRelayPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_RELAY)) {
    RTC_LOG(LS_VERBOSE)
        << "AllocationSequence: Relay ports disabled, skipping.";
    return;
  }

  RTC_DCHECK(config_);
  RTC_DCHECK(!config_->relays.empty());
  if (!(config_ && !config_->relays.empty())) {
    RTC_LOG(LS_WARNING)
        << "AllocationSequence: No relay server configured, skipping.";
    return;
  }

  int relative_priority = config_->relays.size();
  for (RelayServerConfig& relay : config_->relays) {
    CreateTurnPort(relay, relative_priority--);
  }
}

void AllocationSequence::CreateTurnPort(const RelayServerConfig& config,
                                        int relative_priority) {
  PortList::const_iterator relay_port;
  for (relay_port = config.ports.begin(); relay_port != config.ports.end();
       ++relay_port) {

    if (IsFlagSet(PORTALLOCATOR_DISABLE_UDP_RELAY) &&
        relay_port->proto == PROTO_UDP) {
      continue;
    }

    int server_ip_family = relay_port->address.ipaddr().family();
    int local_ip_family = network_->GetBestIP().family();
    if (server_ip_family != AF_UNSPEC && server_ip_family != local_ip_family) {
      RTC_LOG(LS_INFO)
          << "Server and local address families are not compatible. "
             "Server address: "
          << relay_port->address.ipaddr().ToSensitiveString()
          << " Local address: " << network_->GetBestIP().ToSensitiveString();
      continue;
    }

    CreateRelayPortArgs args;
    args.network_thread = session_->network_thread();
    args.socket_factory = session_->socket_factory();
    args.network = network_;
    args.username = session_->username();
    args.password = session_->password();
    args.server_address = &(*relay_port);
    args.config = &config;
    args.turn_customizer = session_->allocator()->turn_customizer();
    args.field_trials = session_->allocator()->field_trials();
    args.relative_priority = relative_priority;

    std::unique_ptr<cricket::Port> port;

    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) &&
        relay_port->proto == PROTO_UDP && udp_socket_) {
      port = session_->allocator()->relay_port_factory()->Create(
          args, udp_socket_.get());

      if (!port) {
        RTC_LOG(LS_WARNING) << "Failed to create relay port with "
                            << args.server_address->address.ToSensitiveString();
        continue;
      }

      relay_ports_.push_back(port.get());

      port->SubscribePortDestroyed(
          [this](PortInterface* port) { OnPortDestroyed(port); });

    } else {
      port = session_->allocator()->relay_port_factory()->Create(
          args, session_->allocator()->min_port(),
          session_->allocator()->max_port());

      if (!port) {
        RTC_LOG(LS_WARNING) << "Failed to create relay port with "
                            << args.server_address->address.ToSensitiveString();
        continue;
      }
    }
    RTC_DCHECK(port != NULL);
    port->SetIceTiebreaker(session_->allocator()->ice_tiebreaker());
    session_->AddAllocatedPort(port.release(), this);
  }
}

void AllocationSequence::OnReadPacket(rtc::AsyncPacketSocket* socket,
                                      const rtc::ReceivedPacket& packet) {
  RTC_DCHECK(socket == udp_socket_.get());

  bool turn_port_found = false;

  for (auto* port : relay_ports_) {
    if (port->CanHandleIncomingPacketsFrom(packet.source_address())) {
      if (port->HandleIncomingPacket(socket, packet)) {
        return;
      }
      turn_port_found = true;
    }
  }

  if (udp_port_) {
    const ServerAddresses& stun_servers = udp_port_->server_addresses();

    if (!turn_port_found ||
        stun_servers.find(packet.source_address()) != stun_servers.end()) {
      RTC_DCHECK(udp_port_->SharedSocket());
      udp_port_->HandleIncomingPacket(socket, packet);
    }
  }
}

void AllocationSequence::OnPortDestroyed(PortInterface* port) {
  if (udp_port_ == port) {
    udp_port_ = NULL;
    return;
  }

  auto it = absl::c_find(relay_ports_, port);
  if (it != relay_ports_.end()) {
    relay_ports_.erase(it);
  } else {
    RTC_LOG(LS_ERROR) << "Unexpected OnPortDestroyed for nonexistent port.";
    RTC_DCHECK_NOTREACHED();
  }
}

PortConfiguration::PortConfiguration(
    const ServerAddresses& stun_servers,
    absl::string_view username,
    absl::string_view password,
    const webrtc::FieldTrialsView* field_trials)
    : stun_servers(stun_servers), username(username), password(password) {
  if (!stun_servers.empty())
    stun_address = *(stun_servers.begin());

  if (field_trials) {
    use_turn_server_as_stun_server_disabled =
        field_trials->IsDisabled("WebRTC-UseTurnServerAsStunServer");
  }
}

ServerAddresses PortConfiguration::StunServers() {
  if (!stun_address.IsNil() &&
      stun_servers.find(stun_address) == stun_servers.end()) {
    stun_servers.insert(stun_address);
  }

  if (!stun_servers.empty() && use_turn_server_as_stun_server_disabled) {
    return stun_servers;
  }

  ServerAddresses turn_servers = GetRelayServerAddresses(PROTO_UDP);
  for (const rtc::SocketAddress& turn_server : turn_servers) {
    if (stun_servers.find(turn_server) == stun_servers.end()) {
      stun_servers.insert(turn_server);
    }
  }
  return stun_servers;
}

void PortConfiguration::AddRelay(const RelayServerConfig& config) {
  relays.push_back(config);
}

bool PortConfiguration::SupportsProtocol(const RelayServerConfig& relay,
                                         ProtocolType type) const {
  PortList::const_iterator relay_port;
  for (relay_port = relay.ports.begin(); relay_port != relay.ports.end();
       ++relay_port) {
    if (relay_port->proto == type)
      return true;
  }
  return false;
}

bool PortConfiguration::SupportsProtocol(ProtocolType type) const {
  for (size_t i = 0; i < relays.size(); ++i) {
    if (SupportsProtocol(relays[i], type))
      return true;
  }
  return false;
}

ServerAddresses PortConfiguration::GetRelayServerAddresses(
    ProtocolType type) const {
  ServerAddresses servers;
  for (size_t i = 0; i < relays.size(); ++i) {
    if (SupportsProtocol(relays[i], type)) {
      servers.insert(relays[i].ports.front().address);
    }
  }
  return servers;
}

}
