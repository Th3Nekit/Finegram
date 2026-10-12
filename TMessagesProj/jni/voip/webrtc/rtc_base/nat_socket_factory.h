/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_NAT_SOCKET_FACTORY_H_
#define RTC_BASE_NAT_SOCKET_FACTORY_H_

#include <stddef.h>

#include <cstdint>
#include <map>
#include <memory>
#include <set>

#include "api/array_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/nat_server.h"
#include "rtc_base/nat_types.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/socket_server.h"
#include "rtc_base/thread.h"

namespace rtc {

const size_t kNATEncodedIPv4AddressSize = 8U;
const size_t kNATEncodedIPv6AddressSize = 20U;

class NATInternalSocketFactory {
 public:
  virtual ~NATInternalSocketFactory() {}
  virtual Socket* CreateInternalSocket(int family,
                                       int type,
                                       const SocketAddress& local_addr,
                                       SocketAddress* nat_addr) = 0;
};

class NATSocketFactory : public SocketFactory, public NATInternalSocketFactory {
 public:
  NATSocketFactory(SocketFactory* factory,
                   const SocketAddress& nat_udp_addr,
                   const SocketAddress& nat_tcp_addr);

  NATSocketFactory(const NATSocketFactory&) = delete;
  NATSocketFactory& operator=(const NATSocketFactory&) = delete;

  Socket* CreateSocket(int family, int type) override;

  Socket* CreateInternalSocket(int family,
                               int type,
                               const SocketAddress& local_addr,
                               SocketAddress* nat_addr) override;

 private:
  SocketFactory* factory_;
  SocketAddress nat_udp_addr_;
  SocketAddress nat_tcp_addr_;
};

class NATSocketServer : public SocketServer, public NATInternalSocketFactory {
 public:
  class Translator;

  class TranslatorMap : private std::map<SocketAddress, Translator*> {
   public:
    ~TranslatorMap();
    Translator* Get(const SocketAddress& ext_ip);
    Translator* Add(const SocketAddress& ext_ip, Translator*);
    void Remove(const SocketAddress& ext_ip);
    Translator* FindClient(const SocketAddress& int_ip);
  };

  class Translator {
   public:
    Translator(NATSocketServer* server,
               NATType type,
               const SocketAddress& int_addr,
               Thread& external_socket_thread,
               SocketFactory* ext_factory,
               const SocketAddress& ext_addr);
    ~Translator();

    SocketFactory* internal_factory() { return internal_server_.get(); }
    SocketAddress internal_udp_address() const {
      return nat_server_->internal_udp_address();
    }
    SocketAddress internal_tcp_address() const {
      return SocketAddress();
    }

    Translator* GetTranslator(const SocketAddress& ext_ip);
    Translator* AddTranslator(const SocketAddress& ext_ip,
                              const SocketAddress& int_ip,
                              NATType type);
    void RemoveTranslator(const SocketAddress& ext_ip);

    bool AddClient(const SocketAddress& int_ip);
    void RemoveClient(const SocketAddress& int_ip);

    Translator* FindClient(const SocketAddress& int_ip);

   private:
    NATSocketServer* server_;
    std::unique_ptr<SocketServer> internal_server_;
    std::unique_ptr<NATServer> nat_server_;
    TranslatorMap nats_;
    std::set<SocketAddress> clients_;
  };

  explicit NATSocketServer(SocketServer* ss);

  NATSocketServer(const NATSocketServer&) = delete;
  NATSocketServer& operator=(const NATSocketServer&) = delete;

  SocketServer* socketserver() { return server_; }
  Thread* queue() { return msg_queue_; }

  Translator* GetTranslator(const SocketAddress& ext_ip);
  Translator* AddTranslator(const SocketAddress& ext_ip,
                            const SocketAddress& int_ip,
                            NATType type);
  void RemoveTranslator(const SocketAddress& ext_ip);

  Socket* CreateSocket(int family, int type) override;

  void SetMessageQueue(Thread* queue) override;
  bool Wait(webrtc::TimeDelta max_wait_duration, bool process_io) override;
  void WakeUp() override;

  Socket* CreateInternalSocket(int family,
                               int type,
                               const SocketAddress& local_addr,
                               SocketAddress* nat_addr) override;

 private:
  SocketServer* server_;
  Thread* msg_queue_;
  TranslatorMap nats_;
};

size_t PackAddressForNAT(char* buf,
                         size_t buf_size,
                         const SocketAddress& remote_addr);
size_t UnpackAddressFromNAT(rtc::ArrayView<const uint8_t> buf,
                            SocketAddress* remote_addr);
}

#endif
