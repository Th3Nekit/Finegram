/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/port.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/rtc_error.h"
#include "api/units/time_delta.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/stun_request.h"
#include "rtc_base/byte_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/crc32.h"
#include "rtc_base/helpers.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/mdns_responder_interface.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/string_encode.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/trace_event.h"

namespace cricket {
namespace {

using ::webrtc::RTCError;
using ::webrtc::RTCErrorType;
using ::webrtc::TaskQueueBase;
using ::webrtc::TimeDelta;

rtc::PacketInfoProtocolType ConvertProtocolTypeToPacketInfoProtocolType(
    cricket::ProtocolType type) {
  switch (type) {
    case cricket::ProtocolType::PROTO_UDP:
      return rtc::PacketInfoProtocolType::kUdp;
    case cricket::ProtocolType::PROTO_TCP:
      return rtc::PacketInfoProtocolType::kTcp;
    case cricket::ProtocolType::PROTO_SSLTCP:
      return rtc::PacketInfoProtocolType::kSsltcp;
    case cricket::ProtocolType::PROTO_TLS:
      return rtc::PacketInfoProtocolType::kTls;
    default:
      return rtc::PacketInfoProtocolType::kUnknown;
  }
}

const int kPortTimeoutDelay = cricket::STUN_TOTAL_TIMEOUT + 5000;

}

static const char* const PROTO_NAMES[] = {UDP_PROTOCOL_NAME, TCP_PROTOCOL_NAME,
                                          SSLTCP_PROTOCOL_NAME,
                                          TLS_PROTOCOL_NAME};

const char* ProtoToString(ProtocolType proto) {
  return PROTO_NAMES[proto];
}

absl::optional<ProtocolType> StringToProto(absl::string_view proto_name) {
  for (size_t i = 0; i <= PROTO_LAST; ++i) {
    if (absl::EqualsIgnoreCase(PROTO_NAMES[i], proto_name)) {
      return static_cast<ProtocolType>(i);
    }
  }
  return absl::nullopt;
}

const int DISCARD_PORT = 9;
const char TCPTYPE_ACTIVE_STR[] = "active";
const char TCPTYPE_PASSIVE_STR[] = "passive";
const char TCPTYPE_SIMOPEN_STR[] = "so";

std::string Port::ComputeFoundation(absl::string_view type,
                                    absl::string_view protocol,
                                    absl::string_view relay_protocol,
                                    const rtc::SocketAddress& base_address) {

  rtc::StringBuilder sb;
  sb << type << base_address.ipaddr().ToString() << protocol << relay_protocol
     << rtc::ToString(IceTiebreaker());
  return rtc::ToString(rtc::ComputeCrc32(sb.Release()));
}

Port::Port(TaskQueueBase* thread,
           absl::string_view type,
           rtc::PacketSocketFactory* factory,
           const rtc::Network* network,
           absl::string_view username_fragment,
           absl::string_view password,
           const webrtc::FieldTrialsView* field_trials)
    : thread_(thread),
      factory_(factory),
      type_(type),
      send_retransmit_count_attribute_(false),
      network_(network),
      min_port_(0),
      max_port_(0),
      component_(ICE_CANDIDATE_COMPONENT_DEFAULT),
      generation_(0),
      ice_username_fragment_(username_fragment),
      password_(password),
      timeout_delay_(kPortTimeoutDelay),
      enable_port_packets_(false),
      ice_role_(ICEROLE_UNKNOWN),
      tiebreaker_(0),
      shared_socket_(true),
      weak_factory_(this),
      field_trials_(field_trials) {
  RTC_DCHECK(factory_ != NULL);
  Construct();
}

Port::Port(TaskQueueBase* thread,
           absl::string_view type,
           rtc::PacketSocketFactory* factory,
           const rtc::Network* network,
           uint16_t min_port,
           uint16_t max_port,
           absl::string_view username_fragment,
           absl::string_view password,
           const webrtc::FieldTrialsView* field_trials)
    : thread_(thread),
      factory_(factory),
      type_(type),
      send_retransmit_count_attribute_(false),
      network_(network),
      min_port_(min_port),
      max_port_(max_port),
      component_(ICE_CANDIDATE_COMPONENT_DEFAULT),
      generation_(0),
      ice_username_fragment_(username_fragment),
      password_(password),
      timeout_delay_(kPortTimeoutDelay),
      enable_port_packets_(false),
      ice_role_(ICEROLE_UNKNOWN),
      tiebreaker_(0),
      shared_socket_(false),
      weak_factory_(this),
      field_trials_(field_trials) {
  RTC_DCHECK(factory_ != NULL);
  Construct();
}

void Port::Construct() {
  RTC_DCHECK_RUN_ON(thread_);

  if (ice_username_fragment_.empty()) {
    RTC_DCHECK(password_.empty());
    ice_username_fragment_ = rtc::CreateRandomString(ICE_UFRAG_LENGTH);
    password_ = rtc::CreateRandomString(ICE_PWD_LENGTH);
  }
  network_->SignalTypeChanged.connect(this, &Port::OnNetworkTypeChanged);
  network_cost_ = network_->GetCost(field_trials());

  PostDestroyIfDead(            true);
  RTC_LOG(LS_INFO) << ToString() << ": Port created with network cost "
                   << network_cost_;
}

Port::~Port() {
  RTC_DCHECK_RUN_ON(thread_);
  DestroyAllConnections();
  CancelPendingTasks();
}

const absl::string_view Port::Type() const {
  return type_;
}
const rtc::Network* Port::Network() const {
  return network_;
}

IceRole Port::GetIceRole() const {
  return ice_role_;
}

void Port::SetIceRole(IceRole role) {
  ice_role_ = role;
}

void Port::SetIceTiebreaker(uint64_t tiebreaker) {
  tiebreaker_ = tiebreaker;
}

uint64_t Port::IceTiebreaker() const {
  return tiebreaker_;
}

bool Port::SharedSocket() const {
  return shared_socket_;
}

void Port::SetIceParameters(int component,
                            absl::string_view username_fragment,
                            absl::string_view password) {
  RTC_DCHECK_RUN_ON(thread_);
  component_ = component;
  ice_username_fragment_ = std::string(username_fragment);
  password_ = std::string(password);
  for (Candidate& c : candidates_) {
    c.set_component(component);
    c.set_username(username_fragment);
    c.set_password(password);
  }

  for (auto& [unused, connection] : connections_) {
    connection->UpdateLocalIceParameters(component, username_fragment,
                                         password);
  }
}

const std::vector<Candidate>& Port::Candidates() const {
  return candidates_;
}

Connection* Port::GetConnection(const rtc::SocketAddress& remote_addr) {
  AddressMap::const_iterator iter = connections_.find(remote_addr);
  if (iter != connections_.end())
    return iter->second;
  else
    return NULL;
}

void Port::AddAddress(const rtc::SocketAddress& address,
                      const rtc::SocketAddress& base_address,
                      const rtc::SocketAddress& related_address,
                      absl::string_view protocol,
                      absl::string_view relay_protocol,
                      absl::string_view tcptype,
                      absl::string_view type,
                      uint32_t type_preference,
                      uint32_t relay_preference,
                      absl::string_view url,
                      bool is_final) {
  RTC_DCHECK_RUN_ON(thread_);

  std::string foundation =
      ComputeFoundation(type, protocol, relay_protocol, base_address);
  Candidate c(component_, protocol, address, 0U, username_fragment(), password_,
              type, generation_, foundation, network_->id(), network_cost_);

#if RTC_DCHECK_IS_ON
  if (protocol == TCP_PROTOCOL_NAME && c.is_local()) {
    RTC_DCHECK(!tcptype.empty());
  }
#endif

  c.set_relay_protocol(relay_protocol);
  c.set_priority(
      c.GetPriority(type_preference, network_->preference(), relay_preference,
                    field_trials_->IsEnabled(
                        "WebRTC-IncreaseIceCandidatePriorityHostSrflx")));
  c.set_tcptype(tcptype);
  c.set_network_name(network_->name());
  c.set_network_type(network_->type());
  c.set_underlying_type_for_vpn(network_->underlying_type_for_vpn());
  c.set_url(url);
  c.set_related_address(related_address);

  bool pending = MaybeObfuscateAddress(c, is_final);

  if (!pending) {
    FinishAddingAddress(c, is_final);
  }
}

bool Port::MaybeObfuscateAddress(const Candidate& c, bool is_final) {

  if (network_->GetMdnsResponder() == nullptr) {
    return false;
  }
  if (!c.is_local()) {
    return false;
  }

  auto copy = c;
  auto weak_ptr = weak_factory_.GetWeakPtr();
  auto callback = [weak_ptr, copy, is_final](const rtc::IPAddress& addr,
                                             absl::string_view name) mutable {
    RTC_DCHECK(copy.address().ipaddr() == addr);
    rtc::SocketAddress hostname_address(name, copy.address().port());

    hostname_address.SetResolvedIP(addr);
    copy.set_address(hostname_address);
    copy.set_related_address(rtc::SocketAddress());
    if (weak_ptr != nullptr) {
      RTC_DCHECK_RUN_ON(weak_ptr->thread_);
      weak_ptr->set_mdns_name_registration_status(
          MdnsNameRegistrationStatus::kCompleted);
      weak_ptr->FinishAddingAddress(copy, is_final);
    }
  };
  set_mdns_name_registration_status(MdnsNameRegistrationStatus::kInProgress);
  network_->GetMdnsResponder()->CreateNameForAddress(copy.address().ipaddr(),
                                                     callback);
  return true;
}

void Port::FinishAddingAddress(const Candidate& c, bool is_final) {
  candidates_.push_back(c);
  SignalCandidateReady(this, c);

  PostAddAddress(is_final);
}

void Port::PostAddAddress(bool is_final) {
  if (is_final) {
    SignalPortComplete(this);
  }
}

void Port::AddOrReplaceConnection(Connection* conn) {
  auto ret = connections_.insert(
      std::make_pair(conn->remote_candidate().address(), conn));

  if (ret.second == false && ret.first->second != conn) {
    RTC_LOG(LS_WARNING)
        << ToString()
        << ": A new connection was created on an existing remote address. "
           "New remote candidate: "
        << conn->remote_candidate().ToSensitiveString();
    std::unique_ptr<Connection> old_conn = absl::WrapUnique(ret.first->second);
    ret.first->second = conn;
    HandleConnectionDestroyed(old_conn.get());
    old_conn->Shutdown();
  }
}

void Port::OnReadPacket(const rtc::ReceivedPacket& packet, ProtocolType proto) {
  const char* data = reinterpret_cast<const char*>(packet.payload().data());
  size_t size = packet.payload().size();
  const rtc::SocketAddress& addr = packet.source_address();

  if (enable_port_packets_) {
    SignalReadPacket(this, data, size, addr);
    return;
  }

  std::unique_ptr<IceMessage> msg;
  std::string remote_username;
  if (!GetStunMessage(data, size, addr, &msg, &remote_username)) {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Received non-STUN packet from unknown address: "
                      << addr.ToSensitiveString();
  } else if (!msg) {

  } else if (msg->type() == STUN_BINDING_REQUEST) {
    RTC_LOG(LS_INFO) << "Received " << StunMethodToString(msg->type())
                     << " id=" << rtc::hex_encode(msg->transaction_id())
                     << " from unknown address " << addr.ToSensitiveString();

    SignalUnknownAddress(this, addr, proto, msg.get(), remote_username, false);

    if (!MaybeIceRoleConflict(addr, msg.get(), remote_username)) {
      RTC_LOG(LS_INFO) << "Received conflicting role from the peer.";
      return;
    }
  } else if (msg->type() == GOOG_PING_REQUEST) {

    SendBindingErrorResponse(msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                             STUN_ERROR_REASON_BAD_REQUEST);
  } else {

    if (msg->type() != STUN_BINDING_RESPONSE &&
        msg->type() != GOOG_PING_RESPONSE &&
        msg->type() != GOOG_PING_ERROR_RESPONSE) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Received unexpected STUN message type: "
                        << msg->type() << " from unknown address: "
                        << addr.ToSensitiveString();
    }
  }
}

void Port::OnReadyToSend() {
  AddressMap::iterator iter = connections_.begin();
  for (; iter != connections_.end(); ++iter) {
    iter->second->OnReadyToSend();
  }
}

void Port::AddPrflxCandidate(const Candidate& local) {
  RTC_DCHECK_RUN_ON(thread_);
  candidates_.push_back(local);
}

bool Port::GetStunMessage(const char* data,
                          size_t size,
                          const rtc::SocketAddress& addr,
                          std::unique_ptr<IceMessage>* out_msg,
                          std::string* out_username) {
  RTC_DCHECK_RUN_ON(thread_);

  RTC_DCHECK(out_msg != NULL);
  RTC_DCHECK(out_username != NULL);
  out_username->clear();

  int types[] = {GOOG_PING_REQUEST, GOOG_PING_RESPONSE,
                 GOOG_PING_ERROR_RESPONSE};
  if (!StunMessage::IsStunMethod(types, data, size) &&
      !StunMessage::ValidateFingerprint(data, size)) {
    return false;
  }

  std::unique_ptr<IceMessage> stun_msg(new IceMessage());
  rtc::ByteBufferReader buf(
      rtc::MakeArrayView(reinterpret_cast<const uint8_t*>(data), size));
  if (!stun_msg->Read(&buf) || (buf.Length() > 0)) {
    return false;
  }

  std::vector<uint16_t> unknown_attributes =
      stun_msg->GetNonComprehendedAttributes();

  if (stun_msg->type() == STUN_BINDING_REQUEST) {

    if (!stun_msg->GetByteString(STUN_ATTR_USERNAME) ||
        !stun_msg->GetByteString(STUN_ATTR_MESSAGE_INTEGRITY)) {
      RTC_LOG(LS_ERROR) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type())
                        << " without username/M-I from: "
                        << addr.ToSensitiveString();
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                               STUN_ERROR_REASON_BAD_REQUEST);
      return true;
    }

    std::string local_ufrag;
    std::string remote_ufrag;
    if (!ParseStunUsername(stun_msg.get(), &local_ufrag, &remote_ufrag) ||
        local_ufrag != username_fragment()) {
      RTC_LOG(LS_ERROR) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type())
                        << " with bad local username " << local_ufrag
                        << " from " << addr.ToSensitiveString();
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                               STUN_ERROR_REASON_UNAUTHORIZED);
      return true;
    }

    if (stun_msg->ValidateMessageIntegrity(password_) !=
        StunMessage::IntegrityStatus::kIntegrityOk) {
      RTC_LOG(LS_ERROR) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type())
                        << " with bad M-I from " << addr.ToSensitiveString()
                        << ", password_=" << password_;
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                               STUN_ERROR_REASON_UNAUTHORIZED);
      return true;
    }

    if (!unknown_attributes.empty()) {
      SendUnknownAttributesErrorResponse(stun_msg.get(), addr,
                                         unknown_attributes);
      return true;
    }

    out_username->assign(remote_ufrag);
  } else if ((stun_msg->type() == STUN_BINDING_RESPONSE) ||
             (stun_msg->type() == STUN_BINDING_ERROR_RESPONSE)) {
    if (stun_msg->type() == STUN_BINDING_ERROR_RESPONSE) {
      if (const StunErrorCodeAttribute* error_code = stun_msg->GetErrorCode()) {
        RTC_LOG(LS_ERROR) << ToString() << ": Received "
                          << StunMethodToString(stun_msg->type())
                          << ": class=" << error_code->eclass()
                          << " number=" << error_code->number() << " reason='"
                          << error_code->reason() << "' from "
                          << addr.ToSensitiveString();

      } else {
        RTC_LOG(LS_ERROR) << ToString() << ": Received "
                          << StunMethodToString(stun_msg->type())
                          << " without a error code from "
                          << addr.ToSensitiveString();
        return true;
      }
    }

    if (!unknown_attributes.empty()) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Discarding STUN response due to unknown "
                           "comprehension-required attribute";
      return true;
    }

    out_username->clear();
  } else if (stun_msg->type() == STUN_BINDING_INDICATION) {
    RTC_LOG(LS_VERBOSE) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type()) << ": from "
                        << addr.ToSensitiveString();
    out_username->clear();

    if (!unknown_attributes.empty()) {
      RTC_LOG(LS_ERROR) << ToString()
                        << ": Discarding STUN indication due to "
                           "unknown comprehension-required attribute";
      return true;
    }

  } else if (stun_msg->type() == GOOG_PING_REQUEST) {
    if (stun_msg->ValidateMessageIntegrity(password_) !=
        StunMessage::IntegrityStatus::kIntegrityOk) {
      RTC_LOG(LS_ERROR) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type())
                        << " with bad M-I from " << addr.ToSensitiveString()
                        << ", password_=" << password_;
      SendBindingErrorResponse(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                               STUN_ERROR_REASON_UNAUTHORIZED);
      return true;
    }
    RTC_LOG(LS_VERBOSE) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type()) << " from "
                        << addr.ToSensitiveString();
    out_username->clear();
  } else if (stun_msg->type() == GOOG_PING_RESPONSE ||
             stun_msg->type() == GOOG_PING_ERROR_RESPONSE) {

    RTC_LOG(LS_VERBOSE) << ToString() << ": Received "
                        << StunMethodToString(stun_msg->type()) << " from "
                        << addr.ToSensitiveString();
    out_username->clear();
  } else {
    RTC_LOG(LS_ERROR) << ToString()
                      << ": Received STUN packet with invalid type ("
                      << stun_msg->type() << ") from "
                      << addr.ToSensitiveString();
    return true;
  }

  *out_msg = std::move(stun_msg);
  return true;
}

bool Port::IsCompatibleAddress(const rtc::SocketAddress& addr) {

  rtc::IPAddress ip = network_->GetBestIP();

  if (addr.family() != ip.family()) {
    return false;
  }

  if (ip.family() == AF_INET6 &&
      (IPIsLinkLocal(ip) != IPIsLinkLocal(addr.ipaddr()))) {
    return false;
  }
  return true;
}

rtc::DiffServCodePoint Port::StunDscpValue() const {

  return rtc::DSCP_NO_CHANGE;
}

void Port::DestroyAllConnections() {
  RTC_DCHECK_RUN_ON(thread_);
  for (auto& [unused, connection] : connections_) {
    connection->Shutdown();
    delete connection;
  }
  connections_.clear();
}

void Port::set_timeout_delay(int delay) {
  RTC_DCHECK_RUN_ON(thread_);

  timeout_delay_ = delay;
}

bool Port::ParseStunUsername(const StunMessage* stun_msg,
                             std::string* local_ufrag,
                             std::string* remote_ufrag) const {

  local_ufrag->clear();
  remote_ufrag->clear();
  const StunByteStringAttribute* username_attr =
      stun_msg->GetByteString(STUN_ATTR_USERNAME);
  if (username_attr == NULL)
    return false;

  const absl::string_view username = username_attr->string_view();
  size_t colon_pos = username.find(':');
  if (colon_pos == absl::string_view::npos) {
    return false;
  }

  *local_ufrag = std::string(username.substr(0, colon_pos));
  *remote_ufrag = std::string(username.substr(colon_pos + 1, username.size()));
  return true;
}

bool Port::MaybeIceRoleConflict(const rtc::SocketAddress& addr,
                                IceMessage* stun_msg,
                                absl::string_view remote_ufrag) {
  RTC_DCHECK_RUN_ON(thread_);

  bool ret = true;
  IceRole remote_ice_role = ICEROLE_UNKNOWN;
  uint64_t remote_tiebreaker = 0;
  const StunUInt64Attribute* stun_attr =
      stun_msg->GetUInt64(STUN_ATTR_ICE_CONTROLLING);
  if (stun_attr) {
    remote_ice_role = ICEROLE_CONTROLLING;
    remote_tiebreaker = stun_attr->value();
  }

  if (remote_ice_role == ICEROLE_CONTROLLING &&
      username_fragment() == remote_ufrag &&
      remote_tiebreaker == IceTiebreaker()) {
    return true;
  }

  stun_attr = stun_msg->GetUInt64(STUN_ATTR_ICE_CONTROLLED);
  if (stun_attr) {
    remote_ice_role = ICEROLE_CONTROLLED;
    remote_tiebreaker = stun_attr->value();
  }

  switch (ice_role_) {
    case ICEROLE_CONTROLLING:
      if (ICEROLE_CONTROLLING == remote_ice_role) {
        if (remote_tiebreaker >= tiebreaker_) {
          SignalRoleConflict(this);
        } else {

          SendBindingErrorResponse(stun_msg, addr, STUN_ERROR_ROLE_CONFLICT,
                                   STUN_ERROR_REASON_ROLE_CONFLICT);
          ret = false;
        }
      }
      break;
    case ICEROLE_CONTROLLED:
      if (ICEROLE_CONTROLLED == remote_ice_role) {
        if (remote_tiebreaker < tiebreaker_) {
          SignalRoleConflict(this);
        } else {

          SendBindingErrorResponse(stun_msg, addr, STUN_ERROR_ROLE_CONFLICT,
                                   STUN_ERROR_REASON_ROLE_CONFLICT);
          ret = false;
        }
      }
      break;
    default:
      RTC_DCHECK_NOTREACHED();
  }
  return ret;
}

std::string Port::CreateStunUsername(absl::string_view remote_username) const {
  RTC_DCHECK_RUN_ON(thread_);
  return std::string(remote_username) + ":" + username_fragment();
}

bool Port::HandleIncomingPacket(rtc::AsyncPacketSocket* socket,
                                const rtc::ReceivedPacket& packet) {
  RTC_DCHECK_NOTREACHED();
  return false;
}

bool Port::CanHandleIncomingPacketsFrom(const rtc::SocketAddress&) const {
  return false;
}

void Port::SendBindingErrorResponse(StunMessage* message,
                                    const rtc::SocketAddress& addr,
                                    int error_code,
                                    absl::string_view reason) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(message->type() == STUN_BINDING_REQUEST ||
             message->type() == GOOG_PING_REQUEST);

  StunMessage response(message->type() == STUN_BINDING_REQUEST
                           ? STUN_BINDING_ERROR_RESPONSE
                           : GOOG_PING_ERROR_RESPONSE,
                       message->transaction_id());

  auto error_attr = StunAttribute::CreateErrorCode();
  error_attr->SetCode(error_code);
  error_attr->SetReason(std::string(reason));
  response.AddAttribute(std::move(error_attr));

  if (error_code != STUN_ERROR_BAD_REQUEST &&
      error_code != STUN_ERROR_UNAUTHORIZED &&
      message->type() != GOOG_PING_REQUEST) {
    if (message->type() == STUN_BINDING_REQUEST) {
      response.AddMessageIntegrity(password_);
    } else {
      response.AddMessageIntegrity32(password_);
    }
  }

  if (message->type() == STUN_BINDING_REQUEST) {
    response.AddFingerprint();
  }

  rtc::ByteBufferWriter buf;
  response.Write(&buf);
  rtc::PacketOptions options(StunDscpValue());
  options.info_signaled_after_sent.packet_type =
      rtc::PacketType::kIceConnectivityCheckResponse;
  SendTo(buf.Data(), buf.Length(), addr, options, false);
  RTC_LOG(LS_INFO) << ToString() << ": Sending STUN "
                   << StunMethodToString(response.type())
                   << ": reason=" << reason << " to "
                   << addr.ToSensitiveString();
}

void Port::SendUnknownAttributesErrorResponse(
    StunMessage* message,
    const rtc::SocketAddress& addr,
    const std::vector<uint16_t>& unknown_types) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(message->type() == STUN_BINDING_REQUEST);

  StunMessage response(STUN_BINDING_ERROR_RESPONSE, message->transaction_id());

  auto error_attr = StunAttribute::CreateErrorCode();
  error_attr->SetCode(STUN_ERROR_UNKNOWN_ATTRIBUTE);
  error_attr->SetReason(STUN_ERROR_REASON_UNKNOWN_ATTRIBUTE);
  response.AddAttribute(std::move(error_attr));

  std::unique_ptr<StunUInt16ListAttribute> unknown_attr =
      StunAttribute::CreateUnknownAttributes();
  for (uint16_t type : unknown_types) {
    unknown_attr->AddType(type);
  }
  response.AddAttribute(std::move(unknown_attr));

  response.AddMessageIntegrity(password_);
  response.AddFingerprint();

  rtc::ByteBufferWriter buf;
  response.Write(&buf);
  rtc::PacketOptions options(StunDscpValue());
  options.info_signaled_after_sent.packet_type =
      rtc::PacketType::kIceConnectivityCheckResponse;
  SendTo(buf.Data(), buf.Length(), addr, options, false);
  RTC_LOG(LS_ERROR) << ToString() << ": Sending STUN binding error: reason="
                    << STUN_ERROR_UNKNOWN_ATTRIBUTE << " to "
                    << addr.ToSensitiveString();
}

void Port::KeepAliveUntilPruned() {

  if (state_ == State::INIT) {
    state_ = State::KEEP_ALIVE_UNTIL_PRUNED;
  }
}

void Port::Prune() {
  state_ = State::PRUNED;
  PostDestroyIfDead(            false);
}

void Port::CancelPendingTasks() {
  TRACE_EVENT0("webrtc", "Port::CancelPendingTasks");
  RTC_DCHECK_RUN_ON(thread_);
  weak_factory_.InvalidateWeakPtrs();
}

void Port::PostDestroyIfDead(bool delayed) {
  rtc::WeakPtr<Port> weak_ptr = NewWeakPtr();
  auto task = [weak_ptr = std::move(weak_ptr)] {
    if (weak_ptr) {
      weak_ptr->DestroyIfDead();
    }
  };
  if (delayed) {
    thread_->PostDelayedTask(std::move(task),
                             TimeDelta::Millis(timeout_delay_));
  } else {
    thread_->PostTask(std::move(task));
  }
}

void Port::DestroyIfDead() {
  RTC_DCHECK_RUN_ON(thread_);
  bool dead =
      (state_ == State::INIT || state_ == State::PRUNED) &&
      connections_.empty() &&
      rtc::TimeMillis() - last_time_all_connections_removed_ >= timeout_delay_;
  if (dead) {
    Destroy();
  }
}

void Port::SubscribePortDestroyed(
    std::function<void(PortInterface*)> callback) {
  port_destroyed_callback_list_.AddReceiver(callback);
}

void Port::SendPortDestroyed(Port* port) {
  port_destroyed_callback_list_.Send(port);
}
void Port::OnNetworkTypeChanged(const rtc::Network* network) {
  RTC_DCHECK(network == network_);

  UpdateNetworkCost();
}

std::string Port::ToString() const {
  rtc::StringBuilder ss;
  ss << "Port[" << rtc::ToHex(reinterpret_cast<uintptr_t>(this)) << ":"
     << content_name_ << ":" << component_ << ":" << generation_ << ":" << type_
     << ":" << network_->ToString() << "]";
  return ss.Release();
}

void Port::UpdateNetworkCost() {
  RTC_DCHECK_RUN_ON(thread_);
  uint16_t new_cost = network_->GetCost(field_trials());
  if (network_cost_ == new_cost) {
    return;
  }
  RTC_LOG(LS_INFO) << "Network cost changed from " << network_cost_ << " to "
                   << new_cost
                   << ". Number of candidates created: " << candidates_.size()
                   << ". Number of connections created: "
                   << connections_.size();
  network_cost_ = new_cost;
  for (cricket::Candidate& candidate : candidates_)
    candidate.set_network_cost(network_cost_);

  for (auto& [unused, connection] : connections_)
    connection->SetLocalCandidateNetworkCost(network_cost_);
}

void Port::EnablePortPackets() {
  enable_port_packets_ = true;
}

bool Port::OnConnectionDestroyed(Connection* conn) {
  if (connections_.erase(conn->remote_candidate().address()) == 0) {

    RTC_DCHECK_NOTREACHED() << "Calling Destroy recursively?";
    return false;
  }

  HandleConnectionDestroyed(conn);

  if (connections_.empty()) {
    last_time_all_connections_removed_ = rtc::TimeMillis();
    PostDestroyIfDead(            true);
  }

  return true;
}

void Port::DestroyConnectionInternal(Connection* conn, bool async) {
  RTC_DCHECK_RUN_ON(thread_);
  if (!OnConnectionDestroyed(conn))
    return;

  conn->Shutdown();
  if (async) {

    thread_->PostTask([conn = absl::WrapUnique(conn)]() {});
  } else {
    delete conn;
  }
}

void Port::Destroy() {
  RTC_DCHECK(connections_.empty());
  RTC_LOG(LS_INFO) << ToString() << ": Port deleted";
  SendPortDestroyed(this);
  delete this;
}

const std::string& Port::username_fragment() const {
  RTC_DCHECK_RUN_ON(thread_);
  return ice_username_fragment_;
}

void Port::CopyPortInformationToPacketInfo(rtc::PacketInfo* info) const {
  info->protocol = ConvertProtocolTypeToPacketInfoProtocolType(GetProtocol());
  info->network_id = Network()->id();
}

}
