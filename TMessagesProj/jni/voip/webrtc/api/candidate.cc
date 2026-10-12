/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/candidate.h"

#include "absl/base/attributes.h"
#include "rtc_base/helpers.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"

namespace cricket {

ABSL_CONST_INIT const absl::string_view LOCAL_PORT_TYPE = "local";
ABSL_CONST_INIT const absl::string_view STUN_PORT_TYPE = "stun";
ABSL_CONST_INIT const absl::string_view PRFLX_PORT_TYPE = "prflx";
ABSL_CONST_INIT const absl::string_view RELAY_PORT_TYPE = "relay";

Candidate::Candidate()
    : id_(rtc::CreateRandomString(8)),
      component_(0),
      priority_(0),
      type_(LOCAL_PORT_TYPE),
      network_type_(rtc::ADAPTER_TYPE_UNKNOWN),
      underlying_type_for_vpn_(rtc::ADAPTER_TYPE_UNKNOWN),
      generation_(0),
      network_id_(0),
      network_cost_(0) {}

Candidate::Candidate(int component,
                     absl::string_view protocol,
                     const rtc::SocketAddress& address,
                     uint32_t priority,
                     absl::string_view username,
                     absl::string_view password,
                     absl::string_view type,
                     uint32_t generation,
                     absl::string_view foundation,
                     uint16_t network_id,
                     uint16_t network_cost)
    : id_(rtc::CreateRandomString(8)),
      component_(component),
      protocol_(protocol),
      address_(address),
      priority_(priority),
      username_(username),
      password_(password),
      type_(type),
      network_type_(rtc::ADAPTER_TYPE_UNKNOWN),
      underlying_type_for_vpn_(rtc::ADAPTER_TYPE_UNKNOWN),
      generation_(generation),
      foundation_(foundation),
      network_id_(network_id),
      network_cost_(network_cost) {}

Candidate::Candidate(const Candidate&) = default;

Candidate::~Candidate() = default;

void Candidate::generate_id() {
  id_ = rtc::CreateRandomString(8);
}

bool Candidate::is_local() const {
  return type_ == LOCAL_PORT_TYPE;
}
bool Candidate::is_stun() const {
  return type_ == STUN_PORT_TYPE;
}
bool Candidate::is_prflx() const {
  return type_ == PRFLX_PORT_TYPE;
}
bool Candidate::is_relay() const {
  return type_ == RELAY_PORT_TYPE;
}

absl::string_view Candidate::type_name() const {

  if (is_local())
    return "host";
  if (is_stun())
    return "srflx";
  return type_;
}

bool Candidate::IsEquivalent(const Candidate& c) const {

  return (component_ == c.component_) && (protocol_ == c.protocol_) &&
         (address_ == c.address_) && (username_ == c.username_) &&
         (password_ == c.password_) && (type_ == c.type_) &&
         (generation_ == c.generation_) && (foundation_ == c.foundation_) &&
         (related_address_ == c.related_address_) &&
         (network_id_ == c.network_id_);
}

bool Candidate::MatchesForRemoval(const Candidate& c) const {
  return component_ == c.component_ && protocol_ == c.protocol_ &&
         address_ == c.address_;
}

std::string Candidate::ToStringInternal(bool sensitive) const {
  rtc::StringBuilder ost;
  std::string address =
      sensitive ? address_.ToSensitiveString() : address_.ToString();
  std::string related_address = sensitive ? related_address_.ToSensitiveString()
                                          : related_address_.ToString();
  ost << "Cand[" << transport_name_ << ":" << foundation_ << ":" << component_
      << ":" << protocol_ << ":" << priority_ << ":" << address << ":"
      << type_name() << ":" << related_address << ":" << username_ << ":"
      << password_ << ":" << network_id_ << ":" << network_cost_ << ":"
      << generation_ << "]";
  return ost.Release();
}

uint32_t Candidate::GetPriority(uint32_t type_preference,
                                int network_adapter_preference,
                                int relay_preference,
                                bool adjust_local_preference) const {

  int addr_pref = IPAddressPrecedence(address_.ipaddr());
  int local_preference =
      ((network_adapter_preference << 8) | addr_pref) + relay_preference;

  RTC_DCHECK_LT(local_preference + kMaxTurnServers, 0x10000);
  if (adjust_local_preference && relay_protocol_.empty()) {
    local_preference += kMaxTurnServers;
  }

  return (type_preference << 24) | (local_preference << 8) | (256 - component_);
}

bool Candidate::operator==(const Candidate& o) const {
  return id_ == o.id_ && component_ == o.component_ &&
         protocol_ == o.protocol_ && relay_protocol_ == o.relay_protocol_ &&
         address_ == o.address_ && priority_ == o.priority_ &&
         username_ == o.username_ && password_ == o.password_ &&
         type_ == o.type_ && network_name_ == o.network_name_ &&
         network_type_ == o.network_type_ && generation_ == o.generation_ &&
         foundation_ == o.foundation_ &&
         related_address_ == o.related_address_ && tcptype_ == o.tcptype_ &&
         transport_name_ == o.transport_name_ && network_id_ == o.network_id_;
}

bool Candidate::operator!=(const Candidate& o) const {
  return !(*this == o);
}

Candidate Candidate::ToSanitizedCopy(bool use_hostname_address,
                                     bool filter_related_address) const {
  Candidate copy(*this);
  if (use_hostname_address) {
    rtc::IPAddress ip;
    if (address().hostname().empty()) {

      rtc::SocketAddress redacted_addr("redacted-ip.invalid", address().port());
      copy.set_address(redacted_addr);
    } else if (IPFromString(address().hostname(), &ip)) {

      rtc::SocketAddress redacted_addr("redacted-literal.invalid",
                                       address().port());
      copy.set_address(redacted_addr);
    } else {
      rtc::SocketAddress hostname_only_addr(address().hostname(),
                                            address().port());
      copy.set_address(hostname_only_addr);
    }
  }
  if (filter_related_address) {
    copy.set_related_address(
        rtc::EmptySocketAddressWithFamily(copy.address().family()));
  }
  return copy;
}

void Candidate::Assign(std::string& s, absl::string_view view) {

  s.assign(view.data(), view.size());
}

}
