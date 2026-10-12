/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "call/bitrate_allocator.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "absl/algorithm/container.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {
using bitrate_allocator_impl::AllocatableTrack;

const uint8_t kTransmissionMaxBitrateMultiplier = 2;
const int kDefaultBitrateBps = 300000;

const double kToggleFactor = 0.1;
const uint32_t kMinToggleBitrateBps = 20000;

const int64_t kBweLogIntervalMs = 5000;

double MediaRatio(uint32_t allocated_bitrate, uint32_t protection_bitrate) {
  RTC_DCHECK_GT(allocated_bitrate, 0);
  if (protection_bitrate == 0)
    return 1.0;

  uint32_t media_bitrate = allocated_bitrate - protection_bitrate;
  return media_bitrate / static_cast<double>(allocated_bitrate);
}

bool EnoughBitrateForAllObservers(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    uint32_t sum_min_bitrates) {
  if (bitrate < sum_min_bitrates)
    return false;

  uint32_t extra_bitrate_per_observer =
      (bitrate - sum_min_bitrates) /
      static_cast<uint32_t>(allocatable_tracks.size());
  for (const auto& observer_config : allocatable_tracks) {
    if (observer_config.config.min_bitrate_bps + extra_bitrate_per_observer <
        observer_config.MinBitrateWithHysteresis()) {
      return false;
    }
  }
  return true;
}

void DistributeBitrateEvenly(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    bool include_zero_allocations,
    int max_multiplier,
    std::map<BitrateAllocatorObserver*, int>* allocation) {
  RTC_DCHECK_EQ(allocation->size(), allocatable_tracks.size());

  std::multimap<uint32_t, const AllocatableTrack*> list_max_bitrates;
  for (const auto& observer_config : allocatable_tracks) {
    if (include_zero_allocations ||
        allocation->at(observer_config.observer) != 0) {
      list_max_bitrates.insert(
          {observer_config.config.max_bitrate_bps, &observer_config});
    }
  }
  auto it = list_max_bitrates.begin();
  while (it != list_max_bitrates.end()) {
    RTC_DCHECK_GT(bitrate, 0);
    uint32_t extra_allocation =
        bitrate / static_cast<uint32_t>(list_max_bitrates.size());
    uint32_t total_allocation =
        extra_allocation + allocation->at(it->second->observer);
    bitrate -= extra_allocation;
    if (total_allocation > max_multiplier * it->first) {

      bitrate += total_allocation - max_multiplier * it->first;
      total_allocation = max_multiplier * it->first;
    }

    allocation->at(it->second->observer) = total_allocation;
    it = list_max_bitrates.erase(it);
  }
}

void DistributeBitrateRelatively(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t remaining_bitrate,
    const std::map<BitrateAllocatorObserver*, int>& observers_capacities,
    std::map<BitrateAllocatorObserver*, int>* allocation) {
  RTC_DCHECK_EQ(allocation->size(), allocatable_tracks.size());
  RTC_DCHECK_EQ(observers_capacities.size(), allocatable_tracks.size());

  struct PriorityRateObserverConfig {
    BitrateAllocatorObserver* allocation_key;

    int capacity_bps;
    double bitrate_priority;
  };

  double bitrate_priority_sum = 0;
  std::vector<PriorityRateObserverConfig> priority_rate_observers;
  for (const auto& observer_config : allocatable_tracks) {
    priority_rate_observers.push_back(PriorityRateObserverConfig{
        observer_config.observer,
        observers_capacities.at(observer_config.observer),
        observer_config.config.bitrate_priority});
    bitrate_priority_sum += observer_config.config.bitrate_priority;
  }

  absl::c_sort(priority_rate_observers, [](const auto& a, const auto& b) {
    return a.capacity_bps / a.bitrate_priority <
           b.capacity_bps / b.bitrate_priority;
  });
  size_t i;
  for (i = 0; i < priority_rate_observers.size(); ++i) {
    const auto& priority_rate_observer = priority_rate_observers[i];

    double observer_share =
        priority_rate_observer.bitrate_priority / bitrate_priority_sum;
    double allocation_bps = observer_share * remaining_bitrate;
    bool enough_bitrate = allocation_bps >= priority_rate_observer.capacity_bps;
    if (!enough_bitrate)
      break;
    allocation->at(priority_rate_observer.allocation_key) +=
        priority_rate_observer.capacity_bps;
    remaining_bitrate -= priority_rate_observer.capacity_bps;
    bitrate_priority_sum -= priority_rate_observer.bitrate_priority;
  }

  for (; i < priority_rate_observers.size(); ++i) {
    const auto& priority_rate_observer = priority_rate_observers[i];
    double fraction_allocated =
        priority_rate_observer.bitrate_priority / bitrate_priority_sum;
    allocation->at(priority_rate_observer.allocation_key) +=
        fraction_allocated * remaining_bitrate;
  }
}

std::map<BitrateAllocatorObserver*, int> LowRateAllocation(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate) {
  std::map<BitrateAllocatorObserver*, int> allocation;

  int64_t remaining_bitrate = bitrate;
  for (const auto& observer_config : allocatable_tracks) {
    int32_t allocated_bitrate = 0;
    if (observer_config.config.enforce_min_bitrate)
      allocated_bitrate = observer_config.config.min_bitrate_bps;

    allocation[observer_config.observer] = allocated_bitrate;
    remaining_bitrate -= allocated_bitrate;
  }

  if (remaining_bitrate > 0) {
    for (const auto& observer_config : allocatable_tracks) {
      if (observer_config.config.enforce_min_bitrate ||
          observer_config.LastAllocatedBitrate() == 0)
        continue;

      uint32_t required_bitrate = observer_config.MinBitrateWithHysteresis();
      if (remaining_bitrate >= required_bitrate) {
        allocation[observer_config.observer] = required_bitrate;
        remaining_bitrate -= required_bitrate;
      }
    }
  }

  if (remaining_bitrate > 0) {
    for (const auto& observer_config : allocatable_tracks) {
      if (observer_config.LastAllocatedBitrate() != 0)
        continue;

      uint32_t required_bitrate = observer_config.MinBitrateWithHysteresis();
      if (remaining_bitrate >= required_bitrate) {
        allocation[observer_config.observer] = required_bitrate;
        remaining_bitrate -= required_bitrate;
      }
    }
  }

  if (remaining_bitrate > 0)
    DistributeBitrateEvenly(allocatable_tracks, remaining_bitrate, false, 1,
                            &allocation);

  RTC_DCHECK_EQ(allocation.size(), allocatable_tracks.size());
  return allocation;
}

std::map<BitrateAllocatorObserver*, int> NormalRateAllocation(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    uint32_t sum_min_bitrates) {
  std::map<BitrateAllocatorObserver*, int> allocation;
  std::map<BitrateAllocatorObserver*, int> observers_capacities;
  for (const auto& observer_config : allocatable_tracks) {
    allocation[observer_config.observer] =
        observer_config.config.min_bitrate_bps;
    observers_capacities[observer_config.observer] =
        observer_config.config.max_bitrate_bps -
        observer_config.config.min_bitrate_bps;
  }

  bitrate -= sum_min_bitrates;

  for (const auto& observer_config : allocatable_tracks) {
    int64_t priority_margin = observer_config.config.priority_bitrate_bps -
                              allocation[observer_config.observer];
    if (priority_margin > 0 && bitrate > 0) {
      int64_t extra_bitrate = std::min<int64_t>(priority_margin, bitrate);
      allocation[observer_config.observer] +=
          rtc::dchecked_cast<int>(extra_bitrate);
      observers_capacities[observer_config.observer] -= extra_bitrate;
      bitrate -= extra_bitrate;
    }
  }

  if (bitrate > 0)
    DistributeBitrateRelatively(allocatable_tracks, bitrate,
                                observers_capacities, &allocation);

  return allocation;
}

std::map<BitrateAllocatorObserver*, int> MaxRateAllocation(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate,
    uint32_t sum_max_bitrates) {
  std::map<BitrateAllocatorObserver*, int> allocation;

  for (const auto& observer_config : allocatable_tracks) {
    allocation[observer_config.observer] =
        observer_config.config.max_bitrate_bps;
    bitrate -= observer_config.config.max_bitrate_bps;
  }
  DistributeBitrateEvenly(allocatable_tracks, bitrate, true,
                          kTransmissionMaxBitrateMultiplier, &allocation);
  return allocation;
}

std::map<BitrateAllocatorObserver*, int> ZeroRateAllocation(
    const std::vector<AllocatableTrack>& allocatable_tracks) {
  std::map<BitrateAllocatorObserver*, int> allocation;
  for (const auto& observer_config : allocatable_tracks)
    allocation[observer_config.observer] = 0;
  return allocation;
}

std::map<BitrateAllocatorObserver*, int> AllocateBitrates(
    const std::vector<AllocatableTrack>& allocatable_tracks,
    uint32_t bitrate) {
  if (allocatable_tracks.empty())
    return std::map<BitrateAllocatorObserver*, int>();

  if (bitrate == 0)
    return ZeroRateAllocation(allocatable_tracks);

  uint32_t sum_min_bitrates = 0;
  uint32_t sum_max_bitrates = 0;
  for (const auto& observer_config : allocatable_tracks) {
    sum_min_bitrates += observer_config.config.min_bitrate_bps;
    sum_max_bitrates += observer_config.config.max_bitrate_bps;
  }

  if (!EnoughBitrateForAllObservers(allocatable_tracks, bitrate,
                                    sum_min_bitrates))
    return LowRateAllocation(allocatable_tracks, bitrate);

  if (bitrate <= sum_max_bitrates)
    return NormalRateAllocation(allocatable_tracks, bitrate, sum_min_bitrates);

  return MaxRateAllocation(allocatable_tracks, bitrate, sum_max_bitrates);
}

}

BitrateAllocator::BitrateAllocator(LimitObserver* limit_observer)
    : limit_observer_(limit_observer),
      last_target_bps_(0),
      last_stable_target_bps_(0),
      last_non_zero_bitrate_bps_(kDefaultBitrateBps),
      last_fraction_loss_(0),
      last_rtt_(0),
      last_bwe_period_ms_(1000),
      num_pause_events_(0),
      last_bwe_log_time_(0) {
  sequenced_checker_.Detach();
}

BitrateAllocator::~BitrateAllocator() {
  RTC_HISTOGRAM_COUNTS_100("WebRTC.Call.NumberOfPauseEvents",
                           num_pause_events_);
}

void BitrateAllocator::UpdateStartRate(uint32_t start_rate_bps) {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  last_non_zero_bitrate_bps_ = start_rate_bps;
}

void BitrateAllocator::OnNetworkEstimateChanged(TargetTransferRate msg) {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  last_target_bps_ = msg.target_rate.bps();
  last_stable_target_bps_ = msg.stable_target_rate.bps();
  last_non_zero_bitrate_bps_ =
      last_target_bps_ > 0 ? last_target_bps_ : last_non_zero_bitrate_bps_;

  int loss_ratio_255 = msg.network_estimate.loss_rate_ratio * 255;
  last_fraction_loss_ =
      rtc::dchecked_cast<uint8_t>(rtc::SafeClamp(loss_ratio_255, 0, 255));
  last_rtt_ = msg.network_estimate.round_trip_time.ms();
  last_bwe_period_ms_ = msg.network_estimate.bwe_period.ms();

  int64_t now = msg.at_time.ms();
  if (now > last_bwe_log_time_ + kBweLogIntervalMs) {
    RTC_LOG(LS_INFO) << "Current BWE " << last_target_bps_;
    last_bwe_log_time_ = now;
  }

  auto allocation = AllocateBitrates(allocatable_tracks_, last_target_bps_);
  auto stable_bitrate_allocation =
      AllocateBitrates(allocatable_tracks_, last_stable_target_bps_);

  for (auto& config : allocatable_tracks_) {
    uint32_t allocated_bitrate = allocation[config.observer];
    uint32_t allocated_stable_target_rate =
        stable_bitrate_allocation[config.observer];
    BitrateAllocationUpdate update;
    update.target_bitrate = DataRate::BitsPerSec(allocated_bitrate);
    update.stable_target_bitrate =
        DataRate::BitsPerSec(allocated_stable_target_rate);
    update.packet_loss_ratio = last_fraction_loss_ / 256.0;
    update.round_trip_time = TimeDelta::Millis(last_rtt_);
    update.bwe_period = TimeDelta::Millis(last_bwe_period_ms_);
    update.cwnd_reduce_ratio = msg.cwnd_reduce_ratio;
    uint32_t protection_bitrate = config.observer->OnBitrateUpdated(update);

    if (allocated_bitrate == 0 && config.allocated_bitrate_bps > 0) {
      if (last_target_bps_ > 0)
        ++num_pause_events_;

      uint32_t predicted_protection_bps =
          (1.0 - config.media_ratio) * config.config.min_bitrate_bps;
      RTC_LOG(LS_INFO) << "Pausing observer " << config.observer
                       << " with configured min bitrate "
                       << config.config.min_bitrate_bps
                       << " and current estimate of " << last_target_bps_
                       << " and protection bitrate "
                       << predicted_protection_bps;
    } else if (allocated_bitrate > 0 && config.allocated_bitrate_bps == 0) {
      if (last_target_bps_ > 0)
        ++num_pause_events_;
      RTC_LOG(LS_INFO) << "Resuming observer " << config.observer
                       << ", configured min bitrate "
                       << config.config.min_bitrate_bps
                       << ", current allocation " << allocated_bitrate
                       << " and protection bitrate " << protection_bitrate;
    }

    if (allocated_bitrate > 0)
      config.media_ratio = MediaRatio(allocated_bitrate, protection_bitrate);
    config.allocated_bitrate_bps = allocated_bitrate;
  }
  UpdateAllocationLimits();
}

void BitrateAllocator::AddObserver(BitrateAllocatorObserver* observer,
                                   MediaStreamAllocationConfig config) {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  RTC_DCHECK_GT(config.bitrate_priority, 0);
  RTC_DCHECK(std::isnormal(config.bitrate_priority));
  auto it = absl::c_find_if(
      allocatable_tracks_,
      [observer](const auto& config) { return config.observer == observer; });

  if (it != allocatable_tracks_.end()) {
    it->config = config;
  } else {
    allocatable_tracks_.push_back(AllocatableTrack(observer, config));
  }

  if (last_target_bps_ > 0) {

    auto allocation = AllocateBitrates(allocatable_tracks_, last_target_bps_);
    auto stable_bitrate_allocation =
        AllocateBitrates(allocatable_tracks_, last_stable_target_bps_);
    for (auto& config : allocatable_tracks_) {
      uint32_t allocated_bitrate = allocation[config.observer];
      uint32_t allocated_stable_bitrate =
          stable_bitrate_allocation[config.observer];
      BitrateAllocationUpdate update;
      update.target_bitrate = DataRate::BitsPerSec(allocated_bitrate);
      update.stable_target_bitrate =
          DataRate::BitsPerSec(allocated_stable_bitrate);
      update.packet_loss_ratio = last_fraction_loss_ / 256.0;
      update.round_trip_time = TimeDelta::Millis(last_rtt_);
      update.bwe_period = TimeDelta::Millis(last_bwe_period_ms_);
      uint32_t protection_bitrate = config.observer->OnBitrateUpdated(update);
      config.allocated_bitrate_bps = allocated_bitrate;
      if (allocated_bitrate > 0)
        config.media_ratio = MediaRatio(allocated_bitrate, protection_bitrate);
    }
  } else {

    BitrateAllocationUpdate update;
    update.target_bitrate = DataRate::Zero();
    update.stable_target_bitrate = DataRate::Zero();
    update.packet_loss_ratio = last_fraction_loss_ / 256.0;
    update.round_trip_time = TimeDelta::Millis(last_rtt_);
    update.bwe_period = TimeDelta::Millis(last_bwe_period_ms_);
    observer->OnBitrateUpdated(update);
  }
  UpdateAllocationLimits();
}

void BitrateAllocator::UpdateAllocationLimits() {
  BitrateAllocationLimits limits;
  for (const auto& config : allocatable_tracks_) {
    uint32_t stream_padding = config.config.pad_up_bitrate_bps;
    if (config.config.enforce_min_bitrate) {
      limits.min_allocatable_rate +=
          DataRate::BitsPerSec(config.config.min_bitrate_bps);
    } else if (config.allocated_bitrate_bps == 0) {
      stream_padding =
          std::max(config.MinBitrateWithHysteresis(), stream_padding);
    }
    limits.max_padding_rate += DataRate::BitsPerSec(stream_padding);
    limits.max_allocatable_rate +=
        DataRate::BitsPerSec(config.config.max_bitrate_bps);
  }

  if (limits.min_allocatable_rate == current_limits_.min_allocatable_rate &&
      limits.max_allocatable_rate == current_limits_.max_allocatable_rate &&
      limits.max_padding_rate == current_limits_.max_padding_rate) {
    return;
  }
  current_limits_ = limits;

  RTC_LOG(LS_INFO) << "UpdateAllocationLimits : total_requested_min_bitrate: "
                   << ToString(limits.min_allocatable_rate)
                   << ", total_requested_padding_bitrate: "
                   << ToString(limits.max_padding_rate)
                   << ", total_requested_max_bitrate: "
                   << ToString(limits.max_allocatable_rate);

  limit_observer_->OnAllocationLimitsChanged(limits);
}

void BitrateAllocator::RemoveObserver(BitrateAllocatorObserver* observer) {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  for (auto it = allocatable_tracks_.begin(); it != allocatable_tracks_.end();
       ++it) {
    if (it->observer == observer) {
      allocatable_tracks_.erase(it);
      break;
    }
  }

  UpdateAllocationLimits();
}

int BitrateAllocator::GetStartBitrate(
    BitrateAllocatorObserver* observer) const {
  RTC_DCHECK_RUN_ON(&sequenced_checker_);
  auto it = absl::c_find_if(
      allocatable_tracks_,
      [observer](const auto& config) { return config.observer == observer; });
  if (it == allocatable_tracks_.end()) {

    return last_non_zero_bitrate_bps_ /
           static_cast<int>((allocatable_tracks_.size() + 1));
  } else if (it->allocated_bitrate_bps == -1) {

    return last_non_zero_bitrate_bps_ /
           static_cast<int>(allocatable_tracks_.size());
  } else {

    return it->allocated_bitrate_bps;
  }
}

uint32_t bitrate_allocator_impl::AllocatableTrack::LastAllocatedBitrate()
    const {

  return allocated_bitrate_bps == -1 ? config.min_bitrate_bps
                                     : allocated_bitrate_bps;
}

uint32_t bitrate_allocator_impl::AllocatableTrack::MinBitrateWithHysteresis()
    const {
  uint32_t min_bitrate = config.min_bitrate_bps;
  if (LastAllocatedBitrate() == 0) {
    min_bitrate += std::max(static_cast<uint32_t>(kToggleFactor * min_bitrate),
                            kMinToggleBitrateBps);
  }

  if (media_ratio > 0.0 && media_ratio < 1.0)
    min_bitrate += min_bitrate * (1.0 - media_ratio);

  return min_bitrate;
}

}
