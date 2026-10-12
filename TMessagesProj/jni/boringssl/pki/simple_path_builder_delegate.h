// Copyright 2017 The Chromium Authors
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

#ifndef BSSL_PKI_SIMPLE_PATH_BUILDER_DELEGATE_H_
#define BSSL_PKI_SIMPLE_PATH_BUILDER_DELEGATE_H_

#include <stddef.h>

#include <openssl/base.h>
#include <openssl/pki/signature_verify_cache.h>

#include "path_builder.h"
#include "signature_algorithm.h"

BSSL_NAMESPACE_BEGIN

class CertErrors;

class OPENSSL_EXPORT SimplePathBuilderDelegate
    : public CertPathBuilderDelegate {
 public:
  enum class DigestPolicy {

    kStrong,

    kWeakAllowSha1,

    kMaxValue = kWeakAllowSha1
  };

  static const CertErrorId kRsaModulusTooSmall;

  SimplePathBuilderDelegate(size_t min_rsa_modulus_length_bits,
                            DigestPolicy digest_policy);

  bool IsSignatureAlgorithmAcceptable(SignatureAlgorithm signature_algorithm,
                                      CertErrors *errors) override;

  bool IsPublicKeyAcceptable(EVP_PKEY *public_key, CertErrors *errors) override;

  void CheckPathAfterVerification(const CertPathBuilder &path_builder,
                                  CertPathBuilderResultPath *path) override;

  bool IsDeadlineExpired() override;

  SignatureVerifyCache *GetVerifyCache() override;

  bool IsDebugLogEnabled() override;

  void DebugLog(std::string_view msg) override;

  bool AcceptPreCertificates() override;

 private:
  const size_t min_rsa_modulus_length_bits_;
  const DigestPolicy digest_policy_;
};

BSSL_NAMESPACE_END

#endif
