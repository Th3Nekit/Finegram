/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef CALL_SIMULATED_NETWORK_H_
#define CALL_SIMULATED_NETWORK_H_

#include <stdint.h>

#include <deque>
#include <queue>
#include <vector>

#include "absl/types/optional.h"
#include "api/sequence_checker.h"
#include "api/test/simulated_network.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/random.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class RTC_EXPORT SimulatedNetwork : public SimulatedNetworkInterface {
 public:
  using Config = BuiltInNetworkBehaviorConfig;
  explicit SimulatedNetwork(Config config, uint64_t random_seed = 1);
  ~SimulatedNetwork() override;

  void SetConfig(const Config& config) override;
  void UpdateConfig(std::function<void(BuiltInNetworkBehaviorConfig*)>
                        config_modifier) override;
  void PauseTransmissionUntil(int64_t until_us) override;

  bool EnqueuePacket(PacketInFlightInfo packet) override;
  std::vector<PacketDeliveryInfo> DequeueDeliverablePackets(
      int64_t receive_time_us) override;

  absl::optional<int64_t> NextDeliveryTimeUs() const override;

 private:
  struct PacketInfo {
    PacketInFlightInfo packet;

    int64_t arrival_time_us;
  };

  struct ConfigState {

    Config config;

    double prob_loss_bursting;

    double prob_start_bursting;

    int64_t pause_transmission_until_us = 0;
  };

  void UpdateCapacityQueue(ConfigState state, int64_t time_now_us)
      RTC_RUN_ON(&process_checker_);
  ConfigState GetConfigState() const;

  mutable Mutex config_lock_;

  rtc::RaceChecker process_checker_;

  std::queue<PacketInfo> capacity_link_ RTC_GUARDED_BY(process_checker_);

  std::deque<PacketInfo> delay_link_ RTC_GUARDED_BY(process_checker_);

  absl::optional<int64_t> next_process_time_us_
      RTC_GUARDED_BY(process_checker_);

  ConfigState config_state_ RTC_GUARDED_BY(config_lock_);

  Random random_ RTC_GUARDED_BY(process_checker_);

  bool bursting_;

  int64_t last_enqueue_time_us_;

  int64_t last_capacity_link_exit_time_;
};

}

#endif
