/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_TEST_CLIENT_H_
#define RTC_BASE_TEST_CLIENT_H_

#include <memory>
#include <vector>

#include "api/units/timestamp.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/buffer.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/synchronization/mutex.h"

namespace rtc {

class TestClient : public sigslot::has_slots<> {
 public:

  struct Packet {
    Packet(const rtc::ReceivedPacket& received_packet);
    Packet(const Packet& p);

    SocketAddress addr;
    Buffer buf;
    absl::optional<webrtc::Timestamp> packet_time;
  };

  static const int kTimeoutMs = 5000;

  explicit TestClient(std::unique_ptr<AsyncPacketSocket> socket);

  TestClient(std::unique_ptr<AsyncPacketSocket> socket,
             ThreadProcessingFakeClock* fake_clock);
  ~TestClient() override;

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  SocketAddress address() const { return socket_->GetLocalAddress(); }
  SocketAddress remote_address() const { return socket_->GetRemoteAddress(); }

  bool CheckConnState(AsyncPacketSocket::State state);

  bool CheckConnected() {
    return CheckConnState(AsyncPacketSocket::STATE_CONNECTED);
  }

  int Send(const char* buf, size_t size);

  int SendTo(const char* buf, size_t size, const SocketAddress& dest);

  std::unique_ptr<Packet> NextPacket(int timeout_ms);

  bool CheckNextPacket(const char* buf, size_t len, SocketAddress* addr);

  bool CheckNoPacket();

  int GetError();
  int SetOption(Socket::Option opt, int value);

  bool ready_to_send() const { return ready_to_send_count() > 0; }

  int ready_to_send_count() const { return ready_to_send_count_; }

 private:

  static const int kNoPacketTimeoutMs = 1000;

  Socket::ConnState GetState();

  void OnPacket(AsyncPacketSocket* socket,
                const rtc::ReceivedPacket& received_packet);
  void OnReadyToSend(AsyncPacketSocket* socket);
  bool CheckTimestamp(absl::optional<webrtc::Timestamp> packet_timestamp);
  void AdvanceTime(int ms);

  ThreadProcessingFakeClock* fake_clock_ = nullptr;
  webrtc::Mutex mutex_;
  std::unique_ptr<AsyncPacketSocket> socket_;
  std::vector<std::unique_ptr<Packet>> packets_;
  int ready_to_send_count_ = 0;
  absl::optional<webrtc::Timestamp> prev_packet_timestamp_;
};

}

#endif
