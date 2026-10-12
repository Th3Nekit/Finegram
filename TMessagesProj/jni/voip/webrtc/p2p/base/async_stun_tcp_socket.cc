/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/async_stun_tcp_socket.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <cstddef>
#include <cstdint>

#include "api/array_view.h"
#include "api/transport/stun.h"
#include "api/units/timestamp.h"
#include "rtc_base/byte_order.h"
#include "rtc_base/checks.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/network/sent_packet.h"
#include "rtc_base/time_utils.h"

namespace cricket {

static const size_t kMaxPacketSize = 64 * 1024;

typedef uint16_t PacketLength;
static const size_t kPacketLenSize = sizeof(PacketLength);
static const size_t kPacketLenOffset = 2;
static const size_t kBufSize = kMaxPacketSize + kStunHeaderSize;
static const size_t kTurnChannelDataHdrSize = 4;

inline bool IsStunMessage(uint16_t msg_type) {

  return (msg_type & 0xC000) ? false : true;
}

AsyncStunTCPSocket* AsyncStunTCPSocket::Create(
    rtc::Socket* socket,
    const rtc::SocketAddress& bind_address,
    const rtc::SocketAddress& remote_address) {
  return new AsyncStunTCPSocket(
      AsyncTCPSocketBase::ConnectSocket(socket, bind_address, remote_address));
}

AsyncStunTCPSocket::AsyncStunTCPSocket(rtc::Socket* socket)
    : rtc::AsyncTCPSocketBase(socket, kBufSize) {}

int AsyncStunTCPSocket::Send(const void* pv,
                             size_t cb,
                             const rtc::PacketOptions& options) {
  if (cb > kBufSize || cb < kPacketLenSize + kPacketLenOffset) {
    SetError(EMSGSIZE);
    return -1;
  }

  if (!IsOutBufferEmpty())
    return static_cast<int>(cb);

  int pad_bytes;
  size_t expected_pkt_len = GetExpectedLength(pv, cb, &pad_bytes);

  if (cb != expected_pkt_len)
    return -1;

  AppendToOutBuffer(pv, cb);

  RTC_DCHECK(pad_bytes < 4);
  char padding[4] = {0};
  AppendToOutBuffer(padding, pad_bytes);

  int res = FlushOutBuffer();
  if (res <= 0) {

    ClearOutBuffer();
    return res;
  }

  rtc::SentPacket sent_packet(options.packet_id, rtc::TimeMillis());
  SignalSentPacket(this, sent_packet);

  return static_cast<int>(cb);
}

size_t AsyncStunTCPSocket::ProcessInput(rtc::ArrayView<const uint8_t> data) {
  rtc::SocketAddress remote_addr(GetRemoteAddress());

  size_t processed_bytes = 0;
  while (true) {
    size_t bytes_left = data.size() - processed_bytes;

    if (bytes_left < kPacketLenOffset + kPacketLenSize)
      return processed_bytes;

    int pad_bytes;
    size_t expected_pkt_len = GetExpectedLength(data.data() + processed_bytes,
                                                bytes_left, &pad_bytes);
    size_t actual_length = expected_pkt_len + pad_bytes;

    if (bytes_left < actual_length) {
      return processed_bytes;
    }

    rtc::ReceivedPacket received_packet(
        data.subview(processed_bytes, expected_pkt_len), remote_addr,
        webrtc::Timestamp::Micros(rtc::TimeMicros()));
    NotifyPacketReceived(received_packet);
    processed_bytes += actual_length;
  }
}

size_t AsyncStunTCPSocket::GetExpectedLength(const void* data,
                                             size_t len,
                                             int* pad_bytes) {
  *pad_bytes = 0;
  PacketLength pkt_len =
      rtc::GetBE16(static_cast<const char*>(data) + kPacketLenOffset);
  size_t expected_pkt_len;
  uint16_t msg_type = rtc::GetBE16(data);
  if (IsStunMessage(msg_type)) {

    expected_pkt_len = kStunHeaderSize + pkt_len;
  } else {

    expected_pkt_len = kTurnChannelDataHdrSize + pkt_len;

    if (expected_pkt_len % 4)
      *pad_bytes = 4 - (expected_pkt_len % 4);
  }
  return expected_pkt_len;
}

}
