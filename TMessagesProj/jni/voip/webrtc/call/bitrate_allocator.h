/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_BITRATE_ALLOCATOR_H_
#define CALL_BITRATE_ALLOCATOR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/call/bitrate_allocation.h"
#include "api/sequence_checker.h"
#include "api/transport/network_types.h"
#include "rtc_base/system/no_unique_address.h"

namespace webrtc {

class Clock;

class BitrateAllocatorObserver {
 public:

  virtual uint32_t OnBitrateUpdated(BitrateAllocationUpdate update) = 0;

 protected:
  virtual ~BitrateAllocatorObserver() {}
};

struct MediaStreamAllocationConfig {

  uint32_t min_bitrate_bps;

  uint32_t max_bitrate_bps;
  uint32_t pad_up_bitrate_bps;
  int64_t priority_bitrate_bps;

  bool enforce_min_bitrate;

  double bitrate_priority;
};

class BitrateAllocatorInterface {
 public:
  virtual void AddObserver(BitrateAllocatorObserver* observer,
                           MediaStreamAllocationConfig config) = 0;
  virtual void RemoveObserver(BitrateAllocatorObserver* observer) = 0;
  virtual int GetStartBitrate(BitrateAllocatorObserver* observer) const = 0;

 protected:
  virtual ~BitrateAllocatorInterface() = default;
};

namespace bitrate_allocator_impl {
struct AllocatableTrack {
  AllocatableTrack(BitrateAllocatorObserver* observer,
                   MediaStreamAllocationConfig allocation_config)
      : observer(observer),
        config(allocation_config),
        allocated_bitrate_bps(-1),
        media_ratio(1.0) {}
  BitrateAllocatorObserver* observer;
  MediaStreamAllocationConfig config;
  int64_t allocated_bitrate_bps;
  double media_ratio;

  uint32_t LastAllocatedBitrate() const;

  uint32_t MinBitrateWithHysteresis() const;
};
}

class BitrateAllocator : public BitrateAllocatorInterface {
 public:

  class LimitObserver {
   public:
    virtual void OnAllocationLimitsChanged(BitrateAllocationLimits limits) = 0;

   protected:
    virtual ~LimitObserver() = default;
  };

  explicit BitrateAllocator(LimitObserver* limit_observer);
  ~BitrateAllocator() override;

  void UpdateStartRate(uint32_t start_rate_bps);

  void OnNetworkEstimateChanged(TargetTransferRate msg);

  void AddObserver(BitrateAllocatorObserver* observer,
                   MediaStreamAllocationConfig config) override;

  void RemoveObserver(BitrateAllocatorObserver* observer) override;

  int GetStartBitrate(BitrateAllocatorObserver* observer) const override;

 private:
  using AllocatableTrack = bitrate_allocator_impl::AllocatableTrack;

  void UpdateAllocationLimits() RTC_RUN_ON(&sequenced_checker_);

  static uint8_t GetTransmissionMaxBitrateMultiplier();

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequenced_checker_;
  LimitObserver* const limit_observer_ RTC_GUARDED_BY(&sequenced_checker_);

  std::vector<AllocatableTrack> allocatable_tracks_
      RTC_GUARDED_BY(&sequenced_checker_);
  uint32_t last_target_bps_ RTC_GUARDED_BY(&sequenced_checker_);
  uint32_t last_stable_target_bps_ RTC_GUARDED_BY(&sequenced_checker_);
  uint32_t last_non_zero_bitrate_bps_ RTC_GUARDED_BY(&sequenced_checker_);
  uint8_t last_fraction_loss_ RTC_GUARDED_BY(&sequenced_checker_);
  int64_t last_rtt_ RTC_GUARDED_BY(&sequenced_checker_);
  int64_t last_bwe_period_ms_ RTC_GUARDED_BY(&sequenced_checker_);

  int num_pause_events_ RTC_GUARDED_BY(&sequenced_checker_);
  int64_t last_bwe_log_time_ RTC_GUARDED_BY(&sequenced_checker_);
  BitrateAllocationLimits current_limits_ RTC_GUARDED_BY(&sequenced_checker_);
};

}
#endif
