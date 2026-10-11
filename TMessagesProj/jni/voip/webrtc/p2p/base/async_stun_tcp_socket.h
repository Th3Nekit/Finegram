/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_ASYNC_STUN_TCP_SOCKET_H_
#define P2P_BASE_ASYNC_STUN_TCP_SOCKET_H_

#include <stddef.h>

#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_tcp_socket.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"

namespace cricket {

class AsyncStunTCPSocket : public rtc::AsyncTCPSocketBase {
 public:

  static AsyncStunTCPSocket* Create(rtc::Socket* socket,
                                    const rtc::SocketAddress& bind_address,
                                    const rtc::SocketAddress& remote_address);

  explicit AsyncStunTCPSocket(rtc::Socket* socket);

  AsyncStunTCPSocket(const AsyncStunTCPSocket&) = delete;
  AsyncStunTCPSocket& operator=(const AsyncStunTCPSocket&) = delete;

  int Send(const void* pv,
           size_t cb,
           const rtc::PacketOptions& options) override;
  size_t ProcessInput(rtc::ArrayView<const uint8_t> data) override;

 private:

  size_t GetExpectedLength(const void* data, size_t len, int* pad_bytes);
};

}

#endif
