/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/virtual_socket_server.h"

#include <errno.h>
#include <math.h>

#include <map>
#include <memory>
#include <vector>

#include "absl/algorithm/container.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/logging.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/socket_address_pair.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

namespace rtc {

using ::webrtc::MutexLock;
using ::webrtc::TaskQueueBase;
using ::webrtc::TimeDelta;

#if defined(WEBRTC_WIN)
const in_addr kInitialNextIPv4 = {{{0x01, 0, 0, 0}}};
#else

const in_addr kInitialNextIPv4 = {0x01000000};
#endif

const in6_addr kInitialNextIPv6 = {
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2}}};

const uint16_t kFirstEphemeralPort = 49152;
const uint16_t kLastEphemeralPort = 65535;
const uint16_t kEphemeralPortCount =
    kLastEphemeralPort - kFirstEphemeralPort + 1;
const uint32_t kDefaultNetworkCapacity = 64 * 1024;
const uint32_t kDefaultTcpBufferSize = 32 * 1024;

const uint32_t UDP_HEADER_SIZE = 28;
const uint32_t TCP_HEADER_SIZE = 40;
const uint32_t TCP_MSS = 1400;

const int NUM_SAMPLES = 1000;

class Packet {
 public:
  Packet(const char* data, size_t size, const SocketAddress& from)
      : size_(size), consumed_(0), from_(from) {
    RTC_DCHECK(nullptr != data);
    data_ = new char[size_];
    memcpy(data_, data, size_);
  }

  ~Packet() { delete[] data_; }

  const char* data() const { return data_ + consumed_; }
  size_t size() const { return size_ - consumed_; }
  const SocketAddress& from() const { return from_; }

  void Consume(size_t size) {
    RTC_DCHECK(size + consumed_ < size_);
    consumed_ += size;
  }

 private:
  char* data_;
  size_t size_, consumed_;
  SocketAddress from_;
};

VirtualSocket::VirtualSocket(VirtualSocketServer* server, int family, int type)
    : server_(server),
      type_(type),
      state_(CS_CLOSED),
      error_(0),
      network_size_(0),
      recv_buffer_size_(0),
      bound_(false),
      was_any_(false) {
  RTC_DCHECK((type_ == SOCK_DGRAM) || (type_ == SOCK_STREAM));
  server->SignalReadyToSend.connect(this,
                                    &VirtualSocket::OnSocketServerReadyToSend);
}

VirtualSocket::~VirtualSocket() {
  Close();
}

SocketAddress VirtualSocket::GetLocalAddress() const {
  return local_addr_;
}

SocketAddress VirtualSocket::GetRemoteAddress() const {
  return remote_addr_;
}

void VirtualSocket::SetLocalAddress(const SocketAddress& addr) {
  local_addr_ = addr;
}

int VirtualSocket::Bind(const SocketAddress& addr) {
  if (!local_addr_.IsNil()) {
    error_ = EINVAL;
    return -1;
  }
  local_addr_ = server_->AssignBindAddress(addr);
  int result = server_->Bind(this, local_addr_);
  if (result != 0) {
    local_addr_.Clear();
    error_ = EADDRINUSE;
  } else {
    bound_ = true;
    was_any_ = addr.IsAnyIP();
  }
  return result;
}

int VirtualSocket::Connect(const SocketAddress& addr) {
  return InitiateConnect(addr, true);
}

VirtualSocket::SafetyBlock::SafetyBlock(VirtualSocket* socket)
    : socket_(*socket) {}

VirtualSocket::SafetyBlock::~SafetyBlock() {

  RTC_DCHECK(!alive_);
  RTC_DCHECK(posted_connects_.empty());
  RTC_DCHECK(recv_buffer_.empty());
  RTC_DCHECK(!listen_queue_.has_value());
}

void VirtualSocket::SafetyBlock::SetNotAlive() {
  VirtualSocketServer* const server = socket_.server_;
  const SocketAddress& local_addr = socket_.local_addr_;

  MutexLock lock(&mutex_);

  if (listen_queue_.has_value()) {
    for (const SocketAddress& remote_addr : *listen_queue_) {
      server->Disconnect(remote_addr);
    }
    listen_queue_ = absl::nullopt;
  }

  for (const SocketAddress& remote_addr : posted_connects_) {

    VirtualSocket* lookup_socket =
        server->LookupConnection(local_addr, remote_addr);
    if (lookup_socket) {

      server->Disconnect(lookup_socket);
      server->RemoveConnection(local_addr, remote_addr);
    } else {
      server->Disconnect(remote_addr);
    }
  }
  posted_connects_.clear();

  recv_buffer_.clear();

  alive_ = false;
}

void VirtualSocket::SafetyBlock::PostSignalReadEvent() {
  if (pending_read_signal_event_) {

    return;
  }

  pending_read_signal_event_ = true;
  rtc::scoped_refptr<SafetyBlock> safety(this);
  socket_.server_->msg_queue_->PostTask(
      [safety = std::move(safety)] { safety->MaybeSignalReadEvent(); });
}

void VirtualSocket::SafetyBlock::MaybeSignalReadEvent() {
  {
    MutexLock lock(&mutex_);
    pending_read_signal_event_ = false;
    if (!alive_ || recv_buffer_.empty()) {
      return;
    }
  }
  socket_.SignalReadEvent(&socket_);
}

int VirtualSocket::Close() {
  if (!local_addr_.IsNil() && bound_) {

    server_->Unbind(local_addr_, this);
    bound_ = false;
  }

  if (state_ == CS_CONNECTED && type_ == SOCK_STREAM) {
    server_->Disconnect(local_addr_, remote_addr_);
  }

  safety_->SetNotAlive();

  state_ = CS_CLOSED;
  local_addr_.Clear();
  remote_addr_.Clear();
  return 0;
}

int VirtualSocket::Send(const void* pv, size_t cb) {
  if (CS_CONNECTED != state_) {
    error_ = ENOTCONN;
    return -1;
  }
  if (SOCK_DGRAM == type_) {
    return SendUdp(pv, cb, remote_addr_);
  } else {
    return SendTcp(pv, cb);
  }
}

int VirtualSocket::SendTo(const void* pv,
                          size_t cb,
                          const SocketAddress& addr) {
  if (SOCK_DGRAM == type_) {
    return SendUdp(pv, cb, addr);
  } else {
    if (CS_CONNECTED != state_) {
      error_ = ENOTCONN;
      return -1;
    }
    return SendTcp(pv, cb);
  }
}

int VirtualSocket::Recv(void* pv, size_t cb, int64_t* timestamp) {
  SocketAddress addr;
  return RecvFrom(pv, cb, &addr, timestamp);
}

int VirtualSocket::RecvFrom(void* pv,
                            size_t cb,
                            SocketAddress* paddr,
                            int64_t* timestamp) {
  if (timestamp) {
    *timestamp = -1;
  }

  int data_read = safety_->RecvFrom(pv, cb, *paddr);
  if (data_read < 0) {
    error_ = EAGAIN;
    return -1;
  }

  if (type_ == SOCK_STREAM) {
    bool was_full = (recv_buffer_size_ == server_->recv_buffer_capacity());
    recv_buffer_size_ -= data_read;
    if (was_full) {
      server_->SendTcp(remote_addr_);
    }
  }

  return data_read;
}

int VirtualSocket::SafetyBlock::RecvFrom(void* buffer,
                                         size_t size,
                                         SocketAddress& addr) {
  MutexLock lock(&mutex_);

  if (recv_buffer_.empty()) {
    return -1;
  }

  Packet& packet = *recv_buffer_.front();
  size_t data_read = std::min(size, packet.size());
  memcpy(buffer, packet.data(), data_read);
  addr = packet.from();

  if (data_read < packet.size()) {
    packet.Consume(data_read);
  } else {
    recv_buffer_.pop_front();
  }

  if (!recv_buffer_.empty()) {
    PostSignalReadEvent();
  }

  return data_read;
}

int VirtualSocket::Listen(int backlog) {
  RTC_DCHECK(SOCK_STREAM == type_);
  RTC_DCHECK(CS_CLOSED == state_);
  if (local_addr_.IsNil()) {
    error_ = EINVAL;
    return -1;
  }
  safety_->Listen();
  state_ = CS_CONNECTING;
  return 0;
}

void VirtualSocket::SafetyBlock::Listen() {
  MutexLock lock(&mutex_);
  RTC_DCHECK(!listen_queue_.has_value());
  listen_queue_.emplace();
}

VirtualSocket* VirtualSocket::Accept(SocketAddress* paddr) {
  SafetyBlock::AcceptResult result = safety_->Accept();
  if (result.error != 0) {
    error_ = result.error;
    return nullptr;
  }
  if (paddr) {
    *paddr = result.remote_addr;
  }
  return result.socket.release();
}

VirtualSocket::SafetyBlock::AcceptResult VirtualSocket::SafetyBlock::Accept() {
  AcceptResult result;
  MutexLock lock(&mutex_);
  RTC_DCHECK(alive_);
  if (!listen_queue_.has_value()) {
    result.error = EINVAL;
    return result;
  }
  while (!listen_queue_->empty()) {
    auto socket = std::make_unique<VirtualSocket>(socket_.server_, AF_INET,
                                                  socket_.type_);

    socket->SetLocalAddress(socket_.local_addr_);

    socket->set_was_any(socket_.was_any());
    SocketAddress remote_addr = listen_queue_->front();
    listen_queue_->pop_front();
    if (socket->InitiateConnect(remote_addr, false) != 0) {
      continue;
    }
    socket->CompleteConnect(remote_addr);
    result.socket = std::move(socket);
    result.remote_addr = remote_addr;
    return result;
  }
  result.error = EWOULDBLOCK;
  return result;
}

int VirtualSocket::GetError() const {
  return error_;
}

void VirtualSocket::SetError(int error) {
  error_ = error;
}

Socket::ConnState VirtualSocket::GetState() const {
  return state_;
}

int VirtualSocket::GetOption(Option opt, int* value) {
  OptionsMap::const_iterator it = options_map_.find(opt);
  if (it == options_map_.end()) {
    return -1;
  }
  *value = it->second;
  return 0;
}

int VirtualSocket::SetOption(Option opt, int value) {
  options_map_[opt] = value;
  return 0;
}

void VirtualSocket::PostPacket(TimeDelta delay,
                               std::unique_ptr<Packet> packet) {
  rtc::scoped_refptr<SafetyBlock> safety = safety_;
  VirtualSocket* socket = this;
  server_->msg_queue_->PostDelayedTask(
      [safety = std::move(safety), socket,
       packet = std::move(packet)]() mutable {
        if (safety->AddPacket(std::move(packet))) {
          socket->SignalReadEvent(socket);
        }
      },
      delay);
}

bool VirtualSocket::SafetyBlock::AddPacket(std::unique_ptr<Packet> packet) {
  MutexLock lock(&mutex_);
  if (alive_) {
    recv_buffer_.push_back(std::move(packet));
  }
  return alive_;
}

void VirtualSocket::PostConnect(TimeDelta delay,
                                const SocketAddress& remote_addr) {
  safety_->PostConnect(delay, remote_addr);
}

void VirtualSocket::SafetyBlock::PostConnect(TimeDelta delay,
                                             const SocketAddress& remote_addr) {
  rtc::scoped_refptr<SafetyBlock> safety(this);

  MutexLock lock(&mutex_);
  RTC_DCHECK(alive_);

  auto it = posted_connects_.insert(posted_connects_.end(), remote_addr);
  auto task = [safety = std::move(safety), it] {
    switch (safety->Connect(it)) {
      case Signal::kNone:
        break;
      case Signal::kReadEvent:
        safety->socket_.SignalReadEvent(&safety->socket_);
        break;
      case Signal::kConnectEvent:
        safety->socket_.SignalConnectEvent(&safety->socket_);
        break;
    }
  };
  socket_.server_->msg_queue_->PostDelayedTask(std::move(task), delay);
}

VirtualSocket::SafetyBlock::Signal VirtualSocket::SafetyBlock::Connect(
    VirtualSocket::SafetyBlock::PostedConnects::iterator remote_addr_it) {
  MutexLock lock(&mutex_);
  if (!alive_) {
    return Signal::kNone;
  }
  RTC_DCHECK(!posted_connects_.empty());
  SocketAddress remote_addr = *remote_addr_it;
  posted_connects_.erase(remote_addr_it);

  if (listen_queue_.has_value()) {
    listen_queue_->push_back(remote_addr);
    return Signal::kReadEvent;
  }
  if (socket_.type_ == SOCK_STREAM && socket_.state_ == CS_CONNECTING) {
    socket_.CompleteConnect(remote_addr);
    return Signal::kConnectEvent;
  }
  RTC_LOG(LS_VERBOSE) << "Socket at " << socket_.local_addr_.ToString()
                      << " is not listening";
  socket_.server_->Disconnect(remote_addr);
  return Signal::kNone;
}

bool VirtualSocket::SafetyBlock::IsAlive() {
  MutexLock lock(&mutex_);
  return alive_;
}

void VirtualSocket::PostDisconnect(TimeDelta delay) {

  VirtualSocket* socket = this;
  rtc::scoped_refptr<SafetyBlock> safety = safety_;
  auto task = [safety = std::move(safety), socket] {
    if (!safety->IsAlive()) {
      return;
    }
    RTC_DCHECK_EQ(socket->type_, SOCK_STREAM);
    if (socket->state_ == CS_CLOSED) {
      return;
    }
    int error_to_signal = (socket->state_ == CS_CONNECTING) ? ECONNREFUSED : 0;
    socket->state_ = CS_CLOSED;
    socket->remote_addr_.Clear();
    socket->SignalCloseEvent(socket, error_to_signal);
  };
  server_->msg_queue_->PostDelayedTask(std::move(task), delay);
}

int VirtualSocket::InitiateConnect(const SocketAddress& addr, bool use_delay) {
  if (!remote_addr_.IsNil()) {
    error_ = (CS_CONNECTED == state_) ? EISCONN : EINPROGRESS;
    return -1;
  }
  if (local_addr_.IsNil()) {

    int result = 0;
    if (addr.ipaddr().family() == AF_INET) {
      result = Bind(SocketAddress("0.0.0.0", 0));
    } else if (addr.ipaddr().family() == AF_INET6) {
      result = Bind(SocketAddress("::", 0));
    }
    if (result != 0) {
      return result;
    }
  }
  if (type_ == SOCK_DGRAM) {
    remote_addr_ = addr;
    state_ = CS_CONNECTED;
  } else {
    int result = server_->Connect(this, addr, use_delay);
    if (result != 0) {
      error_ = EHOSTUNREACH;
      return -1;
    }
    state_ = CS_CONNECTING;
  }
  return 0;
}

void VirtualSocket::CompleteConnect(const SocketAddress& addr) {
  RTC_DCHECK(CS_CONNECTING == state_);
  remote_addr_ = addr;
  state_ = CS_CONNECTED;
  server_->AddConnection(remote_addr_, local_addr_, this);
}

int VirtualSocket::SendUdp(const void* pv,
                           size_t cb,
                           const SocketAddress& addr) {

  if (local_addr_.IsNil()) {
    local_addr_ = server_->AssignBindAddress(
        EmptySocketAddressWithFamily(addr.ipaddr().family()));
    int result = server_->Bind(this, local_addr_);
    if (result != 0) {
      local_addr_.Clear();
      error_ = EADDRINUSE;
      return result;
    }
  }

  return server_->SendUdp(this, static_cast<const char*>(pv), cb, addr);
}

int VirtualSocket::SendTcp(const void* pv, size_t cb) {
  size_t capacity = server_->send_buffer_capacity() - send_buffer_.size();
  if (0 == capacity) {
    ready_to_send_ = false;
    error_ = EWOULDBLOCK;
    return -1;
  }
  size_t consumed = std::min(cb, capacity);
  const char* cpv = static_cast<const char*>(pv);
  send_buffer_.insert(send_buffer_.end(), cpv, cpv + consumed);
  server_->SendTcp(this);
  return static_cast<int>(consumed);
}

void VirtualSocket::OnSocketServerReadyToSend() {
  if (ready_to_send_) {

    return;
  }
  if (type_ == SOCK_DGRAM) {
    ready_to_send_ = true;
    SignalWriteEvent(this);
  } else {
    RTC_DCHECK(type_ == SOCK_STREAM);

    server_->SendTcp(this);
  }
}

void VirtualSocket::SetToBlocked() {
  ready_to_send_ = false;
  error_ = EWOULDBLOCK;
}

void VirtualSocket::UpdateRecv(size_t data_size) {
  recv_buffer_size_ += data_size;
}

void VirtualSocket::UpdateSend(size_t data_size) {
  size_t new_buffer_size = send_buffer_.size() - data_size;

  if (data_size < send_buffer_.size()) {

    memmove(&send_buffer_[0], &send_buffer_[data_size], new_buffer_size);
  }
  send_buffer_.resize(new_buffer_size);
}

void VirtualSocket::MaybeSignalWriteEvent(size_t capacity) {
  if (!ready_to_send_ && (send_buffer_.size() < capacity)) {
    ready_to_send_ = true;
    SignalWriteEvent(this);
  }
}

uint32_t VirtualSocket::AddPacket(int64_t cur_time, size_t packet_size) {
  network_size_ += packet_size;
  uint32_t send_delay =
      server_->SendDelay(static_cast<uint32_t>(network_size_));

  NetworkEntry entry;
  entry.size = packet_size;
  entry.done_time = cur_time + send_delay;
  network_.push_back(entry);

  return send_delay;
}

int64_t VirtualSocket::UpdateOrderedDelivery(int64_t ts) {

  ts = std::max(ts, last_delivery_time_);

  last_delivery_time_ = ts;
  return ts;
}

size_t VirtualSocket::PurgeNetworkPackets(int64_t cur_time) {
  while (!network_.empty() && (network_.front().done_time <= cur_time)) {
    RTC_DCHECK(network_size_ >= network_.front().size);
    network_size_ -= network_.front().size;
    network_.pop_front();
  }
  return network_size_;
}

VirtualSocketServer::VirtualSocketServer() : VirtualSocketServer(nullptr) {}

VirtualSocketServer::VirtualSocketServer(ThreadProcessingFakeClock* fake_clock)
    : fake_clock_(fake_clock),
      msg_queue_(nullptr),
      stop_on_idle_(false),
      next_ipv4_(kInitialNextIPv4),
      next_ipv6_(kInitialNextIPv6),
      next_port_(kFirstEphemeralPort),
      bindings_(new AddressMap()),
      connections_(new ConnectionMap()),
      bandwidth_(0),
      network_capacity_(kDefaultNetworkCapacity),
      send_buffer_capacity_(kDefaultTcpBufferSize),
      recv_buffer_capacity_(kDefaultTcpBufferSize),
      delay_mean_(0),
      delay_stddev_(0),
      delay_samples_(NUM_SAMPLES),
      drop_prob_(0.0) {
  UpdateDelayDistribution();
}

VirtualSocketServer::~VirtualSocketServer() {
  delete bindings_;
  delete connections_;
}

IPAddress VirtualSocketServer::GetNextIP(int family) {
  if (family == AF_INET) {
    IPAddress next_ip(next_ipv4_);
    next_ipv4_.s_addr = HostToNetwork32(NetworkToHost32(next_ipv4_.s_addr) + 1);
    return next_ip;
  } else if (family == AF_INET6) {
    IPAddress next_ip(next_ipv6_);
    uint32_t* as_ints = reinterpret_cast<uint32_t*>(&next_ipv6_.s6_addr);
    as_ints[3] += 1;
    return next_ip;
  }
  return IPAddress();
}

uint16_t VirtualSocketServer::GetNextPort() {
  uint16_t port = next_port_;
  if (next_port_ < kLastEphemeralPort) {
    ++next_port_;
  } else {
    next_port_ = kFirstEphemeralPort;
  }
  return port;
}

void VirtualSocketServer::SetSendingBlocked(bool blocked) {
  {
    webrtc::MutexLock lock(&mutex_);
    if (blocked == sending_blocked_) {

      return;
    }
    sending_blocked_ = blocked;
  }
  if (!blocked) {

    SignalReadyToSend();
  }
}

VirtualSocket* VirtualSocketServer::CreateSocket(int family, int type) {
  return new VirtualSocket(this, family, type);
}

void VirtualSocketServer::SetMessageQueue(Thread* msg_queue) {
  msg_queue_ = msg_queue;
}

bool VirtualSocketServer::Wait(webrtc::TimeDelta max_wait_duration,
                               bool process_io) {
  RTC_DCHECK_RUN_ON(msg_queue_);
  if (stop_on_idle_ && Thread::Current()->empty()) {
    return false;
  }

  wakeup_.Wait(max_wait_duration);
  return true;
}

void VirtualSocketServer::WakeUp() {
  wakeup_.Set();
}

void VirtualSocketServer::SetAlternativeLocalAddress(
    const rtc::IPAddress& address,
    const rtc::IPAddress& alternative) {
  alternative_address_mapping_[address] = alternative;
}

bool VirtualSocketServer::ProcessMessagesUntilIdle() {
  RTC_DCHECK_RUN_ON(msg_queue_);
  stop_on_idle_ = true;
  while (!msg_queue_->empty()) {
    if (fake_clock_) {

      fake_clock_->AdvanceTime(webrtc::TimeDelta::Millis(1));
    } else {

      msg_queue_->ProcessMessages(Thread::kForever);
    }
  }
  stop_on_idle_ = false;
  return !msg_queue_->IsQuitting();
}

void VirtualSocketServer::SetNextPortForTesting(uint16_t port) {
  next_port_ = port;
}

bool VirtualSocketServer::CloseTcpConnections(
    const SocketAddress& addr_local,
    const SocketAddress& addr_remote) {
  VirtualSocket* socket = LookupConnection(addr_local, addr_remote);
  if (!socket) {
    return false;
  }

  socket->SignalCloseEvent(socket, 0);

  socket->Close();

  return true;
}

int VirtualSocketServer::Bind(VirtualSocket* socket,
                              const SocketAddress& addr) {
  RTC_DCHECK(nullptr != socket);

  RTC_DCHECK(!IPIsUnspec(addr.ipaddr()));
  RTC_DCHECK(addr.port() != 0);

  SocketAddress normalized(addr.ipaddr().Normalized(), addr.port());

  AddressMap::value_type entry(normalized, socket);
  return bindings_->insert(entry).second ? 0 : -1;
}

SocketAddress VirtualSocketServer::AssignBindAddress(
    const SocketAddress& app_addr) {
  RTC_DCHECK(!IPIsUnspec(app_addr.ipaddr()));

  SocketAddress addr;
  addr.SetIP(app_addr.ipaddr().Normalized());

  auto alternative = alternative_address_mapping_.find(addr.ipaddr());
  if (alternative != alternative_address_mapping_.end()) {
    addr.SetIP(alternative->second);
  }

  if (app_addr.port() != 0) {
    addr.SetPort(app_addr.port());
  } else {

    for (int i = 0; i < kEphemeralPortCount; ++i) {
      addr.SetPort(GetNextPort());
      if (bindings_->find(addr) == bindings_->end()) {
        break;
      }
    }
  }

  return addr;
}

VirtualSocket* VirtualSocketServer::LookupBinding(const SocketAddress& addr) {
  SocketAddress normalized(addr.ipaddr().Normalized(), addr.port());
  AddressMap::iterator it = bindings_->find(normalized);
  if (it != bindings_->end()) {
    return it->second;
  }

  IPAddress default_ip = GetDefaultSourceAddress(addr.ipaddr().family());
  if (!IPIsUnspec(default_ip) && addr.ipaddr() == default_ip) {

    SocketAddress sock_addr =
        EmptySocketAddressWithFamily(addr.ipaddr().family());
    sock_addr.SetPort(addr.port());
    return LookupBinding(sock_addr);
  }

  return nullptr;
}

int VirtualSocketServer::Unbind(const SocketAddress& addr,
                                VirtualSocket* socket) {
  SocketAddress normalized(addr.ipaddr().Normalized(), addr.port());
  RTC_DCHECK((*bindings_)[normalized] == socket);
  bindings_->erase(bindings_->find(normalized));
  return 0;
}

void VirtualSocketServer::AddConnection(const SocketAddress& local,
                                        const SocketAddress& remote,
                                        VirtualSocket* remote_socket) {

  SocketAddress local_normalized(local.ipaddr().Normalized(), local.port());
  SocketAddress remote_normalized(remote.ipaddr().Normalized(), remote.port());
  SocketAddressPair address_pair(local_normalized, remote_normalized);
  connections_->insert(std::pair<SocketAddressPair, VirtualSocket*>(
      address_pair, remote_socket));
}

VirtualSocket* VirtualSocketServer::LookupConnection(
    const SocketAddress& local,
    const SocketAddress& remote) {
  SocketAddress local_normalized(local.ipaddr().Normalized(), local.port());
  SocketAddress remote_normalized(remote.ipaddr().Normalized(), remote.port());
  SocketAddressPair address_pair(local_normalized, remote_normalized);
  ConnectionMap::iterator it = connections_->find(address_pair);
  return (connections_->end() != it) ? it->second : nullptr;
}

void VirtualSocketServer::RemoveConnection(const SocketAddress& local,
                                           const SocketAddress& remote) {
  SocketAddress local_normalized(local.ipaddr().Normalized(), local.port());
  SocketAddress remote_normalized(remote.ipaddr().Normalized(), remote.port());
  SocketAddressPair address_pair(local_normalized, remote_normalized);
  connections_->erase(address_pair);
}

static double Random() {
  return static_cast<double>(rand()) / RAND_MAX;
}

int VirtualSocketServer::Connect(VirtualSocket* socket,
                                 const SocketAddress& remote_addr,
                                 bool use_delay) {
  RTC_DCHECK(msg_queue_);

  TimeDelta delay = TimeDelta::Millis(use_delay ? GetTransitDelay(socket) : 0);
  VirtualSocket* remote = LookupBinding(remote_addr);
  if (!CanInteractWith(socket, remote)) {
    RTC_LOG(LS_INFO) << "Address family mismatch between "
                     << socket->GetLocalAddress().ToString() << " and "
                     << remote_addr.ToString();
    return -1;
  }
  if (remote != nullptr) {
    remote->PostConnect(delay, socket->GetLocalAddress());
  } else {
    RTC_LOG(LS_INFO) << "No one listening at " << remote_addr.ToString();
    socket->PostDisconnect(delay);
  }
  return 0;
}

bool VirtualSocketServer::Disconnect(VirtualSocket* socket) {
  if (!socket || !msg_queue_)
    return false;

  socket->PostDisconnect(TimeDelta::Millis(GetTransitDelay(socket)));
  return true;
}

bool VirtualSocketServer::Disconnect(const SocketAddress& addr) {
  return Disconnect(LookupBinding(addr));
}

bool VirtualSocketServer::Disconnect(const SocketAddress& local_addr,
                                     const SocketAddress& remote_addr) {

  VirtualSocket* socket = LookupConnection(local_addr, remote_addr);
  if (!socket) {

    socket = LookupBinding(remote_addr);
  }
  Disconnect(socket);

  RemoveConnection(remote_addr, local_addr);
  RemoveConnection(local_addr, remote_addr);
  return socket != nullptr;
}

int VirtualSocketServer::SendUdp(VirtualSocket* socket,
                                 const char* data,
                                 size_t data_size,
                                 const SocketAddress& remote_addr) {
  {
    webrtc::MutexLock lock(&mutex_);
    ++sent_packets_;
    if (sending_blocked_) {
      socket->SetToBlocked();
      return -1;
    }

    if (data_size > max_udp_payload_) {
      RTC_LOG(LS_VERBOSE) << "Dropping too large UDP payload of size "
                          << data_size << ", UDP payload limit is "
                          << max_udp_payload_;

      return data_size;
    }

    if (Random() < drop_prob_) {
      RTC_LOG(LS_VERBOSE) << "Dropping packet: bad luck";
      return static_cast<int>(data_size);
    }
  }

  VirtualSocket* recipient = LookupBinding(remote_addr);
  if (!recipient) {

    std::unique_ptr<VirtualSocket> dummy_socket(
        CreateSocket(AF_INET, SOCK_DGRAM));
    dummy_socket->SetLocalAddress(remote_addr);
    if (!CanInteractWith(socket, dummy_socket.get())) {
      RTC_LOG(LS_VERBOSE) << "Incompatible address families: "
                          << socket->GetLocalAddress().ToString() << " and "
                          << remote_addr.ToString();
      return -1;
    }
    RTC_LOG(LS_VERBOSE) << "No one listening at " << remote_addr.ToString();
    return static_cast<int>(data_size);
  }

  if (!CanInteractWith(socket, recipient)) {
    RTC_LOG(LS_VERBOSE) << "Incompatible address families: "
                        << socket->GetLocalAddress().ToString() << " and "
                        << remote_addr.ToString();
    return -1;
  }

  {
    int64_t cur_time = TimeMillis();
    size_t network_size = socket->PurgeNetworkPackets(cur_time);

    {
      webrtc::MutexLock lock(&mutex_);
      size_t packet_size = data_size + UDP_HEADER_SIZE;
      if (network_size + packet_size > network_capacity_) {
        RTC_LOG(LS_VERBOSE) << "Dropping packet: network capacity exceeded";
        return static_cast<int>(data_size);
      }
    }

    AddPacketToNetwork(socket, recipient, cur_time, data, data_size,
                       UDP_HEADER_SIZE, false);

    return static_cast<int>(data_size);
  }
}

void VirtualSocketServer::SendTcp(VirtualSocket* socket) {
  {
    webrtc::MutexLock lock(&mutex_);
    ++sent_packets_;
    if (sending_blocked_) {

      return;
    }
  }

  VirtualSocket* recipient =
      LookupConnection(socket->GetLocalAddress(), socket->GetRemoteAddress());
  if (!recipient) {
    RTC_LOG(LS_VERBOSE) << "Sending data to no one.";
    return;
  }

  int64_t cur_time = TimeMillis();
  socket->PurgeNetworkPackets(cur_time);

  while (true) {
    size_t available = recv_buffer_capacity() - recipient->recv_buffer_size();
    size_t max_data_size =
        std::min<size_t>(available, TCP_MSS - TCP_HEADER_SIZE);
    size_t data_size = std::min(socket->send_buffer_size(), max_data_size);
    if (0 == data_size)
      break;

    AddPacketToNetwork(socket, recipient, cur_time, socket->send_buffer_data(),
                       data_size, TCP_HEADER_SIZE, true);
    recipient->UpdateRecv(data_size);
    socket->UpdateSend(data_size);
  }

  socket->MaybeSignalWriteEvent(send_buffer_capacity());
}

void VirtualSocketServer::SendTcp(const SocketAddress& addr) {
  VirtualSocket* sender = LookupBinding(addr);
  RTC_DCHECK(nullptr != sender);
  SendTcp(sender);
}

void VirtualSocketServer::AddPacketToNetwork(VirtualSocket* sender,
                                             VirtualSocket* recipient,
                                             int64_t cur_time,
                                             const char* data,
                                             size_t data_size,
                                             size_t header_size,
                                             bool ordered) {
  RTC_DCHECK(msg_queue_);
  uint32_t send_delay = sender->AddPacket(cur_time, data_size + header_size);

  uint32_t transit_delay = GetTransitDelay(sender);

  SocketAddress sender_addr = sender->GetLocalAddress();
  IPAddress default_ip = GetDefaultSourceAddress(sender_addr.ipaddr().family());
  if (sender_addr.IsAnyIP() && !IPIsUnspec(default_ip)) {
    sender_addr.SetIP(default_ip);
  }

  int64_t ts = cur_time + send_delay + transit_delay;
  if (ordered) {
    ts = sender->UpdateOrderedDelivery(ts);
  }
  recipient->PostPacket(TimeDelta::Millis(ts - cur_time),
                        std::make_unique<Packet>(data, data_size, sender_addr));
}

uint32_t VirtualSocketServer::SendDelay(uint32_t size) {
  webrtc::MutexLock lock(&mutex_);
  if (bandwidth_ == 0)
    return 0;
  else
    return 1000 * size / bandwidth_;
}

#if 0
void PrintFunction(std::vector<std::pair<double, double> >* f) {
  return;
  double sum = 0;
  for (uint32_t i = 0; i < f->size(); ++i) {
    std::cout << (*f)[i].first << '\t' << (*f)[i].second << std::endl;
    sum += (*f)[i].second;
  }
  if (!f->empty()) {
    const double mean = sum / f->size();
    double sum_sq_dev = 0;
    for (uint32_t i = 0; i < f->size(); ++i) {
      double dev = (*f)[i].second - mean;
      sum_sq_dev += dev * dev;
    }
    std::cout << "Mean = " << mean << " StdDev = "
              << sqrt(sum_sq_dev / f->size()) << std::endl;
  }
}
#endif

void VirtualSocketServer::UpdateDelayDistribution() {
  webrtc::MutexLock lock(&mutex_);
  delay_dist_ = CreateDistribution(delay_mean_, delay_stddev_, delay_samples_);
}

static double PI = 4 * atan(1.0);

static double Normal(double x, double mean, double stddev) {
  double a = (x - mean) * (x - mean) / (2 * stddev * stddev);
  return exp(-a) / (stddev * sqrt(2 * PI));
}

#if 0
static double Pareto(double x, double min, double k) {
  if (x < min)
    return 0;
  else
    return k * std::pow(min, k) / std::pow(x, k+1);
}
#endif

std::unique_ptr<VirtualSocketServer::Function>
VirtualSocketServer::CreateDistribution(uint32_t mean,
                                        uint32_t stddev,
                                        uint32_t samples) {
  auto f = std::make_unique<Function>();

  if (0 == stddev) {
    f->push_back(Point(mean, 1.0));
  } else {
    double start = 0;
    if (mean >= 4 * static_cast<double>(stddev))
      start = mean - 4 * static_cast<double>(stddev);
    double end = mean + 4 * static_cast<double>(stddev);

    for (uint32_t i = 0; i < samples; i++) {
      double x = start + (end - start) * i / (samples - 1);
      double y = Normal(x, mean, stddev);
      f->push_back(Point(x, y));
    }
  }
  return Resample(Invert(Accumulate(std::move(f))), 0, 1, samples);
}

uint32_t VirtualSocketServer::GetTransitDelay(Socket* socket) {

  auto iter = delay_by_ip_.find(socket->GetLocalAddress().ipaddr());
  if (iter != delay_by_ip_.end()) {
    return static_cast<uint32_t>(iter->second);
  }

  size_t index = rand() % delay_dist_->size();
  double delay = (*delay_dist_)[index].second;

  return static_cast<uint32_t>(delay);
}

struct FunctionDomainCmp {
  bool operator()(const VirtualSocketServer::Point& p1,
                  const VirtualSocketServer::Point& p2) {
    return p1.first < p2.first;
  }
  bool operator()(double v1, const VirtualSocketServer::Point& p2) {
    return v1 < p2.first;
  }
  bool operator()(const VirtualSocketServer::Point& p1, double v2) {
    return p1.first < v2;
  }
};

std::unique_ptr<VirtualSocketServer::Function> VirtualSocketServer::Accumulate(
    std::unique_ptr<Function> f) {
  RTC_DCHECK(f->size() >= 1);
  double v = 0;
  for (Function::size_type i = 0; i < f->size() - 1; ++i) {
    double dx = (*f)[i + 1].first - (*f)[i].first;
    double avgy = ((*f)[i + 1].second + (*f)[i].second) / 2;
    (*f)[i].second = v;
    v = v + dx * avgy;
  }
  (*f)[f->size() - 1].second = v;
  return f;
}

std::unique_ptr<VirtualSocketServer::Function> VirtualSocketServer::Invert(
    std::unique_ptr<Function> f) {
  for (Function::size_type i = 0; i < f->size(); ++i)
    std::swap((*f)[i].first, (*f)[i].second);

  absl::c_sort(*f, FunctionDomainCmp());
  return f;
}

std::unique_ptr<VirtualSocketServer::Function> VirtualSocketServer::Resample(
    std::unique_ptr<Function> f,
    double x1,
    double x2,
    uint32_t samples) {
  auto g = std::make_unique<Function>();

  for (size_t i = 0; i < samples; i++) {
    double x = x1 + (x2 - x1) * i / (samples - 1);
    double y = Evaluate(f.get(), x);
    g->push_back(Point(x, y));
  }

  return g;
}

double VirtualSocketServer::Evaluate(const Function* f, double x) {
  Function::const_iterator iter =
      absl::c_lower_bound(*f, x, FunctionDomainCmp());
  if (iter == f->begin()) {
    return (*f)[0].second;
  } else if (iter == f->end()) {
    RTC_DCHECK(f->size() >= 1);
    return (*f)[f->size() - 1].second;
  } else if (iter->first == x) {
    return iter->second;
  } else {
    double x1 = (iter - 1)->first;
    double y1 = (iter - 1)->second;
    double x2 = iter->first;
    double y2 = iter->second;
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
  }
}

bool VirtualSocketServer::CanInteractWith(VirtualSocket* local,
                                          VirtualSocket* remote) {
  if (!local || !remote) {
    return false;
  }
  IPAddress local_ip = local->GetLocalAddress().ipaddr();
  IPAddress remote_ip = remote->GetLocalAddress().ipaddr();
  IPAddress local_normalized = local_ip.Normalized();
  IPAddress remote_normalized = remote_ip.Normalized();

  if (local_normalized.family() == remote_normalized.family()) {
    return true;
  }

  int remote_v6_only = 0;
  remote->GetOption(Socket::OPT_IPV6_V6ONLY, &remote_v6_only);
  if (local_ip.family() == AF_INET && !remote_v6_only && IPIsAny(remote_ip)) {
    return true;
  }

  int local_v6_only = 0;
  local->GetOption(Socket::OPT_IPV6_V6ONLY, &local_v6_only);
  if (remote_ip.family() == AF_INET && !local_v6_only && IPIsAny(local_ip)) {
    return true;
  }

  if (local_ip.family() == AF_INET6 && local->was_any()) {
    return true;
  }
  if (remote_ip.family() == AF_INET6 && remote->was_any()) {
    return true;
  }

  return false;
}

IPAddress VirtualSocketServer::GetDefaultSourceAddress(int family) {
  if (family == AF_INET) {
    return default_source_address_v4_;
  }
  if (family == AF_INET6) {
    return default_source_address_v6_;
  }
  return IPAddress();
}
void VirtualSocketServer::SetDefaultSourceAddress(const IPAddress& from_addr) {
  RTC_DCHECK(!IPIsAny(from_addr));
  if (from_addr.family() == AF_INET) {
    default_source_address_v4_ = from_addr;
  } else if (from_addr.family() == AF_INET6) {
    default_source_address_v6_ = from_addr;
  }
}

void VirtualSocketServer::set_bandwidth(uint32_t bandwidth) {
  webrtc::MutexLock lock(&mutex_);
  bandwidth_ = bandwidth;
}
void VirtualSocketServer::set_network_capacity(uint32_t capacity) {
  webrtc::MutexLock lock(&mutex_);
  network_capacity_ = capacity;
}

uint32_t VirtualSocketServer::send_buffer_capacity() const {
  webrtc::MutexLock lock(&mutex_);
  return send_buffer_capacity_;
}
void VirtualSocketServer::set_send_buffer_capacity(uint32_t capacity) {
  webrtc::MutexLock lock(&mutex_);
  send_buffer_capacity_ = capacity;
}

uint32_t VirtualSocketServer::recv_buffer_capacity() const {
  webrtc::MutexLock lock(&mutex_);
  return recv_buffer_capacity_;
}
void VirtualSocketServer::set_recv_buffer_capacity(uint32_t capacity) {
  webrtc::MutexLock lock(&mutex_);
  recv_buffer_capacity_ = capacity;
}

void VirtualSocketServer::set_delay_mean(uint32_t delay_mean) {
  webrtc::MutexLock lock(&mutex_);
  delay_mean_ = delay_mean;
}
void VirtualSocketServer::set_delay_stddev(uint32_t delay_stddev) {
  webrtc::MutexLock lock(&mutex_);
  delay_stddev_ = delay_stddev;
}
void VirtualSocketServer::set_delay_samples(uint32_t delay_samples) {
  webrtc::MutexLock lock(&mutex_);
  delay_samples_ = delay_samples;
}

void VirtualSocketServer::set_drop_probability(double drop_prob) {
  RTC_DCHECK_GE(drop_prob, 0.0);
  RTC_DCHECK_LE(drop_prob, 1.0);

  webrtc::MutexLock lock(&mutex_);
  drop_prob_ = drop_prob;
}

void VirtualSocketServer::set_max_udp_payload(size_t payload_size) {
  webrtc::MutexLock lock(&mutex_);
  max_udp_payload_ = payload_size;
}

uint32_t VirtualSocketServer::sent_packets() const {
  webrtc::MutexLock lock(&mutex_);
  return sent_packets_;
}

}
