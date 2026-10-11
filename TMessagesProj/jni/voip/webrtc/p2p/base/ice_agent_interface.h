/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ICE_AGENT_INTERFACE_H_
#define P2P_BASE_ICE_AGENT_INTERFACE_H_

#include "api/array_view.h"
#include "p2p/base/connection.h"
#include "p2p/base/ice_switch_reason.h"

namespace cricket {

class IceAgentInterface {
 public:
  virtual ~IceAgentInterface() = default;

  virtual int64_t GetLastPingSentMs() const = 0;

  virtual IceRole GetIceRole() const = 0;

  virtual void OnStartedPinging() = 0;

  virtual void UpdateConnectionStates() = 0;

  virtual void UpdateState() = 0;

  virtual void ForgetLearnedStateForConnections(
      rtc::ArrayView<const Connection* const> connections) = 0;

  virtual void SendPingRequest(const Connection* connection) = 0;

  virtual void SwitchSelectedConnection(const Connection* new_connection,
                                        IceSwitchReason reason) = 0;

  virtual bool PruneConnections(
      rtc::ArrayView<const Connection* const> connections) = 0;
};

}

#endif
