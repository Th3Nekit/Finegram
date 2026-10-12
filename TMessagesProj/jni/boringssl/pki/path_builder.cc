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

#include "path_builder.h"

#include <cassert>
#include <memory>
#include <set>
#include <unordered_set>

#include <openssl/base.h>
#include <openssl/pki/verify_error.h>
#include <openssl/sha.h>

#include "cert_issuer_source.h"
#include "certificate_policies.h"
#include "common_cert_errors.h"
#include "parse_certificate.h"
#include "parse_name.h"
#include "parser.h"
#include "string_util.h"
#include "trust_store.h"
#include "verify_certificate_chain.h"
#include "verify_name_match.h"

BSSL_NAMESPACE_BEGIN

namespace {

using CertIssuerSources = std::vector<CertIssuerSource *>;

std::string FingerPrintParsedCertificate(const bssl::ParsedCertificate *cert) {
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(cert->der_cert().data(), cert->der_cert().size(), digest);
  return bssl::string_util::HexEncode(digest);
}

std::string CertDebugString(const ParsedCertificate *cert) {
  RDNSequence subject;
  std::string subject_str;
  if (!ParseName(cert->tbs().subject_tlv, &subject) ||
      !ConvertToRFC2253(subject, &subject_str)) {
    subject_str = "???";
  }

  return FingerPrintParsedCertificate(cert) + " " + subject_str;
}

std::string PathDebugString(const ParsedCertificateList &certs) {
  std::string s;
  for (const auto &cert : certs) {
    if (!s.empty()) {
      s += "\n";
    }
    s += " " + CertDebugString(cert.get());
  }
  return s;
}

struct IssuerEntry {
  std::shared_ptr<const ParsedCertificate> cert;
  CertificateTrust trust;
  int trust_and_key_id_match_ordering;
};

enum KeyIdentifierMatch {

  kMatch = 0,

  kNoData = 1,

  kMismatch = 2,
};

KeyIdentifierMatch CalculateKeyIdentifierMatch(
    const ParsedCertificate *target, const ParsedCertificate *issuer) {
  if (!target->authority_key_identifier()) {
    return kNoData;
  }

  if (target->authority_key_identifier()->key_identifier &&
      issuer->subject_key_identifier()) {
    if (target->authority_key_identifier()->key_identifier !=
        issuer->subject_key_identifier().value()) {
      return kMismatch;
    }
    return kMatch;
  }

  return kNoData;
}

int TrustAndKeyIdentifierMatchToOrder(const ParsedCertificate *target,
                                      const ParsedCertificate *issuer,
                                      const CertificateTrust &issuer_trust) {
  enum {
    kTrustedAndKeyIdMatch = 0,
    kTrustedAndKeyIdNoData = 1,
    kKeyIdMatch = 2,
    kKeyIdNoData = 3,
    kTrustedAndKeyIdMismatch = 4,
    kKeyIdMismatch = 5,
    kDistrustedAndKeyIdMatch = 6,
    kDistrustedAndKeyIdNoData = 7,
    kDistrustedAndKeyIdMismatch = 8,
  };

  KeyIdentifierMatch key_id_match = CalculateKeyIdentifierMatch(target, issuer);
  switch (issuer_trust.type) {
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      switch (key_id_match) {
        case kMatch:
          return kTrustedAndKeyIdMatch;
        case kNoData:
          return kTrustedAndKeyIdNoData;
        case kMismatch:
          return kTrustedAndKeyIdMismatch;
      }
      break;
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::TRUSTED_LEAF:
      switch (key_id_match) {
        case kMatch:
          return kKeyIdMatch;
        case kNoData:
          return kKeyIdNoData;
        case kMismatch:
          return kKeyIdMismatch;
      }
      break;
    case CertificateTrustType::DISTRUSTED:
      switch (key_id_match) {
        case kMatch:
          return kDistrustedAndKeyIdMatch;
        case kNoData:
          return kDistrustedAndKeyIdNoData;
        case kMismatch:
          return kDistrustedAndKeyIdMismatch;
      }
      break;
  }
  assert(0);
  return -1;
}

class CertIssuersIter {
 public:

  CertIssuersIter(std::shared_ptr<const ParsedCertificate> cert,
                  CertIssuerSources *cert_issuer_sources,
                  TrustStore *trust_store);

  CertIssuersIter(const CertIssuersIter &) = delete;
  CertIssuersIter &operator=(const CertIssuersIter &) = delete;

  void GetNextIssuer(IssuerEntry *out);

  bool had_non_skipped_issuers() const {
    return issuers_.size() > skipped_issuer_count_;
  }

  void increment_skipped_issuer_count() { skipped_issuer_count_++; }

  const ParsedCertificate *cert() const { return cert_.get(); }
  std::shared_ptr<const ParsedCertificate> reference_cert() const {
    return cert_;
  }

 private:
  void AddIssuers(ParsedCertificateList issuers);
  void DoAsyncIssuerQuery();

  bool HasCurrentIssuer() const { return cur_issuer_ < issuers_.size(); }

  void SortRemainingIssuers();

  std::shared_ptr<const ParsedCertificate> cert_;
  CertIssuerSources *cert_issuer_sources_;
  TrustStore *trust_store_;

  std::vector<IssuerEntry> issuers_;

  size_t cur_issuer_ = 0;

  size_t skipped_issuer_count_ = 0;

  bool issuers_needs_sort_ = false;

  std::unordered_set<std::string_view> present_issuers_;

  bool did_initial_query_ = false;
  bool did_async_issuer_query_ = false;

  size_t cur_async_request_ = 0;

  std::vector<std::unique_ptr<CertIssuerSource::Request>>
      pending_async_requests_;
};

CertIssuersIter::CertIssuersIter(
    std::shared_ptr<const ParsedCertificate> in_cert,
    CertIssuerSources *cert_issuer_sources, TrustStore *trust_store)
    : cert_(std::move(in_cert)),
      cert_issuer_sources_(cert_issuer_sources),
      trust_store_(trust_store) {}

void CertIssuersIter::GetNextIssuer(IssuerEntry *out) {
  if (!did_initial_query_) {
    did_initial_query_ = true;
    for (auto *cert_issuer_source : *cert_issuer_sources_) {
      ParsedCertificateList new_issuers;
      cert_issuer_source->SyncGetIssuersOf(cert(), &new_issuers);
      AddIssuers(std::move(new_issuers));
    }
  }

  if (!HasCurrentIssuer()) {
    if (!did_async_issuer_query_) {

      DoAsyncIssuerQuery();
    }

    while (!HasCurrentIssuer() &&
           cur_async_request_ < pending_async_requests_.size()) {
      ParsedCertificateList new_issuers;
      pending_async_requests_[cur_async_request_]->GetNext(&new_issuers);
      if (new_issuers.empty()) {

        pending_async_requests_[cur_async_request_++].reset();
      } else {
        AddIssuers(std::move(new_issuers));
      }
    }
  }

  if (HasCurrentIssuer()) {
    SortRemainingIssuers();

    *out = issuers_[cur_issuer_++];
    return;
  }

  *out = IssuerEntry();
}

void CertIssuersIter::AddIssuers(ParsedCertificateList new_issuers) {
  for (std::shared_ptr<const ParsedCertificate> &issuer : new_issuers) {
    if (present_issuers_.find(BytesAsStringView(issuer->der_cert())) !=
        present_issuers_.end()) {
      continue;
    }
    present_issuers_.insert(BytesAsStringView(issuer->der_cert()));

    IssuerEntry entry;
    entry.cert = std::move(issuer);
    entry.trust = trust_store_->GetTrust(entry.cert.get());
    entry.trust_and_key_id_match_ordering = TrustAndKeyIdentifierMatchToOrder(
        cert(), entry.cert.get(), entry.trust);

    issuers_.push_back(std::move(entry));
    issuers_needs_sort_ = true;
  }
}

void CertIssuersIter::DoAsyncIssuerQuery() {
  BSSL_CHECK(!did_async_issuer_query_);
  did_async_issuer_query_ = true;
  cur_async_request_ = 0;
  for (auto *cert_issuer_source : *cert_issuer_sources_) {
    std::unique_ptr<CertIssuerSource::Request> request;
    cert_issuer_source->AsyncGetIssuersOf(cert(), &request);
    if (request) {
      pending_async_requests_.push_back(std::move(request));
    }
  }
}

void CertIssuersIter::SortRemainingIssuers() {
  if (!issuers_needs_sort_) {
    return;
  }

  std::stable_sort(
      issuers_.begin() + cur_issuer_, issuers_.end(),
      [](const IssuerEntry &issuer1, const IssuerEntry &issuer2) {

        const bool issuer1_self_issued = issuer1.cert->normalized_subject() ==
                                         issuer1.cert->normalized_issuer();
        const bool issuer2_self_issued = issuer2.cert->normalized_subject() ==
                                         issuer2.cert->normalized_issuer();
        return std::tie(issuer1.trust_and_key_id_match_ordering,
                        issuer2_self_issued,

                        issuer2.cert->tbs().validity_not_before,
                        issuer2.cert->tbs().validity_not_after) <
               std::tie(issuer2.trust_and_key_id_match_ordering,
                        issuer1_self_issued,
                        issuer1.cert->tbs().validity_not_before,
                        issuer1.cert->tbs().validity_not_after);
      });

  issuers_needs_sort_ = false;
}

class CertIssuerIterPath {
 public:

  bool IsPresent(const ParsedCertificate *cert) const {
    return present_certs_.find(GetKey(cert)) != present_certs_.end();
  }

  void Append(std::unique_ptr<CertIssuersIter> cert_issuers_iter) {
    bool added =
        present_certs_.insert(GetKey(cert_issuers_iter->cert())).second;
    BSSL_CHECK(added);
    cur_path_.push_back(std::move(cert_issuers_iter));
  }

  void Pop() {
    size_t num_erased = present_certs_.erase(GetKey(cur_path_.back()->cert()));
    BSSL_CHECK(num_erased == 1U);
    cur_path_.pop_back();
  }

  void CopyPath(ParsedCertificateList *out_path) {
    out_path->clear();
    for (const auto &node : cur_path_) {
      out_path->push_back(node->reference_cert());
    }
  }

  bool Empty() const { return cur_path_.empty(); }

  CertIssuersIter *back() { return cur_path_.back().get(); }

  size_t Length() const { return cur_path_.size(); }

  std::string PathDebugString() {
    std::string s;
    for (const auto &node : cur_path_) {
      if (!s.empty()) {
        s += "\n";
      }
      s += " " + CertDebugString(node->cert());
    }
    return s;
  }

 private:
  using Key = std::tuple<std::string_view, std::string_view, std::string_view>;

  static Key GetKey(const ParsedCertificate *cert) {

    return Key(BytesAsStringView(cert->normalized_subject()),
               BytesAsStringView(cert->subject_alt_names_extension().value),
               BytesAsStringView(cert->tbs().spki_tlv));
  }

  std::vector<std::unique_ptr<CertIssuersIter>> cur_path_;

  std::set<Key> present_certs_;
};

}

const ParsedCertificate *CertPathBuilderResultPath::GetTrustedCert() const {
  if (certs.empty()) {
    return nullptr;
  }

  switch (last_cert_trust.type) {
    case CertificateTrustType::TRUSTED_ANCHOR:
    case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
    case CertificateTrustType::TRUSTED_LEAF:
      return certs.back().get();
    case CertificateTrustType::UNSPECIFIED:
    case CertificateTrustType::DISTRUSTED:
      return nullptr;
  }

  assert(0);
  return nullptr;
}

class CertPathIter {
 public:
  CertPathIter(std::shared_ptr<const ParsedCertificate> cert,
               TrustStore *trust_store);

  CertPathIter(const CertPathIter &) = delete;
  CertPathIter &operator=(const CertPathIter &) = delete;

  void AddCertIssuerSource(CertIssuerSource *cert_issuer_source);

  bool GetNextPath(ParsedCertificateList *out_certs,
                   CertificateTrust *out_last_cert_trust,
                   CertPathErrors *out_errors,
                   CertPathBuilderDelegate *delegate, uint32_t *iteration_count,
                   const uint32_t max_iteration_count,
                   const uint32_t max_path_building_depth);

 private:

  IssuerEntry next_issuer_;

  CertIssuerIterPath cur_path_;

  CertIssuerSources cert_issuer_sources_;

  TrustStore *trust_store_;
};

CertPathIter::CertPathIter(std::shared_ptr<const ParsedCertificate> cert,
                           TrustStore *trust_store)
    : trust_store_(trust_store) {

  next_issuer_.cert = std::move(cert);
  next_issuer_.trust = trust_store_->GetTrust(next_issuer_.cert.get());
}

void CertPathIter::AddCertIssuerSource(CertIssuerSource *cert_issuer_source) {
  cert_issuer_sources_.push_back(cert_issuer_source);
}

bool CertPathIter::GetNextPath(ParsedCertificateList *out_certs,
                               CertificateTrust *out_last_cert_trust,
                               CertPathErrors *out_errors,
                               CertPathBuilderDelegate *delegate,
                               uint32_t *iteration_count,
                               const uint32_t max_iteration_count,
                               const uint32_t max_path_building_depth) {
  out_certs->clear();
  *out_last_cert_trust = CertificateTrust::ForUnspecified();

  while (true) {
    if (delegate->IsDeadlineExpired()) {
      if (cur_path_.Empty()) {

        if (next_issuer_.cert) {
          out_certs->push_back(next_issuer_.cert);
        }
      } else {
        cur_path_.CopyPath(out_certs);
      }
      out_errors->GetOtherErrors()->AddError(cert_errors::kDeadlineExceeded);
      return false;
    }

    if (max_path_building_depth > 0 &&
        cur_path_.Length() >= max_path_building_depth) {
      cur_path_.CopyPath(out_certs);
      out_errors->GetOtherErrors()->AddError(cert_errors::kDepthLimitExceeded);
      if (delegate->IsDebugLogEnabled()) {
        delegate->DebugLog(
            "CertPathIter reached depth limit. Returning "
            "partial path and backtracking:\n" +
            PathDebugString(*out_certs));
      }
      cur_path_.Pop();
      return true;
    }

    if (!next_issuer_.cert) {
      if (cur_path_.Empty()) {
        if (delegate->IsDebugLogEnabled()) {
          delegate->DebugLog("CertPathIter exhausted all paths...");
        }
        return false;
      }

      (*iteration_count)++;
      if (max_iteration_count > 0 && *iteration_count > max_iteration_count) {
        cur_path_.CopyPath(out_certs);
        out_errors->GetOtherErrors()->AddError(
            cert_errors::kIterationLimitExceeded);
        return false;
      }

      cur_path_.back()->GetNextIssuer(&next_issuer_);
      if (!next_issuer_.cert) {
        if (!cur_path_.back()->had_non_skipped_issuers()) {

          cur_path_.CopyPath(out_certs);
          out_errors->GetErrorsForCert(out_certs->size() - 1)
              ->AddError(cert_errors::kNoIssuersFound);
          if (delegate->IsDebugLogEnabled()) {
            delegate->DebugLog(
                "CertPathIter returning partial path and backtracking:\n" +
                PathDebugString(*out_certs));
          }
          cur_path_.Pop();
          return true;
        } else {

          if (delegate->IsDebugLogEnabled()) {
            delegate->DebugLog("CertPathIter backtracking...");
          }
          cur_path_.Pop();
          continue;
        }
      }
    }

    switch (next_issuer_.trust.type) {
      case CertificateTrustType::TRUSTED_ANCHOR:

        if (cur_path_.Empty()) {
          if (delegate->IsDebugLogEnabled()) {
            delegate->DebugLog(
                "Leaf is a trust anchor, considering as UNSPECIFIED");
          }
          next_issuer_.trust = CertificateTrust::ForUnspecified();
        }
        break;
      case CertificateTrustType::TRUSTED_LEAF:

        if (!cur_path_.Empty()) {
          if (delegate->IsDebugLogEnabled()) {
            delegate->DebugLog(
                "Issuer is a trust leaf, considering as UNSPECIFIED");
          }
          next_issuer_.trust = CertificateTrust::ForUnspecified();
        }
        break;
      case CertificateTrustType::DISTRUSTED:
      case CertificateTrustType::UNSPECIFIED:
      case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:

        break;
    }

    switch (next_issuer_.trust.type) {
      case CertificateTrustType::TRUSTED_LEAF:
      case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
        if (cur_path_.Empty() && next_issuer_.trust.require_leaf_selfsigned &&
            !VerifyCertificateIsSelfSigned(*next_issuer_.cert,
                                           delegate->GetVerifyCache(),
                                                      nullptr)) {
          if (delegate->IsDebugLogEnabled()) {
            delegate->DebugLog(
                "Leaf is trusted with require_leaf_selfsigned but is "
                "not self-signed, considering as UNSPECIFIED");
          }
          next_issuer_.trust = CertificateTrust::ForUnspecified();
        }
        break;
      case CertificateTrustType::TRUSTED_ANCHOR:
      case CertificateTrustType::DISTRUSTED:
      case CertificateTrustType::UNSPECIFIED:

        break;
    }

    switch (next_issuer_.trust.type) {

      case CertificateTrustType::DISTRUSTED:
      case CertificateTrustType::TRUSTED_ANCHOR:
      case CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
      case CertificateTrustType::TRUSTED_LEAF: {

        cur_path_.CopyPath(out_certs);
        out_certs->push_back(std::move(next_issuer_.cert));
        if (delegate->IsDebugLogEnabled()) {
          delegate->DebugLog("CertPathIter returning path:\n" +
                             PathDebugString(*out_certs));
        }
        *out_last_cert_trust = next_issuer_.trust;
        next_issuer_ = IssuerEntry();
        return true;
      }
      case CertificateTrustType::UNSPECIFIED: {

        if (cur_path_.IsPresent(next_issuer_.cert.get())) {
          cur_path_.back()->increment_skipped_issuer_count();
          if (delegate->IsDebugLogEnabled()) {
            delegate->DebugLog("CertPathIter skipping dupe cert: " +
                               CertDebugString(next_issuer_.cert.get()));
          }
          next_issuer_ = IssuerEntry();
          continue;
        }

        cur_path_.Append(std::make_unique<CertIssuersIter>(
            std::move(next_issuer_.cert), &cert_issuer_sources_, trust_store_));
        next_issuer_ = IssuerEntry();
        if (delegate->IsDebugLogEnabled()) {
          delegate->DebugLog("CertPathIter cur_path_ =\n" +
                             cur_path_.PathDebugString());
        }

        continue;
      }
    }
  }
}

CertPathBuilderResultPath::CertPathBuilderResultPath() = default;
CertPathBuilderResultPath::~CertPathBuilderResultPath() = default;

bool CertPathBuilderResultPath::IsValid() const {
  return GetTrustedCert() && !errors.ContainsHighSeverityErrors();
}

VerifyError CertPathBuilderResultPath::GetVerifyError() const {

  std::string diagnostic = errors.ToDebugString(certs);
  if (!errors.ContainsHighSeverityErrors()) {

    if (GetTrustedCert()) {
      return VerifyError(VerifyError::StatusCode::PATH_VERIFIED, 0,
                         std::move(diagnostic));
    } else {
      return VerifyError(VerifyError::StatusCode::VERIFICATION_FAILURE, -1,
                         std::move(diagnostic));
    }
  }

  if (errors.ContainsError(cert_errors::kInternalError) ||
      errors.ContainsError(cert_errors::kChainIsEmpty)) {
    return VerifyError(VerifyError::StatusCode::VERIFICATION_FAILURE, -1,
                       std::move(diagnostic));
  }

  if (errors.ContainsError(cert_errors::kDeadlineExceeded)) {
    return VerifyError(VerifyError::StatusCode::PATH_DEADLINE_EXCEEDED, -1,
                       std::move(diagnostic));
  }
  if (errors.ContainsError(cert_errors::kIterationLimitExceeded)) {
    return VerifyError(VerifyError::StatusCode::PATH_ITERATION_COUNT_EXCEEDED,
                       -1, std::move(diagnostic));
  }
  if (errors.ContainsError(cert_errors::kDepthLimitExceeded)) {
    return VerifyError(VerifyError::StatusCode::PATH_DEPTH_LIMIT_REACHED, -1,
                       std::move(diagnostic));
  }

  ptrdiff_t depth = -1;
  std::optional<CertErrorId> single_error =
      errors.FindSingleHighSeverityError(depth);
  if (!single_error.has_value()) {
    return VerifyError(VerifyError::StatusCode::PATH_MULTIPLE_ERRORS, -1,
                       std::move(diagnostic));
  }

  if (single_error.value() == cert_errors::kValidityFailedNotAfter) {
    return VerifyError(VerifyError::StatusCode::CERTIFICATE_EXPIRED, depth,
                       std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kValidityFailedNotBefore) {
    return VerifyError(VerifyError::StatusCode::CERTIFICATE_NOT_YET_VALID,
                       depth, std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kDistrustedByTrustStore ||
      single_error.value() == cert_errors::kCertIsNotTrustAnchor ||
      single_error.value() == cert_errors::kMaxPathLengthViolated ||
      single_error.value() == cert_errors::kSubjectDoesNotMatchIssuer ||
      single_error.value() == cert_errors::kNoIssuersFound) {
    return VerifyError(VerifyError::StatusCode::PATH_NOT_FOUND, depth,
                       std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kVerifySignedDataFailed) {
    return VerifyError(VerifyError::StatusCode::CERTIFICATE_INVALID_SIGNATURE,
                       depth, std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kUnacceptableSignatureAlgorithm) {
    return VerifyError(
        VerifyError::StatusCode::CERTIFICATE_UNSUPPORTED_SIGNATURE_ALGORITHM,
        depth, std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kUnacceptablePublicKey) {
    return VerifyError(VerifyError::StatusCode::CERTIFICATE_UNSUPPORTED_KEY,
                       depth, std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kEkuLacksServerAuth ||
      single_error.value() == cert_errors::kEkuLacksServerAuthButHasAnyEKU ||
      single_error.value() == cert_errors::kEkuLacksClientAuth ||
      single_error.value() == cert_errors::kEkuLacksClientAuthButHasAnyEKU ||
      single_error.value() == cert_errors::kEkuLacksClientAuthOrServerAuth) {
    return VerifyError(VerifyError::StatusCode::CERTIFICATE_NO_MATCHING_EKU,
                       depth, std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kCertificateRevoked) {
    return VerifyError(VerifyError::StatusCode::CERTIFICATE_REVOKED, depth,
                       std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kNoRevocationMechanism) {
    return VerifyError(
        VerifyError::StatusCode::CERTIFICATE_NO_REVOCATION_MECHANISM, depth,
        std::move(diagnostic));
  }
  if (single_error.value() == cert_errors::kUnableToCheckRevocation) {
    return VerifyError(
        VerifyError::StatusCode::CERTIFICATE_UNABLE_TO_CHECK_REVOCATION, depth,
        std::move(diagnostic));
  }

  return VerifyError((depth < 0) ? VerifyError::StatusCode::VERIFICATION_FAILURE
                                 : VerifyError::StatusCode::CERTIFICATE_INVALID,
                     depth, std::move(diagnostic));
}

CertPathBuilder::Result::Result() = default;
CertPathBuilder::Result::Result(Result &&) = default;
CertPathBuilder::Result::~Result() = default;
CertPathBuilder::Result &CertPathBuilder::Result::operator=(Result &&) =
    default;

bool CertPathBuilder::Result::HasValidPath() const {
  return GetBestValidPath() != nullptr;
}

bool CertPathBuilder::Result::AnyPathContainsError(CertErrorId error_id) const {
  for (const auto &path : paths) {
    if (path->errors.ContainsError(error_id)) {
      return true;
    }
  }

  return false;
}

const VerifyError CertPathBuilder::Result::GetBestPathVerifyError() const {
  if (HasValidPath()) {
    return GetBestValidPath()->GetVerifyError();
  }

  if (exceeded_iteration_limit) {
    return VerifyError(
        VerifyError::StatusCode::PATH_ITERATION_COUNT_EXCEEDED, -1,
        "Iteration count exceeded, could not find a trusted path.");
  }
  if (exceeded_deadline) {
    return VerifyError(VerifyError::StatusCode::PATH_DEADLINE_EXCEEDED, -1,
                       "Deadline exceeded. Could not find a trusted path.");
  }
  if (AnyPathContainsError(cert_errors::kDepthLimitExceeded)) {
    return VerifyError(VerifyError::StatusCode::PATH_DEPTH_LIMIT_REACHED, -1,
                       "Depth limit reached. Could not find a trusted path.");
  }

  if (paths.empty()) {
    return VerifyError(VerifyError::StatusCode::VERIFICATION_FAILURE, -1,
                       "No paths in path builder result.");
  }

  CertPathBuilderResultPath *path = paths[best_result_index].get();
  return path->GetVerifyError();
}

const CertPathBuilderResultPath *CertPathBuilder::Result::GetBestValidPath()
    const {
  const CertPathBuilderResultPath *result_path = GetBestPathPossiblyInvalid();

  if (result_path && result_path->IsValid()) {
    return result_path;
  }

  return nullptr;
}

const CertPathBuilderResultPath *
CertPathBuilder::Result::GetBestPathPossiblyInvalid() const {
  BSSL_CHECK((paths.empty() && best_result_index == 0) ||
             best_result_index < paths.size());

  if (best_result_index >= paths.size()) {
    return nullptr;
  }

  return paths[best_result_index].get();
}

CertPathBuilder::CertPathBuilder(
    std::shared_ptr<const ParsedCertificate> cert, TrustStore *trust_store,
    CertPathBuilderDelegate *delegate, const der::GeneralizedTime &time,
    KeyPurpose key_purpose, InitialExplicitPolicy initial_explicit_policy,
    const std::set<der::Input> &user_initial_policy_set,
    InitialPolicyMappingInhibit initial_policy_mapping_inhibit,
    InitialAnyPolicyInhibit initial_any_policy_inhibit)
    : cert_path_iter_(
          std::make_unique<CertPathIter>(std::move(cert), trust_store)),
      delegate_(delegate),
      time_(time),
      key_purpose_(key_purpose),
      initial_explicit_policy_(initial_explicit_policy),
      user_initial_policy_set_(user_initial_policy_set),
      initial_policy_mapping_inhibit_(initial_policy_mapping_inhibit),
      initial_any_policy_inhibit_(initial_any_policy_inhibit) {
  BSSL_CHECK(delegate);

  AddCertIssuerSource(trust_store);
}

CertPathBuilder::~CertPathBuilder() = default;

void CertPathBuilder::AddCertIssuerSource(
    CertIssuerSource *cert_issuer_source) {
  cert_path_iter_->AddCertIssuerSource(cert_issuer_source);
}

void CertPathBuilder::SetIterationLimit(uint32_t limit) {
  max_iteration_count_ = limit;
}

void CertPathBuilder::SetDepthLimit(uint32_t limit) {
  max_path_building_depth_ = limit;
}

void CertPathBuilder::SetValidPathLimit(size_t limit) {
  valid_path_limit_ = limit;
}

void CertPathBuilder::SetExploreAllPaths(bool explore_all_paths) {
  valid_path_limit_ = explore_all_paths ? 0 : 1;
}

CertPathBuilder::Result CertPathBuilder::Run() {
  uint32_t iteration_count = 0;

  while (true) {
    std::unique_ptr<CertPathBuilderResultPath> result_path =
        std::make_unique<CertPathBuilderResultPath>();

    if (!cert_path_iter_->GetNextPath(
            &result_path->certs, &result_path->last_cert_trust,
            &result_path->errors, delegate_, &iteration_count,
            max_iteration_count_, max_path_building_depth_)) {

      if (result_path->errors.ContainsError(
              cert_errors::kIterationLimitExceeded)) {
        out_result_.exceeded_iteration_limit = true;
      }
      if (result_path->errors.ContainsError(cert_errors::kDeadlineExceeded)) {
        out_result_.exceeded_deadline = true;
      }
      if (!result_path->certs.empty()) {

        if (!result_path->errors.ContainsHighSeverityErrors()) {
          result_path->errors.GetOtherErrors()->AddError(
              cert_errors::kInternalError);
        }

        delegate_->CheckPathAfterVerification(*this, result_path.get());

        AddResultPath(std::move(result_path));
      }
      out_result_.iteration_count = iteration_count;
      return std::move(out_result_);
    }

    if (result_path->last_cert_trust.HasUnspecifiedTrust()) {

      if (!result_path->errors.ContainsHighSeverityErrors()) {
        result_path->errors.GetOtherErrors()->AddError(
            cert_errors::kInternalError);
      }
    } else {

      VerifyCertificateChain(
          result_path->certs, result_path->last_cert_trust, delegate_, time_,
          key_purpose_, initial_explicit_policy_, user_initial_policy_set_,
          initial_policy_mapping_inhibit_, initial_any_policy_inhibit_,
          &result_path->user_constrained_policy_set, &result_path->errors);
    }

    delegate_->CheckPathAfterVerification(*this, result_path.get());

    bool path_is_good = result_path->IsValid();

    AddResultPath(std::move(result_path));

    if (path_is_good) {
      valid_path_count_++;
      if (valid_path_limit_ > 0 && valid_path_count_ == valid_path_limit_) {
        out_result_.iteration_count = iteration_count;

        return std::move(out_result_);
      }
    }

  }
}

void CertPathBuilder::AddResultPath(
    std::unique_ptr<CertPathBuilderResultPath> result_path) {

  if (!out_result_.HasValidPath()) {
    const CertPathBuilderResultPath *old_best_path =
        out_result_.GetBestPathPossiblyInvalid();

    if (result_path->IsValid() ||
        (!result_path->last_cert_trust.HasUnspecifiedTrust() && old_best_path &&
         old_best_path->last_cert_trust.HasUnspecifiedTrust())) {
      out_result_.best_result_index = out_result_.paths.size();
    }
  }
  if (result_path->certs.size() > out_result_.max_depth_seen) {
    out_result_.max_depth_seen = result_path->certs.size();
  }
  out_result_.paths.push_back(std::move(result_path));
}

BSSL_NAMESPACE_END
