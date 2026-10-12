/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_PACKET_SOCKET_FACTORY_H_
#define API_PACKET_SOCKET_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "api/async_dns_resolver.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/proxy_info.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

class SSLCertificateVerifier;
class AsyncResolverInterface;

struct PacketSocketTcpOptions {
  PacketSocketTcpOptions() = default;
  ~PacketSocketTcpOptions() = default;

  int opts = 0;
  std::vector<std::string> tls_alpn_protocols;
  std::vector<std::string> tls_elliptic_curves;

  SSLCertificateVerifier* tls_cert_verifier = nullptr;
};

class RTC_EXPORT PacketSocketFactory {
 public:
  enum Options {
    OPT_STUN = 0x04,

    OPT_TLS = 0x02,
    OPT_TLS_FAKE = 0x01,
    OPT_TLS_INSECURE = 0x08,

    OPT_SSLTCP = OPT_TLS_FAKE,
  };

  PacketSocketFactory() = default;
  virtual ~PacketSocketFactory() = default;

  virtual AsyncPacketSocket* CreateUdpSocket(const SocketAddress& address,
                                             uint16_t min_port,
                                             uint16_t max_port) = 0;
  virtual AsyncListenSocket* CreateServerTcpSocket(
      const SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) = 0;

  virtual AsyncPacketSocket* CreateClientTcpSocket(
      const SocketAddress& local_address,
      const SocketAddress& remote_address,
      const ProxyInfo& proxy_info,
      const std::string& user_agent,
      const PacketSocketTcpOptions& tcp_options) = 0;

  virtual std::unique_ptr<webrtc::AsyncDnsResolverInterface>
  CreateAsyncDnsResolver() = 0;

 private:
  PacketSocketFactory(const PacketSocketFactory&) = delete;
  PacketSocketFactory& operator=(const PacketSocketFactory&) = delete;
};

}

#endif
