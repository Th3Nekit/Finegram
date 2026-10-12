/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TCP_PORT_H_
#define P2P_BASE_TCP_PORT_H_

#include <list>
#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "p2p/base/connection.h"
#include "p2p/base/port.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/containers/flat_map.h"
#include "rtc_base/network/received_packet.h"

namespace cricket {

class TCPConnection;

class TCPPort : public Port {
 public:
  static std::unique_ptr<TCPPort> Create(
      rtc::Thread* thread,
      rtc::PacketSocketFactory* factory,
      const rtc::Network* network,
      uint16_t min_port,
      uint16_t max_port,
      absl::string_view username,
      absl::string_view password,
      bool allow_listen,
      const webrtc::FieldTrialsView* field_trials = nullptr) {

    return absl::WrapUnique(new TCPPort(thread, factory, network, min_port,
                                        max_port, username, password,
                                        allow_listen, field_trials));
  }
  ~TCPPort() override;

  Connection* CreateConnection(const Candidate& address,
                               CandidateOrigin origin) override;

  void PrepareAddress() override;

  int GetOption(rtc::Socket::Option opt, int* value) override;
  int SetOption(rtc::Socket::Option opt, int value) override;
  int GetError() override;
  bool SupportsProtocol(absl::string_view protocol) const override;
  ProtocolType GetProtocol() const override;

 protected:
  TCPPort(rtc::Thread* thread,
          rtc::PacketSocketFactory* factory,
          const rtc::Network* network,
          uint16_t min_port,
          uint16_t max_port,
          absl::string_view username,
          absl::string_view password,
          bool allow_listen,
          const webrtc::FieldTrialsView* field_trials);

  int SendTo(const void* data,
             size_t size,
             const rtc::SocketAddress& addr,
             const rtc::PacketOptions& options,
             bool payload) override;

  void OnNewConnection(rtc::AsyncListenSocket* socket,
                       rtc::AsyncPacketSocket* new_socket);

 private:
  struct Incoming {
    rtc::SocketAddress addr;
    rtc::AsyncPacketSocket* socket;
  };

  void TryCreateServerSocket();

  rtc::AsyncPacketSocket* GetIncoming(const rtc::SocketAddress& addr,
                                      bool remove = false);

  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::ReceivedPacket& packet);

  void OnSentPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::SentPacket& sent_packet) override;

  void OnReadyToSend(rtc::AsyncPacketSocket* socket);

  bool allow_listen_;
  std::unique_ptr<rtc::AsyncListenSocket> listen_socket_;

  webrtc::flat_map<rtc::Socket::Option, int> socket_options_;

  int error_;
  std::list<Incoming> incoming_;

  friend class TCPConnection;
};

class TCPConnection : public Connection, public sigslot::has_slots<> {
 public:

  TCPConnection(rtc::WeakPtr<Port> tcp_port,
                const Candidate& candidate,
                rtc::AsyncPacketSocket* socket = nullptr);
  ~TCPConnection() override;

  int Send(const void* data,
           size_t size,
           const rtc::PacketOptions& options) override;
  int GetError() override;

  rtc::AsyncPacketSocket* socket() { return socket_.get(); }

  int reconnection_timeout() const { return reconnection_timeout_; }
  void set_reconnection_timeout(int timeout_in_ms) {
    reconnection_timeout_ = timeout_in_ms;
  }

 protected:

  void OnConnectionRequestResponse(StunRequest* req,
                                   StunMessage* response) override;

 private:
  friend class TCPPort;

  void MaybeReconnect();

  void CreateOutgoingTcpSocket() RTC_RUN_ON(network_thread());

  void ConnectSocketSignals(rtc::AsyncPacketSocket* socket)
      RTC_RUN_ON(network_thread());

  void DisconnectSocketSignals(rtc::AsyncPacketSocket* socket)
      RTC_RUN_ON(network_thread());

  void OnConnect(rtc::AsyncPacketSocket* socket);
  void OnClose(rtc::AsyncPacketSocket* socket, int error);
  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const rtc::ReceivedPacket& packet);
  void OnReadyToSend(rtc::AsyncPacketSocket* socket);
  void OnDestroyed(Connection* c);

  TCPPort* tcp_port() {
    RTC_DCHECK_EQ(port()->GetProtocol(), PROTO_TCP);
    return static_cast<TCPPort*>(port());
  }

  std::unique_ptr<rtc::AsyncPacketSocket> socket_;
  int error_;
  const bool outgoing_;

  bool connection_pending_;

  bool pretending_to_be_writable_;

  int reconnection_timeout_;

  webrtc::ScopedTaskSafety network_safety_;
};

}

#endif
