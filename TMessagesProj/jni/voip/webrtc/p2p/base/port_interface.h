/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_PORT_INTERFACE_H_
#define P2P_BASE_PORT_INTERFACE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/candidate.h"
#include "api/packet_socket_factory.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/proxy_info.h"
#include "rtc_base/socket_address.h"

namespace rtc {
class Network;
struct PacketOptions;
}

namespace cricket {
class Connection;
class IceMessage;
class StunMessage;
class StunStats;

enum ProtocolType {
  PROTO_UDP,
  PROTO_TCP,
  PROTO_SSLTCP,
  PROTO_TLS,
  PROTO_LAST = PROTO_TLS
};

class PortInterface {
 public:
  virtual ~PortInterface();

  virtual const absl::string_view Type() const = 0;
  virtual const rtc::Network* Network() const = 0;

  virtual void SetIceRole(IceRole role) = 0;
  virtual IceRole GetIceRole() const = 0;

  virtual void SetIceTiebreaker(uint64_t tiebreaker) = 0;
  virtual uint64_t IceTiebreaker() const = 0;

  virtual bool SharedSocket() const = 0;

  virtual bool SupportsProtocol(absl::string_view protocol) const = 0;

  virtual void PrepareAddress() = 0;

  virtual Connection* GetConnection(const rtc::SocketAddress& remote_addr) = 0;

  enum CandidateOrigin { ORIGIN_THIS_PORT, ORIGIN_OTHER_PORT, ORIGIN_MESSAGE };
  virtual Connection* CreateConnection(const Candidate& remote_candidate,
                                       CandidateOrigin origin) = 0;

  virtual int SetOption(rtc::Socket::Option opt, int value) = 0;
  virtual int GetOption(rtc::Socket::Option opt, int* value) = 0;
  virtual int GetError() = 0;

  virtual ProtocolType GetProtocol() const = 0;

  virtual const std::vector<Candidate>& Candidates() const = 0;

  virtual int SendTo(const void* data,
                     size_t size,
                     const rtc::SocketAddress& addr,
                     const rtc::PacketOptions& options,
                     bool payload) = 0;

  sigslot::signal6<PortInterface*,
                   const rtc::SocketAddress&,
                   ProtocolType,
                   IceMessage*,
                   const std::string&,
                   bool>
      SignalUnknownAddress;

  virtual void SendBindingErrorResponse(StunMessage* message,
                                        const rtc::SocketAddress& addr,
                                        int error_code,
                                        absl::string_view reason) = 0;

  virtual void SubscribePortDestroyed(
      std::function<void(PortInterface*)> callback) = 0;

  sigslot::signal1<PortInterface*> SignalRoleConflict;

  virtual void EnablePortPackets() = 0;
  sigslot::
      signal4<PortInterface*, const char*, size_t, const rtc::SocketAddress&>
          SignalReadPacket;

  sigslot::signal1<const rtc::SentPacket&> SignalSentPacket;

  virtual std::string ToString() const = 0;

  virtual void GetStunStats(absl::optional<StunStats>* stats) = 0;

  virtual void DestroyConnection(Connection* conn) = 0;

  virtual void DestroyConnectionAsync(Connection* conn) = 0;

  virtual webrtc::TaskQueueBase* thread() = 0;

  virtual rtc::PacketSocketFactory* socket_factory() const = 0;
  virtual const std::string& user_agent() = 0;
  virtual const rtc::ProxyInfo& proxy() = 0;

  virtual uint32_t generation() const = 0;
  virtual void set_generation(uint32_t generation) = 0;
  virtual bool send_retransmit_count_attribute() const = 0;

  virtual const std::string& content_name() const = 0;

  virtual void AddPrflxCandidate(const Candidate& local) = 0;

  virtual std::string ComputeFoundation(
      absl::string_view type,
      absl::string_view protocol,
      absl::string_view relay_protocol,
      const rtc::SocketAddress& base_address) = 0;

 protected:
  PortInterface();
  virtual void UpdateNetworkCost() = 0;

  virtual rtc::DiffServCodePoint StunDscpValue() const = 0;

  virtual bool GetStunMessage(const char* data,
                              size_t size,
                              const rtc::SocketAddress& addr,
                              std::unique_ptr<IceMessage>* out_msg,
                              std::string* out_username) = 0;

  virtual bool ParseStunUsername(const StunMessage* stun_msg,
                                 std::string* local_username,
                                 std::string* remote_username) const = 0;
  virtual std::string CreateStunUsername(
      absl::string_view remote_username) const = 0;

  virtual bool MaybeIceRoleConflict(const rtc::SocketAddress& addr,
                                    IceMessage* stun_msg,
                                    absl::string_view remote_ufrag) = 0;

  virtual int16_t network_cost() const = 0;

  friend class Connection;
};

}

#endif
