/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_STUN_SERVER_H_
#define P2P_BASE_STUN_SERVER_H_

#include <stddef.h>

#include <memory>

#include "absl/strings/string_view.h"
#include "api/transport/stun.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/socket_address.h"

namespace cricket {

const int STUN_SERVER_PORT = 3478;

class StunServer {
 public:

  explicit StunServer(rtc::AsyncUDPSocket* socket);

  virtual ~StunServer();

 protected:

  void OnPacket(rtc::AsyncPacketSocket* socket,
                const rtc::ReceivedPacket& packet);

  virtual void OnBindingRequest(StunMessage* msg,
                                const rtc::SocketAddress& addr);
  void OnAllocateRequest(StunMessage* msg, const rtc::SocketAddress& addr);
  void OnSharedSecretRequest(StunMessage* msg, const rtc::SocketAddress& addr);
  void OnSendRequest(StunMessage* msg, const rtc::SocketAddress& addr);

  void SendErrorResponse(const StunMessage& msg,
                         const rtc::SocketAddress& addr,
                         int error_code,
                         absl::string_view error_desc);

  void SendResponse(const StunMessage& msg, const rtc::SocketAddress& addr);

  void GetStunBindResponse(StunMessage* message,
                           const rtc::SocketAddress& remote_addr,
                           StunMessage* response) const;

 private:
  webrtc::SequenceChecker sequence_checker_;
  std::unique_ptr<rtc::AsyncUDPSocket> socket_;
};

}

#endif
