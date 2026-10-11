/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/goog_cc/robust_throughput_estimator.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/goog_cc/acknowledged_bitrate_estimator_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

RobustThroughputEstimator::RobustThroughputEstimator(
    const RobustThroughputEstimatorSettings& settings)
    : settings_(settings),
      latest_discarded_send_time_(Timestamp::MinusInfinity()) {
  RTC_DCHECK(settings.enabled);
}

RobustThroughputEstimator::~RobustThroughputEstimator() {}

bool RobustThroughputEstimator::FirstPacketOutsideWindow() {
  if (window_.empty())
    return false;
  if (window_.size() > settings_.max_window_packets)
    return true;
  TimeDelta current_window_duration =
      window_.back().receive_time - window_.front().receive_time;
  if (current_window_duration > settings_.max_window_duration)
    return true;
  if (window_.size() > settings_.window_packets &&
      current_window_duration > settings_.min_window_duration) {
    return true;
  }
  return false;
}

void RobustThroughputEstimator::IncomingPacketFeedbackVector(
    const std::vector<PacketResult>& packet_feedback_vector) {
  RTC_DCHECK(std::is_sorted(packet_feedback_vector.begin(),
                            packet_feedback_vector.end(),
                            PacketResult::ReceiveTimeOrder()));
  for (const auto& packet : packet_feedback_vector) {

    if (packet.receive_time.IsInfinite() ||
        packet.sent_packet.send_time.IsInfinite()) {
      continue;
    }

    window_.push_back(packet);
    window_.back().sent_packet.prior_unacked_data =
        window_.back().sent_packet.prior_unacked_data *
        settings_.unacked_weight;

    for (size_t i = window_.size() - 1;
         i > 0 && window_[i].receive_time < window_[i - 1].receive_time; i--) {
      std::swap(window_[i], window_[i - 1]);
    }
    constexpr TimeDelta kMaxReorderingTime = TimeDelta::Seconds(1);
    const TimeDelta receive_delta =
        (window_.back().receive_time - packet.receive_time);
    if (receive_delta > kMaxReorderingTime) {
      RTC_LOG(LS_WARNING)
          << "Severe packet re-ordering or timestamps offset changed: "
          << receive_delta;
      window_.clear();
      latest_discarded_send_time_ = Timestamp::MinusInfinity();
    }
  }

  while (FirstPacketOutsideWindow()) {
    latest_discarded_send_time_ = std::max(
        latest_discarded_send_time_, window_.front().sent_packet.send_time);
    window_.pop_front();
  }
}

absl::optional<DataRate> RobustThroughputEstimator::bitrate() const {
  if (window_.empty() || window_.size() < settings_.required_packets)
    return absl::nullopt;

  TimeDelta largest_recv_gap(TimeDelta::Zero());
  TimeDelta second_largest_recv_gap(TimeDelta::Zero());
  for (size_t i = 1; i < window_.size(); i++) {

    TimeDelta gap = window_[i].receive_time - window_[i - 1].receive_time;
    if (gap > largest_recv_gap) {
      second_largest_recv_gap = largest_recv_gap;
      largest_recv_gap = gap;
    } else if (gap > second_largest_recv_gap) {
      second_largest_recv_gap = gap;
    }
  }

  Timestamp first_send_time = Timestamp::PlusInfinity();
  Timestamp last_send_time = Timestamp::MinusInfinity();
  Timestamp first_recv_time = Timestamp::PlusInfinity();
  Timestamp last_recv_time = Timestamp::MinusInfinity();
  DataSize recv_size = DataSize::Bytes(0);
  DataSize send_size = DataSize::Bytes(0);
  DataSize first_recv_size = DataSize::Bytes(0);
  DataSize last_send_size = DataSize::Bytes(0);
  size_t num_sent_packets_in_window = 0;
  for (const auto& packet : window_) {
    if (packet.receive_time < first_recv_time) {
      first_recv_time = packet.receive_time;
      first_recv_size =
          packet.sent_packet.size + packet.sent_packet.prior_unacked_data;
    }
    last_recv_time = std::max(last_recv_time, packet.receive_time);
    recv_size += packet.sent_packet.size;
    recv_size += packet.sent_packet.prior_unacked_data;

    if (packet.sent_packet.send_time < latest_discarded_send_time_) {

      continue;
    }
    if (packet.sent_packet.send_time > last_send_time) {
      last_send_time = packet.sent_packet.send_time;
      last_send_size =
          packet.sent_packet.size + packet.sent_packet.prior_unacked_data;
    }
    first_send_time = std::min(first_send_time, packet.sent_packet.send_time);

    send_size += packet.sent_packet.size;
    send_size += packet.sent_packet.prior_unacked_data;
    ++num_sent_packets_in_window;
  }

  recv_size -= first_recv_size;
  send_size -= last_send_size;

  RTC_DCHECK(first_recv_time.IsFinite());
  RTC_DCHECK(last_recv_time.IsFinite());
  TimeDelta recv_duration = (last_recv_time - first_recv_time) -
                            largest_recv_gap + second_largest_recv_gap;
  recv_duration = std::max(recv_duration, TimeDelta::Millis(1));

  if (num_sent_packets_in_window < settings_.required_packets) {

    return recv_size / recv_duration;
  }

  RTC_DCHECK(first_send_time.IsFinite());
  RTC_DCHECK(last_send_time.IsFinite());
  TimeDelta send_duration = last_send_time - first_send_time;
  send_duration = std::max(send_duration, TimeDelta::Millis(1));

  return std::min(send_size / send_duration, recv_size / recv_duration);
}

}
