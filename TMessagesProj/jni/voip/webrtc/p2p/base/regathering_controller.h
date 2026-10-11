/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_REGATHERING_CONTROLLER_H_
#define P2P_BASE_REGATHERING_CONTROLLER_H_

#include <memory>

#include "api/task_queue/pending_task_safety_flag.h"
#include "p2p/base/ice_transport_internal.h"
#include "p2p/base/port_allocator.h"
#include "rtc_base/thread.h"

namespace webrtc {

class BasicRegatheringController : public sigslot::has_slots<> {
 public:
  struct Config {
    int regather_on_failed_networks_interval =
        cricket::REGATHER_ON_FAILED_NETWORKS_INTERVAL;
  };

  BasicRegatheringController() = delete;
  BasicRegatheringController(const Config& config,
                             cricket::IceTransportInternal* ice_transport,
                             rtc::Thread* thread);
  ~BasicRegatheringController() override;

  void Start();
  void set_allocator_session(cricket::PortAllocatorSession* allocator_session) {
    allocator_session_ = allocator_session;
  }

  void SetConfig(const Config& config);

 private:

  void OnIceTransportStateChanged(cricket::IceTransportInternal*) {}
  void OnIceTransportWritableState(rtc::PacketTransportInternal*) {}
  void OnIceTransportReceivingState(rtc::PacketTransportInternal*) {}
  void OnIceTransportNetworkRouteChanged(absl::optional<rtc::NetworkRoute>) {}

  void ScheduleRecurringRegatheringOnFailedNetworks();

  void CancelScheduledRecurringRegatheringOnAllNetworks();

  std::unique_ptr<ScopedTaskSafety> pending_regathering_;
  Config config_;
  cricket::IceTransportInternal* ice_transport_;
  cricket::PortAllocatorSession* allocator_session_ = nullptr;
  rtc::Thread* const thread_;
};

}

#endif
