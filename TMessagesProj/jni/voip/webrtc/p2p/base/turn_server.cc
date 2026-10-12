/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/turn_server.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/packet_socket_factory.h"
#include "api/task_queue/task_queue_base.h"
#include "api/transport/stun.h"
#include "p2p/base/async_stun_tcp_socket.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/message_digest.h"
#include "rtc_base/socket_adapters.h"
#include "rtc_base/strings/string_builder.h"

namespace cricket {
namespace {
using ::webrtc::TimeDelta;

constexpr TimeDelta kNonceTimeout = TimeDelta::Minutes(60);
constexpr TimeDelta kDefaultAllocationTimeout = TimeDelta::Minutes(10);
constexpr TimeDelta kPermissionTimeout = TimeDelta::Minutes(5);
constexpr TimeDelta kChannelTimeout = TimeDelta::Minutes(10);

constexpr int kMinChannelNumber = 0x4000;
constexpr int kMaxChannelNumber = 0x7FFF;

constexpr size_t kNonceKeySize = 16;
constexpr size_t kNonceSize = 48;

constexpr size_t TURN_CHANNEL_HEADER_SIZE = 4U;

bool IsTurnChannelData(uint16_t msg_type) {

  return ((msg_type & 0xC000) == 0x4000);
}

}

int GetStunSuccessResponseTypeOrZero(const StunMessage& req) {
  const int resp_type = GetStunSuccessResponseType(req.type());
  return resp_type == -1 ? 0 : resp_type;
}

int GetStunErrorResponseTypeOrZero(const StunMessage& req) {
  const int resp_type = GetStunErrorResponseType(req.type());
  return resp_type == -1 ? 0 : resp_type;
}

static void InitErrorResponse(int code,
                              absl::string_view reason,
                              StunMessage* resp) {
  resp->AddAttribute(std::make_unique<cricket::StunErrorCodeAttribute>(
      STUN_ATTR_ERROR_CODE, code, std::string(reason)));
}

TurnServer::TurnServer(webrtc::TaskQueueBase* thread)
    : thread_(thread),
      nonce_key_(rtc::CreateRandomString(kNonceKeySize)),
      auth_hook_(NULL),
      redirect_hook_(NULL),
      enable_otu_nonce_(false) {}

TurnServer::~TurnServer() {
  RTC_DCHECK_RUN_ON(thread_);
  for (InternalSocketMap::iterator it = server_sockets_.begin();
       it != server_sockets_.end(); ++it) {
    rtc::AsyncPacketSocket* socket = it->first;
    delete socket;
  }

  for (ServerSocketMap::iterator it = server_listen_sockets_.begin();
       it != server_listen_sockets_.end(); ++it) {
    rtc::Socket* socket = it->first;
    delete socket;
  }
}

void TurnServer::AddInternalSocket(rtc::AsyncPacketSocket* socket,
                                   ProtocolType proto) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(server_sockets_.end() == server_sockets_.find(socket));
  server_sockets_[socket] = proto;
  socket->RegisterReceivedPacketCallback(
      [&](rtc::AsyncPacketSocket* socket, const rtc::ReceivedPacket& packet) {
        RTC_DCHECK_RUN_ON(thread_);
        OnInternalPacket(socket, packet);
      });
}

void TurnServer::AddInternalServerSocket(
    rtc::Socket* socket,
    ProtocolType proto,
    std::unique_ptr<rtc::SSLAdapterFactory> ssl_adapter_factory) {
  RTC_DCHECK_RUN_ON(thread_);

  RTC_DCHECK(server_listen_sockets_.end() ==
             server_listen_sockets_.find(socket));
  server_listen_sockets_[socket] = {proto, std::move(ssl_adapter_factory)};
  socket->SignalReadEvent.connect(this, &TurnServer::OnNewInternalConnection);
}

void TurnServer::SetExternalSocketFactory(
    rtc::PacketSocketFactory* factory,
    const rtc::SocketAddress& external_addr) {
  RTC_DCHECK_RUN_ON(thread_);
  external_socket_factory_.reset(factory);
  external_addr_ = external_addr;
}

void TurnServer::OnNewInternalConnection(rtc::Socket* socket) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(server_listen_sockets_.find(socket) !=
             server_listen_sockets_.end());
  AcceptConnection(socket);
}

void TurnServer::AcceptConnection(rtc::Socket* server_socket) {

  rtc::SocketAddress accept_addr;
  rtc::Socket* accepted_socket = server_socket->Accept(&accept_addr);
  if (accepted_socket != NULL) {
    const ServerSocketInfo& info = server_listen_sockets_[server_socket];
    if (info.ssl_adapter_factory) {
      rtc::SSLAdapter* ssl_adapter =
          info.ssl_adapter_factory->CreateAdapter(accepted_socket);
      ssl_adapter->StartSSL("");
      accepted_socket = ssl_adapter;
    }
    cricket::AsyncStunTCPSocket* tcp_socket =
        new cricket::AsyncStunTCPSocket(accepted_socket);

    tcp_socket->SubscribeCloseEvent(this,
                                    [this](rtc::AsyncPacketSocket* s, int err) {
                                      OnInternalSocketClose(s, err);
                                    });

    AddInternalSocket(tcp_socket, info.proto);
  }
}

void TurnServer::OnInternalSocketClose(rtc::AsyncPacketSocket* socket,
                                       int err) {
  RTC_DCHECK_RUN_ON(thread_);
  DestroyInternalSocket(socket);
}

void TurnServer::OnInternalPacket(rtc::AsyncPacketSocket* socket,
                                  const rtc::ReceivedPacket& packet) {
  RTC_DCHECK_RUN_ON(thread_);

  if (packet.payload().size() < TURN_CHANNEL_HEADER_SIZE) {
    return;
  }
  InternalSocketMap::iterator iter = server_sockets_.find(socket);
  RTC_DCHECK(iter != server_sockets_.end());
  TurnServerConnection conn(packet.source_address(), iter->second, socket);
  uint16_t msg_type = rtc::GetBE16(packet.payload().data());
  if (!IsTurnChannelData(msg_type)) {

    HandleStunMessage(&conn, packet.payload());
  } else {

    TurnServerAllocation* allocation = FindAllocation(&conn);
    if (allocation) {
      allocation->HandleChannelData(packet.payload());
    }
    if (stun_message_observer_ != nullptr) {
      stun_message_observer_->ReceivedChannelData(packet.payload());
    }
  }
}

void TurnServer::HandleStunMessage(TurnServerConnection* conn,
                                   rtc::ArrayView<const uint8_t> payload) {
  TurnMessage msg;
  rtc::ByteBufferReader buf(payload);
  if (!msg.Read(&buf) || (buf.Length() > 0)) {
    RTC_LOG(LS_WARNING) << "Received invalid STUN message";
    return;
  }

  if (stun_message_observer_ != nullptr) {
    stun_message_observer_->ReceivedMessage(&msg);
  }

  if (msg.type() == STUN_BINDING_REQUEST) {
    HandleBindingRequest(conn, &msg);
    return;
  }

  if (redirect_hook_ != NULL && msg.type() == STUN_ALLOCATE_REQUEST) {
    rtc::SocketAddress address;
    if (redirect_hook_->ShouldRedirect(conn->src(), &address)) {
      SendErrorResponseWithAlternateServer(conn, &msg, address);
      return;
    }
  }

  TurnServerAllocation* allocation = FindAllocation(conn);
  std::string key;
  if (!allocation) {
    GetKey(&msg, &key);
  } else {
    key = allocation->key();
  }

  if (IsStunRequestType(msg.type())) {
    if (!CheckAuthorization(conn, &msg, key)) {
      return;
    }
  }

  if (!allocation && msg.type() == STUN_ALLOCATE_REQUEST) {
    HandleAllocateRequest(conn, &msg, key);
  } else if (allocation &&
             (msg.type() != STUN_ALLOCATE_REQUEST ||
              msg.transaction_id() == allocation->transaction_id())) {

    if (IsStunRequestType(msg.type()) &&
        msg.GetByteString(STUN_ATTR_USERNAME)->string_view() !=
            allocation->username()) {
      SendErrorResponse(conn, &msg, STUN_ERROR_WRONG_CREDENTIALS,
                        STUN_ERROR_REASON_WRONG_CREDENTIALS);
      return;
    }
    allocation->HandleTurnMessage(&msg);
  } else {

    SendErrorResponse(conn, &msg, STUN_ERROR_ALLOCATION_MISMATCH,
                      STUN_ERROR_REASON_ALLOCATION_MISMATCH);
  }
}

bool TurnServer::GetKey(const StunMessage* msg, std::string* key) {
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  if (!username_attr) {
    return false;
  }

  return (auth_hook_ != NULL &&
          auth_hook_->GetKey(std::string(username_attr->string_view()), realm_,
                             key));
}

bool TurnServer::CheckAuthorization(TurnServerConnection* conn,
                                    StunMessage* msg,
                                    absl::string_view key) {

  RTC_DCHECK(IsStunRequestType(msg->type()));
  const StunByteStringAttribute* mi_attr =
      msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY);
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  const StunByteStringAttribute* realm_attr =
      msg->GetByteString(STUN_ATTR_REALM);
  const StunByteStringAttribute* nonce_attr =
      msg->GetByteString(STUN_ATTR_NONCE);

  if (!mi_attr) {
    SendErrorResponseWithRealmAndNonce(conn, msg, STUN_ERROR_UNAUTHORIZED,
                                       STUN_ERROR_REASON_UNAUTHORIZED);
    return false;
  }

  if (!username_attr || !realm_attr || !nonce_attr) {
    SendErrorResponse(conn, msg, STUN_ERROR_BAD_REQUEST,
                      STUN_ERROR_REASON_BAD_REQUEST);
    return false;
  }

  if (!ValidateNonce(nonce_attr->string_view())) {
    SendErrorResponseWithRealmAndNonce(conn, msg, STUN_ERROR_STALE_NONCE,
                                       STUN_ERROR_REASON_STALE_NONCE);
    return false;
  }

  if (key.empty() || msg->ValidateMessageIntegrity(std::string(key)) !=
                         StunMessage::IntegrityStatus::kIntegrityOk) {
    SendErrorResponseWithRealmAndNonce(conn, msg, STUN_ERROR_UNAUTHORIZED,
                                       STUN_ERROR_REASON_UNAUTHORIZED);
    return false;
  }

  TurnServerAllocation* allocation = FindAllocation(conn);
  if (enable_otu_nonce_ && allocation &&
      allocation->last_nonce() == nonce_attr->string_view()) {
    SendErrorResponseWithRealmAndNonce(conn, msg, STUN_ERROR_STALE_NONCE,
                                       STUN_ERROR_REASON_STALE_NONCE);
    return false;
  }

  if (allocation) {
    allocation->set_last_nonce(nonce_attr->string_view());
  }

  return true;
}

void TurnServer::HandleBindingRequest(TurnServerConnection* conn,
                                      const StunMessage* req) {
  StunMessage response(GetStunSuccessResponseTypeOrZero(*req),
                       req->transaction_id());

  auto mapped_addr_attr = std::make_unique<StunXorAddressAttribute>(
      STUN_ATTR_XOR_MAPPED_ADDRESS, conn->src());
  response.AddAttribute(std::move(mapped_addr_attr));

  SendStun(conn, &response);
}

void TurnServer::HandleAllocateRequest(TurnServerConnection* conn,
                                       const TurnMessage* msg,
                                       absl::string_view key) {

  const StunUInt32Attribute* transport_attr =
      msg->GetUInt32(STUN_ATTR_REQUESTED_TRANSPORT);
  if (!transport_attr) {
    SendErrorResponse(conn, msg, STUN_ERROR_BAD_REQUEST,
                      STUN_ERROR_REASON_BAD_REQUEST);
    return;
  }

  int proto = transport_attr->value() >> 24;
  if (proto != IPPROTO_UDP) {
    SendErrorResponse(conn, msg, STUN_ERROR_UNSUPPORTED_PROTOCOL,
                      STUN_ERROR_REASON_UNSUPPORTED_PROTOCOL);
    return;
  }

  TurnServerAllocation* alloc = CreateAllocation(conn, proto, key);
  if (alloc) {
    alloc->HandleTurnMessage(msg);
  } else {
    SendErrorResponse(conn, msg, STUN_ERROR_SERVER_ERROR,
                      "Failed to allocate socket");
  }
}

std::string TurnServer::GenerateNonce(int64_t now) const {

  std::string input(reinterpret_cast<const char*>(&now), sizeof(now));
  std::string nonce = rtc::hex_encode(input);
  nonce += rtc::ComputeHmac(rtc::DIGEST_MD5, nonce_key_, input);
  RTC_DCHECK(nonce.size() == kNonceSize);

  return nonce;
}

bool TurnServer::ValidateNonce(absl::string_view nonce) const {

  if (nonce.size() != kNonceSize) {
    return false;
  }

  int64_t then;
  char* p = reinterpret_cast<char*>(&then);
  size_t len = rtc::hex_decode(rtc::ArrayView<char>(p, sizeof(then)),
                               nonce.substr(0, sizeof(then) * 2));
  if (len != sizeof(then)) {
    return false;
  }

  if (nonce.substr(sizeof(then) * 2) !=
      rtc::ComputeHmac(rtc::DIGEST_MD5, nonce_key_,
                       std::string(p, sizeof(then)))) {
    return false;
  }

  return TimeDelta::Millis(rtc::TimeMillis() - then) < kNonceTimeout;
}

TurnServerAllocation* TurnServer::FindAllocation(TurnServerConnection* conn) {
  AllocationMap::const_iterator it = allocations_.find(*conn);
  return (it != allocations_.end()) ? it->second.get() : nullptr;
}

TurnServerAllocation* TurnServer::CreateAllocation(TurnServerConnection* conn,
                                                   int proto,
                                                   absl::string_view key) {
  rtc::AsyncPacketSocket* external_socket =
      (external_socket_factory_)
          ? external_socket_factory_->CreateUdpSocket(external_addr_, 0, 0)
          : NULL;
  if (!external_socket) {
    return NULL;
  }

  TurnServerAllocation* allocation =
      new TurnServerAllocation(this, thread_, *conn, external_socket, key);
  allocations_[*conn].reset(allocation);
  return allocation;
}

void TurnServer::SendErrorResponse(TurnServerConnection* conn,
                                   const StunMessage* req,
                                   int code,
                                   absl::string_view reason) {
  RTC_DCHECK_RUN_ON(thread_);
  TurnMessage resp(GetStunErrorResponseTypeOrZero(*req), req->transaction_id());
  InitErrorResponse(code, reason, &resp);

  RTC_LOG(LS_INFO) << "Sending error response, type=" << resp.type()
                   << ", code=" << code << ", reason=" << reason;
  SendStun(conn, &resp);
}

void TurnServer::SendErrorResponseWithRealmAndNonce(TurnServerConnection* conn,
                                                    const StunMessage* msg,
                                                    int code,
                                                    absl::string_view reason) {
  TurnMessage resp(GetStunErrorResponseTypeOrZero(*msg), msg->transaction_id());
  InitErrorResponse(code, reason, &resp);

  int64_t timestamp = rtc::TimeMillis();
  if (ts_for_next_nonce_) {
    timestamp = ts_for_next_nonce_;
    ts_for_next_nonce_ = 0;
  }
  resp.AddAttribute(std::make_unique<StunByteStringAttribute>(
      STUN_ATTR_NONCE, GenerateNonce(timestamp)));
  resp.AddAttribute(
      std::make_unique<StunByteStringAttribute>(STUN_ATTR_REALM, realm_));
  SendStun(conn, &resp);
}

void TurnServer::SendErrorResponseWithAlternateServer(
    TurnServerConnection* conn,
    const StunMessage* msg,
    const rtc::SocketAddress& addr) {
  TurnMessage resp(GetStunErrorResponseTypeOrZero(*msg), msg->transaction_id());
  InitErrorResponse(STUN_ERROR_TRY_ALTERNATE,
                    STUN_ERROR_REASON_TRY_ALTERNATE_SERVER, &resp);
  resp.AddAttribute(
      std::make_unique<StunAddressAttribute>(STUN_ATTR_ALTERNATE_SERVER, addr));
  SendStun(conn, &resp);
}

void TurnServer::SendStun(TurnServerConnection* conn, StunMessage* msg) {
  RTC_DCHECK_RUN_ON(thread_);
  rtc::ByteBufferWriter buf;

  if (!software_.empty()) {
    msg->AddAttribute(std::make_unique<StunByteStringAttribute>(
        STUN_ATTR_SOFTWARE, software_));
  }
  msg->Write(&buf);
  Send(conn, buf);
}

void TurnServer::Send(TurnServerConnection* conn,
                      const rtc::ByteBufferWriter& buf) {
  RTC_DCHECK_RUN_ON(thread_);
  rtc::PacketOptions options;
  conn->socket()->SendTo(buf.Data(), buf.Length(), conn->src(), options);
}

void TurnServer::DestroyAllocation(TurnServerAllocation* allocation) {

  rtc::AsyncPacketSocket* socket = allocation->conn()->socket();
  InternalSocketMap::iterator iter = server_sockets_.find(socket);

  if (iter != server_sockets_.end() && iter->second != cricket::PROTO_UDP) {
    DestroyInternalSocket(socket);
  }

  allocations_.erase(*(allocation->conn()));
}

void TurnServer::DestroyInternalSocket(rtc::AsyncPacketSocket* socket) {
  InternalSocketMap::iterator iter = server_sockets_.find(socket);
  if (iter != server_sockets_.end()) {
    rtc::AsyncPacketSocket* socket = iter->first;
    socket->UnsubscribeCloseEvent(this);
    socket->DeregisterReceivedPacketCallback();
    server_sockets_.erase(iter);
    std::unique_ptr<rtc::AsyncPacketSocket> socket_to_delete =
        absl::WrapUnique(socket);

    thread_->PostTask([socket_to_delete = std::move(socket_to_delete)] {});
  }
}

TurnServerConnection::TurnServerConnection(const rtc::SocketAddress& src,
                                           ProtocolType proto,
                                           rtc::AsyncPacketSocket* socket)
    : src_(src),
      dst_(socket->GetRemoteAddress()),
      proto_(proto),
      socket_(socket) {}

bool TurnServerConnection::operator==(const TurnServerConnection& c) const {
  return src_ == c.src_ && dst_ == c.dst_ && proto_ == c.proto_;
}

bool TurnServerConnection::operator<(const TurnServerConnection& c) const {
  return std::tie(src_, dst_, proto_) < std::tie(c.src_, c.dst_, c.proto_);
}

std::string TurnServerConnection::ToString() const {
  const char* const kProtos[] = {"unknown", "udp", "tcp", "ssltcp"};
  rtc::StringBuilder ost;
  ost << src_.ToSensitiveString() << "-" << dst_.ToSensitiveString() << ":"
      << kProtos[proto_];
  return ost.Release();
}

TurnServerAllocation::TurnServerAllocation(TurnServer* server,
                                           webrtc::TaskQueueBase* thread,
                                           const TurnServerConnection& conn,
                                           rtc::AsyncPacketSocket* socket,
                                           absl::string_view key)
    : server_(server),
      thread_(thread),
      conn_(conn),
      external_socket_(socket),
      key_(key) {
  external_socket_->RegisterReceivedPacketCallback(
      [&](rtc::AsyncPacketSocket* socket, const rtc::ReceivedPacket& packet) {
        RTC_DCHECK_RUN_ON(thread_);
        OnExternalPacket(socket, packet);
      });
}

TurnServerAllocation::~TurnServerAllocation() {
  channels_.clear();
  perms_.clear();
  RTC_LOG(LS_INFO) << ToString() << ": Allocation destroyed";
}

std::string TurnServerAllocation::ToString() const {
  rtc::StringBuilder ost;
  ost << "Alloc[" << conn_.ToString() << "]";
  return ost.Release();
}

void TurnServerAllocation::HandleTurnMessage(const TurnMessage* msg) {
  RTC_DCHECK(msg != NULL);
  switch (msg->type()) {
    case STUN_ALLOCATE_REQUEST:
      HandleAllocateRequest(msg);
      break;
    case TURN_REFRESH_REQUEST:
      HandleRefreshRequest(msg);
      break;
    case TURN_SEND_INDICATION:
      HandleSendIndication(msg);
      break;
    case TURN_CREATE_PERMISSION_REQUEST:
      HandleCreatePermissionRequest(msg);
      break;
    case TURN_CHANNEL_BIND_REQUEST:
      HandleChannelBindRequest(msg);
      break;
    default:

      RTC_LOG(LS_WARNING) << ToString()
                          << ": Invalid TURN message type received: "
                          << msg->type();
  }
}

void TurnServerAllocation::HandleAllocateRequest(const TurnMessage* msg) {

  transaction_id_ = msg->transaction_id();
  const StunByteStringAttribute* username_attr =
      msg->GetByteString(STUN_ATTR_USERNAME);
  RTC_DCHECK(username_attr != NULL);
  username_ = std::string(username_attr->string_view());

  TimeDelta lifetime = ComputeLifetime(*msg);
  PostDeleteSelf(lifetime);

  RTC_LOG(LS_INFO) << ToString() << ": Created allocation with lifetime="
                   << lifetime.seconds();

  TurnMessage response(GetStunSuccessResponseTypeOrZero(*msg),
                       msg->transaction_id());

  auto mapped_addr_attr = std::make_unique<StunXorAddressAttribute>(
      STUN_ATTR_XOR_MAPPED_ADDRESS, conn_.src());
  auto relayed_addr_attr = std::make_unique<StunXorAddressAttribute>(
      STUN_ATTR_XOR_RELAYED_ADDRESS, external_socket_->GetLocalAddress());
  auto lifetime_attr = std::make_unique<StunUInt32Attribute>(
      STUN_ATTR_LIFETIME, lifetime.seconds());
  response.AddAttribute(std::move(mapped_addr_attr));
  response.AddAttribute(std::move(relayed_addr_attr));
  response.AddAttribute(std::move(lifetime_attr));

  SendResponse(&response);
}

void TurnServerAllocation::HandleRefreshRequest(const TurnMessage* msg) {

  TimeDelta lifetime = ComputeLifetime(*msg);

  safety_.reset();
  PostDeleteSelf(lifetime);

  RTC_LOG(LS_INFO) << ToString()
                   << ": Refreshed allocation, lifetime=" << lifetime.seconds();

  TurnMessage response(GetStunSuccessResponseTypeOrZero(*msg),
                       msg->transaction_id());

  auto lifetime_attr = std::make_unique<StunUInt32Attribute>(
      STUN_ATTR_LIFETIME, lifetime.seconds());
  response.AddAttribute(std::move(lifetime_attr));

  SendResponse(&response);
}

void TurnServerAllocation::HandleSendIndication(const TurnMessage* msg) {

  const StunByteStringAttribute* data_attr = msg->GetByteString(STUN_ATTR_DATA);
  const StunAddressAttribute* peer_attr =
      msg->GetAddress(STUN_ATTR_XOR_PEER_ADDRESS);
  if (!data_attr || !peer_attr) {
    RTC_LOG(LS_WARNING) << ToString() << ": Received invalid send indication";
    return;
  }

  if (HasPermission(peer_attr->GetAddress().ipaddr())) {
    SendExternal(reinterpret_cast<char*>(data_attr->array_view().data()),
                 data_attr->length(), peer_attr->GetAddress());
  } else {
    RTC_LOG(LS_WARNING) << ToString()
                        << ": Received send indication without permission"
                           " peer="
                        << peer_attr->GetAddress().ToSensitiveString();
  }
}

void TurnServerAllocation::HandleCreatePermissionRequest(
    const TurnMessage* msg) {

  const StunAddressAttribute* peer_attr =
      msg->GetAddress(STUN_ATTR_XOR_PEER_ADDRESS);
  if (!peer_attr) {
    SendBadRequestResponse(msg);
    return;
  }

  if (server_->reject_private_addresses_ &&
      rtc::IPIsPrivate(peer_attr->GetAddress().ipaddr())) {
    SendErrorResponse(msg, STUN_ERROR_FORBIDDEN, STUN_ERROR_REASON_FORBIDDEN);
    return;
  }

  AddPermission(peer_attr->GetAddress().ipaddr());

  RTC_LOG(LS_INFO) << ToString() << ": Created permission, peer="
                   << peer_attr->GetAddress().ToSensitiveString();

  TurnMessage response(GetStunSuccessResponseTypeOrZero(*msg),
                       msg->transaction_id());
  SendResponse(&response);
}

void TurnServerAllocation::HandleChannelBindRequest(const TurnMessage* msg) {

  const StunUInt32Attribute* channel_attr =
      msg->GetUInt32(STUN_ATTR_CHANNEL_NUMBER);
  const StunAddressAttribute* peer_attr =
      msg->GetAddress(STUN_ATTR_XOR_PEER_ADDRESS);
  if (!channel_attr || !peer_attr) {
    SendBadRequestResponse(msg);
    return;
  }

  int channel_id = channel_attr->value() >> 16;
  if (channel_id < kMinChannelNumber || channel_id > kMaxChannelNumber) {
    SendBadRequestResponse(msg);
    return;
  }

  auto channel1 = FindChannel(channel_id);
  auto channel2 = FindChannel(peer_attr->GetAddress());
  if (channel1 != channel2) {
    SendBadRequestResponse(msg);
    return;
  }

  if (channel1 == channels_.end()) {
    channel1 = channels_.insert(
        channels_.end(), {.id = channel_id, .peer = peer_attr->GetAddress()});
  } else {
    channel1->pending_delete.reset();
  }
  thread_->PostDelayedTask(
      SafeTask(channel1->pending_delete.flag(),
               [this, channel1] { channels_.erase(channel1); }),
      kChannelTimeout);

  AddPermission(peer_attr->GetAddress().ipaddr());

  RTC_LOG(LS_INFO) << ToString() << ": Bound channel, id=" << channel_id
                   << ", peer=" << peer_attr->GetAddress().ToSensitiveString();

  TurnMessage response(GetStunSuccessResponseTypeOrZero(*msg),
                       msg->transaction_id());
  SendResponse(&response);
}

void TurnServerAllocation::HandleChannelData(
    rtc::ArrayView<const uint8_t> payload) {

  uint16_t channel_id = rtc::GetBE16(payload.data());
  auto channel = FindChannel(channel_id);
  if (channel != channels_.end()) {

    SendExternal(payload.data() + TURN_CHANNEL_HEADER_SIZE,
                 payload.size() - TURN_CHANNEL_HEADER_SIZE, channel->peer);
  } else {
    RTC_LOG(LS_WARNING) << ToString()
                        << ": Received channel data for invalid channel, id="
                        << channel_id;
  }
}

void TurnServerAllocation::OnExternalPacket(rtc::AsyncPacketSocket* socket,
                                            const rtc::ReceivedPacket& packet) {
  RTC_DCHECK(external_socket_.get() == socket);
  auto channel = FindChannel(packet.source_address());
  if (channel != channels_.end()) {

    rtc::ByteBufferWriter buf;
    buf.WriteUInt16(channel->id);
    buf.WriteUInt16(static_cast<uint16_t>(packet.payload().size()));
    buf.WriteBytes(packet.payload().data(), packet.payload().size());
    server_->Send(&conn_, buf);
  } else if (!server_->enable_permission_checks_ ||
             HasPermission(packet.source_address().ipaddr())) {

    TurnMessage msg(TURN_DATA_INDICATION);
    msg.AddAttribute(std::make_unique<StunXorAddressAttribute>(
        STUN_ATTR_XOR_PEER_ADDRESS, packet.source_address()));
    msg.AddAttribute(std::make_unique<StunByteStringAttribute>(
        STUN_ATTR_DATA, packet.payload().data(), packet.payload().size()));
    server_->SendStun(&conn_, &msg);
  } else {
    RTC_LOG(LS_WARNING)
        << ToString() << ": Received external packet without permission, peer="
        << packet.source_address().ToSensitiveString();
  }
}

TimeDelta TurnServerAllocation::ComputeLifetime(const TurnMessage& msg) {
  if (const StunUInt32Attribute* attr = msg.GetUInt32(STUN_ATTR_LIFETIME)) {
    return std::min(TimeDelta::Seconds(static_cast<int>(attr->value())),
                    kDefaultAllocationTimeout);
  }
  return kDefaultAllocationTimeout;
}

bool TurnServerAllocation::HasPermission(const rtc::IPAddress& addr) {
  return FindPermission(addr) != perms_.end();
}

void TurnServerAllocation::AddPermission(const rtc::IPAddress& addr) {
  auto perm = FindPermission(addr);
  if (perm == perms_.end()) {
    perm = perms_.insert(perms_.end(), {.peer = addr});
  } else {
    perm->pending_delete.reset();
  }
  thread_->PostDelayedTask(SafeTask(perm->pending_delete.flag(),
                                    [this, perm] { perms_.erase(perm); }),
                           kPermissionTimeout);
}

TurnServerAllocation::PermissionList::iterator
TurnServerAllocation::FindPermission(const rtc::IPAddress& addr) {
  return absl::c_find_if(perms_,
                         [&](const Permission& p) { return p.peer == addr; });
}

TurnServerAllocation::ChannelList::iterator TurnServerAllocation::FindChannel(
    int channel_id) {
  return absl::c_find_if(channels_,
                         [&](const Channel& c) { return c.id == channel_id; });
}

TurnServerAllocation::ChannelList::iterator TurnServerAllocation::FindChannel(
    const rtc::SocketAddress& addr) {
  return absl::c_find_if(channels_,
                         [&](const Channel& c) { return c.peer == addr; });
}

void TurnServerAllocation::SendResponse(TurnMessage* msg) {

  msg->AddMessageIntegrity(key_);
  server_->SendStun(&conn_, msg);
}

void TurnServerAllocation::SendBadRequestResponse(const TurnMessage* req) {
  SendErrorResponse(req, STUN_ERROR_BAD_REQUEST, STUN_ERROR_REASON_BAD_REQUEST);
}

void TurnServerAllocation::SendErrorResponse(const TurnMessage* req,
                                             int code,
                                             absl::string_view reason) {
  server_->SendErrorResponse(&conn_, req, code, reason);
}

void TurnServerAllocation::SendExternal(const void* data,
                                        size_t size,
                                        const rtc::SocketAddress& peer) {
  rtc::PacketOptions options;
  external_socket_->SendTo(data, size, peer, options);
}

void TurnServerAllocation::PostDeleteSelf(TimeDelta delay) {
  auto delete_self = [this] {
    RTC_DCHECK_RUN_ON(server_->thread_);
    server_->DestroyAllocation(this);
  };
  thread_->PostDelayedTask(SafeTask(safety_.flag(), std::move(delete_self)),
                           delay);
}

}
