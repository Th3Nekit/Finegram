/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/simulated_network.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {

int64_t CalculateArrivalTimeUs(int64_t start_time_us,
                               int64_t bits,
                               int capacity_kbps) {

  if (capacity_kbps == 0) {
    return start_time_us;
  }

  return start_time_us + ((1000 * bits + capacity_kbps - 1) / capacity_kbps);
}

}

SimulatedNetwork::SimulatedNetwork(Config config, uint64_t random_seed)
    : random_(random_seed),
      bursting_(false),
      last_enqueue_time_us_(0),
      last_capacity_link_exit_time_(0) {
  SetConfig(config);
}

SimulatedNetwork::~SimulatedNetwork() = default;

void SimulatedNetwork::SetConfig(const Config& config) {
  MutexLock lock(&config_lock_);
  config_state_.config = config;
  double prob_loss = config.loss_percent / 100.0;
  if (config_state_.config.avg_burst_loss_length == -1) {

    config_state_.prob_loss_bursting = prob_loss;
    config_state_.prob_start_bursting = prob_loss;
  } else {

    int avg_burst_loss_length = config.avg_burst_loss_length;
    int min_avg_burst_loss_length = std::ceil(prob_loss / (1 - prob_loss));

    RTC_CHECK_GT(avg_burst_loss_length, min_avg_burst_loss_length)
        << "For a total packet loss of " << config.loss_percent
        << "%% then"
           " avg_burst_loss_length must be "
        << min_avg_burst_loss_length + 1 << " or higher.";

    config_state_.prob_loss_bursting = (1.0 - 1.0 / avg_burst_loss_length);
    config_state_.prob_start_bursting =
        prob_loss / (1 - prob_loss) / avg_burst_loss_length;
  }
}

void SimulatedNetwork::UpdateConfig(
    std::function<void(BuiltInNetworkBehaviorConfig*)> config_modifier) {
  MutexLock lock(&config_lock_);
  config_modifier(&config_state_.config);
}

void SimulatedNetwork::PauseTransmissionUntil(int64_t until_us) {
  MutexLock lock(&config_lock_);
  config_state_.pause_transmission_until_us = until_us;
}

bool SimulatedNetwork::EnqueuePacket(PacketInFlightInfo packet) {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);

  ConfigState state = GetConfigState();

  packet.size += state.config.packet_overhead;

  if (state.config.queue_length_packets > 0 &&
      capacity_link_.size() >= state.config.queue_length_packets) {

    return false;
  }

  int64_t packet_send_time_us = packet.send_time_us;
  if (!capacity_link_.empty()) {
    packet_send_time_us =
        std::max(packet_send_time_us, capacity_link_.back().arrival_time_us);
  }
  capacity_link_.push({.packet = packet,
                       .arrival_time_us = CalculateArrivalTimeUs(
                           packet_send_time_us, packet.size * 8,
                           state.config.link_capacity_kbps)});

  if (!next_process_time_us_) {
    RTC_DCHECK_EQ(capacity_link_.size(), 1);
    next_process_time_us_ = capacity_link_.front().arrival_time_us;
  }

  last_enqueue_time_us_ = packet.send_time_us;
  return true;
}

absl::optional<int64_t> SimulatedNetwork::NextDeliveryTimeUs() const {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);
  return next_process_time_us_;
}

void SimulatedNetwork::UpdateCapacityQueue(ConfigState state,
                                           int64_t time_now_us) {

  if (!capacity_link_.empty()) {
    capacity_link_.front().arrival_time_us = CalculateArrivalTimeUs(
        std::max(capacity_link_.front().packet.send_time_us,
                 last_capacity_link_exit_time_),
        capacity_link_.front().packet.size * 8,
        state.config.link_capacity_kbps);
  }

  if (capacity_link_.empty() ||
      time_now_us < capacity_link_.front().arrival_time_us) {
    return;
  }
  bool reorder_packets = false;

  do {

    PacketInfo packet = capacity_link_.front();
    capacity_link_.pop();

    if (state.pause_transmission_until_us > packet.arrival_time_us) {
      packet.arrival_time_us = state.pause_transmission_until_us;
    }

    last_capacity_link_exit_time_ = packet.arrival_time_us;

    if ((bursting_ && random_.Rand<double>() < state.prob_loss_bursting) ||
        (!bursting_ && random_.Rand<double>() < state.prob_start_bursting)) {
      bursting_ = true;
      packet.arrival_time_us = PacketDeliveryInfo::kNotReceived;
    } else {

      bursting_ = false;
      int64_t arrival_time_jitter_us = std::max(
          random_.Gaussian(state.config.queue_delay_ms * 1000,
                           state.config.delay_standard_deviation_ms * 1000),
          0.0);

      int64_t last_arrival_time_us =
          delay_link_.empty() ? -1 : delay_link_.back().arrival_time_us;
      if (!state.config.allow_reordering && !delay_link_.empty() &&
          packet.arrival_time_us + arrival_time_jitter_us <
              last_arrival_time_us) {
        arrival_time_jitter_us = last_arrival_time_us - packet.arrival_time_us;
      }
      packet.arrival_time_us += arrival_time_jitter_us;

      if (last_arrival_time_us > packet.arrival_time_us) {
        reorder_packets = true;
      }
    }
    delay_link_.emplace_back(packet);

    if (capacity_link_.empty()) {
      break;
    }

    int64_t next_start = std::max(last_capacity_link_exit_time_,
                                  capacity_link_.front().packet.send_time_us);
    capacity_link_.front().arrival_time_us = CalculateArrivalTimeUs(
        next_start, capacity_link_.front().packet.size * 8,
        state.config.link_capacity_kbps);

  } while (capacity_link_.front().arrival_time_us <= time_now_us);

  if (state.config.allow_reordering && reorder_packets) {

    std::stable_sort(delay_link_.begin(), delay_link_.end(),
                     [](const PacketInfo& p1, const PacketInfo& p2) {
                       return p1.arrival_time_us < p2.arrival_time_us;
                     });
  }
}

SimulatedNetwork::ConfigState SimulatedNetwork::GetConfigState() const {
  MutexLock lock(&config_lock_);
  return config_state_;
}

std::vector<PacketDeliveryInfo> SimulatedNetwork::DequeueDeliverablePackets(
    int64_t receive_time_us) {
  RTC_DCHECK_RUNS_SERIALIZED(&process_checker_);

  UpdateCapacityQueue(GetConfigState(), receive_time_us);
  std::vector<PacketDeliveryInfo> packets_to_deliver;

  while (!delay_link_.empty() &&
         receive_time_us >= delay_link_.front().arrival_time_us) {
    PacketInfo packet_info = delay_link_.front();
    packets_to_deliver.emplace_back(
        PacketDeliveryInfo(packet_info.packet, packet_info.arrival_time_us));
    delay_link_.pop_front();
  }

  if (!delay_link_.empty()) {
    next_process_time_us_ = delay_link_.front().arrival_time_us;
  } else if (!capacity_link_.empty()) {
    next_process_time_us_ = capacity_link_.front().arrival_time_us;
  } else {
    next_process_time_us_.reset();
  }
  return packets_to_deliver;
}

}
