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

#ifndef BSSL_PKI_PATH_BUILDER_H_
#define BSSL_PKI_PATH_BUILDER_H_

#include <memory>
#include <vector>

#include <openssl/base.h>
#include <openssl/pki/verify_error.h>

#include "cert_errors.h"
#include "input.h"
#include "parse_values.h"
#include "parsed_certificate.h"
#include "trust_store.h"
#include "verify_certificate_chain.h"

BSSL_NAMESPACE_BEGIN

namespace der {
struct GeneralizedTime;
}

class CertPathBuilder;
class CertPathIter;
class CertIssuerSource;

class OPENSSL_EXPORT CertPathBuilderDelegateData {
 public:
  virtual ~CertPathBuilderDelegateData() = default;
};

struct OPENSSL_EXPORT CertPathBuilderResultPath {
  CertPathBuilderResultPath();
  ~CertPathBuilderResultPath();

  bool IsValid() const;

  VerifyError GetVerifyError() const;

  const ParsedCertificate *GetTrustedCert() const;

  ParsedCertificateList certs;

  CertificateTrust last_cert_trust;

  std::set<der::Input> user_constrained_policy_set;

  std::unique_ptr<CertPathBuilderDelegateData> delegate_data;

  CertPathErrors errors;
};

class OPENSSL_EXPORT CertPathBuilderDelegate
    : public VerifyCertificateChainDelegate {
 public:

  virtual void CheckPathAfterVerification(const CertPathBuilder &path_builder,
                                          CertPathBuilderResultPath *path) = 0;

  virtual bool IsDeadlineExpired() = 0;

  virtual bool IsDebugLogEnabled() = 0;

  virtual void DebugLog(std::string_view msg) = 0;
};

class OPENSSL_EXPORT CertPathBuilder {
 public:

  struct OPENSSL_EXPORT Result {
    Result();
    Result(Result &&);

    Result(const Result &) = delete;
    Result &operator=(const Result &) = delete;

    ~Result();
    Result &operator=(Result &&);

    bool HasValidPath() const;

    bool AnyPathContainsError(CertErrorId error_id) const;

    const VerifyError GetBestPathVerifyError() const;

    const CertPathBuilderResultPath *GetBestValidPath() const;

    const CertPathBuilderResultPath *GetBestPathPossiblyInvalid() const;

    std::vector<std::unique_ptr<CertPathBuilderResultPath>> paths;

    size_t best_result_index = 0;

    uint32_t iteration_count = 0;

    uint32_t max_depth_seen = 0;

    bool exceeded_iteration_limit = false;

    bool exceeded_deadline = false;
  };

  CertPathBuilder(std::shared_ptr<const ParsedCertificate> cert,
                  TrustStore *trust_store, CertPathBuilderDelegate *delegate,
                  const der::GeneralizedTime &time, KeyPurpose key_purpose,
                  InitialExplicitPolicy initial_explicit_policy,
                  const std::set<der::Input> &user_initial_policy_set,
                  InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
                  InitialAnyPolicyInhibit initial_any_policy_inhibit);

  CertPathBuilder(const CertPathBuilder &) = delete;
  CertPathBuilder &operator=(const CertPathBuilder &) = delete;

  ~CertPathBuilder();

  void AddCertIssuerSource(CertIssuerSource *cert_issuer_source);

  void SetIterationLimit(uint32_t limit);

  void SetDepthLimit(uint32_t limit);

  void SetValidPathLimit(size_t limit);

  void SetExploreAllPaths(bool explore_all_paths);

  Result Run();

 private:
  void AddResultPath(std::unique_ptr<CertPathBuilderResultPath> result_path);

  Result out_result_;

  std::unique_ptr<CertPathIter> cert_path_iter_;
  CertPathBuilderDelegate *delegate_;
  const der::GeneralizedTime time_;
  const KeyPurpose key_purpose_;
  const InitialExplicitPolicy initial_explicit_policy_;
  const std::set<der::Input> user_initial_policy_set_;
  const InitialPolicyMappingInhibit initial_policy_mapping_inhibit_;
  const InitialAnyPolicyInhibit initial_any_policy_inhibit_;
  uint32_t max_iteration_count_ = 0;
  uint32_t max_path_building_depth_ = 0;
  size_t valid_path_limit_ = 1;
  size_t valid_path_count_ = 0;
};

BSSL_NAMESPACE_END

#endif
