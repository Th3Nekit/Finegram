/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SOCKET_ADDRESS_H_
#define RTC_BASE_SOCKET_ADDRESS_H_

#include <string>

#include "absl/strings/string_view.h"
#ifdef WEBRTC_UNIT_TEST
#include <ostream>
#endif
#include "rtc_base/ip_address.h"
#include "rtc_base/system/rtc_export.h"

#undef SetPort

struct sockaddr_in;
struct sockaddr_storage;

namespace rtc {

class RTC_EXPORT SocketAddress {
 public:

  SocketAddress();

  SocketAddress(absl::string_view hostname, int port);

  SocketAddress(uint32_t ip_as_host_order_integer, int port);

  SocketAddress(const IPAddress& ip, int port);

  SocketAddress(const SocketAddress& addr);

  void Clear();

  bool IsNil() const;

  bool IsComplete() const;

  SocketAddress& operator=(const SocketAddress& addr);

  void SetIP(uint32_t ip_as_host_order_integer);

  void SetIP(const IPAddress& ip);

  void SetIP(absl::string_view hostname);

  void SetResolvedIP(uint32_t ip_as_host_order_integer);

  void SetResolvedIP(const IPAddress& ip);

  void SetPort(int port);

  const std::string& hostname() const { return hostname_; }

  uint32_t ip() const;

  const IPAddress& ipaddr() const;

  int family() const { return ip_.family(); }

  uint16_t port() const;

  int scope_id() const { return scope_id_; }
  void SetScopeID(int id) { scope_id_ = id; }

  std::string HostAsURIString() const;

  std::string HostAsSensitiveURIString() const;

  std::string PortAsString() const;

  std::string ToString() const;

  std::string ToSensitiveString() const;

  std::string ToSensitiveNameAndAddressString() const;

  bool FromString(absl::string_view str);

#ifdef WEBRTC_UNIT_TEST
  inline std::ostream& operator<<(
      std::ostream& os) {
    return os << HostAsURIString() << ":" << port();
  }
#endif

  bool IsAnyIP() const;

  bool IsLoopbackIP() const;

  bool IsPrivateIP() const;

  bool IsUnresolvedIP() const;

  bool operator==(const SocketAddress& addr) const;
  inline bool operator!=(const SocketAddress& addr) const {
    return !this->operator==(addr);
  }

  bool operator<(const SocketAddress& addr) const;

  bool EqualIPs(const SocketAddress& addr) const;

  bool EqualPorts(const SocketAddress& addr) const;

  size_t Hash() const;

  void ToSockAddr(sockaddr_in* saddr) const;

  bool FromSockAddr(const sockaddr_in& saddr);

  size_t ToDualStackSockAddrStorage(sockaddr_storage* saddr) const;
  size_t ToSockAddrStorage(sockaddr_storage* saddr) const;

 private:
  std::string hostname_;
  IPAddress ip_;
  uint16_t port_;
  int scope_id_;
  bool literal_;
};

RTC_EXPORT bool SocketAddressFromSockAddrStorage(const sockaddr_storage& saddr,
                                                 SocketAddress* out);
SocketAddress EmptySocketAddressWithFamily(int family);

}

#endif
