// Copyright 2019 The Chromium Authors
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

#ifndef BSSL_PKI_CRL_H_
#define BSSL_PKI_CRL_H_

#include <optional>

#include <openssl/base.h>

#include "general_names.h"
#include "input.h"
#include "parse_values.h"
#include "parsed_certificate.h"

BSSL_NAMESPACE_BEGIN

struct ParsedCrlTbsCertList;
struct ParsedDistributionPoint;

enum class CRLRevocationStatus {
  GOOD = 0,
  REVOKED = 1,
  UNKNOWN = 2,
  MAX_VALUE = UNKNOWN
};

[[nodiscard]] OPENSSL_EXPORT bool ParseCrlCertificateList(
    der::Input crl_tlv, der::Input *out_tbs_cert_list_tlv,
    der::Input *out_signature_algorithm_tlv,
    der::BitString *out_signature_value);

[[nodiscard]] OPENSSL_EXPORT bool ParseCrlTbsCertList(
    der::Input tbs_tlv, ParsedCrlTbsCertList *out);

enum class CrlVersion {
  V1,
  V2,
};

struct OPENSSL_EXPORT ParsedCrlTbsCertList {
  ParsedCrlTbsCertList();
  ~ParsedCrlTbsCertList();

  CrlVersion version = CrlVersion::V1;

  der::Input signature_algorithm_tlv;

  der::Input issuer_tlv;

  der::GeneralizedTime this_update;
  std::optional<der::GeneralizedTime> next_update;

  std::optional<der::Input> revoked_certificates_tlv;

  std::optional<der::Input> crl_extensions_tlv;
};

enum class ContainedCertsType {

  ANY_CERTS,

  USER_CERTS,

  CA_CERTS,
};

[[nodiscard]] OPENSSL_EXPORT bool ParseIssuingDistributionPoint(
    der::Input extension_value,
    std::unique_ptr<GeneralNames> *out_distribution_point_names,
    ContainedCertsType *out_only_contains_cert_type);

OPENSSL_EXPORT CRLRevocationStatus
GetCRLStatusForCert(der::Input cert_serial, CrlVersion crl_version,
                    const std::optional<der::Input> &revoked_certificates_tlv);

[[nodiscard]] OPENSSL_EXPORT CRLRevocationStatus CheckCRL(
    std::string_view raw_crl, const ParsedCertificateList &valid_chain,
    size_t target_cert_index, const ParsedDistributionPoint &cert_dp,
    int64_t verify_time_epoch_seconds, std::optional<int64_t> max_age_seconds);

BSSL_NAMESPACE_END

#endif
