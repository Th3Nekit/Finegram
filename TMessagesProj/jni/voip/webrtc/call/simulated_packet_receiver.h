/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_SIMULATED_PACKET_RECEIVER_H_
#define CALL_SIMULATED_PACKET_RECEIVER_H_

#include "api/test/simulated_network.h"
#include "call/packet_receiver.h"

namespace webrtc {

class SimulatedPacketReceiverInterface : public PacketReceiver {
 public:

  virtual void SetReceiver(PacketReceiver* receiver) = 0;

  virtual int AverageDelay() = 0;

  virtual void Process() = 0;

  virtual absl::optional<int64_t> TimeUntilNextProcess() = 0;
};

}

#endif
