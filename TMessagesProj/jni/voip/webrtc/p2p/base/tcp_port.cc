/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  This is a diagram of how TCP reconnect works for the active side. The
 *  passive side just waits for an incoming connection.
 *
 *  - Connected: Indicate whether the TCP socket is connected.
 *
 *  - Writable: Whether the stun binding is completed. Sending a data packet
 *    before stun binding completed will trigger IPC socket layer to shutdown
 *    the connection.
 *
 *  - PendingTCP: `connection_pending_` indicates whether there is an
 *    outstanding TCP connection in progress.
 *
 *  - PretendWri: Tracked by `pretending_to_be_writable_`. Marking connection as
 *    WRITE_TIMEOUT will cause the connection be deleted. Instead, we're
 *    "pretending" we're still writable for a period of time such that reconnect
 *    could work.
 *
 *  Data could only be sent in state 3. Sening data during state 2 & 6 will get
 *  EWOULDBLOCK, 4 & 5 EPIPE.
 *
 *         OS Timeout         7 -------------+
 *   +----------------------->|Connected: N  |
 *   |                        |Writable:  N  |     Timeout
 *   |       Timeout          |Connection is |<----------------+
 *   |   +------------------->|Dead          |                 |
 *   |   |                    +--------------+                 |
 *   |   |                               ^                     |
 *   |   |            OnClose            |                     |
 *   |   |    +-----------------------+  |                     |
 *   |   |    |                       |  |Timeout              |
 *   |   |    v                       |  |                     |
 *   | 4 +----------+          5 -----+--+--+           6 -----+-----+
 *   | |Connected: N|Send() or |Connected: N|           |Connected: Y|
 *   | |Writable:  Y|Ping()    |Writable:  Y|OnConnect  |Writable:  Y|
 *   | |PendingTCP:N+--------> |PendingTCP:Y+---------> |PendingTCP:N|
 *   | |PretendWri:Y|          |PretendWri:Y|           |PretendWri:Y|
 *   | +-----+------+          +------------+           +---+--+-----+
 *   |   ^   ^                                              |  |
 *   |   |   |                     OnClose                  |  |
 *   |   |   +----------------------------------------------+  |
 *   |   |                                                     |
 *   |   |                              Stun Binding Completed |
 *   |   |                                                     |
 *   |   |                    OnClose                          |
 *   |   +------------------------------------------------+    |
 *   |                                                    |    v
 *  1 -----------+           2 -----------+Stun      3 -----------+
 *  |Connected: N|           |Connected: Y|Binding   |Connected: Y|
 *  |Writable:  N|OnConnect  |Writable:  N|Completed |Writable:  Y|
 *  |PendingTCP:Y+---------> |PendingTCP:N+--------> |PendingTCP:N|
 *  |PretendWri:N|           |PretendWri:N|          |PretendWri:N|
 *  +------------+           +------------+          +------------+
 *
 */

#include "p2p/base/tcp_port.h"

#include <errno.h>

#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/units/time_delta.h"
#include "p2p/base/p2p_constants.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/rate_tracker.h"
#include "rtc_base/thread.h"

namespace cricket {
using ::webrtc::SafeTask;
using ::webrtc::TimeDelta;

TCPPort::TCPPort(rtc::Thread* thread,
                 rtc::PacketSocketFactory* factory,
                 const rtc::Network* network,
                 uint16_t min_port,
                 uint16_t max_port,
                 absl::string_view username,
                 absl::string_view password,
                 bool allow_listen,
                 const webrtc::FieldTrialsView* field_trials)
    : Port(thread,
           LOCAL_PORT_TYPE,
           factory,
           network,
           min_port,
           max_port,
           username,
           password,
           field_trials),
      allow_listen_(allow_listen),
      error_(0) {

  if (allow_listen_) {
    TryCreateServerSocket();
  }

  SetOption(rtc::Socket::OPT_NODELAY, 1);
}

TCPPort::~TCPPort() {
  listen_socket_ = nullptr;
  std::list<Incoming>::iterator it;
  for (it = incoming_.begin(); it != incoming_.end(); ++it)
    delete it->socket;
  incoming_.clear();
}

Connection* TCPPort::CreateConnection(const Candidate& address,
                                      CandidateOrigin origin) {
  if (!SupportsProtocol(address.protocol())) {
    return NULL;
  }

  if ((address.tcptype() == TCPTYPE_ACTIVE_STR && !address.is_prflx()) ||
      (address.tcptype().empty() && address.address().port() == 0)) {

    return NULL;
  }

  if (origin == ORIGIN_OTHER_PORT)
    return NULL;

  if ((address.protocol() == SSLTCP_PROTOCOL_NAME) &&
      (origin == ORIGIN_THIS_PORT)) {
    return NULL;
  }

  if (!IsCompatibleAddress(address.address())) {
    return NULL;
  }

  TCPConnection* conn = NULL;
  if (rtc::AsyncPacketSocket* socket = GetIncoming(address.address(), true)) {

    socket->DeregisterReceivedPacketCallback();
    conn = new TCPConnection(NewWeakPtr(), address, socket);
  } else {

    conn = new TCPConnection(NewWeakPtr(), address);
    if (conn->socket()) {
      conn->socket()->SignalReadyToSend.connect(this, &TCPPort::OnReadyToSend);
      conn->socket()->SignalSentPacket.connect(this, &TCPPort::OnSentPacket);
    }
  }
  AddOrReplaceConnection(conn);
  return conn;
}

void TCPPort::PrepareAddress() {
  if (listen_socket_) {

    RTC_LOG(LS_VERBOSE) << "Preparing TCP address, current state: "
                        << static_cast<int>(listen_socket_->GetState());
    AddAddress(listen_socket_->GetLocalAddress(),
               listen_socket_->GetLocalAddress(), rtc::SocketAddress(),
               TCP_PROTOCOL_NAME, "", TCPTYPE_PASSIVE_STR, LOCAL_PORT_TYPE,
               ICE_TYPE_PREFERENCE_HOST_TCP, 0, "", true);
  } else {
    RTC_LOG(LS_INFO) << ToString()
                     << ": Not listening due to firewall restrictions.";

    AddAddress(rtc::SocketAddress(Network()->GetBestIP(), DISCARD_PORT),
               rtc::SocketAddress(Network()->GetBestIP(), 0),
               rtc::SocketAddress(), TCP_PROTOCOL_NAME, "", TCPTYPE_ACTIVE_STR,
               LOCAL_PORT_TYPE, ICE_TYPE_PREFERENCE_HOST_TCP, 0, "", true);
  }
}

int TCPPort::SendTo(const void* data,
                    size_t size,
                    const rtc::SocketAddress& addr,
                    const rtc::PacketOptions& options,
                    bool payload) {
  rtc::AsyncPacketSocket* socket = NULL;
  TCPConnection* conn = static_cast<TCPConnection*>(GetConnection(addr));

  if (conn) {
    if (!conn->connected()) {
      conn->MaybeReconnect();
      return SOCKET_ERROR;
    }
    socket = conn->socket();
    if (!socket) {

      RTC_LOG(LS_INFO) << ToString()
                       << ": Attempted to send to an uninitialized socket: "
                       << addr.ToSensitiveString();
      error_ = EHOSTUNREACH;
      return SOCKET_ERROR;
    }
  } else {
    socket = GetIncoming(addr);
    if (!socket) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Attempted to send to an unknown destination: "
                        << addr.ToSensitiveString();
      error_ = EHOSTUNREACH;
      return SOCKET_ERROR;
    }
  }
  rtc::PacketOptions modified_options(options);
  CopyPortInformationToPacketInfo(&modified_options.info_signaled_after_sent);
  int sent = socket->Send(data, size, modified_options);
  if (sent < 0) {
    error_ = socket->GetError();

    RTC_LOG(LS_ERROR) << ToString() << ": TCP send of " << size
                      << " bytes failed with error " << error_;
  }
  return sent;
}

int TCPPort::GetOption(rtc::Socket::Option opt, int* value) {
  auto const& it = socket_options_.find(opt);
  if (it == socket_options_.end()) {
    return -1;
  }
  *value = it->second;
  return 0;
}

int TCPPort::SetOption(rtc::Socket::Option opt, int value) {
  socket_options_[opt] = value;
  return 0;
}

int TCPPort::GetError() {
  return error_;
}

bool TCPPort::SupportsProtocol(absl::string_view protocol) const {
  return protocol == TCP_PROTOCOL_NAME || protocol == SSLTCP_PROTOCOL_NAME;
}

ProtocolType TCPPort::GetProtocol() const {
  return PROTO_TCP;
}

void TCPPort::OnNewConnection(rtc::AsyncListenSocket* socket,
                              rtc::AsyncPacketSocket* new_socket) {
  RTC_DCHECK_EQ(socket, listen_socket_.get());

  for (const auto& option : socket_options_) {
    new_socket->SetOption(option.first, option.second);
  }
  Incoming incoming;
  incoming.addr = new_socket->GetRemoteAddress();
  incoming.socket = new_socket;
  incoming.socket->RegisterReceivedPacketCallback(
      [&](rtc::AsyncPacketSocket* socket, const rtc::ReceivedPacket& packet) {
        OnReadPacket(socket, packet);
      });
  incoming.socket->SignalReadyToSend.connect(this, &TCPPort::OnReadyToSend);
  incoming.socket->SignalSentPacket.connect(this, &TCPPort::OnSentPacket);

  RTC_LOG(LS_VERBOSE) << ToString() << ": Accepted connection from "
                      << incoming.addr.ToSensitiveString();
  incoming_.push_back(incoming);
}

void TCPPort::TryCreateServerSocket() {
  listen_socket_ = absl::WrapUnique(socket_factory()->CreateServerTcpSocket(
      rtc::SocketAddress(Network()->GetBestIP(), 0), min_port(), max_port(),
      false          ));
  if (!listen_socket_) {
    RTC_LOG(LS_WARNING)
        << ToString()
        << ": TCP server socket creation failed; continuing anyway.";
    return;
  }
  listen_socket_->SignalNewConnection.connect(this, &TCPPort::OnNewConnection);
}

rtc::AsyncPacketSocket* TCPPort::GetIncoming(const rtc::SocketAddress& addr,
                                             bool remove) {
  rtc::AsyncPacketSocket* socket = NULL;
  for (std::list<Incoming>::iterator it = incoming_.begin();
       it != incoming_.end(); ++it) {
    if (it->addr == addr) {
      socket = it->socket;
      if (remove)
        incoming_.erase(it);
      break;
    }
  }
  return socket;
}

void TCPPort::OnReadPacket(rtc::AsyncPacketSocket* socket,
                           const rtc::ReceivedPacket& packet) {
  Port::OnReadPacket(packet, PROTO_TCP);
}

void TCPPort::OnSentPacket(rtc::AsyncPacketSocket* socket,
                           const rtc::SentPacket& sent_packet) {
  PortInterface::SignalSentPacket(sent_packet);
}

void TCPPort::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  Port::OnReadyToSend();
}

TCPConnection::TCPConnection(rtc::WeakPtr<Port> tcp_port,
                             const Candidate& candidate,
                             rtc::AsyncPacketSocket* socket)
    : Connection(std::move(tcp_port), 0, candidate),
      socket_(socket),
      error_(0),
      outgoing_(socket == NULL),
      connection_pending_(false),
      pretending_to_be_writable_(false),
      reconnection_timeout_(cricket::CONNECTION_WRITE_CONNECT_TIMEOUT) {
  RTC_DCHECK_RUN_ON(network_thread_);
  RTC_DCHECK_EQ(port()->GetProtocol(), PROTO_TCP);

  SignalDestroyed.connect(this, &TCPConnection::OnDestroyed);

  if (outgoing_) {
    CreateOutgoingTcpSocket();
  } else {

    RTC_LOG(LS_VERBOSE) << ToString() << ": socket ipaddr: "
                        << socket_->GetLocalAddress().ToSensitiveString()
                        << ", port() Network:" << port()->Network()->ToString();
    RTC_DCHECK(absl::c_any_of(
        port_->Network()->GetIPs(), [this](const rtc::InterfaceAddress& addr) {
          return socket_->GetLocalAddress().ipaddr() == addr;
        }));
    ConnectSocketSignals(socket);
  }
}

TCPConnection::~TCPConnection() {
  RTC_DCHECK_RUN_ON(network_thread_);
}

int TCPConnection::Send(const void* data,
                        size_t size,
                        const rtc::PacketOptions& options) {
  if (!socket_) {
    error_ = ENOTCONN;
    return SOCKET_ERROR;
  }

  if (!connected()) {
    MaybeReconnect();
    return SOCKET_ERROR;
  }

  if (pretending_to_be_writable_ || write_state() != STATE_WRITABLE) {

    error_ = ENOTCONN;
    return SOCKET_ERROR;
  }
  stats_.sent_total_packets++;
  rtc::PacketOptions modified_options(options);
  tcp_port()->CopyPortInformationToPacketInfo(
      &modified_options.info_signaled_after_sent);
  int sent = socket_->Send(data, size, modified_options);
  int64_t now = rtc::TimeMillis();
  if (sent < 0) {
    stats_.sent_discarded_packets++;
    error_ = socket_->GetError();
  } else {
    send_rate_tracker_.AddSamplesAtTime(now, sent);
  }
  last_send_data_ = now;
  return sent;
}

int TCPConnection::GetError() {
  return error_;
}

void TCPConnection::OnConnectionRequestResponse(StunRequest* req,
                                                StunMessage* response) {

  Connection::OnConnectionRequestResponse(req, response);

  if (pretending_to_be_writable_) {
    Connection::OnReadyToSend();
  }
  pretending_to_be_writable_ = false;
  RTC_DCHECK(write_state() == STATE_WRITABLE);
}

void TCPConnection::OnConnect(rtc::AsyncPacketSocket* socket) {
  RTC_DCHECK_EQ(socket, socket_.get());

  if (!port_) {
    RTC_LOG(LS_ERROR) << "TCPConnection: Port has been deleted.";
    return;
  }

  const rtc::SocketAddress& socket_address = socket->GetLocalAddress();
  if (absl::c_any_of(port_->Network()->GetIPs(),
                     [socket_address](const rtc::InterfaceAddress& addr) {
                       return socket_address.ipaddr() == addr;
                     })) {
    RTC_LOG(LS_VERBOSE) << ToString() << ": Connection established to "
                        << socket->GetRemoteAddress().ToSensitiveString();
  } else {
    if (socket->GetLocalAddress().IsLoopbackIP()) {
      RTC_LOG(LS_WARNING) << "Socket is bound to the address:"
                          << socket_address.ipaddr().ToSensitiveString()
                          << ", rather than an address associated with network:"
                          << port_->Network()->ToString()
                          << ". Still allowing it since it's localhost.";
    } else if (IPIsAny(port_->Network()->GetBestIP())) {
      RTC_LOG(LS_WARNING)
          << "Socket is bound to the address:"
          << socket_address.ipaddr().ToSensitiveString()
          << ", rather than an address associated with network:"
          << port_->Network()->ToString()
          << ". Still allowing it since it's the 'any' address"
             ", possibly caused by multiple_routes being disabled.";
    } else {
      RTC_LOG(LS_WARNING) << "Dropping connection as TCP socket bound to IP "
                          << socket_address.ipaddr().ToSensitiveString()
                          << ", rather than an address associated with network:"
                          << port_->Network()->ToString();
      OnClose(socket, 0);
      return;
    }
  }

  set_connected(true);
  connection_pending_ = false;
}

void TCPConnection::OnClose(rtc::AsyncPacketSocket* socket, int error) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK_EQ(socket, socket_.get());
  RTC_LOG(LS_INFO) << ToString() << ": Connection closed with error " << error;

  if (!port_) {
    RTC_LOG(LS_ERROR) << "TCPConnection: Port has been deleted.";
    return;
  }

  if (connected()) {
    set_connected(false);

    pretending_to_be_writable_ = true;

    network_thread()->PostDelayedTask(
        SafeTask(network_safety_.flag(),
                 [this]() {
                   if (pretending_to_be_writable_) {
                     Destroy();
                   }
                 }),
        TimeDelta::Millis(reconnection_timeout()));
  } else if (!pretending_to_be_writable_) {

    DisconnectSocketSignals(socket_.get());
    port()->DestroyConnectionAsync(this);
  }
}

void TCPConnection::MaybeReconnect() {
  RTC_DCHECK_RUN_ON(network_thread());

  if (connected() || connection_pending_ || !outgoing_) {
    return;
  }

  RTC_LOG(LS_INFO) << ToString()
                   << ": TCP Connection with remote is closed, "
                      "trying to reconnect";

  CreateOutgoingTcpSocket();
  error_ = EPIPE;
}

void TCPConnection::OnReadPacket(rtc::AsyncPacketSocket* socket,
                                 const rtc::ReceivedPacket& packet) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK_EQ(socket, socket_.get());
  Connection::OnReadPacket(packet);
}

void TCPConnection::OnReadyToSend(rtc::AsyncPacketSocket* socket) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK_EQ(socket, socket_.get());
  Connection::OnReadyToSend();
}

void TCPConnection::OnDestroyed(Connection* c) {
  RTC_DCHECK_RUN_ON(network_thread());
  RTC_DCHECK_EQ(c, this);
  if (socket_) {
    DisconnectSocketSignals(socket_.get());
  }
}

void TCPConnection::CreateOutgoingTcpSocket() {
  RTC_DCHECK(outgoing_);
  int opts = (remote_candidate().protocol() == SSLTCP_PROTOCOL_NAME)
                 ? rtc::PacketSocketFactory::OPT_TLS_FAKE
                 : 0;

  if (socket_) {
    DisconnectSocketSignals(socket_.get());
  }

  rtc::PacketSocketTcpOptions tcp_opts;
  tcp_opts.opts = opts;
  socket_.reset(port()->socket_factory()->CreateClientTcpSocket(
      rtc::SocketAddress(port()->Network()->GetBestIP(), 0),
      remote_candidate().address(), port()->proxy(), port()->user_agent(),
      tcp_opts));
  if (socket_) {
    RTC_LOG(LS_VERBOSE) << ToString() << ": Connecting from "
                        << socket_->GetLocalAddress().ToSensitiveString()
                        << " to "
                        << remote_candidate().address().ToSensitiveString();
    set_connected(false);
    connection_pending_ = true;
    ConnectSocketSignals(socket_.get());
  } else {
    RTC_LOG(LS_WARNING) << ToString() << ": Failed to create connection to "
                        << remote_candidate().address().ToSensitiveString();
    set_state(IceCandidatePairState::FAILED);

    network_thread()->PostTask(
        SafeTask(network_safety_.flag(), [this]() { FailAndPrune(); }));
  }
}

void TCPConnection::ConnectSocketSignals(rtc::AsyncPacketSocket* socket) {
  if (outgoing_) {
    socket->SignalConnect.connect(this, &TCPConnection::OnConnect);
  }
  socket->RegisterReceivedPacketCallback(
      [&](rtc::AsyncPacketSocket* socket, const rtc::ReceivedPacket& packet) {
        OnReadPacket(socket, packet);
      });
  socket->SignalReadyToSend.connect(this, &TCPConnection::OnReadyToSend);
  socket->SubscribeCloseEvent(this, [this, safety = network_safety_.flag()](
                                        rtc::AsyncPacketSocket* s, int err) {
    if (safety->alive())
      OnClose(s, err);
  });
}

void TCPConnection::DisconnectSocketSignals(rtc::AsyncPacketSocket* socket) {
  if (outgoing_) {
    socket->SignalConnect.disconnect(this);
  }
  socket->DeregisterReceivedPacketCallback();
  socket->SignalReadyToSend.disconnect(this);
  socket->UnsubscribeCloseEvent(this);
}

}
