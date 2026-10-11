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

#include "verify_certificate_chain.h"

#include <algorithm>
#include <cassert>

#include <openssl/base.h>
#include "cert_error_params.h"
#include "cert_errors.h"
#include "common_cert_errors.h"
#include "extended_key_usage.h"
#include "input.h"
#include "name_constraints.h"
#include "parse_certificate.h"
#include "signature_algorithm.h"
#include "trust_store.h"
#include "verify_signed_data.h"

BSSL_NAMESPACE_BEGIN

namespace {

bool IsHandledCriticalExtension(const ParsedExtension &extension) {
  if (extension.oid == der::Input(kBasicConstraintsOid)) {
    return true;
  }

  if (extension.oid == der::Input(kKeyUsageOid)) {
    return true;
  }
  if (extension.oid == der::Input(kExtKeyUsageOid)) {
    return true;
  }
  if (extension.oid == der::Input(kNameConstraintsOid)) {
    return true;
  }
  if (extension.oid == der::Input(kSubjectAltNameOid)) {
    return true;
  }
  if (extension.oid == der::Input(kCertificatePoliciesOid)) {

    std::vector<der::Input> unused_policies;
    CertErrors unused_errors;
    return ParseCertificatePoliciesExtensionOids(
        extension.value, true                                        ,
        &unused_policies, &unused_errors);

  }
  if (extension.oid == der::Input(kPolicyMappingsOid)) {
    return true;
  }
  if (extension.oid == der::Input(kPolicyConstraintsOid)) {
    return true;
  }
  if (extension.oid == der::Input(kInhibitAnyPolicyOid)) {
    return true;
  }

  return false;
}

void VerifyNoUnconsumedCriticalExtensions(const ParsedCertificate &cert,
                                          CertErrors *errors,
                                          bool allow_precertificate,
                                          KeyPurpose key_purpose) {
  for (const auto &it : cert.extensions()) {
    const ParsedExtension &extension = it.second;
    if (extension.critical) {
      if (key_purpose == KeyPurpose::RCS_MLS_CLIENT_AUTH) {
        if (extension.oid == der::Input(kRcsMlsParticipantInformation) ||
            extension.oid == der::Input(kRcsMlsAcsParticipantInformation)) {
          continue;
        }
      }
      if (allow_precertificate && extension.oid == der::Input(kCtPoisonOid)) {
        continue;
      }
      if (extension.oid == der::Input(kMSApplicationPoliciesOid) &&
          cert.has_extended_key_usage()) {

        continue;
      }
      if (!IsHandledCriticalExtension(extension)) {
        errors->AddError(cert_errors::kUnconsumedCriticalExtension,
                         CreateCertErrorParams2Der("oid", extension.oid,
                                                   "value", extension.value));
      }
    }
  }
}

[[nodiscard]] bool IsSelfIssued(const ParsedCertificate &cert) {
  return cert.normalized_subject() == cert.normalized_issuer();
}

void VerifyTimeValidity(const ParsedCertificate &cert,
                        const der::GeneralizedTime &time, CertErrors *errors) {
  if (time < cert.tbs().validity_not_before) {
    errors->AddError(cert_errors::kValidityFailedNotBefore);
  }

  if (cert.tbs().validity_not_after < time) {
    errors->AddError(cert_errors::kValidityFailedNotAfter);
  }
}

bool VerifySignatureAlgorithmsMatch(const ParsedCertificate &cert,
                                    CertErrors *errors) {
  der::Input alg1_tlv = cert.signature_algorithm_tlv();
  der::Input alg2_tlv = cert.tbs().signature_algorithm_tlv;

  if (alg1_tlv == alg2_tlv) {
    return true;
  }

  std::optional<SignatureAlgorithm> alg1 = ParseSignatureAlgorithm(alg1_tlv);
  if (!alg1) {
    errors->AddError(cert_errors::kUnacceptableSignatureAlgorithm);
    return false;
  }
  std::optional<SignatureAlgorithm> alg2 = ParseSignatureAlgorithm(alg2_tlv);
  if (!alg2) {
    errors->AddError(cert_errors::kUnacceptableSignatureAlgorithm);
    return false;
  }

  if (*alg1 == *alg2) {
    errors->AddWarning(
        cert_errors::kSignatureAlgorithmsDifferentEncoding,
        CreateCertErrorParams2Der("Certificate.algorithm", alg1_tlv,
                                  "TBSCertificate.signature", alg2_tlv));
    return true;
  }

  errors->AddError(
      cert_errors::kSignatureAlgorithmMismatch,
      CreateCertErrorParams2Der("Certificate.algorithm", alg1_tlv,
                                "TBSCertificate.signature", alg2_tlv));
  return false;
}

void VerifyExtendedKeyUsage(const ParsedCertificate &cert,
                            KeyPurpose required_key_purpose, CertErrors *errors,
                            bool is_target_cert, bool is_target_cert_issuer) {

  if (required_key_purpose == KeyPurpose::ANY_EKU) {
    return;
  }
  bool has_any_eku = false;
  bool has_server_auth_eku = false;
  bool has_client_auth_eku = false;
  bool has_code_signing_eku = false;
  bool has_time_stamping_eku = false;
  bool has_ocsp_signing_eku = false;
  bool has_rcs_mls_client_eku = false;
  size_t eku_oid_count = 0;
  if (cert.has_extended_key_usage()) {
    for (const auto &key_purpose_oid : cert.extended_key_usage()) {
      eku_oid_count++;
      if (key_purpose_oid == der::Input(kAnyEKU)) {
        has_any_eku = true;
      }
      if (key_purpose_oid == der::Input(kServerAuth)) {
        has_server_auth_eku = true;
      }
      if (key_purpose_oid == der::Input(kClientAuth)) {
        has_client_auth_eku = true;
      }
      if (key_purpose_oid == der::Input(kCodeSigning)) {
        has_code_signing_eku = true;
      }
      if (key_purpose_oid == der::Input(kTimeStamping)) {
        has_time_stamping_eku = true;
      }
      if (key_purpose_oid == der::Input(kOCSPSigning)) {
        has_ocsp_signing_eku = true;
      }
      if (key_purpose_oid == der::Input(kRcsMlsClient)) {
        has_rcs_mls_client_eku = true;
      }
    }
  }

  if (required_key_purpose == KeyPurpose::RCS_MLS_CLIENT_AUTH) {

    if (is_target_cert &&
        (!cert.has_key_usage() ||

         !cert.key_usage().AssertsBit(KEY_USAGE_BIT_DIGITAL_SIGNATURE) ||
         cert.key_usage().bytes().size() != 1 ||
         cert.key_usage().unused_bits() != 7)) {
      errors->AddError(cert_errors::kKeyUsageIncorrectForRcsMlsClient);
    }

    if (!cert.has_extended_key_usage()) {
      errors->AddError(cert_errors::kEkuNotPresent);
    } else if (eku_oid_count != 1 || !has_rcs_mls_client_eku) {
      errors->AddError(cert_errors::kEkuIncorrectForRcsMlsClient);
    }
    return;
  }

  if (required_key_purpose == KeyPurpose::CLIENT_AUTH_STRICT_LEAF) {
    if (!is_target_cert) {
      required_key_purpose = KeyPurpose::CLIENT_AUTH;
    } else {
      required_key_purpose = KeyPurpose::CLIENT_AUTH_STRICT;
    }
  }

  if (required_key_purpose == KeyPurpose::SERVER_AUTH_STRICT_LEAF) {
    if (!is_target_cert) {
      required_key_purpose = KeyPurpose::SERVER_AUTH;
    } else {
      required_key_purpose = KeyPurpose::SERVER_AUTH_STRICT;
    }
  }

  auto add_error_if_strict = [&](CertErrorId id) {
    if (required_key_purpose == KeyPurpose::SERVER_AUTH_STRICT ||
        required_key_purpose == KeyPurpose::CLIENT_AUTH_STRICT) {
      errors->AddError(id);
    } else {
      errors->AddWarning(id);
    }
  };
  if (is_target_cert) {

    if (!cert.has_extended_key_usage()) {

      errors->AddWarning(cert_errors::kEkuNotPresent);
    } else {
      if (has_code_signing_eku) {
        add_error_if_strict(cert_errors::kEkuHasProhibitedCodeSigning);
      }
      if (has_ocsp_signing_eku) {
        add_error_if_strict(cert_errors::kEkuHasProhibitedOCSPSigning);
      }
      if (has_time_stamping_eku) {
        add_error_if_strict(cert_errors::kEkuHasProhibitedTimeStamping);
      }
    }
  } else if (is_target_cert_issuer) {

    if (cert.has_extended_key_usage()) {
      if (has_code_signing_eku) {
        add_error_if_strict(cert_errors::kEkuHasProhibitedCodeSigning);
      }
      if (has_time_stamping_eku) {
        add_error_if_strict(cert_errors::kEkuHasProhibitedTimeStamping);
      }
    }
  }

  switch (required_key_purpose) {
    case KeyPurpose::ANY_EKU:
    case KeyPurpose::CLIENT_AUTH_STRICT_LEAF:
    case KeyPurpose::SERVER_AUTH_STRICT_LEAF:
    case KeyPurpose::RCS_MLS_CLIENT_AUTH:
      assert(0);
      return;
    case KeyPurpose::SERVER_AUTH:
    case KeyPurpose::SERVER_AUTH_STRICT: {
      if (has_any_eku && !has_server_auth_eku) {
        if (is_target_cert || is_target_cert_issuer) {
          errors->AddWarning(cert_errors::kEkuLacksServerAuthButHasAnyEKU);
        } else {

          has_server_auth_eku = true;
        }
      }
      if (is_target_cert_issuer && !cert.has_extended_key_usage()) {

        has_server_auth_eku = true;
      }
      if (required_key_purpose == KeyPurpose::SERVER_AUTH) {

        if (cert.has_extended_key_usage() && !has_server_auth_eku &&
            !has_any_eku) {
          errors->AddError(cert_errors::kEkuLacksServerAuth);
        }
      } else {
        if (!has_server_auth_eku) {
          errors->AddError(cert_errors::kEkuLacksServerAuth);
        }
      }
      break;
    }
    case KeyPurpose::CLIENT_AUTH:
    case KeyPurpose::CLIENT_AUTH_STRICT: {
      if (has_any_eku && !has_client_auth_eku) {
        if (is_target_cert || is_target_cert_issuer) {
          errors->AddWarning(cert_errors::kEkuLacksClientAuthButHasAnyEKU);
        } else {

          has_client_auth_eku = true;
        }
      }
      if (required_key_purpose == KeyPurpose::CLIENT_AUTH) {

        if (cert.has_extended_key_usage() && !has_client_auth_eku &&
            !has_any_eku) {
          errors->AddError(cert_errors::kEkuLacksClientAuth);
        }
      } else {
        if (!has_client_auth_eku) {
          errors->AddError(cert_errors::kEkuLacksClientAuth);
        }
      }
      break;
    }
  }
}

class ValidPolicyGraph {
 public:
  ValidPolicyGraph() = default;

  ValidPolicyGraph(const ValidPolicyGraph &) = delete;
  ValidPolicyGraph &operator=(const ValidPolicyGraph &) = delete;

  struct Node {

    std::vector<der::Input> parent_policies;

    bool mapped = false;

    bool reachable = false;
  };

  using Level = std::map<der::Input, Node>;

  struct LevelDetails {

    std::map<der::Input, std::vector<der::Input>> expected_policy_map;

    bool has_any_policy = false;
  };

  void Init() {
    SetNull();
    StartLevel();
    AddAnyPolicyNode();
  }

  bool IsNull() const {
    return !current_level_.has_any_policy &&
           (levels_.empty() || levels_.back().empty());
  }
  void SetNull() {
    levels_.clear();
    current_level_ = LevelDetails{};
  }

  LevelDetails StartLevel() {

    if (!levels_.empty()) {
      for (const auto &[policy, node] : levels_.back()) {
        if (!node.mapped) {
          current_level_.expected_policy_map[policy].push_back(policy);
        }
      }
    }

    LevelDetails prev_level = std::move(current_level_);
    levels_.emplace_back();
    current_level_ = LevelDetails{};
    return prev_level;
  }

  std::set<der::Input> GetUserConstrainedPolicySet(
      const std::set<der::Input> &user_initial_policy_set) {
    if (levels_.empty()) {
      return {};
    }

    bool user_has_any_policy =
        user_initial_policy_set.count(der::Input(kAnyPolicyOid)) != 0;
    if (current_level_.has_any_policy) {
      if (user_has_any_policy) {
        return {der::Input(kAnyPolicyOid)};
      }
      return user_initial_policy_set;
    }

    for (auto &[policy, node] : levels_.back()) {

      (void)policy;
      node.reachable = true;
    }
    std::set<der::Input> policy_set;
    for (size_t i = levels_.size() - 1; i < levels_.size(); i--) {
      for (auto &[policy, node] : levels_[i]) {
        if (!node.reachable) {
          continue;
        }
        if (node.parent_policies.empty()) {

          if (user_has_any_policy ||
              user_initial_policy_set.count(policy) > 0) {
            policy_set.insert(policy);
          }
        } else if (i > 0) {

          for (der::Input parent : node.parent_policies) {
            auto iter = levels_[i - 1].find(parent);
            if (iter != levels_[i - 1].end()) {
              iter->second.reachable = true;
            }
          }
        }
      }
    }
    return policy_set;
  }

  void AddAnyPolicyNode() {
    assert(!levels_.empty());
    current_level_.has_any_policy = true;
  }

  void AddNode(der::Input policy, std::vector<der::Input> parent_policies) {
    assert(policy != der::Input(kAnyPolicyOid));
    AddNodeReturningIterator(policy, std::move(parent_policies));
  }

  void AddNodeWithParentAnyPolicy(der::Input policy) {

    AddNode(policy, {});
  }

  void AddPolicyMapping(der::Input issuer_policy, der::Input subject_policy) {
    assert(issuer_policy != der::Input(kAnyPolicyOid));
    assert(subject_policy != der::Input(kAnyPolicyOid));
    if (levels_.empty()) {
      return;
    }

    auto issuer_policy_iter = levels_.back().find(issuer_policy);
    if (issuer_policy_iter == levels_.back().end()) {

      if (!current_level_.has_any_policy) {
        return;
      }

      issuer_policy_iter = AddNodeReturningIterator(issuer_policy, {});
    }

    issuer_policy_iter->second.mapped = true;

    current_level_.expected_policy_map[subject_policy].push_back(issuer_policy);
  }

  void DeleteNode(der::Input policy) {
    if (!levels_.empty()) {
      levels_.back().erase(policy);
    }
  }

 private:
  Level::iterator AddNodeReturningIterator(
      der::Input policy, std::vector<der::Input> parent_policies) {
    assert(policy != der::Input(kAnyPolicyOid));
    auto [iter, inserted] = levels_.back().insert(
        std::pair{policy, Node{std::move(parent_policies)}});

    (void)inserted;
    assert(inserted);
    return iter;
  }

  std::vector<Level> levels_;

  LevelDetails current_level_;
};

class PathVerifier {
 public:

  void Run(const ParsedCertificateList &certs,
           const CertificateTrust &last_cert_trust,
           VerifyCertificateChainDelegate *delegate,
           const der::GeneralizedTime &time, KeyPurpose required_key_purpose,
           InitialExplicitPolicy initial_explicit_policy,
           const std::set<der::Input> &user_initial_policy_set,
           InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
           InitialAnyPolicyInhibit initial_any_policy_inhibit,
           std::set<der::Input> *user_constrained_policy_set,
           CertPathErrors *errors);

 private:

  void VerifyPolicies(const ParsedCertificate &cert, bool is_target_cert,
                      CertErrors *errors);

  void VerifyPolicyMappings(const ParsedCertificate &cert, CertErrors *errors);

  void ApplyPolicyConstraints(const ParsedCertificate &cert);

  void BasicCertificateProcessing(const ParsedCertificate &cert,
                                  bool is_target_cert,
                                  bool is_target_cert_issuer,
                                  const der::GeneralizedTime &time,
                                  KeyPurpose required_key_purpose,
                                  CertErrors *errors,
                                  bool *shortcircuit_chain_validation);

  void PrepareForNextCertificate(const ParsedCertificate &cert,
                                 KeyPurpose key_purpose, CertErrors *errors);

  void WrapUp(const ParsedCertificate &cert, KeyPurpose required_key_purpose,
              const std::set<der::Input> &user_initial_policy_set,
              bool allow_precertificate, CertErrors *errors);

  void ApplyTrustAnchorConstraints(const ParsedCertificate &cert,
                                   KeyPurpose required_key_purpose,
                                   CertErrors *errors);

  void ProcessRootCertificate(const ParsedCertificate &cert,
                              const CertificateTrust &trust,
                              const der::GeneralizedTime &time,
                              KeyPurpose required_key_purpose,
                              CertErrors *errors,
                              bool *shortcircuit_chain_validation);

  void ProcessSingleCertChain(const ParsedCertificate &cert,
                              const CertificateTrust &trust,
                              const der::GeneralizedTime &time,
                              KeyPurpose required_key_purpose,
                              CertErrors *errors);

  bssl::UniquePtr<EVP_PKEY> ParseAndCheckPublicKey(der::Input spki,
                                                   CertErrors *errors);

  ValidPolicyGraph valid_policy_graph_;

  std::set<der::Input> user_constrained_policy_set_;

  std::vector<const NameConstraints *> name_constraints_list_;

  size_t explicit_policy_;

  size_t inhibit_any_policy_;

  size_t policy_mapping_;

  bssl::UniquePtr<EVP_PKEY> working_public_key_;

  der::Input working_normalized_issuer_name_;

  size_t max_path_length_;

  VerifyCertificateChainDelegate *delegate_;
};

void PathVerifier::VerifyPolicies(const ParsedCertificate &cert,
                                  bool is_target_cert, CertErrors *errors) {

  if (cert.has_policy_oids() && !valid_policy_graph_.IsNull()) {
    ValidPolicyGraph::LevelDetails previous_level =
        valid_policy_graph_.StartLevel();

    bool cert_has_any_policy = false;
    for (der::Input p_oid : cert.policy_oids()) {
      if (p_oid == der::Input(kAnyPolicyOid)) {
        cert_has_any_policy = true;
        continue;
      }

      auto iter = previous_level.expected_policy_map.find(p_oid);
      if (iter != previous_level.expected_policy_map.end()) {
        valid_policy_graph_.AddNode(
            p_oid,                     std::move(iter->second));
        previous_level.expected_policy_map.erase(iter);
      } else if (previous_level.has_any_policy) {

        valid_policy_graph_.AddNodeWithParentAnyPolicy(p_oid);
      }
    }

    if (cert_has_any_policy && ((inhibit_any_policy_ > 0) ||
                                (!is_target_cert && IsSelfIssued(cert)))) {
      for (auto &[p_oid, parent_policies] :
           previous_level.expected_policy_map) {
        valid_policy_graph_.AddNode(p_oid, std::move(parent_policies));
      }
      if (previous_level.has_any_policy) {
        valid_policy_graph_.AddAnyPolicyNode();
      }
    }

  }

  if (!cert.has_policy_oids()) {
    valid_policy_graph_.SetNull();
  }

  if (!((explicit_policy_ > 0) || !valid_policy_graph_.IsNull())) {
    errors->AddError(cert_errors::kNoValidPolicy);
  }
}

void PathVerifier::VerifyPolicyMappings(const ParsedCertificate &cert,
                                        CertErrors *errors) {
  if (!cert.has_policy_mappings()) {
    return;
  }

  for (const ParsedPolicyMapping &mapping : cert.policy_mappings()) {
    if (mapping.issuer_domain_policy == der::Input(kAnyPolicyOid) ||
        mapping.subject_domain_policy == der::Input(kAnyPolicyOid)) {

      valid_policy_graph_.SetNull();
      errors->AddError(cert_errors::kPolicyMappingAnyPolicy);
      return;
    }
  }

  if (policy_mapping_ > 0) {
    for (const ParsedPolicyMapping &mapping : cert.policy_mappings()) {
      valid_policy_graph_.AddPolicyMapping(mapping.issuer_domain_policy,
                                           mapping.subject_domain_policy);
    }
  }

  if (policy_mapping_ == 0) {
    for (const ParsedPolicyMapping &mapping : cert.policy_mappings()) {
      valid_policy_graph_.DeleteNode(mapping.issuer_domain_policy);
    }
  }
}

void PathVerifier::ApplyPolicyConstraints(const ParsedCertificate &cert) {

  if (cert.has_policy_constraints()) {

    if (cert.policy_constraints().require_explicit_policy &&
        cert.policy_constraints().require_explicit_policy.value() <
            explicit_policy_) {
      explicit_policy_ =
          cert.policy_constraints().require_explicit_policy.value();
    }

    if (cert.policy_constraints().inhibit_policy_mapping &&
        cert.policy_constraints().inhibit_policy_mapping.value() <
            policy_mapping_) {
      policy_mapping_ =
          cert.policy_constraints().inhibit_policy_mapping.value();
    }
  }

  if (cert.inhibit_any_policy() &&
      cert.inhibit_any_policy().value() < inhibit_any_policy_) {
    inhibit_any_policy_ = cert.inhibit_any_policy().value();
  }
}

void PathVerifier::BasicCertificateProcessing(
    const ParsedCertificate &cert, bool is_target_cert,
    bool is_target_cert_issuer, const der::GeneralizedTime &time,
    KeyPurpose required_key_purpose, CertErrors *errors,
    bool *shortcircuit_chain_validation) {
  *shortcircuit_chain_validation = false;

  if (!VerifySignatureAlgorithmsMatch(cert, errors)) {
    BSSL_CHECK(errors->ContainsAnyErrorWithSeverity(CertError::SEVERITY_HIGH));
    *shortcircuit_chain_validation = true;
  }

  if (!cert.signature_algorithm().has_value() ||
      !delegate_->IsSignatureAlgorithmAcceptable(*cert.signature_algorithm(),
                                                 errors)) {
    *shortcircuit_chain_validation = true;
    errors->AddError(cert_errors::kUnacceptableSignatureAlgorithm);
    return;
  }

  if (working_public_key_) {

    if (!VerifySignedData(*cert.signature_algorithm(),
                          cert.tbs_certificate_tlv(), cert.signature_value(),
                          working_public_key_.get(),
                          delegate_->GetVerifyCache())) {
      *shortcircuit_chain_validation = true;
      errors->AddError(cert_errors::kVerifySignedDataFailed);
    }
  } else {

    *shortcircuit_chain_validation = true;
  }
  if (*shortcircuit_chain_validation) {
    return;
  }

  VerifyTimeValidity(cert, time, errors);

  if (cert.normalized_issuer() != working_normalized_issuer_name_) {
    errors->AddError(cert_errors::kSubjectDoesNotMatchIssuer);
  }

  if (!name_constraints_list_.empty() &&
      (!IsSelfIssued(cert) || is_target_cert)) {
    for (const NameConstraints *nc : name_constraints_list_) {
      nc->IsPermittedCert(cert.normalized_subject(), cert.subject_alt_names(),
                          errors);
    }
  }

  VerifyPolicies(cert, is_target_cert, errors);

  VerifyExtendedKeyUsage(cert, required_key_purpose, errors, is_target_cert,
                         is_target_cert_issuer);
}

void PathVerifier::PrepareForNextCertificate(const ParsedCertificate &cert,
                                             KeyPurpose key_purpose,
                                             CertErrors *errors) {

  VerifyPolicyMappings(cert, errors);

  working_normalized_issuer_name_ = cert.normalized_subject();

  working_public_key_ = ParseAndCheckPublicKey(cert.tbs().spki_tlv, errors);

  if (cert.has_name_constraints()) {
    name_constraints_list_.push_back(&cert.name_constraints());
  }

  if (!IsSelfIssued(cert)) {

    if (explicit_policy_ > 0) {
      explicit_policy_ -= 1;
    }

    if (policy_mapping_ > 0) {
      policy_mapping_ -= 1;
    }

    if (inhibit_any_policy_ > 0) {
      inhibit_any_policy_ -= 1;
    }
  }

  ApplyPolicyConstraints(cert);

  if (!cert.has_basic_constraints()) {
    errors->AddError(cert_errors::kMissingBasicConstraints);
  } else if (!cert.basic_constraints().is_ca) {
    errors->AddError(cert_errors::kBasicConstraintsIndicatesNotCa);
  }

  if (!IsSelfIssued(cert)) {
    if (max_path_length_ == 0) {
      errors->AddError(cert_errors::kMaxPathLengthViolated);
    } else {
      --max_path_length_;
    }
  }

  if (cert.has_basic_constraints() && cert.basic_constraints().has_path_len &&
      cert.basic_constraints().path_len < max_path_length_) {
    max_path_length_ = cert.basic_constraints().path_len;
  }

  if (cert.has_key_usage() &&
      !cert.key_usage().AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN)) {
    errors->AddError(cert_errors::kKeyCertSignBitNotSet);
  }

  VerifyNoUnconsumedCriticalExtensions(
      cert, errors, delegate_->AcceptPreCertificates(), key_purpose);
}

void VerifyTargetCertIsNotCA(const ParsedCertificate &cert,
                             KeyPurpose required_key_purpose,
                             CertErrors *errors) {
  if (cert.has_basic_constraints() && cert.basic_constraints().is_ca) {

    switch (required_key_purpose) {
      case KeyPurpose::ANY_EKU:
        break;
      case KeyPurpose::SERVER_AUTH:
      case KeyPurpose::CLIENT_AUTH:
        errors->AddWarning(cert_errors::kTargetCertShouldNotBeCa);
        break;
      case KeyPurpose::SERVER_AUTH_STRICT:
      case KeyPurpose::CLIENT_AUTH_STRICT:
      case KeyPurpose::CLIENT_AUTH_STRICT_LEAF:
      case KeyPurpose::SERVER_AUTH_STRICT_LEAF:
      case KeyPurpose::RCS_MLS_CLIENT_AUTH:
        errors->AddError(cert_errors::kTargetCertShouldNotBeCa);
        break;
    }
  }
}

void PathVerifier::WrapUp(const ParsedCertificate &cert,
                          KeyPurpose required_key_purpose,
                          const std::set<der::Input> &user_initial_policy_set,
                          bool allow_precertificate,
                          CertErrors * errors) {

  if (explicit_policy_ > 0) {
    explicit_policy_ -= 1;
  }

  if (cert.has_policy_constraints() &&
      cert.policy_constraints().require_explicit_policy.has_value() &&
      cert.policy_constraints().require_explicit_policy == 0) {
    explicit_policy_ = 0;
  }

  VerifyNoUnconsumedCriticalExtensions(cert, errors, allow_precertificate,
                                       required_key_purpose);

  user_constrained_policy_set_ =
      valid_policy_graph_.GetUserConstrainedPolicySet(user_initial_policy_set);

  if (explicit_policy_ == 0 && user_constrained_policy_set_.empty()) {
    errors->AddError(cert_errors::kNoValidPolicy);
  }

  VerifyTargetCertIsNotCA(cert, required_key_purpose, errors);

  ParseAndCheckPublicKey(cert.tbs().spki_tlv, errors);
}

void PathVerifier::ApplyTrustAnchorConstraints(const ParsedCertificate &cert,
                                               KeyPurpose required_key_purpose,
                                               CertErrors *errors) {

  if (cert.has_policy_oids()) {
    VerifyPolicies(cert,                    false, errors);
  }

  VerifyPolicyMappings(cert, errors);

  ApplyPolicyConstraints(cert);

  if (cert.has_key_usage() &&
      !cert.key_usage().AssertsBit(KEY_USAGE_BIT_KEY_CERT_SIGN)) {
    errors->AddError(cert_errors::kKeyCertSignBitNotSet);
  }

  VerifyExtendedKeyUsage(cert, required_key_purpose, errors,
                                            false,
                                                   false);

  if (cert.has_name_constraints()) {
    name_constraints_list_.push_back(&cert.name_constraints());
  }

  if (cert.has_basic_constraints()) {

    if (!cert.basic_constraints().is_ca) {
      errors->AddError(cert_errors::kBasicConstraintsIndicatesNotCa);
    }

    if (cert.basic_constraints().has_path_len) {
      max_path_length_ = cert.basic_constraints().path_len;
    }
  }

  VerifyNoUnconsumedCriticalExtensions(cert, errors,
                                                                false,
                                       required_key_purpose);
}

void PathVerifier::ProcessRootCertificate(const ParsedCertificate &cert,
                                          const CertificateTrust &trust,
                                          const der::GeneralizedTime &time,
                                          KeyPurpose required_key_purpose,
                                          CertErrors *errors,
                                          bool *shortcircuit_chain_validation) {
  *shortcircuit_chain_validation = false;
  switch (trust.type) {
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_LEAF:

      errors->AddError(cert_errors::kCertIsNotTrustAnchor);
      *shortcircuit_chain_validation = true;
      break;
    case CertificateTrustType::DISTRUSTED:

      errors->AddError(cert_errors::kDistrustedByTrustStore);
      *shortcircuit_chain_validation = true;
      break;
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      break;
  }
  if (*shortcircuit_chain_validation) {
    return;
  }

  if (trust.enforce_anchor_expiry) {
    VerifyTimeValidity(cert, time, errors);
  }
  if (trust.enforce_anchor_constraints) {
    if (trust.require_anchor_basic_constraints &&
        !cert.has_basic_constraints()) {
      switch (cert.tbs().version) {
        case CertificateVersion::V1:
        case CertificateVersion::V2:
          break;
        case CertificateVersion::V3:
          errors->AddError(cert_errors::kMissingBasicConstraints);
          break;
      }
    }
    ApplyTrustAnchorConstraints(cert, required_key_purpose, errors);
  }

  working_public_key_ = ParseAndCheckPublicKey(cert.tbs().spki_tlv, errors);
  working_normalized_issuer_name_ = cert.normalized_subject();
}

void PathVerifier::ProcessSingleCertChain(const ParsedCertificate &cert,
                                          const CertificateTrust &trust,
                                          const der::GeneralizedTime &time,
                                          KeyPurpose required_key_purpose,
                                          CertErrors *errors) {
  switch (trust.type) {
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_ANCHOR:

      errors->AddError(cert_errors::kCertIsNotTrustAnchor);
      return;
    case CertificateTrustType::DISTRUSTED:

      errors->AddError(cert_errors::kDistrustedByTrustStore);
      return;
    case CertificateTrustType::TRUSTED_LEAF:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      break;
  }

  ParseAndCheckPublicKey(cert.tbs().spki_tlv, errors);

  if (trust.require_leaf_selfsigned) {
    if (!VerifyCertificateIsSelfSigned(cert, delegate_->GetVerifyCache(),
                                       errors)) {

      if (!errors->ContainsAnyErrorWithSeverity(CertError::SEVERITY_HIGH)) {
        errors->AddError(cert_errors::kInternalError);
      }
      return;
    }
  }

  VerifyTimeValidity(cert, time, errors);
  VerifyExtendedKeyUsage(cert, required_key_purpose, errors,
                                            true,
                                                   false);

  VerifyNoUnconsumedCriticalExtensions(cert, errors,
                                                                false,
                                       required_key_purpose);
}

bssl::UniquePtr<EVP_PKEY> PathVerifier::ParseAndCheckPublicKey(
    der::Input spki, CertErrors *errors) {

  bssl::UniquePtr<EVP_PKEY> pkey;
  if (!ParsePublicKey(spki, &pkey)) {
    errors->AddError(cert_errors::kFailedParsingSpki);
    return nullptr;
  }

  if (!delegate_->IsPublicKeyAcceptable(pkey.get(), errors)) {
    errors->AddError(cert_errors::kUnacceptablePublicKey);
  }

  return pkey;
}

void PathVerifier::Run(
    const ParsedCertificateList &certs, const CertificateTrust &last_cert_trust,
    VerifyCertificateChainDelegate *delegate, const der::GeneralizedTime &time,
    KeyPurpose required_key_purpose,
    InitialExplicitPolicy initial_explicit_policy,
    const std::set<der::Input> &user_initial_policy_set,
    InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
    InitialAnyPolicyInhibit initial_any_policy_inhibit,
    std::set<der::Input> *user_constrained_policy_set, CertPathErrors *errors) {

  BSSL_CHECK(delegate);
  BSSL_CHECK(errors);

  delegate_ = delegate;

  if (certs.empty()) {
    errors->GetOtherErrors()->AddError(cert_errors::kChainIsEmpty);
    return;
  }

  if (certs.size() == 1) {
    ProcessSingleCertChain(*certs.front(), last_cert_trust, time,
                           required_key_purpose, errors->GetErrorsForCert(0));
    return;
  }

  const size_t n = certs.size() - 1;

  valid_policy_graph_.Init();

  explicit_policy_ =
      initial_explicit_policy == InitialExplicitPolicy::kTrue ? 0 : n + 1;

  inhibit_any_policy_ =
      initial_any_policy_inhibit == InitialAnyPolicyInhibit::kTrue ? 0 : n + 1;

  policy_mapping_ =
      initial_policy_mapping_inhibit == InitialPolicyMappingInhibit::kTrue
          ? 0
          : n + 1;

  max_path_length_ = n;

  for (size_t i = 0; i < certs.size(); ++i) {
    const size_t index_into_certs = certs.size() - i - 1;

    const bool is_target_cert = index_into_certs == 0;
    const bool is_target_cert_issuer = index_into_certs == 1;
    const bool is_root_cert = i == 0;

    const ParsedCertificate &cert = *certs[index_into_certs];

    CertErrors *cert_errors = errors->GetErrorsForCert(index_into_certs);

    if (is_root_cert) {
      bool shortcircuit_chain_validation = false;
      ProcessRootCertificate(cert, last_cert_trust, time, required_key_purpose,
                             cert_errors, &shortcircuit_chain_validation);
      if (shortcircuit_chain_validation) {

        BSSL_CHECK(cert_errors->ContainsAnyErrorWithSeverity(
            CertError::SEVERITY_HIGH));
        return;
      }

      continue;
    }

    bool shortcircuit_chain_validation = false;

    BasicCertificateProcessing(cert, is_target_cert, is_target_cert_issuer,
                               time, required_key_purpose, cert_errors,
                               &shortcircuit_chain_validation);
    if (shortcircuit_chain_validation) {

      BSSL_CHECK(
          errors->ContainsAnyErrorWithSeverity(CertError::SEVERITY_HIGH));
      return;
    }
    if (!is_target_cert) {
      PrepareForNextCertificate(cert, required_key_purpose, cert_errors);
    } else {
      WrapUp(cert, required_key_purpose, user_initial_policy_set,
             delegate->AcceptPreCertificates(), cert_errors);
    }
  }

  if (user_constrained_policy_set) {
    *user_constrained_policy_set = user_constrained_policy_set_;
  }

}

}

VerifyCertificateChainDelegate::~VerifyCertificateChainDelegate() = default;

void VerifyCertificateChain(
    const ParsedCertificateList &certs, const CertificateTrust &last_cert_trust,
    VerifyCertificateChainDelegate *delegate, const der::GeneralizedTime &time,
    KeyPurpose required_key_purpose,
    InitialExplicitPolicy initial_explicit_policy,
    const std::set<der::Input> &user_initial_policy_set,
    InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
    InitialAnyPolicyInhibit initial_any_policy_inhibit,
    std::set<der::Input> *user_constrained_policy_set, CertPathErrors *errors) {
  PathVerifier verifier;
  verifier.Run(certs, last_cert_trust, delegate, time, required_key_purpose,
               initial_explicit_policy, user_initial_policy_set,
               initial_policy_mapping_inhibit, initial_any_policy_inhibit,
               user_constrained_policy_set, errors);
}

bool VerifyCertificateIsSelfSigned(const ParsedCertificate &cert,
                                   SignatureVerifyCache *cache,
                                   CertErrors *errors) {
  if (cert.normalized_subject() != cert.normalized_issuer()) {
    if (errors) {
      errors->AddError(cert_errors::kSubjectDoesNotMatchIssuer);
    }
    return false;
  }

  if (!cert.signature_algorithm().has_value()) {
    if (errors) {
      errors->AddError(cert_errors::kUnacceptableSignatureAlgorithm);
    }
    return false;
  }

  if (!VerifySignedData(*cert.signature_algorithm(), cert.tbs_certificate_tlv(),
                        cert.signature_value(), cert.tbs().spki_tlv, cache)) {
    if (errors) {
      errors->AddError(cert_errors::kVerifySignedDataFailed);
    }
    return false;
  }

  return true;
}

BSSL_NAMESPACE_END
