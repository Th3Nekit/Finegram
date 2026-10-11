/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CANDIDATE_H_
#define API_CANDIDATE_H_

#include <limits.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/network_constants.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {
enum class IceCandidateType : int { kHost, kSrflx, kPrflx, kRelay };
}

namespace cricket {

RTC_EXPORT extern const absl::string_view LOCAL_PORT_TYPE;
RTC_EXPORT extern const absl::string_view STUN_PORT_TYPE;
RTC_EXPORT extern const absl::string_view PRFLX_PORT_TYPE;
RTC_EXPORT extern const absl::string_view RELAY_PORT_TYPE;

static constexpr size_t kMaxTurnServers = 32;

class RTC_EXPORT Candidate {
 public:
  Candidate();

  Candidate(int component,
            absl::string_view protocol,
            const rtc::SocketAddress& address,
            uint32_t priority,
            absl::string_view username,
            absl::string_view password,
            absl::string_view type ABSL_ATTRIBUTE_LIFETIME_BOUND,
            uint32_t generation,
            absl::string_view foundation,
            uint16_t network_id = 0,
            uint16_t network_cost = 0);
  Candidate(const Candidate&);
  ~Candidate();

  const std::string& id() const { return id_; }

  void generate_id();

  [[deprecated]] void set_id(absl::string_view id) { Assign(id_, id); }

  int component() const { return component_; }
  void set_component(int component) { component_ = component; }

  const std::string& protocol() const { return protocol_; }
  void set_protocol(absl::string_view protocol) { Assign(protocol_, protocol); }

  const std::string& relay_protocol() const { return relay_protocol_; }
  void set_relay_protocol(absl::string_view protocol) {
    Assign(relay_protocol_, protocol);
  }

  const rtc::SocketAddress& address() const { return address_; }
  void set_address(const rtc::SocketAddress& address) { address_ = address; }

  uint32_t priority() const { return priority_; }
  void set_priority(const uint32_t priority) { priority_ = priority; }

  const std::string& username() const { return username_; }
  void set_username(absl::string_view username) { Assign(username_, username); }

  const std::string& password() const { return password_; }
  void set_password(absl::string_view password) { Assign(password_, password); }

  const std::string& type() const { return type_; }

  absl::string_view type_name() const;

  void set_type(absl::string_view type ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    Assign(type_, type);
  }

  bool is_local() const;
  bool is_stun() const;
  bool is_prflx() const;
  bool is_relay() const;

  int type_preference() const {

    if (is_local())
      return 1;
    if (is_stun())
      return 2;
    if (is_relay())
      return 3;
    return 0;
  }

  const std::string& network_name() const { return network_name_; }
  void set_network_name(absl::string_view network_name) {
    Assign(network_name_, network_name);
  }

  rtc::AdapterType network_type() const { return network_type_; }
  void set_network_type(rtc::AdapterType network_type) {
    network_type_ = network_type;
  }

  rtc::AdapterType underlying_type_for_vpn() const {
    return underlying_type_for_vpn_;
  }
  void set_underlying_type_for_vpn(rtc::AdapterType network_type) {
    underlying_type_for_vpn_ = network_type;
  }

  uint32_t generation() const { return generation_; }
  void set_generation(uint32_t generation) { generation_ = generation; }

  void set_network_cost(uint16_t network_cost) {
    RTC_DCHECK_LE(network_cost, rtc::kNetworkCostMax);
    network_cost_ = network_cost;
  }
  uint16_t network_cost() const { return network_cost_; }

  uint16_t network_id() const { return network_id_; }
  void set_network_id(uint16_t network_id) { network_id_ = network_id; }

  const std::string& foundation() const { return foundation_; }
  void set_foundation(absl::string_view foundation) {
    Assign(foundation_, foundation);
  }

  const rtc::SocketAddress& related_address() const { return related_address_; }
  void set_related_address(const rtc::SocketAddress& related_address) {
    related_address_ = related_address;
  }
  const std::string& tcptype() const { return tcptype_; }
  void set_tcptype(absl::string_view tcptype) { Assign(tcptype_, tcptype); }

  const std::string& transport_name() const { return transport_name_; }
  void set_transport_name(absl::string_view transport_name) {
    Assign(transport_name_, transport_name);
  }

  const std::string& url() const { return url_; }
  void set_url(absl::string_view url) { Assign(url_, url); }

  bool IsEquivalent(const Candidate& c) const;

  bool MatchesForRemoval(const Candidate& c) const;

  std::string ToString() const { return ToStringInternal(false); }

  std::string ToSensitiveString() const { return ToStringInternal(true); }

  uint32_t GetPriority(uint32_t type_preference,
                       int network_adapter_preference,
                       int relay_preference,
                       bool adjust_local_preference) const;

  bool operator==(const Candidate& o) const;
  bool operator!=(const Candidate& o) const;

  Candidate ToSanitizedCopy(bool use_hostname_address,
                            bool filter_related_address) const;

 private:

  static void Assign(std::string& s, absl::string_view view);
  std::string ToStringInternal(bool sensitive) const;

  std::string id_;
  int component_;
  std::string protocol_;
  std::string relay_protocol_;
  rtc::SocketAddress address_;
  uint32_t priority_;
  std::string username_;
  std::string password_;
  std::string type_;
  std::string network_name_;
  rtc::AdapterType network_type_;
  rtc::AdapterType underlying_type_for_vpn_;
  uint32_t generation_;
  std::string foundation_;
  rtc::SocketAddress related_address_;
  std::string tcptype_;
  std::string transport_name_;
  uint16_t network_id_;
  uint16_t network_cost_;
  std::string url_;
};

}

#endif
