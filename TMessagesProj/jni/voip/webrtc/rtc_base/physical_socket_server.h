/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PHYSICAL_SOCKET_SERVER_H_
#define RTC_BASE_PHYSICAL_SOCKET_SERVER_H_

#include "api/async_dns_resolver.h"
#include "api/units/time_delta.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

#if defined(WEBRTC_POSIX)
#if defined(WEBRTC_LINUX)

#include <sys/epoll.h>

#define WEBRTC_USE_EPOLL 1
#elif defined(WEBRTC_FUCHSIA)

#include <poll.h>

#define WEBRTC_USE_POLL 1
#else

#endif
#endif

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "rtc_base/deprecated/recursive_critical_section.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/thread_annotations.h"

#if defined(WEBRTC_POSIX)
typedef int SOCKET;
#endif

namespace rtc {

enum DispatcherEvent {
  DE_READ = 0x0001,
  DE_WRITE = 0x0002,
  DE_CONNECT = 0x0004,
  DE_CLOSE = 0x0008,
  DE_ACCEPT = 0x0010,
};

class Signaler;

class Dispatcher {
 public:
  virtual ~Dispatcher() {}
  virtual uint32_t GetRequestedEvents() = 0;
  virtual void OnEvent(uint32_t ff, int err) = 0;
#if defined(WEBRTC_WIN)
  virtual WSAEVENT GetWSAEvent() = 0;
  virtual SOCKET GetSocket() = 0;
  virtual bool CheckSignalClose() = 0;
#elif defined(WEBRTC_POSIX)
  virtual int GetDescriptor() = 0;
  virtual bool IsDescriptorClosed() = 0;
#endif
};

class RTC_EXPORT PhysicalSocketServer : public SocketServer {
 public:
  PhysicalSocketServer();
  ~PhysicalSocketServer() override;

  Socket* CreateSocket(int family, int type) override;

  virtual Socket* WrapSocket(SOCKET s);

  bool Wait(webrtc::TimeDelta max_wait_duration, bool process_io) override;
  void WakeUp() override;

  void Add(Dispatcher* dispatcher);
  void Remove(Dispatcher* dispatcher);
  void Update(Dispatcher* dispatcher);

 private:

  static constexpr size_t kNumEpollEvents = 128;

  static constexpr int kForeverMs = -1;

  static int ToCmsWait(webrtc::TimeDelta max_wait_duration);

#if defined(WEBRTC_POSIX)
  bool WaitSelect(int cmsWait, bool process_io);

#if defined(WEBRTC_USE_EPOLL)
  void AddEpoll(Dispatcher* dispatcher, uint64_t key);
  void RemoveEpoll(Dispatcher* dispatcher);
  void UpdateEpoll(Dispatcher* dispatcher, uint64_t key);
  bool WaitEpoll(int cmsWait);
  bool WaitPollOneDispatcher(int cmsWait, Dispatcher* dispatcher);

  std::array<epoll_event, kNumEpollEvents> epoll_events_;
  const int epoll_fd_ = INVALID_SOCKET;

#elif defined(WEBRTC_USE_POLL)
  void AddPoll(Dispatcher* dispatcher, uint64_t key);
  void RemovePoll(Dispatcher* dispatcher);
  void UpdatePoll(Dispatcher* dispatcher, uint64_t key);
  bool WaitPoll(int cmsWait, bool process_io);

#endif
#endif

  uint64_t next_dispatcher_key_ RTC_GUARDED_BY(crit_) = 0;
  std::unordered_map<uint64_t, Dispatcher*> dispatcher_by_key_
      RTC_GUARDED_BY(crit_);

  std::unordered_map<Dispatcher*, uint64_t> key_by_dispatcher_
      RTC_GUARDED_BY(crit_);

  std::vector<uint64_t> current_dispatcher_keys_;
  Signaler* signal_wakeup_;
  RecursiveCriticalSection crit_;
#if defined(WEBRTC_WIN)
  const WSAEVENT socket_ev_;
#endif
  bool fWait_;

  bool waiting_ = false;
};

class PhysicalSocket : public Socket, public sigslot::has_slots<> {
 public:
  PhysicalSocket(PhysicalSocketServer* ss, SOCKET s = INVALID_SOCKET);
  ~PhysicalSocket() override;

  virtual bool Create(int family, int type);

  SocketAddress GetLocalAddress() const override;
  SocketAddress GetRemoteAddress() const override;

  int Bind(const SocketAddress& bind_addr) override;
  int Connect(const SocketAddress& addr) override;

  int GetError() const override;
  void SetError(int error) override;

  ConnState GetState() const override;

  int GetOption(Option opt, int* value) override;
  int SetOption(Option opt, int value) override;

  int Send(const void* pv, size_t cb) override;
  int SendTo(const void* buffer,
             size_t length,
             const SocketAddress& addr) override;

  int Recv(void* buffer, size_t length, int64_t* timestamp) override;

  int RecvFrom(void* buffer,
               size_t length,
               SocketAddress* out_addr,
               int64_t* timestamp) override;
  int RecvFrom(ReceiveBuffer& buffer) override;

  int Listen(int backlog) override;
  Socket* Accept(SocketAddress* out_addr) override;

  int Close() override;

  SocketServer* socketserver() { return ss_; }

  SOCKET GetSocketFD() const { return s_; }

 protected:
  int DoConnect(const SocketAddress& connect_addr);

  virtual SOCKET DoAccept(SOCKET socket, sockaddr* addr, socklen_t* addrlen);

  virtual int DoSend(SOCKET socket, const char* buf, int len, int flags);

  virtual int DoSendTo(SOCKET socket,
                       const char* buf,
                       int len,
                       int flags,
                       const struct sockaddr* dest_addr,
                       socklen_t addrlen);

  int DoReadFromSocket(void* buffer,
                       size_t length,
                       SocketAddress* out_addr,
                       int64_t* timestamp);

  void OnResolveResult(const webrtc::AsyncDnsResolverResult& resolver);

  void UpdateLastError();
  void MaybeRemapSendError();

  uint8_t enabled_events() const { return enabled_events_; }
  virtual void SetEnabledEvents(uint8_t events);
  virtual void EnableEvents(uint8_t events);
  virtual void DisableEvents(uint8_t events);

  int TranslateOption(Option opt, int* slevel, int* sopt);

  PhysicalSocketServer* ss_;
  SOCKET s_;
  bool udp_;
  int family_ = 0;
  mutable webrtc::Mutex mutex_;
  int error_ RTC_GUARDED_BY(mutex_);
  ConnState state_;
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> resolver_;

#if !defined(NDEBUG)
  std::string dbg_addr_;
#endif

 private:
  const bool read_scm_timestamp_experiment_;
  uint8_t enabled_events_ = 0;
};

class SocketDispatcher : public Dispatcher, public PhysicalSocket {
 public:
  explicit SocketDispatcher(PhysicalSocketServer* ss);
  SocketDispatcher(SOCKET s, PhysicalSocketServer* ss);
  ~SocketDispatcher() override;

  bool Initialize();

  virtual bool Create(int type);
  bool Create(int family, int type) override;

#if defined(WEBRTC_WIN)
  WSAEVENT GetWSAEvent() override;
  SOCKET GetSocket() override;
  bool CheckSignalClose() override;
#elif defined(WEBRTC_POSIX)
  int GetDescriptor() override;
  bool IsDescriptorClosed() override;
#endif

  uint32_t GetRequestedEvents() override;
  void OnEvent(uint32_t ff, int err) override;

  int Close() override;

#if defined(WEBRTC_USE_EPOLL)
 protected:
  void StartBatchedEventUpdates();
  void FinishBatchedEventUpdates();

  void SetEnabledEvents(uint8_t events) override;
  void EnableEvents(uint8_t events) override;
  void DisableEvents(uint8_t events) override;
#endif

 private:
#if defined(WEBRTC_WIN)
  static int next_id_;
  int id_;
  bool signal_close_;
  int signal_err_;
#endif
#if defined(WEBRTC_USE_EPOLL)
  void MaybeUpdateDispatcher(uint8_t old_events);

  int saved_enabled_events_ = -1;
#endif
};

}

#endif
