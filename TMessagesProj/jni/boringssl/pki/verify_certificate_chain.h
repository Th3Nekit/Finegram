// Copyright 2015 The Chromium Authors
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

#ifndef BSSL_PKI_VERIFY_CERTIFICATE_CHAIN_H_
#define BSSL_PKI_VERIFY_CERTIFICATE_CHAIN_H_

#include <set>

#include <openssl/base.h>
#include <openssl/evp.h>
#include <openssl/pki/signature_verify_cache.h>

#include "cert_errors.h"
#include "input.h"
#include "parsed_certificate.h"

BSSL_NAMESPACE_BEGIN

namespace der {
struct GeneralizedTime;
}

struct CertificateTrust;

enum class KeyPurpose {
  ANY_EKU,
  SERVER_AUTH,
  CLIENT_AUTH,
  SERVER_AUTH_STRICT,

  SERVER_AUTH_STRICT_LEAF,
  CLIENT_AUTH_STRICT,

  CLIENT_AUTH_STRICT_LEAF,
  RCS_MLS_CLIENT_AUTH,
};

enum class InitialExplicitPolicy {
  kFalse,
  kTrue,
};

enum class InitialPolicyMappingInhibit {
  kFalse,
  kTrue,
};

enum class InitialAnyPolicyInhibit {
  kFalse,
  kTrue,
};

class OPENSSL_EXPORT VerifyCertificateChainDelegate {
 public:

  virtual bool IsSignatureAlgorithmAcceptable(
      SignatureAlgorithm signature_algorithm, CertErrors *errors) = 0;

  virtual bool IsPublicKeyAcceptable(EVP_PKEY *public_key,
                                     CertErrors *errors) = 0;

  virtual SignatureVerifyCache *GetVerifyCache() = 0;

  virtual bool AcceptPreCertificates() = 0;

  virtual ~VerifyCertificateChainDelegate();
};

OPENSSL_EXPORT void VerifyCertificateChain(
    const ParsedCertificateList &certs, const CertificateTrust &last_cert_trust,
    VerifyCertificateChainDelegate *delegate, const der::GeneralizedTime &time,
    KeyPurpose required_key_purpose,
    InitialExplicitPolicy initial_explicit_policy,
    const std::set<der::Input> &user_initial_policy_set,
    InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
    InitialAnyPolicyInhibit initial_any_policy_inhibit,
    std::set<der::Input> *user_constrained_policy_set, CertPathErrors *errors);

OPENSSL_EXPORT bool VerifyCertificateIsSelfSigned(const ParsedCertificate &cert,
                                                  SignatureVerifyCache *cache,
                                                  CertErrors *errors);

BSSL_NAMESPACE_END

#endif
