/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/basic_ice_controller.h"

namespace {

const int kMinImprovement = 10;

bool IsRelayRelay(const cricket::Connection* conn) {
  return conn->local_candidate().is_relay() &&
         conn->remote_candidate().is_relay();
}

bool IsUdp(const cricket::Connection* conn) {
  return conn->local_candidate().relay_protocol() == cricket::UDP_PROTOCOL_NAME;
}

static constexpr int a_is_better = 1;
static constexpr int b_is_better = -1;
static constexpr int a_and_b_equal = 0;

bool LocalCandidateUsesPreferredNetwork(
    const cricket::Connection* conn,
    absl::optional<rtc::AdapterType> network_preference) {
  rtc::AdapterType network_type = conn->network()->type();
  return network_preference.has_value() && (network_type == network_preference);
}

int CompareCandidatePairsByNetworkPreference(
    const cricket::Connection* a,
    const cricket::Connection* b,
    absl::optional<rtc::AdapterType> network_preference) {
  bool a_uses_preferred_network =
      LocalCandidateUsesPreferredNetwork(a, network_preference);
  bool b_uses_preferred_network =
      LocalCandidateUsesPreferredNetwork(b, network_preference);
  if (a_uses_preferred_network && !b_uses_preferred_network) {
    return a_is_better;
  } else if (!a_uses_preferred_network && b_uses_preferred_network) {
    return b_is_better;
  }
  return a_and_b_equal;
}

}

namespace cricket {

BasicIceController::BasicIceController(const IceControllerFactoryArgs& args)
    : ice_transport_state_func_(args.ice_transport_state_func),
      ice_role_func_(args.ice_role_func),
      is_connection_pruned_func_(args.is_connection_pruned_func),
      field_trials_(args.ice_field_trials) {}

BasicIceController::~BasicIceController() {}

void BasicIceController::SetIceConfig(const IceConfig& config) {
  config_ = config;
}

void BasicIceController::SetSelectedConnection(
    const Connection* selected_connection) {
  selected_connection_ = selected_connection;
}

void BasicIceController::AddConnection(const Connection* connection) {
  connections_.push_back(connection);
  unpinged_connections_.insert(connection);
}

void BasicIceController::OnConnectionDestroyed(const Connection* connection) {
  pinged_connections_.erase(connection);
  unpinged_connections_.erase(connection);
  connections_.erase(absl::c_find(connections_, connection));
  if (selected_connection_ == connection)
    selected_connection_ = nullptr;
}

bool BasicIceController::HasPingableConnection() const {
  int64_t now = rtc::TimeMillis();
  return absl::c_any_of(connections_, [this, now](const Connection* c) {
    return IsPingable(c, now);
  });
}

IceControllerInterface::PingResult BasicIceController::SelectConnectionToPing(
    int64_t last_ping_sent_ms) {

  bool need_more_pings_at_weak_interval =
      absl::c_any_of(connections_, [](const Connection* conn) {
        return conn->active() &&
               conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL;
      });
  int ping_interval = (weak() || need_more_pings_at_weak_interval)
                          ? weak_ping_interval()
                          : strong_ping_interval();

  const Connection* conn = nullptr;
  if (rtc::TimeMillis() >= last_ping_sent_ms + ping_interval) {
    conn = FindNextPingableConnection();
  }
  PingResult res(conn, std::min(ping_interval, check_receiving_interval()));
  return res;
}

void BasicIceController::MarkConnectionPinged(const Connection* conn) {
  if (conn && pinged_connections_.insert(conn).second) {
    unpinged_connections_.erase(conn);
  }
}

const Connection* BasicIceController::FindNextPingableConnection() {
  int64_t now = rtc::TimeMillis();

  if (selected_connection_ && selected_connection_->connected() &&
      selected_connection_->writable() &&
      WritableConnectionPastPingInterval(selected_connection_, now)) {
    return selected_connection_;
  }

  if (weak()) {
    std::vector<const Connection*> pingable_selectable_connections;
    absl::c_copy_if(GetBestWritableConnectionPerNetwork(),
                    std::back_inserter(pingable_selectable_connections),
                    [this, now](const Connection* conn) {
                      return WritableConnectionPastPingInterval(conn, now);
                    });
    auto iter = absl::c_min_element(
        pingable_selectable_connections,
        [](const Connection* conn1, const Connection* conn2) {
          return conn1->last_ping_sent() < conn2->last_ping_sent();
        });
    if (iter != pingable_selectable_connections.end()) {
      return *iter;
    }
  }

  const Connection* oldest_triggered_check =
      FindOldestConnectionNeedingTriggeredCheck(now);
  if (oldest_triggered_check) {
    return oldest_triggered_check;
  }

  RTC_CHECK(connections_.size() ==
            pinged_connections_.size() + unpinged_connections_.size());

  if (absl::c_none_of(unpinged_connections_,
                      [this, now](const Connection* conn) {
                        return this->IsPingable(conn, now);
                      })) {
    unpinged_connections_.insert(pinged_connections_.begin(),
                                 pinged_connections_.end());
    pinged_connections_.clear();
  }

  std::vector<const Connection*> pingable_connections;
  absl::c_copy_if(
      unpinged_connections_, std::back_inserter(pingable_connections),
      [this, now](const Connection* conn) { return IsPingable(conn, now); });
  auto iter = absl::c_max_element(
      pingable_connections,
      [this](const Connection* conn1, const Connection* conn2) {

        if (conn1 == conn2) {
          return false;
        }
        return MorePingable(conn1, conn2) == conn2;
      });
  if (iter != pingable_connections.end()) {
    return *iter;
  }
  return nullptr;
}

const Connection* BasicIceController::FindOldestConnectionNeedingTriggeredCheck(
    int64_t now) {
  const Connection* oldest_needing_triggered_check = nullptr;
  for (auto* conn : connections_) {
    if (!IsPingable(conn, now)) {
      continue;
    }
    bool needs_triggered_check =
        (!conn->writable() &&
         conn->last_ping_received() > conn->last_ping_sent());
    if (needs_triggered_check &&
        (!oldest_needing_triggered_check ||
         (conn->last_ping_received() <
          oldest_needing_triggered_check->last_ping_received()))) {
      oldest_needing_triggered_check = conn;
    }
  }

  if (oldest_needing_triggered_check) {
    RTC_LOG(LS_INFO) << "Selecting connection for triggered check: "
                     << oldest_needing_triggered_check->ToString();
  }
  return oldest_needing_triggered_check;
}

bool BasicIceController::WritableConnectionPastPingInterval(
    const Connection* conn,
    int64_t now) const {
  int interval = CalculateActiveWritablePingInterval(conn, now);
  return conn->last_ping_sent() + interval <= now;
}

int BasicIceController::CalculateActiveWritablePingInterval(
    const Connection* conn,
    int64_t now) const {

  if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
    return weak_ping_interval();
  }

  int stable_interval =
      config_.stable_writable_connection_ping_interval_or_default();
  int weak_or_stablizing_interval = std::min(
      stable_interval, WEAK_OR_STABILIZING_WRITABLE_CONNECTION_PING_INTERVAL);

  return (!weak() && conn->stable(now)) ? stable_interval
                                        : weak_or_stablizing_interval;
}

bool BasicIceController::IsPingable(const Connection* conn, int64_t now) const {
  const Candidate& remote = conn->remote_candidate();

  RTC_DCHECK(!remote.username().empty());
  if (remote.username().empty() || remote.password().empty()) {

    return false;
  }

  if (conn->state() == IceCandidatePairState::FAILED) {
    return false;
  }

  if (!conn->connected() && !conn->writable()) {
    return false;
  }

  if (conn->TooManyOutstandingPings(field_trials_->max_outstanding_pings)) {
    return false;
  }

  if (weak()) {
    return true;
  }

  if (IsBackupConnection(conn)) {
    return conn->rtt_samples() == 0 ||
           (now >= conn->last_ping_response_received() +
                       config_.backup_connection_ping_interval_or_default());
  }

  if (!conn->active()) {
    return false;
  }

  if (!conn->writable()) {
    return true;
  }

  return WritableConnectionPastPingInterval(conn, now);
}

bool BasicIceController::IsBackupConnection(const Connection* conn) const {
  return ice_transport_state_func_() == IceTransportState::STATE_COMPLETED &&
         conn != selected_connection_ && conn->active();
}

const Connection* BasicIceController::MorePingable(const Connection* conn1,
                                                   const Connection* conn2) {
  RTC_DCHECK(conn1 != conn2);
  if (config_.prioritize_most_likely_candidate_pairs) {
    const Connection* most_likely_to_work_conn = MostLikelyToWork(conn1, conn2);
    if (most_likely_to_work_conn) {
      return most_likely_to_work_conn;
    }
  }

  const Connection* least_recently_pinged_conn =
      LeastRecentlyPinged(conn1, conn2);
  if (least_recently_pinged_conn) {
    return least_recently_pinged_conn;
  }

  auto connections = connections_;
  return *(std::find_if(connections.begin(), connections.end(),
                        [conn1, conn2](const Connection* conn) {
                          return conn == conn1 || conn == conn2;
                        }));
}

const Connection* BasicIceController::MostLikelyToWork(
    const Connection* conn1,
    const Connection* conn2) {
  bool rr1 = IsRelayRelay(conn1);
  bool rr2 = IsRelayRelay(conn2);
  if (rr1 && !rr2) {
    return conn1;
  } else if (rr2 && !rr1) {
    return conn2;
  } else if (rr1 && rr2) {
    bool udp1 = IsUdp(conn1);
    bool udp2 = IsUdp(conn2);
    if (udp1 && !udp2) {
      return conn1;
    } else if (udp2 && udp1) {
      return conn2;
    }
  }
  return nullptr;
}

const Connection* BasicIceController::LeastRecentlyPinged(
    const Connection* conn1,
    const Connection* conn2) {
  if (conn1->last_ping_sent() < conn2->last_ping_sent()) {
    return conn1;
  }
  if (conn1->last_ping_sent() > conn2->last_ping_sent()) {
    return conn2;
  }
  return nullptr;
}

std::map<const rtc::Network*, const Connection*>
BasicIceController::GetBestConnectionByNetwork() const {

  std::map<const rtc::Network*, const Connection*> best_connection_by_network;
  if (selected_connection_) {
    best_connection_by_network[selected_connection_->network()] =
        selected_connection_;
  }

  for (const Connection* conn : connections_) {
    const rtc::Network* network = conn->network();

    best_connection_by_network.insert(std::make_pair(network, conn));
  }
  return best_connection_by_network;
}

std::vector<const Connection*>
BasicIceController::GetBestWritableConnectionPerNetwork() const {
  std::vector<const Connection*> connections;
  for (auto kv : GetBestConnectionByNetwork()) {
    const Connection* conn = kv.second;
    if (conn->writable() && conn->connected()) {
      connections.push_back(conn);
    }
  }
  return connections;
}

IceControllerInterface::SwitchResult
BasicIceController::HandleInitialSelectDampening(
    IceSwitchReason reason,
    const Connection* new_connection) {
  if (!field_trials_->initial_select_dampening.has_value() &&
      !field_trials_->initial_select_dampening_ping_received.has_value()) {

    return {new_connection, absl::nullopt};
  }

  int64_t now = rtc::TimeMillis();
  int64_t max_delay = 0;
  if (new_connection->last_ping_received() > 0 &&
      field_trials_->initial_select_dampening_ping_received.has_value()) {
    max_delay = *field_trials_->initial_select_dampening_ping_received;
  } else if (field_trials_->initial_select_dampening.has_value()) {
    max_delay = *field_trials_->initial_select_dampening;
  }

  int64_t start_wait =
      initial_select_timestamp_ms_ == 0 ? now : initial_select_timestamp_ms_;
  int64_t max_wait_until = start_wait + max_delay;

  if (now >= max_wait_until) {
    RTC_LOG(LS_INFO) << "reset initial_select_timestamp_ = "
                     << initial_select_timestamp_ms_
                     << " selection delayed by: " << (now - start_wait) << "ms";
    initial_select_timestamp_ms_ = 0;
    return {new_connection, absl::nullopt};
  }

  if (initial_select_timestamp_ms_ == 0) {

    initial_select_timestamp_ms_ = now;
    RTC_LOG(LS_INFO) << "set initial_select_timestamp_ms_ = "
                     << initial_select_timestamp_ms_;
  }

  int min_delay = max_delay;
  if (field_trials_->initial_select_dampening.has_value()) {
    min_delay = std::min(min_delay, *field_trials_->initial_select_dampening);
  }
  if (field_trials_->initial_select_dampening_ping_received.has_value()) {
    min_delay = std::min(
        min_delay, *field_trials_->initial_select_dampening_ping_received);
  }

  RTC_LOG(LS_INFO) << "delay initial selection up to " << min_delay << "ms";
  return {.connection = absl::nullopt,
          .recheck_event = IceRecheckEvent(
              IceSwitchReason::ICE_CONTROLLER_RECHECK, min_delay)};
}

IceControllerInterface::SwitchResult BasicIceController::ShouldSwitchConnection(
    IceSwitchReason reason,
    const Connection* new_connection) {
  if (!ReadyToSend(new_connection) || selected_connection_ == new_connection) {
    return {absl::nullopt, absl::nullopt};
  }

  if (selected_connection_ == nullptr) {
    return HandleInitialSelectDampening(reason, new_connection);
  }

  int compare_a_b_by_networks = CompareCandidatePairNetworks(
      new_connection, selected_connection_, config_.network_preference);
  if (compare_a_b_by_networks == b_is_better && !new_connection->receiving()) {
    return {absl::nullopt, absl::nullopt};
  }

  bool missed_receiving_unchanged_threshold = false;
  absl::optional<int64_t> receiving_unchanged_threshold(
      rtc::TimeMillis() - config_.receiving_switching_delay_or_default());
  int cmp = CompareConnections(selected_connection_, new_connection,
                               receiving_unchanged_threshold,
                               &missed_receiving_unchanged_threshold);

  absl::optional<IceRecheckEvent> recheck_event;
  if (missed_receiving_unchanged_threshold &&
      config_.receiving_switching_delay_or_default()) {

    recheck_event.emplace(reason,
                          config_.receiving_switching_delay_or_default());
  }

  if (cmp < 0) {
    return {new_connection, absl::nullopt};
  } else if (cmp > 0) {
    return {absl::nullopt, recheck_event};
  }

  if (new_connection->rtt() <= selected_connection_->rtt() - kMinImprovement) {
    return {new_connection, absl::nullopt};
  }

  return {absl::nullopt, recheck_event};
}

IceControllerInterface::SwitchResult
BasicIceController::SortAndSwitchConnection(IceSwitchReason reason) {

  absl::c_stable_sort(
      connections_, [this](const Connection* a, const Connection* b) {
        int cmp = CompareConnections(a, b, absl::nullopt, nullptr);
        if (cmp != 0) {
          return cmp > 0;
        }

        return a->rtt() < b->rtt();
      });

  RTC_LOG(LS_VERBOSE) << "Sorting " << connections_.size()
                      << " available connections due to: "
                      << IceSwitchReasonToString(reason);
  for (size_t i = 0; i < connections_.size(); ++i) {
    RTC_LOG(LS_VERBOSE) << connections_[i]->ToString();
  }

  const Connection* top_connection =
      (!connections_.empty()) ? connections_[0] : nullptr;

  return ShouldSwitchConnection(reason, top_connection);
}

bool BasicIceController::ReadyToSend(const Connection* connection) const {

  return connection != nullptr &&
         (connection->writable() ||
          connection->write_state() == Connection::STATE_WRITE_UNRELIABLE ||
          PresumedWritable(connection));
}

bool BasicIceController::PresumedWritable(const Connection* conn) const {
  return (conn->write_state() == Connection::STATE_WRITE_INIT &&
          config_.presume_writable_when_fully_relayed &&
          conn->local_candidate().is_relay() &&
          (conn->remote_candidate().is_relay() ||
           conn->remote_candidate().is_prflx()));
}

int BasicIceController::CompareConnectionStates(
    const Connection* a,
    const Connection* b,
    absl::optional<int64_t> receiving_unchanged_threshold,
    bool* missed_receiving_unchanged_threshold) const {

  bool a_writable = a->writable() || PresumedWritable(a);
  bool b_writable = b->writable() || PresumedWritable(b);
  if (a_writable && !b_writable) {
    return a_is_better;
  }
  if (!a_writable && b_writable) {
    return b_is_better;
  }

  if (a->write_state() < b->write_state()) {
    return a_is_better;
  }
  if (b->write_state() < a->write_state()) {
    return b_is_better;
  }

  if (a->receiving() && !b->receiving()) {
    return a_is_better;
  }
  if (!a->receiving() && b->receiving()) {
    if (!receiving_unchanged_threshold ||
        (a->receiving_unchanged_since() <= *receiving_unchanged_threshold &&
         b->receiving_unchanged_since() <= *receiving_unchanged_threshold)) {
      return b_is_better;
    }
    *missed_receiving_unchanged_threshold = true;
  }

  if (a->write_state() == Connection::STATE_WRITABLE &&
      b->write_state() == Connection::STATE_WRITABLE) {
    if (a->connected() && !b->connected()) {
      return a_is_better;
    }
    if (!a->connected() && b->connected()) {
      return b_is_better;
    }
  }

  return 0;
}

int BasicIceController::CompareConnectionCandidates(const Connection* a,
                                                    const Connection* b) const {
  int compare_a_b_by_networks =
      CompareCandidatePairNetworks(a, b, config_.network_preference);
  if (compare_a_b_by_networks != a_and_b_equal) {
    return compare_a_b_by_networks;
  }

  if (a->priority() > b->priority()) {
    return a_is_better;
  }
  if (a->priority() < b->priority()) {
    return b_is_better;
  }

  int cmp = (a->remote_candidate().generation() + a->generation()) -
            (b->remote_candidate().generation() + b->generation());
  if (cmp != 0) {
    return cmp;
  }

  bool a_pruned = is_connection_pruned_func_(a);
  bool b_pruned = is_connection_pruned_func_(b);
  if (!a_pruned && b_pruned) {
    return a_is_better;
  }
  if (a_pruned && !b_pruned) {
    return b_is_better;
  }

  return 0;
}

int BasicIceController::CompareConnections(
    const Connection* a,
    const Connection* b,
    absl::optional<int64_t> receiving_unchanged_threshold,
    bool* missed_receiving_unchanged_threshold) const {
  RTC_CHECK(a != nullptr);
  RTC_CHECK(b != nullptr);

  int state_cmp = CompareConnectionStates(a, b, receiving_unchanged_threshold,
                                          missed_receiving_unchanged_threshold);
  if (state_cmp != 0) {
    return state_cmp;
  }

  if (ice_role_func_() == ICEROLE_CONTROLLED) {

    if (a->remote_nomination() > b->remote_nomination()) {
      return a_is_better;
    }
    if (a->remote_nomination() < b->remote_nomination()) {
      return b_is_better;
    }

    if (a->last_data_received() > b->last_data_received()) {
      return a_is_better;
    }
    if (a->last_data_received() < b->last_data_received()) {
      return b_is_better;
    }
  }

  return CompareConnectionCandidates(a, b);
}

int BasicIceController::CompareCandidatePairNetworks(
    const Connection* a,
    const Connection* b,
    absl::optional<rtc::AdapterType> network_preference) const {
  int compare_a_b_by_network_preference =
      CompareCandidatePairsByNetworkPreference(a, b,
                                               config_.network_preference);

  if (compare_a_b_by_network_preference != a_and_b_equal) {
    return compare_a_b_by_network_preference;
  }

  bool a_vpn = a->network()->IsVpn();
  bool b_vpn = b->network()->IsVpn();
  switch (config_.vpn_preference) {
    case webrtc::VpnPreference::kDefault:
      break;
    case webrtc::VpnPreference::kOnlyUseVpn:
    case webrtc::VpnPreference::kPreferVpn:
      if (a_vpn && !b_vpn) {
        return a_is_better;
      } else if (!a_vpn && b_vpn) {
        return b_is_better;
      }
      break;
    case webrtc::VpnPreference::kNeverUseVpn:
    case webrtc::VpnPreference::kAvoidVpn:
      if (a_vpn && !b_vpn) {
        return b_is_better;
      } else if (!a_vpn && b_vpn) {
        return a_is_better;
      }
      break;
    default:
      break;
  }

  uint32_t a_cost = a->ComputeNetworkCost();
  uint32_t b_cost = b->ComputeNetworkCost();

  if (a_cost < b_cost) {
    return a_is_better;
  }
  if (a_cost > b_cost) {
    return b_is_better;
  }
  return a_and_b_equal;
}

std::vector<const Connection*> BasicIceController::PruneConnections() {

  std::vector<const Connection*> connections_to_prune;
  auto best_connection_by_network = GetBestConnectionByNetwork();
  for (const Connection* conn : connections_) {
    const Connection* best_conn = selected_connection_;
    if (!rtc::IPIsAny(conn->network()->GetBestIP())) {

      best_conn = best_connection_by_network[conn->network()];
    }

    if (best_conn && conn != best_conn && !best_conn->weak() &&
        CompareConnectionCandidates(best_conn, conn) >= 0) {
      connections_to_prune.push_back(conn);
    }
  }
  return connections_to_prune;
}

bool BasicIceController::GetUseCandidateAttr(const Connection* conn,
                                             NominationMode mode,
                                             IceMode remote_ice_mode) const {
  switch (mode) {
    case NominationMode::REGULAR:

      return false;
    case NominationMode::AGGRESSIVE:
      if (remote_ice_mode == ICEMODE_LITE) {
        return GetUseCandidateAttr(conn, NominationMode::REGULAR,
                                   remote_ice_mode);
      }
      return true;
    case NominationMode::SEMI_AGGRESSIVE: {

      bool selected = conn == selected_connection_;
      if (remote_ice_mode == ICEMODE_LITE) {
        return selected && conn->writable();
      }
      bool better_than_selected =
          !selected_connection_ || !selected_connection_->writable() ||
          CompareConnectionCandidates(selected_connection_, conn) < 0;
      return selected || better_than_selected;
    }
    default:
      RTC_DCHECK_NOTREACHED();
      return false;
  }
}

}
