// Copyright 2016 The Chromium Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BSSL_PKI_TRUST_STORE_H_
#define BSSL_PKI_TRUST_STORE_H_

#include <optional>

#include <openssl/base.h>

#include "cert_issuer_source.h"
#include "parsed_certificate.h"

BSSL_NAMESPACE_BEGIN

enum class CertificateTrustType {

  DISTRUSTED,

  UNSPECIFIED,

  TRUSTED_ANCHOR,

  TRUSTED_ANCHOR_OR_LEAF,

  TRUSTED_LEAF,

  LAST = TRUSTED_ANCHOR
};

struct OPENSSL_EXPORT CertificateTrust {
  static constexpr CertificateTrust ForTrustAnchor() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_ANCHOR;
    return result;
  }

  static constexpr CertificateTrust ForTrustAnchorOrLeaf() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF;
    return result;
  }

  static constexpr CertificateTrust ForTrustedLeaf() {
    CertificateTrust result;
    result.type = CertificateTrustType::TRUSTED_LEAF;
    return result;
  }

  static constexpr CertificateTrust ForUnspecified() {
    CertificateTrust result;
    return result;
  }

  static constexpr CertificateTrust ForDistrusted() {
    CertificateTrust result;
    result.type = CertificateTrustType::DISTRUSTED;
    return result;
  }

  constexpr CertificateTrust WithEnforceAnchorExpiry(bool value = true) const {
    CertificateTrust result = *this;
    result.enforce_anchor_expiry = value;
    return result;
  }

  constexpr CertificateTrust WithEnforceAnchorConstraints(
      bool value = true) const {
    CertificateTrust result = *this;
    result.enforce_anchor_constraints = value;
    return result;
  }

  constexpr CertificateTrust WithRequireAnchorBasicConstraints(
      bool value = true) const {
    CertificateTrust result = *this;
    result.require_anchor_basic_constraints = value;
    return result;
  }

  constexpr CertificateTrust WithRequireLeafSelfSigned(
      bool value = true) const {
    CertificateTrust result = *this;
    result.require_leaf_selfsigned = value;
    return result;
  }

  bool IsTrustAnchor() const;
  bool IsTrustLeaf() const;
  bool IsDistrusted() const;
  bool HasUnspecifiedTrust() const;

  std::string ToDebugString() const;

  static std::optional<CertificateTrust> FromDebugString(
      const std::string &trust_string);

  CertificateTrustType type = CertificateTrustType::UNSPECIFIED;

  bool enforce_anchor_expiry = false;
  bool enforce_anchor_constraints = false;

  bool require_anchor_basic_constraints = false;

  bool require_leaf_selfsigned = false;
};

class OPENSSL_EXPORT TrustStore : public CertIssuerSource {
 public:
  TrustStore();

  TrustStore(const TrustStore &) = delete;
  TrustStore &operator=(const TrustStore &) = delete;

  virtual CertificateTrust GetTrust(const ParsedCertificate *cert) = 0;

  void AsyncGetIssuersOf(const ParsedCertificate *cert,
                         std::unique_ptr<Request> *out_req) final;
};

BSSL_NAMESPACE_END

#endif
