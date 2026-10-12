/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_VIRTUAL_SOCKET_SERVER_H_
#define RTC_BASE_VIRTUAL_SOCKET_SERVER_H_

#include <deque>
#include <map>
#include <vector>

#include "absl/types/optional.h"
#include "api/make_ref_counted.h"
#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/task_queue_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/mutex.h"

namespace rtc {

class Packet;
class VirtualSocketServer;
class SocketAddressPair;

class VirtualSocket : public Socket, public sigslot::has_slots<> {
 public:
  VirtualSocket(VirtualSocketServer* server, int family, int type);
  ~VirtualSocket() override;

  SocketAddress GetLocalAddress() const override;
  SocketAddress GetRemoteAddress() const override;

  int Bind(const SocketAddress& addr) override;
  int Connect(const SocketAddress& addr) override;
  int Close() override;
  int Send(const void* pv, size_t cb) override;
  int SendTo(const void* pv, size_t cb, const SocketAddress& addr) override;
  int Recv(void* pv, size_t cb, int64_t* timestamp) override;
  int RecvFrom(void* pv,
               size_t cb,
               SocketAddress* paddr,
               int64_t* timestamp) override;
  int Listen(int backlog) override;
  VirtualSocket* Accept(SocketAddress* paddr) override;

  int GetError() const override;
  void SetError(int error) override;
  ConnState GetState() const override;
  int GetOption(Option opt, int* value) override;
  int SetOption(Option opt, int value) override;

  size_t recv_buffer_size() const { return recv_buffer_size_; }
  size_t send_buffer_size() const { return send_buffer_.size(); }
  const char* send_buffer_data() const { return send_buffer_.data(); }

  void SetLocalAddress(const SocketAddress& addr);

  bool was_any() { return was_any_; }
  void set_was_any(bool was_any) { was_any_ = was_any; }

  void SetToBlocked();

  void UpdateRecv(size_t data_size);
  void UpdateSend(size_t data_size);

  void MaybeSignalWriteEvent(size_t capacity);

  uint32_t AddPacket(int64_t cur_time, size_t packet_size);

  int64_t UpdateOrderedDelivery(int64_t ts);

  size_t PurgeNetworkPackets(int64_t cur_time);

  void PostPacket(webrtc::TimeDelta delay, std::unique_ptr<Packet> packet);
  void PostConnect(webrtc::TimeDelta delay, const SocketAddress& remote_addr);
  void PostDisconnect(webrtc::TimeDelta delay);

 private:

  class SafetyBlock : public RefCountedNonVirtual<SafetyBlock> {
   public:
    explicit SafetyBlock(VirtualSocket* socket);
    SafetyBlock(const SafetyBlock&) = delete;
    SafetyBlock& operator=(const SafetyBlock&) = delete;
    ~SafetyBlock();

    void SetNotAlive();
    bool IsAlive();

    int RecvFrom(void* buffer, size_t size, SocketAddress& addr);

    void Listen();

    struct AcceptResult {
      int error = 0;
      std::unique_ptr<VirtualSocket> socket;
      SocketAddress remote_addr;
    };
    AcceptResult Accept();

    bool AddPacket(std::unique_ptr<Packet> packet);
    void PostConnect(webrtc::TimeDelta delay, const SocketAddress& remote_addr);

   private:
    enum class Signal { kNone, kReadEvent, kConnectEvent };

    using PostedConnects = std::list<SocketAddress>;

    void PostSignalReadEvent() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    void MaybeSignalReadEvent();
    Signal Connect(PostedConnects::iterator remote_addr_it);

    webrtc::Mutex mutex_;
    VirtualSocket& socket_;
    bool alive_ RTC_GUARDED_BY(mutex_) = true;

    bool pending_read_signal_event_ RTC_GUARDED_BY(mutex_) = false;

    PostedConnects posted_connects_ RTC_GUARDED_BY(mutex_);

    std::list<std::unique_ptr<Packet>> recv_buffer_ RTC_GUARDED_BY(mutex_);

    absl::optional<std::deque<SocketAddress>> listen_queue_
        RTC_GUARDED_BY(mutex_);
  };

  struct NetworkEntry {
    size_t size;
    int64_t done_time;
  };

  typedef std::deque<NetworkEntry> NetworkQueue;
  typedef std::vector<char> SendBuffer;
  typedef std::map<Option, int> OptionsMap;

  int InitiateConnect(const SocketAddress& addr, bool use_delay);
  void CompleteConnect(const SocketAddress& addr);
  int SendUdp(const void* pv, size_t cb, const SocketAddress& addr);
  int SendTcp(const void* pv, size_t cb);

  void OnSocketServerReadyToSend();

  VirtualSocketServer* const server_;
  const int type_;
  ConnState state_;
  int error_;
  SocketAddress local_addr_;
  SocketAddress remote_addr_;

  const scoped_refptr<SafetyBlock> safety_ =
      make_ref_counted<SafetyBlock>(this);

  SendBuffer send_buffer_;

  bool ready_to_send_ = true;

  NetworkQueue network_;
  size_t network_size_;

  int64_t last_delivery_time_ = 0;

  size_t recv_buffer_size_;

  bool bound_;

  bool was_any_;

  OptionsMap options_map_;
};

class VirtualSocketServer : public SocketServer {
 public:
  VirtualSocketServer();

  explicit VirtualSocketServer(ThreadProcessingFakeClock* fake_clock);
  ~VirtualSocketServer() override;

  VirtualSocketServer(const VirtualSocketServer&) = delete;
  VirtualSocketServer& operator=(const VirtualSocketServer&) = delete;

  IPAddress GetDefaultSourceAddress(int family);
  void SetDefaultSourceAddress(const IPAddress& from_addr);

  void set_bandwidth(uint32_t bandwidth) RTC_LOCKS_EXCLUDED(mutex_);

  void set_network_capacity(uint32_t capacity) RTC_LOCKS_EXCLUDED(mutex_);

  uint32_t send_buffer_capacity() const RTC_LOCKS_EXCLUDED(mutex_);
  void set_send_buffer_capacity(uint32_t capacity) RTC_LOCKS_EXCLUDED(mutex_);

  uint32_t recv_buffer_capacity() const RTC_LOCKS_EXCLUDED(mutex_);
  void set_recv_buffer_capacity(uint32_t capacity) RTC_LOCKS_EXCLUDED(mutex_);

  void set_delay_mean(uint32_t delay_mean) RTC_LOCKS_EXCLUDED(mutex_);
  void set_delay_stddev(uint32_t delay_stddev) RTC_LOCKS_EXCLUDED(mutex_);
  void set_delay_samples(uint32_t delay_samples) RTC_LOCKS_EXCLUDED(mutex_);

  void UpdateDelayDistribution() RTC_LOCKS_EXCLUDED(mutex_);

  void set_drop_probability(double drop_prob) RTC_LOCKS_EXCLUDED(mutex_);

  void set_max_udp_payload(size_t payload_size) RTC_LOCKS_EXCLUDED(mutex_);

  void SetSendingBlocked(bool blocked) RTC_LOCKS_EXCLUDED(mutex_);

  VirtualSocket* CreateSocket(int family, int type) override;

  void SetMessageQueue(Thread* queue) override;
  bool Wait(webrtc::TimeDelta max_wait_duration, bool process_io) override;
  void WakeUp() override;

  void SetDelayOnAddress(const rtc::SocketAddress& address, int delay_ms) {
    delay_by_ip_[address.ipaddr()] = delay_ms;
  }

  void SetAlternativeLocalAddress(const rtc::IPAddress& address,
                                  const rtc::IPAddress& alternative);

  typedef std::pair<double, double> Point;
  typedef std::vector<Point> Function;

  static std::unique_ptr<Function> CreateDistribution(uint32_t mean,
                                                      uint32_t stddev,
                                                      uint32_t samples);

  bool ProcessMessagesUntilIdle();

  void SetNextPortForTesting(uint16_t port);

  bool CloseTcpConnections(const SocketAddress& addr_local,
                           const SocketAddress& addr_remote);

  uint32_t sent_packets() const RTC_LOCKS_EXCLUDED(mutex_);

  SocketAddress AssignBindAddress(const SocketAddress& app_addr);

  int Bind(VirtualSocket* socket, const SocketAddress& addr);

  int Unbind(const SocketAddress& addr, VirtualSocket* socket);

  void AddConnection(const SocketAddress& client,
                     const SocketAddress& server,
                     VirtualSocket* socket);

  int Connect(VirtualSocket* socket,
              const SocketAddress& remote_addr,
              bool use_delay);

  bool Disconnect(VirtualSocket* socket);

  bool Disconnect(const SocketAddress& addr);

  bool Disconnect(const SocketAddress& local_addr,
                  const SocketAddress& remote_addr);

  int SendUdp(VirtualSocket* socket,
              const char* data,
              size_t data_size,
              const SocketAddress& remote_addr);

  void SendTcp(VirtualSocket* socket) RTC_LOCKS_EXCLUDED(mutex_);

  void SendTcp(const SocketAddress& addr) RTC_LOCKS_EXCLUDED(mutex_);

  uint32_t SendDelay(uint32_t size) RTC_LOCKS_EXCLUDED(mutex_);

  sigslot::signal0<> SignalReadyToSend;

 protected:

  IPAddress GetNextIP(int family);

  VirtualSocket* LookupBinding(const SocketAddress& addr);

 private:
  friend VirtualSocket;
  uint16_t GetNextPort();

  VirtualSocket* LookupConnection(const SocketAddress& client,
                                  const SocketAddress& server);

  void RemoveConnection(const SocketAddress& client,
                        const SocketAddress& server);

  void AddPacketToNetwork(VirtualSocket* socket,
                          VirtualSocket* recipient,
                          int64_t cur_time,
                          const char* data,
                          size_t data_size,
                          size_t header_size,
                          bool ordered);

  uint32_t GetTransitDelay(Socket* socket);

  static std::unique_ptr<Function> Accumulate(std::unique_ptr<Function> f);
  static std::unique_ptr<Function> Invert(std::unique_ptr<Function> f);
  static std::unique_ptr<Function> Resample(std::unique_ptr<Function> f,
                                            double x1,
                                            double x2,
                                            uint32_t samples);
  static double Evaluate(const Function* f, double x);

  static bool CanInteractWith(VirtualSocket* local, VirtualSocket* remote);

  typedef std::map<SocketAddress, VirtualSocket*> AddressMap;
  typedef std::map<SocketAddressPair, VirtualSocket*> ConnectionMap;

  ThreadProcessingFakeClock* fake_clock_ = nullptr;

  Event wakeup_;
  Thread* msg_queue_;
  bool stop_on_idle_;
  in_addr next_ipv4_;
  in6_addr next_ipv6_;
  uint16_t next_port_;
  AddressMap* bindings_;
  ConnectionMap* connections_;

  IPAddress default_source_address_v4_;
  IPAddress default_source_address_v6_;

  mutable webrtc::Mutex mutex_;

  uint32_t bandwidth_ RTC_GUARDED_BY(mutex_);
  uint32_t network_capacity_ RTC_GUARDED_BY(mutex_);
  uint32_t send_buffer_capacity_ RTC_GUARDED_BY(mutex_);
  uint32_t recv_buffer_capacity_ RTC_GUARDED_BY(mutex_);
  uint32_t delay_mean_ RTC_GUARDED_BY(mutex_);
  uint32_t delay_stddev_ RTC_GUARDED_BY(mutex_);
  uint32_t delay_samples_ RTC_GUARDED_BY(mutex_);

  uint32_t sent_packets_ RTC_GUARDED_BY(mutex_) = 0;

  std::map<rtc::IPAddress, int> delay_by_ip_;
  std::map<rtc::IPAddress, rtc::IPAddress> alternative_address_mapping_;
  std::unique_ptr<Function> delay_dist_;

  double drop_prob_ RTC_GUARDED_BY(mutex_);

  size_t max_udp_payload_ RTC_GUARDED_BY(mutex_) = 65507;

  bool sending_blocked_ RTC_GUARDED_BY(mutex_) = false;
};

}

#endif
