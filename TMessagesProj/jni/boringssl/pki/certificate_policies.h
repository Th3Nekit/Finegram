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

#ifndef BSSL_PKI_CERTIFICATE_POLICIES_H_
#define BSSL_PKI_CERTIFICATE_POLICIES_H_

#include <stdint.h>
#include <vector>

#include <optional>
#include "input.h"

BSSL_NAMESPACE_BEGIN

class CertErrors;

inline constexpr uint8_t kAnyPolicyOid[] = {0x55, 0x1D, 0x20, 0x00};

inline constexpr uint8_t kInhibitAnyPolicyOid[] = {0x55, 0x1d, 0x36};

inline constexpr uint8_t kPolicyMappingsOid[] = {0x55, 0x1d, 0x21};

inline constexpr uint8_t kCpsPointerId[] = {0x2b, 0x06, 0x01, 0x05,
                                            0x05, 0x07, 0x02, 0x01};

inline constexpr uint8_t kUserNoticeId[] = {0x2b, 0x06, 0x01, 0x05,
                                            0x05, 0x07, 0x02, 0x02};

struct PolicyQualifierInfo {
  der::Input qualifier_oid;
  der::Input qualifier;
};

struct OPENSSL_EXPORT PolicyInformation {
  PolicyInformation();
  ~PolicyInformation();
  PolicyInformation(const PolicyInformation &);
  PolicyInformation(PolicyInformation &&);

  der::Input policy_oid;
  std::vector<PolicyQualifierInfo> policy_qualifiers;
};

OPENSSL_EXPORT bool ParseCertificatePoliciesExtension(
    der::Input extension_value, std::vector<PolicyInformation> *policies,
    CertErrors *errors);

OPENSSL_EXPORT bool ParseCertificatePoliciesExtensionOids(
    der::Input extension_value, bool fail_parsing_unknown_qualifier_oids,
    std::vector<der::Input> *policy_oids, CertErrors *errors);

struct ParsedPolicyConstraints {
  std::optional<uint8_t> require_explicit_policy;

  std::optional<uint8_t> inhibit_policy_mapping;
};

[[nodiscard]] OPENSSL_EXPORT bool ParsePolicyConstraints(
    der::Input policy_constraints_tlv, ParsedPolicyConstraints *out);

[[nodiscard]] OPENSSL_EXPORT std::optional<uint8_t> ParseInhibitAnyPolicy(
    der::Input inhibit_any_policy_tlv);

struct ParsedPolicyMapping {
  der::Input issuer_domain_policy;
  der::Input subject_domain_policy;
};

[[nodiscard]] OPENSSL_EXPORT bool ParsePolicyMappings(
    der::Input policy_mappings_tlv, std::vector<ParsedPolicyMapping> *mappings);

BSSL_NAMESPACE_END

#endif
