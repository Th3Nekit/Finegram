/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/jsep_session_description.h"

#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/port.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/media_session.h"
#include "pc/webrtc_sdp.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helper.h"
#include "rtc_base/socket_address.h"

using cricket::Candidate;
using cricket::SessionDescription;

namespace webrtc {
namespace {

constexpr char kDummyAddress[] = "0.0.0.0";
constexpr int kDummyPort = 9;

void UpdateConnectionAddress(
    const JsepCandidateCollection& candidate_collection,
    cricket::MediaContentDescription* media_desc) {
  int port = kDummyPort;
  std::string ip = kDummyAddress;
  std::string hostname;
  int current_preference = 0;
  int current_family = AF_UNSPEC;
  for (size_t i = 0; i < candidate_collection.count(); ++i) {
    const IceCandidateInterface* jsep_candidate = candidate_collection.at(i);
    if (jsep_candidate->candidate().component() !=
        cricket::ICE_CANDIDATE_COMPONENT_RTP) {
      continue;
    }

    if (jsep_candidate->candidate().protocol() != cricket::UDP_PROTOCOL_NAME) {
      continue;
    }
    const int preference = jsep_candidate->candidate().type_preference();
    const int family = jsep_candidate->candidate().address().ipaddr().family();

    if ((preference <= current_preference && current_family == family) ||
        (current_family == AF_INET && family == AF_INET6)) {
      continue;
    }
    current_preference = preference;
    current_family = family;
    const rtc::SocketAddress& candidate_addr =
        jsep_candidate->candidate().address();
    port = candidate_addr.port();
    ip = candidate_addr.ipaddr().ToString();
    hostname = candidate_addr.hostname();
  }
  rtc::SocketAddress connection_addr(ip, port);
  if (rtc::IPIsUnspec(connection_addr.ipaddr()) && !hostname.empty()) {

    connection_addr = rtc::SocketAddress(kDummyAddress, kDummyPort);
  }
  media_desc->set_connection_address(connection_addr);
}

}

SdpType SessionDescriptionInterface::GetType() const {
  absl::optional<SdpType> maybe_type = SdpTypeFromString(type());
  if (maybe_type) {
    return *maybe_type;
  } else {
    RTC_LOG(LS_WARNING) << "Default implementation of "
                           "SessionDescriptionInterface::GetType does not "
                           "recognize the result from type(), returning "
                           "kOffer.";
    return SdpType::kOffer;
  }
}

SessionDescriptionInterface* CreateSessionDescription(const std::string& type,
                                                      const std::string& sdp,
                                                      SdpParseError* error) {
  absl::optional<SdpType> maybe_type = SdpTypeFromString(type);
  if (!maybe_type) {
    return nullptr;
  }

  return CreateSessionDescription(*maybe_type, sdp, error).release();
}

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& sdp) {
  return CreateSessionDescription(type, sdp, nullptr);
}

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& sdp,
    SdpParseError* error_out) {
  auto jsep_desc = std::make_unique<JsepSessionDescription>(type);
  if (type != SdpType::kRollback) {
    if (!SdpDeserialize(sdp, jsep_desc.get(), error_out)) {
      return nullptr;
    }
  }
  return std::move(jsep_desc);
}

std::unique_ptr<SessionDescriptionInterface> CreateSessionDescription(
    SdpType type,
    const std::string& session_id,
    const std::string& session_version,
    std::unique_ptr<cricket::SessionDescription> description) {
  auto jsep_description = std::make_unique<JsepSessionDescription>(type);
  bool initialize_success = jsep_description->Initialize(
      std::move(description), session_id, session_version);
  RTC_DCHECK(initialize_success);
  return std::move(jsep_description);
}

JsepSessionDescription::JsepSessionDescription(SdpType type) : type_(type) {}

JsepSessionDescription::JsepSessionDescription(const std::string& type) {
  absl::optional<SdpType> maybe_type = SdpTypeFromString(type);
  if (maybe_type) {
    type_ = *maybe_type;
  } else {
    RTC_LOG(LS_WARNING)
        << "JsepSessionDescription constructed with invalid type string: "
        << type << ". Assuming it is an offer.";
    type_ = SdpType::kOffer;
  }
}

JsepSessionDescription::JsepSessionDescription(
    SdpType type,
    std::unique_ptr<cricket::SessionDescription> description,
    absl::string_view session_id,
    absl::string_view session_version)
    : description_(std::move(description)),
      session_id_(session_id),
      session_version_(session_version),
      type_(type) {
  RTC_DCHECK(description_);
  candidate_collection_.resize(number_of_mediasections());
}

JsepSessionDescription::~JsepSessionDescription() {}

bool JsepSessionDescription::Initialize(
    std::unique_ptr<cricket::SessionDescription> description,
    const std::string& session_id,
    const std::string& session_version) {
  if (!description)
    return false;

  session_id_ = session_id;
  session_version_ = session_version;
  description_ = std::move(description);
  candidate_collection_.resize(number_of_mediasections());
  return true;
}

std::unique_ptr<SessionDescriptionInterface> JsepSessionDescription::Clone()
    const {
  auto new_description = std::make_unique<JsepSessionDescription>(type_);
  new_description->session_id_ = session_id_;
  new_description->session_version_ = session_version_;
  if (description_) {
    new_description->description_ = description_->Clone();
  }
  for (const auto& collection : candidate_collection_) {
    new_description->candidate_collection_.push_back(collection.Clone());
  }
  return new_description;
}

bool JsepSessionDescription::AddCandidate(
    const IceCandidateInterface* candidate) {
  if (!candidate)
    return false;
  size_t mediasection_index = 0;
  if (!GetMediasectionIndex(candidate, &mediasection_index)) {
    return false;
  }
  if (mediasection_index >= number_of_mediasections())
    return false;
  const std::string& content_name =
      description_->contents()[mediasection_index].name;
  const cricket::TransportInfo* transport_info =
      description_->GetTransportInfoByName(content_name);
  if (!transport_info) {
    return false;
  }

  Candidate updated_candidate = candidate->candidate();
  if (updated_candidate.username().empty()) {
    updated_candidate.set_username(transport_info->description.ice_ufrag);
  }
  if (updated_candidate.password().empty()) {
    updated_candidate.set_password(transport_info->description.ice_pwd);
  }

  std::unique_ptr<JsepIceCandidate> updated_candidate_wrapper(
      new JsepIceCandidate(candidate->sdp_mid(),
                           static_cast<int>(mediasection_index),
                           updated_candidate));
  if (!candidate_collection_[mediasection_index].HasCandidate(
          updated_candidate_wrapper.get())) {
    candidate_collection_[mediasection_index].add(
        updated_candidate_wrapper.release());
    UpdateConnectionAddress(
        candidate_collection_[mediasection_index],
        description_->contents()[mediasection_index].media_description());
  }

  return true;
}

size_t JsepSessionDescription::RemoveCandidates(
    const std::vector<Candidate>& candidates) {
  size_t num_removed = 0;
  for (auto& candidate : candidates) {
    int mediasection_index = GetMediasectionIndex(candidate);
    if (mediasection_index < 0) {

      continue;
    }
    num_removed += candidate_collection_[mediasection_index].remove(candidate);
    UpdateConnectionAddress(
        candidate_collection_[mediasection_index],
        description_->contents()[mediasection_index].media_description());
  }
  return num_removed;
}

size_t JsepSessionDescription::number_of_mediasections() const {
  if (!description_)
    return 0;
  return description_->contents().size();
}

const IceCandidateCollection* JsepSessionDescription::candidates(
    size_t mediasection_index) const {
  if (mediasection_index >= candidate_collection_.size())
    return NULL;
  return &candidate_collection_[mediasection_index];
}

bool JsepSessionDescription::ToString(std::string* out) const {
  if (!description_ || !out) {
    return false;
  }
  *out = SdpSerialize(*this);
  return !out->empty();
}

bool JsepSessionDescription::GetMediasectionIndex(
    const IceCandidateInterface* candidate,
    size_t* index) {
  if (!candidate || !index) {
    return false;
  }

  if (candidate->sdp_mid().empty() &&
      (candidate->sdp_mline_index() < 0 ||
       static_cast<size_t>(candidate->sdp_mline_index()) >=
           description_->contents().size())) {
    return false;
  }

  if (candidate->sdp_mline_index() >= 0)
    *index = static_cast<size_t>(candidate->sdp_mline_index());
  if (description_ && !candidate->sdp_mid().empty()) {
    bool found = false;

    for (size_t i = 0; i < description_->contents().size(); ++i) {
      if (candidate->sdp_mid() == description_->contents().at(i).name) {
        *index = i;
        found = true;
        break;
      }
    }
    if (!found) {

      return false;
    }
  }
  return true;
}

int JsepSessionDescription::GetMediasectionIndex(const Candidate& candidate) {

  const std::string& transport_name = candidate.transport_name();
  for (size_t i = 0; i < description_->contents().size(); ++i) {
    if (transport_name == description_->contents().at(i).name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

}
