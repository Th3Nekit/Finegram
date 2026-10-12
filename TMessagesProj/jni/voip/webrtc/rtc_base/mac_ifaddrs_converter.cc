/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/ifaddrs_converter.h"
#include "rtc_base/logging.h"

#if !defined(WEBRTC_IOS)
#include <net/if_media.h>
#include <netinet/in_var.h>
#else
#define SCOPE6_ID_MAX 16

struct in6_addrlifetime {
  time_t ia6t_expire;
  time_t ia6t_preferred;
  u_int32_t ia6t_vltime;
  u_int32_t ia6t_pltime;
};

struct in6_ifstat {
  u_quad_t ifs6_in_receive;
  u_quad_t ifs6_in_hdrerr;
  u_quad_t ifs6_in_toobig;
  u_quad_t ifs6_in_noroute;
  u_quad_t ifs6_in_addrerr;
  u_quad_t ifs6_in_protounknown;

  u_quad_t ifs6_in_truncated;
  u_quad_t ifs6_in_discard;

  u_quad_t ifs6_in_deliver;

  u_quad_t ifs6_out_forward;

  u_quad_t ifs6_out_request;

  u_quad_t ifs6_out_discard;
  u_quad_t ifs6_out_fragok;
  u_quad_t ifs6_out_fragfail;
  u_quad_t ifs6_out_fragcreat;

  u_quad_t ifs6_reass_reqd;

  u_quad_t ifs6_reass_ok;

  u_quad_t ifs6_reass_fail;

  u_quad_t ifs6_in_mcast;
  u_quad_t ifs6_out_mcast;
};
struct icmp6_ifstat {

  u_quad_t ifs6_in_msg;

  u_quad_t ifs6_in_error;

  u_quad_t ifs6_in_dstunreach;

  u_quad_t ifs6_in_adminprohib;

  u_quad_t ifs6_in_timeexceed;

  u_quad_t ifs6_in_paramprob;

  u_quad_t ifs6_in_pkttoobig;

  u_quad_t ifs6_in_echo;

  u_quad_t ifs6_in_echoreply;

  u_quad_t ifs6_in_routersolicit;

  u_quad_t ifs6_in_routeradvert;

  u_quad_t ifs6_in_neighborsolicit;

  u_quad_t ifs6_in_neighboradvert;

  u_quad_t ifs6_in_redirect;

  u_quad_t ifs6_in_mldquery;

  u_quad_t ifs6_in_mldreport;

  u_quad_t ifs6_in_mlddone;

  u_quad_t ifs6_out_msg;

  u_quad_t ifs6_out_error;

  u_quad_t ifs6_out_dstunreach;

  u_quad_t ifs6_out_adminprohib;

  u_quad_t ifs6_out_timeexceed;

  u_quad_t ifs6_out_paramprob;

  u_quad_t ifs6_out_pkttoobig;

  u_quad_t ifs6_out_echo;

  u_quad_t ifs6_out_echoreply;

  u_quad_t ifs6_out_routersolicit;

  u_quad_t ifs6_out_routeradvert;

  u_quad_t ifs6_out_neighborsolicit;

  u_quad_t ifs6_out_neighboradvert;

  u_quad_t ifs6_out_redirect;

  u_quad_t ifs6_out_mldquery;

  u_quad_t ifs6_out_mldreport;

  u_quad_t ifs6_out_mlddone;
};

struct in6_ifreq {
  char ifr_name[IFNAMSIZ];
  union {
    struct sockaddr_in6 ifru_addr;
    struct sockaddr_in6 ifru_dstaddr;
    int ifru_flags;
    int ifru_flags6;
    int ifru_metric;
    int ifru_intval;
    caddr_t ifru_data;
    struct in6_addrlifetime ifru_lifetime;
    struct in6_ifstat ifru_stat;
    struct icmp6_ifstat ifru_icmp6stat;
    u_int32_t ifru_scope_id[SCOPE6_ID_MAX];
  } ifr_ifru;
};

#define SIOCGIFAFLAG_IN6 _IOWR('i', 73, struct in6_ifreq)

#define IN6_IFF_ANYCAST 0x0001
#define IN6_IFF_TENTATIVE 0x0002
#define IN6_IFF_DUPLICATED 0x0004
#define IN6_IFF_DETACHED 0x0008
#define IN6_IFF_DEPRECATED 0x0010
#define IN6_IFF_TEMPORARY 0x0080

#endif

namespace rtc {

namespace {

class IPv6AttributesGetter {
 public:
  IPv6AttributesGetter();
  virtual ~IPv6AttributesGetter();
  bool IsInitialized() const;
  bool GetIPAttributes(const char* ifname,
                       const sockaddr* sock_addr,
                       int* native_attributes);

 private:

  int ioctl_socket_;
};

IPv6AttributesGetter::IPv6AttributesGetter()
    : ioctl_socket_(
          socket(AF_INET6, SOCK_DGRAM, 0                           )) {
  RTC_DCHECK_GE(ioctl_socket_, 0);
}

bool IPv6AttributesGetter::IsInitialized() const {
  return ioctl_socket_ >= 0;
}

IPv6AttributesGetter::~IPv6AttributesGetter() {
  if (!IsInitialized()) {
    return;
  }
  close(ioctl_socket_);
}

bool IPv6AttributesGetter::GetIPAttributes(const char* ifname,
                                           const sockaddr* sock_addr,
                                           int* native_attributes) {
  if (!IsInitialized()) {
    return false;
  }

  struct in6_ifreq ifr = {};
  strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
  memcpy(&ifr.ifr_ifru.ifru_addr, sock_addr, sock_addr->sa_len);
  int rv = ioctl(ioctl_socket_, SIOCGIFAFLAG_IN6, &ifr);
  if (rv >= 0) {
    *native_attributes = ifr.ifr_ifru.ifru_flags;
  } else {
    RTC_LOG(LS_ERROR) << "ioctl returns " << errno;
  }
  return (rv >= 0);
}

bool ConvertNativeToIPAttributes(int native_attributes, int* net_attributes) {

  if (native_attributes & (IN6_IFF_ANYCAST | IN6_IFF_DUPLICATED |
                           IN6_IFF_TENTATIVE | IN6_IFF_DETACHED)) {
    return false;
  }

  if (native_attributes & IN6_IFF_TEMPORARY) {
    *net_attributes |= IPV6_ADDRESS_FLAG_TEMPORARY;
  }

  if (native_attributes & IN6_IFF_DEPRECATED) {
    *net_attributes |= IPV6_ADDRESS_FLAG_DEPRECATED;
  }

  return true;
}

class MacIfAddrsConverter : public IfAddrsConverter {
 public:
  MacIfAddrsConverter() : ip_attribute_getter_(new IPv6AttributesGetter()) {}
  ~MacIfAddrsConverter() override {}

  bool ConvertNativeAttributesToIPAttributes(const struct ifaddrs* interface,
                                             int* ip_attributes) override {
    int native_attributes;
    if (!ip_attribute_getter_->GetIPAttributes(
            interface->ifa_name, interface->ifa_addr, &native_attributes)) {
      return false;
    }

    if (!ConvertNativeToIPAttributes(native_attributes, ip_attributes)) {
      return false;
    }

    return true;
  }

 private:
  std::unique_ptr<IPv6AttributesGetter> ip_attribute_getter_;
};

}

IfAddrsConverter* CreateIfAddrsConverter() {
  return new MacIfAddrsConverter();
}

}
