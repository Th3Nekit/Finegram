/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SOCKET_H_
#define RTC_BASE_SOCKET_H_

#include <errno.h>

#include "absl/types/optional.h"
#include "rtc_base/checks.h"

#if defined(WEBRTC_POSIX)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#define SOCKET_EACCES EACCES
#endif

#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif

#include "api/units/timestamp.h"
#include "rtc_base/buffer.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

#if defined(WEBRTC_WIN)
#undef EWOULDBLOCK  // Remove errno.h's definition for each macro below.
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#undef EALREADY
#define EALREADY WSAEALREADY
#undef EMSGSIZE
#define EMSGSIZE WSAEMSGSIZE
#undef EADDRINUSE
#define EADDRINUSE WSAEADDRINUSE
#undef EADDRNOTAVAIL
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#undef ENETDOWN
#define ENETDOWN WSAENETDOWN
#undef ECONNABORTED
#define ECONNABORTED WSAECONNABORTED
#undef ENOBUFS
#define ENOBUFS WSAENOBUFS
#undef EISCONN
#define EISCONN WSAEISCONN
#undef ENOTCONN
#define ENOTCONN WSAENOTCONN
#undef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#undef EHOSTUNREACH
#define EHOSTUNREACH WSAEHOSTUNREACH
#undef ENETUNREACH
#define ENETUNREACH WSAENETUNREACH
#define SOCKET_EACCES WSAEACCES
#endif

#if defined(WEBRTC_POSIX)
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket(s) close(s)
#endif

namespace rtc {

inline bool IsBlockingError(int e) {
  return (e == EWOULDBLOCK) || (e == EAGAIN) || (e == EINPROGRESS);
}

class RTC_EXPORT Socket {
 public:
  struct ReceiveBuffer {
    ReceiveBuffer(Buffer& payload) : payload(payload) {}

    absl::optional<webrtc::Timestamp> arrival_time;
    SocketAddress source_address;
    Buffer& payload;
  };
  virtual ~Socket() {}

  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  virtual SocketAddress GetLocalAddress() const = 0;

  virtual SocketAddress GetRemoteAddress() const = 0;

  virtual int Bind(const SocketAddress& addr) = 0;
  virtual int Connect(const SocketAddress& addr) = 0;
  virtual int Send(const void* pv, size_t cb) = 0;
  virtual int SendTo(const void* pv, size_t cb, const SocketAddress& addr) = 0;

  virtual int Recv(void* pv, size_t cb, int64_t* timestamp) = 0;

  virtual int RecvFrom(void* pv,
                       size_t cb,
                       SocketAddress* paddr,
                       int64_t* timestamp) {

    RTC_CHECK_NOTREACHED();
  }

  virtual int RecvFrom(ReceiveBuffer& buffer);
  virtual int Listen(int backlog) = 0;
  virtual Socket* Accept(SocketAddress* paddr) = 0;
  virtual int Close() = 0;
  virtual int GetError() const = 0;
  virtual void SetError(int error) = 0;
  inline bool IsBlocking() const { return IsBlockingError(GetError()); }

  enum ConnState { CS_CLOSED, CS_CONNECTING, CS_CONNECTED };
  virtual ConnState GetState() const = 0;

  enum Option {
    OPT_DONTFRAGMENT,
    OPT_RCVBUF,
    OPT_SNDBUF,
    OPT_NODELAY,
    OPT_IPV6_V6ONLY,
    OPT_DSCP,
    OPT_RTP_SENDTIME_EXTN_ID,

  };
  virtual int GetOption(Option opt, int* value) = 0;
  virtual int SetOption(Option opt, int value) = 0;

  sigslot::signal1<Socket*, sigslot::multi_threaded_local> SignalReadEvent;

  sigslot::signal1<Socket*, sigslot::multi_threaded_local> SignalWriteEvent;
  sigslot::signal1<Socket*> SignalConnectEvent;
  sigslot::signal2<Socket*, int> SignalCloseEvent;

 protected:
  Socket() {}
};

}

#endif
