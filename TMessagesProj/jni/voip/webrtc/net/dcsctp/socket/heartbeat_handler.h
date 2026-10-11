/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_HEARTBEAT_HANDLER_H_
#define NET_DCSCTP_SOCKET_HEARTBEAT_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "net/dcsctp/packet/chunk/heartbeat_ack_chunk.h"
#include "net/dcsctp/packet/chunk/heartbeat_request_chunk.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/socket/context.h"
#include "net/dcsctp/timer/timer.h"

namespace dcsctp {

class HeartbeatHandler {
 public:
  HeartbeatHandler(absl::string_view log_prefix,
                   const DcSctpOptions& options,
                   Context* context,
                   TimerManager* timer_manager);

  void RestartTimer();

  void HandleHeartbeatRequest(HeartbeatRequestChunk chunk);

  void HandleHeartbeatAck(HeartbeatAckChunk chunk);

 private:
  webrtc::TimeDelta OnIntervalTimerExpiry();
  webrtc::TimeDelta OnTimeoutTimerExpiry();

  const absl::string_view log_prefix_;
  Context* ctx_;
  TimerManager* timer_manager_;

  const webrtc::TimeDelta interval_duration_;

  const bool interval_duration_should_include_rtt_;
  const std::unique_ptr<Timer> interval_timer_;
  const std::unique_ptr<Timer> timeout_timer_;
};
}

#endif
