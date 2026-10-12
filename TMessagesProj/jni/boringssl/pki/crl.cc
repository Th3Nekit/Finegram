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

#include <algorithm>
#include <iterator>

#include <openssl/base.h>
#include <openssl/bytestring.h>

#include "cert_errors.h"
#include "crl.h"
#include "input.h"
#include "parse_values.h"
#include "parser.h"
#include "revocation_util.h"
#include "signature_algorithm.h"
#include "verify_name_match.h"
#include "verify_signed_data.h"

BSSL_NAMESPACE_BEGIN

namespace {

inline constexpr uint8_t kIssuingDistributionPointOid[] = {0x55, 0x1d, 0x1c};

[[nodiscard]] bool NormalizeNameTLV(der::Input name_tlv,
                                    std::string *out_normalized_name) {
  der::Parser parser(name_tlv);
  der::Input name_rdn;
  bssl::CertErrors unused_errors;
  return parser.ReadTag(CBS_ASN1_SEQUENCE, &name_rdn) &&
         NormalizeName(name_rdn, out_normalized_name, &unused_errors) &&
         !parser.HasMore();
}

bool ContainsExactMatchingName(std::vector<std::string_view> a,
                               std::vector<std::string_view> b) {
  std::sort(a.begin(), a.end());
  std::sort(b.begin(), b.end());
  std::vector<std::string_view> names_in_common;
  std::set_intersection(a.begin(), a.end(), b.begin(), b.end(),
                        std::back_inserter(names_in_common));
  return !names_in_common.empty();
}

}

bool ParseCrlCertificateList(der::Input crl_tlv,
                             der::Input *out_tbs_cert_list_tlv,
                             der::Input *out_signature_algorithm_tlv,
                             der::BitString *out_signature_value) {
  der::Parser parser(crl_tlv);

  der::Parser certificate_list_parser;
  if (!parser.ReadSequence(&certificate_list_parser)) {
    return false;
  }

  if (!certificate_list_parser.ReadRawTLV(out_tbs_cert_list_tlv)) {
    return false;
  }

  if (!certificate_list_parser.ReadRawTLV(out_signature_algorithm_tlv)) {
    return false;
  }

  std::optional<der::BitString> signature_value =
      certificate_list_parser.ReadBitString();
  if (!signature_value) {
    return false;
  }
  *out_signature_value = signature_value.value();

  if (certificate_list_parser.HasMore()) {
    return false;
  }

  if (parser.HasMore()) {
    return false;
  }

  return true;
}

bool ParseCrlTbsCertList(der::Input tbs_tlv, ParsedCrlTbsCertList *out) {
  der::Parser parser(tbs_tlv);

  der::Parser tbs_parser;
  if (!parser.ReadSequence(&tbs_parser)) {
    return false;
  }

  std::optional<der::Input> version_der;
  if (!tbs_parser.ReadOptionalTag(CBS_ASN1_INTEGER, &version_der)) {
    return false;
  }
  if (version_der.has_value()) {
    uint64_t version64;
    if (!der::ParseUint64(*version_der, &version64)) {
      return false;
    }

    if (version64 != 1) {
      return false;
    }
    out->version = CrlVersion::V2;
  } else {

    out->version = CrlVersion::V1;
  }

  if (!tbs_parser.ReadRawTLV(&out->signature_algorithm_tlv)) {
    return false;
  }

  if (!tbs_parser.ReadRawTLV(&out->issuer_tlv)) {
    return false;
  }

  if (!ReadUTCOrGeneralizedTime(&tbs_parser, &out->this_update)) {
    return false;
  }

  CBS_ASN1_TAG maybe_next_update_tag;
  der::Input unused_next_update_input;
  if (tbs_parser.PeekTagAndValue(&maybe_next_update_tag,
                                 &unused_next_update_input) &&
      (maybe_next_update_tag == CBS_ASN1_UTCTIME ||
       maybe_next_update_tag == CBS_ASN1_GENERALIZEDTIME)) {
    der::GeneralizedTime next_update_time;
    if (!ReadUTCOrGeneralizedTime(&tbs_parser, &next_update_time)) {
      return false;
    }
    out->next_update = next_update_time;
  } else {
    out->next_update = std::nullopt;
  }

  der::Input unused_revoked_certificates;
  CBS_ASN1_TAG maybe_revoked_certifigates_tag;
  if (tbs_parser.PeekTagAndValue(&maybe_revoked_certifigates_tag,
                                 &unused_revoked_certificates) &&
      maybe_revoked_certifigates_tag == CBS_ASN1_SEQUENCE) {
    der::Input revoked_certificates_tlv;
    if (!tbs_parser.ReadRawTLV(&revoked_certificates_tlv)) {
      return false;
    }
    out->revoked_certificates_tlv = revoked_certificates_tlv;
  } else {
    out->revoked_certificates_tlv = std::nullopt;
  }

  if (!tbs_parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0,
          &out->crl_extensions_tlv)) {
    return false;
  }
  if (out->crl_extensions_tlv.has_value()) {
    if (out->version != CrlVersion::V2) {
      return false;
    }
  }

  if (tbs_parser.HasMore()) {

    return false;
  }

  if (parser.HasMore()) {
    return false;
  }

  return true;
}

bool ParseIssuingDistributionPoint(
    der::Input extension_value,
    std::unique_ptr<GeneralNames> *out_distribution_point_names,
    ContainedCertsType *out_only_contains_cert_type) {
  der::Parser idp_extension_value_parser(extension_value);

  der::Parser idp_parser;
  if (!idp_extension_value_parser.ReadSequence(&idp_parser)) {
    return false;
  }

  if (!idp_parser.HasMore()) {
    return false;
  }

  std::optional<der::Input> distribution_point;
  if (!idp_parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0,
          &distribution_point)) {
    return false;
  }

  if (distribution_point.has_value()) {

    der::Parser dp_name_parser(*distribution_point);

    std::optional<der::Input> der_full_name;
    if (!dp_name_parser.ReadOptionalTag(
            CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0,
            &der_full_name)) {
      return false;
    }
    if (!der_full_name) {

      return false;
    }
    CertErrors errors;
    *out_distribution_point_names =
        GeneralNames::CreateFromValue(*der_full_name, &errors);
    if (!*out_distribution_point_names) {
      return false;
    }

    if (dp_name_parser.HasMore()) {

      return false;
    }
  }

  *out_only_contains_cert_type = ContainedCertsType::ANY_CERTS;

  std::optional<der::Input> only_contains_user_certs;
  if (!idp_parser.ReadOptionalTag(CBS_ASN1_CONTEXT_SPECIFIC | 1,
                                  &only_contains_user_certs)) {
    return false;
  }
  if (only_contains_user_certs.has_value()) {
    bool bool_value;
    if (!der::ParseBool(*only_contains_user_certs, &bool_value)) {
      return false;
    }
    if (!bool_value) {
      return false;
    }
    *out_only_contains_cert_type = ContainedCertsType::USER_CERTS;
  }

  std::optional<der::Input> only_contains_ca_certs;
  if (!idp_parser.ReadOptionalTag(CBS_ASN1_CONTEXT_SPECIFIC | 2,
                                  &only_contains_ca_certs)) {
    return false;
  }
  if (only_contains_ca_certs.has_value()) {
    bool bool_value;
    if (!der::ParseBool(*only_contains_ca_certs, &bool_value)) {
      return false;
    }
    if (!bool_value) {
      return false;
    }
    if (*out_only_contains_cert_type != ContainedCertsType::ANY_CERTS) {

      return false;
    }
    *out_only_contains_cert_type = ContainedCertsType::CA_CERTS;
  }

  if (idp_parser.HasMore()) {
    return false;
  }

  return true;
}

CRLRevocationStatus GetCRLStatusForCert(
    der::Input cert_serial, CrlVersion crl_version,
    const std::optional<der::Input> &revoked_certificates_tlv) {
  if (!revoked_certificates_tlv.has_value()) {

    return CRLRevocationStatus::GOOD;
  }

  der::Parser parser(*revoked_certificates_tlv);

  der::Parser revoked_certificates_parser;
  if (!parser.ReadSequence(&revoked_certificates_parser)) {
    return CRLRevocationStatus::UNKNOWN;
  }

  if (!revoked_certificates_parser.HasMore()) {
    return CRLRevocationStatus::UNKNOWN;
  }

  if (parser.HasMore()) {
    return CRLRevocationStatus::UNKNOWN;
  }

  bool found_matching_serial = false;

  while (revoked_certificates_parser.HasMore()) {

    der::Parser crl_entry_parser;
    if (!revoked_certificates_parser.ReadSequence(&crl_entry_parser)) {
      return CRLRevocationStatus::UNKNOWN;
    }

    der::Input revoked_cert_serial_number;

    if (!crl_entry_parser.ReadTag(CBS_ASN1_INTEGER,
                                  &revoked_cert_serial_number)) {
      return CRLRevocationStatus::UNKNOWN;
    }

    der::GeneralizedTime unused_revocation_date;
    if (!ReadUTCOrGeneralizedTime(&crl_entry_parser, &unused_revocation_date)) {
      return CRLRevocationStatus::UNKNOWN;
    }

    if (crl_entry_parser.HasMore()) {

      if (crl_version != CrlVersion::V2) {
        return CRLRevocationStatus::UNKNOWN;
      }

      der::Input crl_entry_extensions_tlv;
      if (!crl_entry_parser.ReadRawTLV(&crl_entry_extensions_tlv)) {
        return CRLRevocationStatus::UNKNOWN;
      }

      std::map<der::Input, ParsedExtension> extensions;
      if (!ParseExtensions(crl_entry_extensions_tlv, &extensions)) {
        return CRLRevocationStatus::UNKNOWN;
      }

      for (const auto &ext : extensions) {
        if (ext.second.critical) {
          return CRLRevocationStatus::UNKNOWN;
        }
      }
    }

    if (crl_entry_parser.HasMore()) {
      return CRLRevocationStatus::UNKNOWN;
    }

    if (revoked_cert_serial_number == cert_serial) {

      found_matching_serial = true;
    }
  }

  if (found_matching_serial) {
    return CRLRevocationStatus::REVOKED;
  }

  return CRLRevocationStatus::GOOD;
}

ParsedCrlTbsCertList::ParsedCrlTbsCertList() = default;
ParsedCrlTbsCertList::~ParsedCrlTbsCertList() = default;

CRLRevocationStatus CheckCRL(std::string_view raw_crl,
                             const ParsedCertificateList &valid_chain,
                             size_t target_cert_index,
                             const ParsedDistributionPoint &cert_dp,
                             int64_t verify_time_epoch_seconds,
                             std::optional<int64_t> max_age_seconds) {
  BSSL_CHECK(target_cert_index < valid_chain.size());

  if (cert_dp.reasons) {

    return CRLRevocationStatus::UNKNOWN;
  }
  if (cert_dp.crl_issuer) {

    return CRLRevocationStatus::UNKNOWN;
  }

  const ParsedCertificate *target_cert = valid_chain[target_cert_index].get();

  der::Input tbs_cert_list_tlv;
  der::Input signature_algorithm_tlv;
  der::BitString signature_value;
  if (!ParseCrlCertificateList(der::Input(raw_crl), &tbs_cert_list_tlv,
                               &signature_algorithm_tlv, &signature_value)) {
    return CRLRevocationStatus::UNKNOWN;
  }

  ParsedCrlTbsCertList tbs_cert_list;
  if (!ParseCrlTbsCertList(tbs_cert_list_tlv, &tbs_cert_list)) {
    return CRLRevocationStatus::UNKNOWN;
  }

  std::optional<SignatureAlgorithm> signature_algorithm =
      ParseSignatureAlgorithm(signature_algorithm_tlv);
  if (!signature_algorithm) {
    return CRLRevocationStatus::UNKNOWN;
  }

  std::optional<SignatureAlgorithm> tbs_alg =
      ParseSignatureAlgorithm(tbs_cert_list.signature_algorithm_tlv);
  if (!tbs_alg || *signature_algorithm != *tbs_alg) {
    return CRLRevocationStatus::UNKNOWN;
  }

  if (!CheckRevocationDateValid(tbs_cert_list.this_update,
                                tbs_cert_list.next_update.has_value()
                                    ? &tbs_cert_list.next_update.value()
                                    : nullptr,
                                verify_time_epoch_seconds, max_age_seconds)) {
    return CRLRevocationStatus::UNKNOWN;
  }

  std::string normalized_crl_issuer;
  if (!NormalizeNameTLV(tbs_cert_list.issuer_tlv, &normalized_crl_issuer)) {
    return CRLRevocationStatus::UNKNOWN;
  }
  if (der::Input(normalized_crl_issuer) != target_cert->normalized_issuer()) {
    return CRLRevocationStatus::UNKNOWN;
  }

  if (tbs_cert_list.crl_extensions_tlv.has_value()) {
    std::map<der::Input, ParsedExtension> extensions;
    if (!ParseExtensions(*tbs_cert_list.crl_extensions_tlv, &extensions)) {
      return CRLRevocationStatus::UNKNOWN;
    }

    ParsedExtension idp_extension;
    if (ConsumeExtension(der::Input(kIssuingDistributionPointOid), &extensions,
                         &idp_extension)) {
      std::unique_ptr<GeneralNames> distribution_point_names;
      ContainedCertsType only_contains_cert_type;
      if (!ParseIssuingDistributionPoint(idp_extension.value,
                                         &distribution_point_names,
                                         &only_contains_cert_type)) {
        return CRLRevocationStatus::UNKNOWN;
      }

      if (distribution_point_names) {

        if (!cert_dp.distribution_point_fullname ||
            !ContainsExactMatchingName(
                cert_dp.distribution_point_fullname
                    ->uniform_resource_identifiers,
                distribution_point_names->uniform_resource_identifiers)) {
          return CRLRevocationStatus::UNKNOWN;
        }

      }

      switch (only_contains_cert_type) {
        case ContainedCertsType::USER_CERTS:

          if ((target_cert->has_basic_constraints() &&
               target_cert->basic_constraints().is_ca) ||
              target_cert->tbs().version == CertificateVersion::V1 ||
              target_cert->tbs().version == CertificateVersion::V2) {
            return CRLRevocationStatus::UNKNOWN;
          }
          break;

        case ContainedCertsType::CA_CERTS:

          if (!target_cert->has_basic_constraints() ||
              !target_cert->basic_constraints().is_ca) {
            return CRLRevocationStatus::UNKNOWN;
          }
          break;

        case ContainedCertsType::ANY_CERTS:

          break;
      }
    }

    for (const auto &ext : extensions) {

      if (ext.second.critical) {
        return CRLRevocationStatus::UNKNOWN;
      }
    }
  }

  for (size_t i = target_cert_index + 1; i < valid_chain.size(); ++i) {
    const ParsedCertificate *issuer_cert = valid_chain[i].get();

    if (der::Input(normalized_crl_issuer) !=
        issuer_cert->normalized_subject()) {
      continue;
    }

    if (issuer_cert->has_key_usage() &&
        !issuer_cert->key_usage().AssertsBit(KEY_USAGE_BIT_CRL_SIGN)) {
      continue;
    }

    if (!VerifySignedData(*signature_algorithm, tbs_cert_list_tlv,
                          signature_value, issuer_cert->tbs().spki_tlv,
                                    nullptr)) {
      continue;
    }

    return GetCRLStatusForCert(target_cert->tbs().serial_number,
                               tbs_cert_list.version,
                               tbs_cert_list.revoked_certificates_tlv);

  }

  return CRLRevocationStatus::UNKNOWN;
}

BSSL_NAMESPACE_END
