/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NAT_SERVER_H_
#define RTC_BASE_NAT_SERVER_H_

#include <map>
#include <set>

#include "rtc_base/async_udp_socket.h"
#include "rtc_base/nat_types.h"
#include "rtc_base/proxy_server.h"
#include "rtc_base/socket_address_pair.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread.h"

namespace rtc {

struct RouteCmp {
  explicit RouteCmp(NAT* nat);
  size_t operator()(const SocketAddressPair& r) const;
  bool operator()(const SocketAddressPair& r1,
                  const SocketAddressPair& r2) const;

  bool symmetric;
};

struct AddrCmp {
  explicit AddrCmp(NAT* nat);
  size_t operator()(const SocketAddress& r) const;
  bool operator()(const SocketAddress& r1, const SocketAddress& r2) const;

  bool use_ip;
  bool use_port;
};

const int NAT_SERVER_UDP_PORT = 4237;
const int NAT_SERVER_TCP_PORT = 4238;

class NATServer {
 public:
  NATServer(NATType type,
            rtc::Thread& internal_socket_thread,
            SocketFactory* internal,
            const SocketAddress& internal_udp_addr,
            const SocketAddress& internal_tcp_addr,
            rtc::Thread& external_socket_thread,
            SocketFactory* external,
            const SocketAddress& external_ip);
  ~NATServer();

  NATServer(const NATServer&) = delete;
  NATServer& operator=(const NATServer&) = delete;

  SocketAddress internal_udp_address() const {
    return udp_server_socket_->GetLocalAddress();
  }

  SocketAddress internal_tcp_address() const {
    return tcp_proxy_server_->GetServerAddress();
  }

  void OnInternalUDPPacket(AsyncPacketSocket* socket,
                           const rtc::ReceivedPacket& packet);
  void OnExternalUDPPacket(AsyncPacketSocket* socket,
                           const rtc::ReceivedPacket& packet);

 private:
  typedef std::set<SocketAddress, AddrCmp> AddressSet;

  struct TransEntry {
    TransEntry(const SocketAddressPair& r, AsyncUDPSocket* s, NAT* nat);
    ~TransEntry();

    void AllowlistInsert(const SocketAddress& addr);
    bool AllowlistContains(const SocketAddress& ext_addr);

    SocketAddressPair route;
    AsyncUDPSocket* socket;
    AddressSet* allowlist;
    webrtc::Mutex mutex_;
  };

  typedef std::map<SocketAddressPair, TransEntry*, RouteCmp> InternalMap;
  typedef std::map<SocketAddress, TransEntry*> ExternalMap;

  void Translate(const SocketAddressPair& route);

  bool ShouldFilterOut(TransEntry* entry, const SocketAddress& ext_addr);

  NAT* nat_;
  rtc::Thread& internal_socket_thread_;
  rtc::Thread& external_socket_thread_;
  SocketFactory* external_;
  SocketAddress external_ip_;
  AsyncUDPSocket* udp_server_socket_;
  ProxyServer* tcp_proxy_server_;
  InternalMap* int_map_;
  ExternalMap* ext_map_;
};

}

#endif
