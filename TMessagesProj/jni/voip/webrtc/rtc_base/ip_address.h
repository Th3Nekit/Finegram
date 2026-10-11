/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_IP_ADDRESS_H_
#define RTC_BASE_IP_ADDRESS_H_

#if defined(WEBRTC_POSIX)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "absl/strings/string_view.h"
#endif
#if defined(WEBRTC_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <string.h>

#include <string>

#include "rtc_base/byte_order.h"
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif
#include "absl/strings/string_view.h"
#include "rtc_base/system/rtc_export.h"
namespace rtc {

enum IPv6AddressFlag {
  IPV6_ADDRESS_FLAG_NONE = 0x00,

  IPV6_ADDRESS_FLAG_TEMPORARY = 1 << 0,

  IPV6_ADDRESS_FLAG_DEPRECATED = 1 << 1,
};

class RTC_EXPORT IPAddress {
 public:
  IPAddress() : family_(AF_UNSPEC) { ::memset(&u_, 0, sizeof(u_)); }

  explicit IPAddress(const in_addr& ip4) : family_(AF_INET) {
    memset(&u_, 0, sizeof(u_));
    u_.ip4 = ip4;
  }

  explicit IPAddress(const in6_addr& ip6) : family_(AF_INET6) { u_.ip6 = ip6; }

  explicit IPAddress(uint32_t ip_in_host_byte_order) : family_(AF_INET) {
    memset(&u_, 0, sizeof(u_));
    u_.ip4.s_addr = HostToNetwork32(ip_in_host_byte_order);
  }

  IPAddress(const IPAddress& other) : family_(other.family_) {
    ::memcpy(&u_, &other.u_, sizeof(u_));
  }

  virtual ~IPAddress() {}

  const IPAddress& operator=(const IPAddress& other) {
    family_ = other.family_;
    ::memcpy(&u_, &other.u_, sizeof(u_));
    return *this;
  }

  bool operator==(const IPAddress& other) const;
  bool operator!=(const IPAddress& other) const;
  bool operator<(const IPAddress& other) const;
  bool operator>(const IPAddress& other) const;

#ifdef WEBRTC_UNIT_TEST
  inline std::ostream& operator<<(
      std::ostream& os) {
    return os << ToString();
  }
#endif

  int family() const { return family_; }
  in_addr ipv4_address() const;
  in6_addr ipv6_address() const;

  size_t Size() const;

  std::string ToString() const;

  std::string ToSensitiveString() const;

  IPAddress Normalized() const;

  IPAddress AsIPv6Address() const;

  uint32_t v4AddressAsHostOrderInteger() const;

  int overhead() const;

  bool IsNil() const;

 private:
  int family_;
  union {
    in_addr ip4;
    in6_addr ip6;
  } u_;
};

class RTC_EXPORT InterfaceAddress : public IPAddress {
 public:
  InterfaceAddress() : ipv6_flags_(IPV6_ADDRESS_FLAG_NONE) {}

  explicit InterfaceAddress(IPAddress ip)
      : IPAddress(ip), ipv6_flags_(IPV6_ADDRESS_FLAG_NONE) {}

  InterfaceAddress(IPAddress addr, int ipv6_flags)
      : IPAddress(addr), ipv6_flags_(ipv6_flags) {}

  InterfaceAddress(const in6_addr& ip6, int ipv6_flags)
      : IPAddress(ip6), ipv6_flags_(ipv6_flags) {}

  InterfaceAddress(const InterfaceAddress& other) = default;
  const InterfaceAddress& operator=(const InterfaceAddress& other);

  bool operator==(const InterfaceAddress& other) const;
  bool operator!=(const InterfaceAddress& other) const;

  int ipv6_flags() const { return ipv6_flags_; }

  std::string ToString() const;

 private:
  int ipv6_flags_;
};

bool IPFromAddrInfo(struct addrinfo* info, IPAddress* out);
RTC_EXPORT bool IPFromString(absl::string_view str, IPAddress* out);
RTC_EXPORT bool IPFromString(absl::string_view str,
                             int flags,
                             InterfaceAddress* out);
bool IPIsAny(const IPAddress& ip);
RTC_EXPORT bool IPIsLoopback(const IPAddress& ip);
RTC_EXPORT bool IPIsLinkLocal(const IPAddress& ip);

bool IPIsPrivateNetwork(const IPAddress& ip);

bool IPIsSharedNetwork(const IPAddress& ip);

RTC_EXPORT bool IPIsPrivate(const IPAddress& ip);
RTC_EXPORT bool IPIsUnspec(const IPAddress& ip);
size_t HashIP(const IPAddress& ip);

bool IPIs6Bone(const IPAddress& ip);
bool IPIs6To4(const IPAddress& ip);
RTC_EXPORT bool IPIsMacBased(const IPAddress& ip);
bool IPIsSiteLocal(const IPAddress& ip);
bool IPIsTeredo(const IPAddress& ip);
bool IPIsULA(const IPAddress& ip);
bool IPIsV4Compatibility(const IPAddress& ip);
bool IPIsV4Mapped(const IPAddress& ip);

int IPAddressPrecedence(const IPAddress& ip);

RTC_EXPORT IPAddress TruncateIP(const IPAddress& ip, int length);

IPAddress GetLoopbackIP(int family);
IPAddress GetAnyIP(int family);

int CountIPMaskBits(const IPAddress& mask);

}

#endif
