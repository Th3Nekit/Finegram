/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_CONTEXT_H_
#define NET_DCSCTP_SOCKET_CONTEXT_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "api/units/time_delta.h"
#include "net/dcsctp/common/internal_types.h"
#include "net/dcsctp/packet/sctp_packet.h"
#include "net/dcsctp/public/dcsctp_socket.h"
#include "net/dcsctp/public/types.h"

namespace dcsctp {

class Context {
 public:
  virtual ~Context() = default;

  virtual bool is_connection_established() const = 0;

  virtual TSN my_initial_tsn() const = 0;

  virtual TSN peer_initial_tsn() const = 0;

  virtual DcSctpSocketCallbacks& callbacks() const = 0;

  virtual void ObserveRTT(webrtc::TimeDelta rtt_ms) = 0;

  virtual webrtc::TimeDelta current_rto() const = 0;

  virtual bool IncrementTxErrorCounter(absl::string_view reason) = 0;

  virtual void ClearTxErrorCounter() = 0;

  virtual bool HasTooManyTxErrors() const = 0;

  virtual SctpPacket::Builder PacketBuilder() const = 0;

  virtual void Send(SctpPacket::Builder& builder) = 0;
};

}

#endif
