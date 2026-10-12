/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/transport_description_factory.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "p2p/base/transport_description.h"
#include "rtc_base/logging.h"
#include "rtc_base/ssl_fingerprint.h"

namespace cricket {

TransportDescriptionFactory::TransportDescriptionFactory(
    const webrtc::FieldTrialsView& field_trials)
    : field_trials_(field_trials) {}

TransportDescriptionFactory::~TransportDescriptionFactory() = default;

std::unique_ptr<TransportDescription> TransportDescriptionFactory::CreateOffer(
    const TransportOptions& options,
    const TransportDescription* current_description,
    IceCredentialsIterator* ice_credentials) const {
  auto desc = std::make_unique<TransportDescription>();

  if (!current_description || options.ice_restart) {
    IceParameters credentials = ice_credentials->GetIceCredentials();
    desc->ice_ufrag = credentials.ufrag;
    desc->ice_pwd = credentials.pwd;
  } else {
    desc->ice_ufrag = current_description->ice_ufrag;
    desc->ice_pwd = current_description->ice_pwd;
  }
  desc->AddOption(ICE_OPTION_TRICKLE);
  if (options.enable_ice_renomination) {
    desc->AddOption(ICE_OPTION_RENOMINATION);
  }

  if (insecure_ && !certificate_) {
    return desc;
  }

  if (!SetSecurityInfo(desc.get(), CONNECTIONROLE_ACTPASS)) {
    return NULL;
  }

  return desc;
}

std::unique_ptr<TransportDescription> TransportDescriptionFactory::CreateAnswer(
    const TransportDescription* offer,
    const TransportOptions& options,
    bool require_transport_attributes,
    const TransportDescription* current_description,
    IceCredentialsIterator* ice_credentials) const {

  if (!offer) {
    RTC_LOG(LS_WARNING) << "Failed to create TransportDescription answer "
                           "because offer is NULL";
    return NULL;
  }

  auto desc = std::make_unique<TransportDescription>();

  if (!current_description || options.ice_restart) {
    IceParameters credentials = ice_credentials->GetIceCredentials();
    desc->ice_ufrag = credentials.ufrag;
    desc->ice_pwd = credentials.pwd;
  } else {
    desc->ice_ufrag = current_description->ice_ufrag;
    desc->ice_pwd = current_description->ice_pwd;
  }
  desc->AddOption(ICE_OPTION_TRICKLE);
  if (options.enable_ice_renomination) {
    desc->AddOption(ICE_OPTION_RENOMINATION);
  }

  if ((!certificate_ || !offer->identity_fingerprint.get()) && insecure()) {
    return desc;
  }
  if (!offer->identity_fingerprint.get()) {
    if (require_transport_attributes) {

      RTC_LOG(LS_WARNING) << "Failed to create TransportDescription answer "
                             "because of incompatible security settings";
      return NULL;
    }

    return desc;
  }

  RTC_CHECK(certificate_);
  ConnectionRole role = CONNECTIONROLE_NONE;

  if (offer->connection_role == CONNECTIONROLE_ACTPASS) {
    role = (options.prefer_passive_role) ? CONNECTIONROLE_PASSIVE
                                         : CONNECTIONROLE_ACTIVE;
  } else if (offer->connection_role == CONNECTIONROLE_ACTIVE) {
    role = CONNECTIONROLE_PASSIVE;
  } else if (offer->connection_role == CONNECTIONROLE_PASSIVE) {
    role = CONNECTIONROLE_ACTIVE;
  } else if (offer->connection_role == CONNECTIONROLE_NONE) {

    RTC_LOG(LS_WARNING) << "Remote offer connection role is NONE, which is "
                           "a protocol violation";
    role = (options.prefer_passive_role) ? CONNECTIONROLE_PASSIVE
                                         : CONNECTIONROLE_ACTIVE;
  } else {
    RTC_LOG(LS_ERROR) << "Remote offer connection role is " << role
                      << " which is a protocol violation";
    RTC_DCHECK_NOTREACHED();
    return NULL;
  }
  if (!SetSecurityInfo(desc.get(), role)) {
    return NULL;
  }
  return desc;
}

bool TransportDescriptionFactory::SetSecurityInfo(TransportDescription* desc,
                                                  ConnectionRole role) const {
  if (!certificate_) {
    RTC_LOG(LS_ERROR) << "Cannot create identity digest with no certificate";
    return false;
  }

  desc->identity_fingerprint =
      rtc::SSLFingerprint::CreateFromCertificate(*certificate_);
  if (!desc->identity_fingerprint) {
    return false;
  }

  desc->connection_role = role;
  return true;
}

}
