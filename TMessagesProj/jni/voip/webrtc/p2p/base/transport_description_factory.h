/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TRANSPORT_DESCRIPTION_FACTORY_H_
#define P2P_BASE_TRANSPORT_DESCRIPTION_FACTORY_H_

#include <memory>
#include <utility>

#include "api/field_trials_view.h"
#include "p2p/base/ice_credentials_iterator.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/rtc_certificate.h"

namespace rtc {
class SSLIdentity;
}

namespace cricket {

struct TransportOptions {
  bool ice_restart = false;
  bool prefer_passive_role = false;

  bool enable_ice_renomination = false;
};

class TransportDescriptionFactory {
 public:

  explicit TransportDescriptionFactory(
      const webrtc::FieldTrialsView& field_trials);
  ~TransportDescriptionFactory();

  const rtc::scoped_refptr<rtc::RTCCertificate>& certificate() const {
    return certificate_;
  }

  void set_certificate(rtc::scoped_refptr<rtc::RTCCertificate> certificate) {
    certificate_ = std::move(certificate);
  }

  std::unique_ptr<TransportDescription> CreateOffer(
      const TransportOptions& options,
      const TransportDescription* current_description,
      IceCredentialsIterator* ice_credentials) const;

  std::unique_ptr<TransportDescription> CreateAnswer(
      const TransportDescription* offer,
      const TransportOptions& options,
      bool require_transport_attributes,
      const TransportDescription* current_description,
      IceCredentialsIterator* ice_credentials) const;

  const webrtc::FieldTrialsView& trials() const { return field_trials_; }

  bool insecure() const { return insecure_; }
  void SetInsecureForTesting() { insecure_ = true; }

 private:
  bool SetSecurityInfo(TransportDescription* description,
                       ConnectionRole role) const;
  bool insecure_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  const webrtc::FieldTrialsView& field_trials_;
};

}

#endif
