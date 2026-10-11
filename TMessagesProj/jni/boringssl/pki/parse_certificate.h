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

#ifndef BSSL_PKI_PARSE_CERTIFICATE_H_
#define BSSL_PKI_PARSE_CERTIFICATE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <openssl/base.h>

#include "general_names.h"
#include "input.h"
#include "parse_values.h"

BSSL_NAMESPACE_BEGIN

namespace der {
class Parser;
}

class CertErrors;
struct ParsedTbsCertificate;

[[nodiscard]] OPENSSL_EXPORT bool VerifySerialNumber(der::Input value,
                                                     bool warnings_only,
                                                     CertErrors *errors);

[[nodiscard]] OPENSSL_EXPORT bool ReadUTCOrGeneralizedTime(
    der::Parser *parser, der::GeneralizedTime *out);

[[nodiscard]] OPENSSL_EXPORT bool ParseValidity(
    der::Input validity_tlv, der::GeneralizedTime *not_before,
    der::GeneralizedTime *not_after);

struct OPENSSL_EXPORT ParseCertificateOptions {

  bool allow_invalid_serial_numbers = false;
};

[[nodiscard]] OPENSSL_EXPORT bool ParseCertificate(
    der::Input certificate_tlv, der::Input *out_tbs_certificate_tlv,
    der::Input *out_signature_algorithm_tlv,
    der::BitString *out_signature_value, CertErrors *out_errors);

[[nodiscard]] OPENSSL_EXPORT bool ParseTbsCertificate(
    der::Input tbs_tlv, const ParseCertificateOptions &options,
    ParsedTbsCertificate *out, CertErrors *errors);

enum class CertificateVersion {
  V1,
  V2,
  V3,
};

struct OPENSSL_EXPORT ParsedTbsCertificate {
  ParsedTbsCertificate();
  ParsedTbsCertificate(ParsedTbsCertificate &&other);
  ParsedTbsCertificate &operator=(ParsedTbsCertificate &&other) = default;
  ~ParsedTbsCertificate();

  CertificateVersion version = CertificateVersion::V1;

  der::Input serial_number;

  der::Input signature_algorithm_tlv;

  der::Input issuer_tlv;

  der::GeneralizedTime validity_not_before;
  der::GeneralizedTime validity_not_after;

  der::Input subject_tlv;

  der::Input spki_tlv;

  std::optional<der::BitString> issuer_unique_id;

  std::optional<der::BitString> subject_unique_id;

  std::optional<der::Input> extensions_tlv;
};

struct OPENSSL_EXPORT ParsedExtension {
  der::Input oid;

  der::Input value;
  bool critical = false;
};

[[nodiscard]] OPENSSL_EXPORT bool ParseExtension(der::Input extension_tlv,
                                                 ParsedExtension *out);

inline constexpr uint8_t kSubjectKeyIdentifierOid[] = {0x55, 0x1d, 0x0e};

inline constexpr uint8_t kKeyUsageOid[] = {0x55, 0x1d, 0x0f};

inline constexpr uint8_t kSubjectAltNameOid[] = {0x55, 0x1d, 0x11};

inline constexpr uint8_t kBasicConstraintsOid[] = {0x55, 0x1d, 0x13};

inline constexpr uint8_t kNameConstraintsOid[] = {0x55, 0x1d, 0x1e};

inline constexpr uint8_t kCertificatePoliciesOid[] = {0x55, 0x1d, 0x20};

inline constexpr uint8_t kAuthorityKeyIdentifierOid[] = {0x55, 0x1d, 0x23};

inline constexpr uint8_t kPolicyConstraintsOid[] = {0x55, 0x1d, 0x24};

inline constexpr uint8_t kExtKeyUsageOid[] = {0x55, 0x1d, 0x25};

inline constexpr uint8_t kAuthorityInfoAccessOid[] = {0x2B, 0x06, 0x01, 0x05,
                                                      0x05, 0x07, 0x01, 0x01};

inline constexpr uint8_t kAdCaIssuersOid[] = {0x2B, 0x06, 0x01, 0x05,
                                              0x05, 0x07, 0x30, 0x02};

inline constexpr uint8_t kAdOcspOid[] = {0x2B, 0x06, 0x01, 0x05,
                                         0x05, 0x07, 0x30, 0x01};

inline constexpr uint8_t kCrlDistributionPointsOid[] = {0x55, 0x1d, 0x1f};

inline constexpr uint8_t kCtPoisonOid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                           0xD6, 0x79, 0x02, 0x04, 0x03};

inline constexpr uint8_t kMSApplicationPoliciesOid[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x15, 0x0a};

inline constexpr uint8_t kRcsMlsParticipantInformation[] = {0x67, 0x81, 0x12,
                                                            0x02, 0x01, 0x04};

inline constexpr uint8_t kRcsMlsAcsParticipantInformation[] = {
    0x67, 0x81, 0x12, 0x02, 0x01, 0x05};

[[nodiscard]] OPENSSL_EXPORT bool ParseExtensions(
    der::Input extensions_tlv,
    std::map<der::Input, ParsedExtension> *extensions);

[[nodiscard]] OPENSSL_EXPORT bool ConsumeExtension(
    der::Input oid,
    std::map<der::Input, ParsedExtension> *unconsumed_extensions,
    ParsedExtension *extension);

struct ParsedBasicConstraints {
  bool is_ca = false;
  bool has_path_len = false;
  uint8_t path_len = 0;
};

[[nodiscard]] OPENSSL_EXPORT bool ParseBasicConstraints(
    der::Input basic_constraints_tlv, ParsedBasicConstraints *out);

enum KeyUsageBit {
  KEY_USAGE_BIT_DIGITAL_SIGNATURE = 0,
  KEY_USAGE_BIT_NON_REPUDIATION = 1,
  KEY_USAGE_BIT_KEY_ENCIPHERMENT = 2,
  KEY_USAGE_BIT_DATA_ENCIPHERMENT = 3,
  KEY_USAGE_BIT_KEY_AGREEMENT = 4,
  KEY_USAGE_BIT_KEY_CERT_SIGN = 5,
  KEY_USAGE_BIT_CRL_SIGN = 6,
  KEY_USAGE_BIT_ENCIPHER_ONLY = 7,
  KEY_USAGE_BIT_DECIPHER_ONLY = 8,
};

[[nodiscard]] OPENSSL_EXPORT bool ParseKeyUsage(der::Input key_usage_tlv,
                                                der::BitString *key_usage);

struct AuthorityInfoAccessDescription {

  der::Input access_method_oid;

  der::Input access_location;
};

[[nodiscard]] OPENSSL_EXPORT bool ParseAuthorityInfoAccess(
    der::Input authority_info_access_tlv,
    std::vector<AuthorityInfoAccessDescription> *out_access_descriptions);

[[nodiscard]] OPENSSL_EXPORT bool ParseAuthorityInfoAccessURIs(
    der::Input authority_info_access_tlv,
    std::vector<std::string_view> *out_ca_issuers_uris,
    std::vector<std::string_view> *out_ocsp_uris);

struct OPENSSL_EXPORT ParsedDistributionPoint {
  ParsedDistributionPoint();
  ParsedDistributionPoint(ParsedDistributionPoint &&other);
  ~ParsedDistributionPoint();

  std::unique_ptr<GeneralNames> distribution_point_fullname;

  std::optional<der::Input> distribution_point_name_relative_to_crl_issuer;

  std::optional<der::Input> reasons;

  std::optional<der::Input> crl_issuer;
};

[[nodiscard]] OPENSSL_EXPORT bool ParseCrlDistributionPoints(
    der::Input distribution_points_tlv,
    std::vector<ParsedDistributionPoint> *distribution_points);

struct OPENSSL_EXPORT ParsedAuthorityKeyIdentifier {
  ParsedAuthorityKeyIdentifier();
  ~ParsedAuthorityKeyIdentifier();
  ParsedAuthorityKeyIdentifier(ParsedAuthorityKeyIdentifier &&other);
  ParsedAuthorityKeyIdentifier &operator=(ParsedAuthorityKeyIdentifier &&other);

  std::optional<der::Input> key_identifier;

  std::optional<der::Input> authority_cert_issuer;

  std::optional<der::Input> authority_cert_serial_number;
};

[[nodiscard]] OPENSSL_EXPORT bool ParseAuthorityKeyIdentifier(
    der::Input extension_value,
    ParsedAuthorityKeyIdentifier *authority_key_identifier);

[[nodiscard]] OPENSSL_EXPORT bool ParseSubjectKeyIdentifier(
    der::Input extension_value, der::Input *subject_key_identifier);

BSSL_NAMESPACE_END

#endif
