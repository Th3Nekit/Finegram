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

#include "ocsp.h"

#include <openssl/bytestring.h>
#include <openssl/digest.h>
#include <openssl/mem.h>
#include <openssl/pool.h>
#include <openssl/sha.h>
#include "cert_errors.h"
#include "extended_key_usage.h"
#include "parsed_certificate.h"
#include "revocation_util.h"
#include "string_util.h"
#include "verify_name_match.h"
#include "verify_signed_data.h"

BSSL_NAMESPACE_BEGIN

OCSPCertID::OCSPCertID() = default;
OCSPCertID::~OCSPCertID() = default;

OCSPSingleResponse::OCSPSingleResponse() = default;
OCSPSingleResponse::~OCSPSingleResponse() = default;

OCSPResponseData::OCSPResponseData() = default;
OCSPResponseData::~OCSPResponseData() = default;

OCSPResponse::OCSPResponse() = default;
OCSPResponse::~OCSPResponse() = default;

bool ParseOCSPCertID(der::Input raw_tlv, OCSPCertID *out) {
  der::Parser outer_parser(raw_tlv);
  der::Parser parser;
  if (!outer_parser.ReadSequence(&parser)) {
    return false;
  }
  if (outer_parser.HasMore()) {
    return false;
  }

  der::Input sigalg_tlv;
  if (!parser.ReadRawTLV(&sigalg_tlv)) {
    return false;
  }
  if (!ParseHashAlgorithm(sigalg_tlv, &out->hash_algorithm)) {
    return false;
  }
  if (!parser.ReadTag(CBS_ASN1_OCTETSTRING, &out->issuer_name_hash)) {
    return false;
  }
  if (!parser.ReadTag(CBS_ASN1_OCTETSTRING, &out->issuer_key_hash)) {
    return false;
  }
  if (!parser.ReadTag(CBS_ASN1_INTEGER, &out->serial_number)) {
    return false;
  }
  CertErrors errors;
  if (!VerifySerialNumber(out->serial_number, false                  ,
                          &errors)) {
    return false;
  }

  return !parser.HasMore();
}

namespace {

bool ParseRevokedInfo(der::Input raw_tlv, OCSPCertStatus *out) {
  der::Parser parser(raw_tlv);
  if (!parser.ReadGeneralizedTime(&(out->revocation_time))) {
    return false;
  }

  der::Input reason_input;
  if (!parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0, &reason_input,
          &(out->has_reason))) {
    return false;
  }
  if (out->has_reason) {
    der::Parser reason_parser(reason_input);
    der::Input reason_value_input;
    uint8_t reason_value;
    if (!reason_parser.ReadTag(CBS_ASN1_ENUMERATED, &reason_value_input)) {
      return false;
    }
    if (!der::ParseUint8(reason_value_input, &reason_value)) {
      return false;
    }
    if (reason_value >
        static_cast<uint8_t>(OCSPCertStatus::RevocationReason::LAST)) {
      return false;
    }
    out->revocation_reason =
        static_cast<OCSPCertStatus::RevocationReason>(reason_value);
    if (out->revocation_reason == OCSPCertStatus::RevocationReason::UNUSED) {
      return false;
    }
    if (reason_parser.HasMore()) {
      return false;
    }
  }
  return !parser.HasMore();
}

bool ParseCertStatus(der::Input raw_tlv, OCSPCertStatus *out) {
  der::Parser parser(raw_tlv);
  CBS_ASN1_TAG status_tag;
  der::Input status;
  if (!parser.ReadTagAndValue(&status_tag, &status)) {
    return false;
  }

  out->has_reason = false;
  if (status_tag == (CBS_ASN1_CONTEXT_SPECIFIC | 0)) {
    out->status = OCSPRevocationStatus::GOOD;
  } else if (status_tag ==
             (CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1)) {
    out->status = OCSPRevocationStatus::REVOKED;
    if (!ParseRevokedInfo(status, out)) {
      return false;
    }
  } else if (status_tag == (CBS_ASN1_CONTEXT_SPECIFIC | 2)) {
    out->status = OCSPRevocationStatus::UNKNOWN;
  } else {
    return false;
  }

  return !parser.HasMore();
}

bool AppendHashAsOctetString(const EVP_MD *hash_type, CBB *cbb,
                             der::Input value) {
  CBB octet_string;
  unsigned hash_len;
  uint8_t hash_buffer[EVP_MAX_MD_SIZE];

  return CBB_add_asn1(cbb, &octet_string, CBS_ASN1_OCTETSTRING) &&
         EVP_Digest(value.data(), value.size(), hash_buffer, &hash_len,
                    hash_type, nullptr) &&
         CBB_add_bytes(&octet_string, hash_buffer, hash_len) && CBB_flush(cbb);
}

}

bool ParseOCSPSingleResponse(der::Input raw_tlv, OCSPSingleResponse *out) {
  der::Parser outer_parser(raw_tlv);
  der::Parser parser;
  if (!outer_parser.ReadSequence(&parser)) {
    return false;
  }
  if (outer_parser.HasMore()) {
    return false;
  }

  if (!parser.ReadRawTLV(&(out->cert_id_tlv))) {
    return false;
  }
  der::Input status_tlv;
  if (!parser.ReadRawTLV(&status_tlv)) {
    return false;
  }
  if (!ParseCertStatus(status_tlv, &(out->cert_status))) {
    return false;
  }
  if (!parser.ReadGeneralizedTime(&(out->this_update))) {
    return false;
  }

  der::Input next_update_input;
  if (!parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0,
          &next_update_input, &(out->has_next_update))) {
    return false;
  }
  if (out->has_next_update) {
    der::Parser next_update_parser(next_update_input);
    if (!next_update_parser.ReadGeneralizedTime(&(out->next_update))) {
      return false;
    }
    if (next_update_parser.HasMore()) {
      return false;
    }
  }

  if (!parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1,
          &(out->extensions), &(out->has_extensions))) {
    return false;
  }

  return !parser.HasMore();
}

namespace {

bool ParseResponderID(der::Input raw_tlv, OCSPResponseData::ResponderID *out) {
  der::Parser parser(raw_tlv);
  CBS_ASN1_TAG id_tag;
  der::Input id_input;
  if (!parser.ReadTagAndValue(&id_tag, &id_input)) {
    return false;
  }

  if (id_tag == (CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1)) {
    out->type = OCSPResponseData::ResponderType::NAME;
    out->name = id_input;
  } else if (id_tag == (CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 2)) {
    der::Parser key_parser(id_input);
    der::Input key_hash;
    if (!key_parser.ReadTag(CBS_ASN1_OCTETSTRING, &key_hash)) {
      return false;
    }
    if (key_parser.HasMore()) {
      return false;
    }
    if (key_hash.size() != SHA_DIGEST_LENGTH) {
      return false;
    }

    out->type = OCSPResponseData::ResponderType::KEY_HASH;
    out->key_hash = key_hash;
  } else {
    return false;
  }
  return !parser.HasMore();
}

}

bool ParseOCSPResponseData(der::Input raw_tlv, OCSPResponseData *out) {
  der::Parser outer_parser(raw_tlv);
  der::Parser parser;
  if (!outer_parser.ReadSequence(&parser)) {
    return false;
  }
  if (outer_parser.HasMore()) {
    return false;
  }

  der::Input version_input;
  bool version_present;
  if (!parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0, &version_input,
          &version_present)) {
    return false;
  }

  if (version_present) {
    der::Parser version_parser(version_input);
    if (!version_parser.ReadUint8(&(out->version))) {
      return false;
    }
    if (version_parser.HasMore()) {
      return false;
    }
  } else {
    out->version = 0;
  }

  if (out->version != 0) {
    return false;
  }

  der::Input responder_input;
  if (!parser.ReadRawTLV(&responder_input)) {
    return false;
  }
  if (!ParseResponderID(responder_input, &(out->responder_id))) {
    return false;
  }
  if (!parser.ReadGeneralizedTime(&(out->produced_at))) {
    return false;
  }

  der::Parser responses_parser;
  if (!parser.ReadSequence(&responses_parser)) {
    return false;
  }
  out->responses.clear();
  while (responses_parser.HasMore()) {
    der::Input single_response;
    if (!responses_parser.ReadRawTLV(&single_response)) {
      return false;
    }
    out->responses.push_back(single_response);
  }

  if (!parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1,
          &(out->extensions), &(out->has_extensions))) {
    return false;
  }

  return !parser.HasMore();
}

namespace {

bool ParseBasicOCSPResponse(der::Input raw_tlv, OCSPResponse *out) {
  der::Parser outer_parser(raw_tlv);
  der::Parser parser;
  if (!outer_parser.ReadSequence(&parser)) {
    return false;
  }
  if (outer_parser.HasMore()) {
    return false;
  }

  if (!parser.ReadRawTLV(&(out->data))) {
    return false;
  }
  der::Input sigalg_tlv;
  if (!parser.ReadRawTLV(&sigalg_tlv)) {
    return false;
  }

  std::optional<SignatureAlgorithm> sigalg =
      ParseSignatureAlgorithm(sigalg_tlv);
  if (!sigalg) {
    return false;
  }
  out->signature_algorithm = sigalg.value();
  std::optional<der::BitString> signature = parser.ReadBitString();
  if (!signature) {
    return false;
  }
  out->signature = signature.value();
  der::Input certs_input;
  if (!parser.ReadOptionalTag(
          CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0, &certs_input,
          &(out->has_certs))) {
    return false;
  }

  out->certs.clear();
  if (out->has_certs) {
    der::Parser certs_seq_parser(certs_input);
    der::Parser certs_parser;
    if (!certs_seq_parser.ReadSequence(&certs_parser)) {
      return false;
    }
    if (certs_seq_parser.HasMore()) {
      return false;
    }
    while (certs_parser.HasMore()) {
      der::Input cert_tlv;
      if (!certs_parser.ReadRawTLV(&cert_tlv)) {
        return false;
      }
      out->certs.push_back(cert_tlv);
    }
  }

  return !parser.HasMore();
}

}

bool ParseOCSPResponse(der::Input raw_tlv, OCSPResponse *out) {
  der::Parser outer_parser(raw_tlv);
  der::Parser parser;
  if (!outer_parser.ReadSequence(&parser)) {
    return false;
  }
  if (outer_parser.HasMore()) {
    return false;
  }

  der::Input response_status_input;
  uint8_t response_status;
  if (!parser.ReadTag(CBS_ASN1_ENUMERATED, &response_status_input)) {
    return false;
  }
  if (!der::ParseUint8(response_status_input, &response_status)) {
    return false;
  }
  if (response_status >
      static_cast<uint8_t>(OCSPResponse::ResponseStatus::LAST)) {
    return false;
  }
  out->status = static_cast<OCSPResponse::ResponseStatus>(response_status);
  if (out->status == OCSPResponse::ResponseStatus::UNUSED) {
    return false;
  }

  if (out->status == OCSPResponse::ResponseStatus::SUCCESSFUL) {
    der::Parser outer_bytes_parser;
    der::Parser bytes_parser;
    if (!parser.ReadConstructed(
            CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0,
            &outer_bytes_parser)) {
      return false;
    }
    if (!outer_bytes_parser.ReadSequence(&bytes_parser)) {
      return false;
    }
    if (outer_bytes_parser.HasMore()) {
      return false;
    }

    der::Input type_oid;
    if (!bytes_parser.ReadTag(CBS_ASN1_OBJECT, &type_oid)) {
      return false;
    }
    if (type_oid != der::Input(kBasicOCSPResponseOid)) {
      return false;
    }

    der::Input response;
    if (!bytes_parser.ReadTag(CBS_ASN1_OCTETSTRING, &response)) {
      return false;
    }
    if (!ParseBasicOCSPResponse(response, out)) {
      return false;
    }
    if (bytes_parser.HasMore()) {
      return false;
    }
  }

  return !parser.HasMore();
}

namespace {

bool VerifyHash(const EVP_MD *type, der::Input hash, der::Input value) {
  unsigned value_hash_len;
  uint8_t value_hash[EVP_MAX_MD_SIZE];
  if (!EVP_Digest(value.data(), value.size(), value_hash, &value_hash_len, type,
                  nullptr)) {
    return false;
  }

  return hash == der::Input(value_hash, value_hash_len);
}

bool GetSubjectPublicKeyBytes(der::Input spki_tlv, der::Input *spk_tlv) {
  CBS outer, inner, alg, spk;
  uint8_t unused_bit_count;
  CBS_init(&outer, spki_tlv.data(), spki_tlv.size());

  if (!CBS_get_asn1(&outer, &inner, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&inner, &alg, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&inner, &spk, CBS_ASN1_BITSTRING) ||
      !CBS_get_u8(&spk, &unused_bit_count) || unused_bit_count != 0) {
    return false;
  }
  *spk_tlv = der::Input(CBS_data(&spk), CBS_len(&spk));
  return true;
}

bool CheckCertIDMatchesCertificate(
    const OCSPCertID &id, const ParsedCertificate *certificate,
    const ParsedCertificate *issuer_certificate) {
  const EVP_MD *type = nullptr;
  switch (id.hash_algorithm) {
    case DigestAlgorithm::Md2:
    case DigestAlgorithm::Md4:
    case DigestAlgorithm::Md5:

      return false;
    case DigestAlgorithm::Sha1:
      type = EVP_sha1();
      break;
    case DigestAlgorithm::Sha256:
      type = EVP_sha256();
      break;
    case DigestAlgorithm::Sha384:
      type = EVP_sha384();
      break;
    case DigestAlgorithm::Sha512:
      type = EVP_sha512();
      break;
  }

  if (!VerifyHash(type, id.issuer_name_hash, certificate->tbs().issuer_tlv)) {
    return false;
  }

  der::Input key_tlv;
  if (!GetSubjectPublicKeyBytes(issuer_certificate->tbs().spki_tlv, &key_tlv)) {
    return false;
  }

  if (!VerifyHash(type, id.issuer_key_hash, key_tlv)) {
    return false;
  }

  return id.serial_number == certificate->tbs().serial_number;
}

std::shared_ptr<const ParsedCertificate> OCSPParseCertificate(
    std::string_view der) {
  ParseCertificateOptions parse_options;
  parse_options.allow_invalid_serial_numbers = true;

  CertErrors errors;
  return ParsedCertificate::Create(
      bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
          reinterpret_cast<const uint8_t *>(der.data()), der.size(), nullptr)),
      {}, &errors);
}

[[nodiscard]] bool CheckResponderIDMatchesCertificate(
    const OCSPResponseData::ResponderID &id, const ParsedCertificate *cert) {
  switch (id.type) {
    case OCSPResponseData::ResponderType::NAME: {
      der::Input name_rdn;
      der::Input cert_rdn;
      if (!der::Parser(id.name).ReadTag(CBS_ASN1_SEQUENCE, &name_rdn) ||
          !der::Parser(cert->tbs().subject_tlv)
               .ReadTag(CBS_ASN1_SEQUENCE, &cert_rdn)) {
        return false;
      }
      return VerifyNameMatch(name_rdn, cert_rdn);
    }
    case OCSPResponseData::ResponderType::KEY_HASH: {
      der::Input key;
      if (!GetSubjectPublicKeyBytes(cert->tbs().spki_tlv, &key)) {
        return false;
      }
      return VerifyHash(EVP_sha1(), id.key_hash, key);
    }
  }

  return false;
}

[[nodiscard]] bool VerifyAuthorizedResponderCert(
    const ParsedCertificate *responder_certificate,
    const ParsedCertificate *issuer_certificate) {

  if (!responder_certificate->signature_algorithm().has_value() ||
      !VerifySignedData(*responder_certificate->signature_algorithm(),
                        responder_certificate->tbs_certificate_tlv(),
                        responder_certificate->signature_value(),
                        issuer_certificate->tbs().spki_tlv,
                                  nullptr)) {
    return false;
  }

  if (!responder_certificate->has_extended_key_usage()) {
    return false;
  }

  for (const auto &key_purpose_oid :
       responder_certificate->extended_key_usage()) {
    if (key_purpose_oid == der::Input(kOCSPSigning)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool VerifyOCSPResponseSignatureGivenCert(
    const OCSPResponse &response, const ParsedCertificate *cert) {

  return VerifySignedData(response.signature_algorithm, response.data,
                          response.signature, cert->tbs().spki_tlv,
                                    nullptr);
}

[[nodiscard]] bool VerifyOCSPResponseSignature(
    const OCSPResponse &response, const OCSPResponseData &response_data,
    const ParsedCertificate *issuer_certificate) {

  if (CheckResponderIDMatchesCertificate(response_data.responder_id,
                                         issuer_certificate) &&
      VerifyOCSPResponseSignatureGivenCert(response, issuer_certificate)) {
    return true;
  }

  for (const auto &responder_cert_tlv : response.certs) {
    std::shared_ptr<const ParsedCertificate> cur_responder_certificate =
        OCSPParseCertificate(BytesAsStringView(responder_cert_tlv));

    if (!cur_responder_certificate) {
      continue;
    }

    if (!CheckResponderIDMatchesCertificate(response_data.responder_id,
                                            cur_responder_certificate.get())) {
      continue;
    }

    if (!VerifyAuthorizedResponderCert(cur_responder_certificate.get(),
                                       issuer_certificate)) {
      continue;
    }

    if (VerifyOCSPResponseSignatureGivenCert(response,
                                             cur_responder_certificate.get())) {
      return true;
    }
  }

  return false;
}

bool ParseOCSPResponseDataExtensions(
    der::Input response_extensions,
    OCSPVerifyResult::ResponseStatus *response_details) {
  std::map<der::Input, ParsedExtension> extensions;
  if (!ParseExtensions(response_extensions, &extensions)) {
    *response_details = OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR;
    return false;
  }

  for (const auto &ext : extensions) {

    if (ext.second.critical) {
      *response_details = OCSPVerifyResult::UNHANDLED_CRITICAL_EXTENSION;
      return false;
    }
  }

  return true;
}

bool ParseOCSPSingleResponseExtensions(
    der::Input single_extensions,
    OCSPVerifyResult::ResponseStatus *response_details) {
  std::map<der::Input, ParsedExtension> extensions;
  if (!ParseExtensions(single_extensions, &extensions)) {
    *response_details = OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR;
    return false;
  }

  const uint8_t ct_ocsp_ext_oid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                     0xD6, 0x79, 0x02, 0x04, 0x05};
  der::Input ct_ext_oid(ct_ocsp_ext_oid);

  for (const auto &ext : extensions) {

    if (ext.second.oid == ct_ext_oid) {
      continue;
    }

    if (ext.second.critical) {
      *response_details = OCSPVerifyResult::UNHANDLED_CRITICAL_EXTENSION;
      return false;
    }
  }

  return true;
}

OCSPRevocationStatus GetRevocationStatusForCert(
    const OCSPResponseData &response_data, const ParsedCertificate *cert,
    const ParsedCertificate *issuer_certificate,
    int64_t verify_time_epoch_seconds, std::optional<int64_t> max_age_seconds,
    OCSPVerifyResult::ResponseStatus *response_details) {
  OCSPRevocationStatus result = OCSPRevocationStatus::UNKNOWN;
  *response_details = OCSPVerifyResult::NO_MATCHING_RESPONSE;

  for (const auto &single_response_der : response_data.responses) {

    OCSPSingleResponse single_response;
    if (!ParseOCSPSingleResponse(single_response_der, &single_response)) {
      return OCSPRevocationStatus::UNKNOWN;
    }

    if (single_response.has_extensions &&
        !ParseOCSPSingleResponseExtensions(single_response.extensions,
                                           response_details)) {
      return OCSPRevocationStatus::UNKNOWN;
    }

    OCSPCertID cert_id;
    if (!ParseOCSPCertID(single_response.cert_id_tlv, &cert_id)) {
      return OCSPRevocationStatus::UNKNOWN;
    }
    if (!CheckCertIDMatchesCertificate(cert_id, cert, issuer_certificate)) {
      continue;
    }

    if (!CheckRevocationDateValid(single_response.this_update,
                                  single_response.has_next_update
                                      ? &single_response.next_update
                                      : nullptr,
                                  verify_time_epoch_seconds, max_age_seconds)) {
      if (*response_details != OCSPVerifyResult::PROVIDED) {
        *response_details = OCSPVerifyResult::INVALID_DATE;
      }
      continue;
    }

    if (*response_details != OCSPVerifyResult::PROVIDED ||
        result == OCSPRevocationStatus::GOOD ||
        single_response.cert_status.status == OCSPRevocationStatus::REVOKED) {
      result = single_response.cert_status.status;
    }
    *response_details = OCSPVerifyResult::PROVIDED;
  }

  return result;
}

OCSPRevocationStatus CheckOCSP(
    std::string_view raw_response, std::string_view certificate_der,
    const ParsedCertificate *certificate,
    std::string_view issuer_certificate_der,
    const ParsedCertificate *issuer_certificate,
    int64_t verify_time_epoch_seconds, std::optional<int64_t> max_age_seconds,
    OCSPVerifyResult::ResponseStatus *response_details) {
  *response_details = OCSPVerifyResult::NOT_CHECKED;

  if (raw_response.empty()) {
    *response_details = OCSPVerifyResult::MISSING;
    return OCSPRevocationStatus::UNKNOWN;
  }

  der::Input response_der(raw_response);
  OCSPResponse response;
  if (!ParseOCSPResponse(response_der, &response)) {
    *response_details = OCSPVerifyResult::PARSE_RESPONSE_ERROR;
    return OCSPRevocationStatus::UNKNOWN;
  }

  if (response.status != OCSPResponse::ResponseStatus::SUCCESSFUL) {
    *response_details = OCSPVerifyResult::ERROR_RESPONSE;
    return OCSPRevocationStatus::UNKNOWN;
  }

  OCSPResponseData response_data;
  if (!ParseOCSPResponseData(response.data, &response_data)) {
    *response_details = OCSPVerifyResult::PARSE_RESPONSE_DATA_ERROR;
    return OCSPRevocationStatus::UNKNOWN;
  }

  if (response_data.has_extensions &&
      !ParseOCSPResponseDataExtensions(response_data.extensions,
                                       response_details)) {
    return OCSPRevocationStatus::UNKNOWN;
  }

  std::shared_ptr<const ParsedCertificate> parsed_certificate;
  std::shared_ptr<const ParsedCertificate> parsed_issuer_certificate;
  if (!certificate) {
    parsed_certificate = OCSPParseCertificate(certificate_der);
    certificate = parsed_certificate.get();
  }
  if (!issuer_certificate) {
    parsed_issuer_certificate = OCSPParseCertificate(issuer_certificate_der);
    issuer_certificate = parsed_issuer_certificate.get();
  }

  if (!certificate || !issuer_certificate) {
    *response_details = OCSPVerifyResult::NOT_CHECKED;
    return OCSPRevocationStatus::UNKNOWN;
  }

  if (response_data.produced_at < certificate->tbs().validity_not_before ||
      response_data.produced_at > certificate->tbs().validity_not_after) {
    *response_details = OCSPVerifyResult::BAD_PRODUCED_AT;
    return OCSPRevocationStatus::UNKNOWN;
  }

  OCSPRevocationStatus status = GetRevocationStatusForCert(
      response_data, certificate, issuer_certificate, verify_time_epoch_seconds,
      max_age_seconds, response_details);

  if (!VerifyOCSPResponseSignature(response, response_data,
                                   issuer_certificate)) {
    return OCSPRevocationStatus::UNKNOWN;
  }

  return status;
}

}

OCSPRevocationStatus CheckOCSP(
    std::string_view raw_response, std::string_view certificate_der,
    std::string_view issuer_certificate_der, int64_t verify_time_epoch_seconds,
    std::optional<int64_t> max_age_seconds,
    OCSPVerifyResult::ResponseStatus *response_details) {
  return CheckOCSP(raw_response, certificate_der, nullptr,
                   issuer_certificate_der, nullptr, verify_time_epoch_seconds,
                   max_age_seconds, response_details);
}

OCSPRevocationStatus CheckOCSP(
    std::string_view raw_response, const ParsedCertificate *certificate,
    const ParsedCertificate *issuer_certificate,
    int64_t verify_time_epoch_seconds, std::optional<int64_t> max_age_seconds,
    OCSPVerifyResult::ResponseStatus *response_details) {
  return CheckOCSP(raw_response, std::string_view(), certificate,
                   std::string_view(), issuer_certificate,
                   verify_time_epoch_seconds, max_age_seconds,
                   response_details);
}

bool CreateOCSPRequest(const ParsedCertificate *cert,
                       const ParsedCertificate *issuer,
                       std::vector<uint8_t> *request_der) {
  request_der->clear();

  bssl::ScopedCBB cbb;

  const size_t kInitialBufferSize = 100;

  if (!CBB_init(cbb.get(), kInitialBufferSize)) {
    return false;
  }

  CBB ocsp_request;
  if (!CBB_add_asn1(cbb.get(), &ocsp_request, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  CBB tbs_request;
  if (!CBB_add_asn1(&ocsp_request, &tbs_request, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  CBB request_list;
  if (!CBB_add_asn1(&tbs_request, &request_list, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  CBB request;
  if (!CBB_add_asn1(&request_list, &request, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  CBB req_cert;
  if (!CBB_add_asn1(&request, &req_cert, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  const EVP_MD *md = EVP_sha1();
  if (!EVP_marshal_digest_algorithm(&req_cert, md)) {
    return false;
  }

  AppendHashAsOctetString(md, &req_cert, issuer->tbs().subject_tlv);

  der::Input key_tlv;
  if (!GetSubjectPublicKeyBytes(issuer->tbs().spki_tlv, &key_tlv)) {
    return false;
  }
  AppendHashAsOctetString(md, &req_cert, key_tlv);

  CBB serial_number;
  if (!CBB_add_asn1(&req_cert, &serial_number, CBS_ASN1_INTEGER)) {
    return false;
  }
  if (!CBB_add_bytes(&serial_number, cert->tbs().serial_number.data(),
                     cert->tbs().serial_number.size())) {
    return false;
  }

  uint8_t *result_bytes;
  size_t result_bytes_length;
  if (!CBB_finish(cbb.get(), &result_bytes, &result_bytes_length)) {
    return false;
  }
  bssl::UniquePtr<uint8_t> delete_tbs_cert_bytes(result_bytes);

  request_der->assign(result_bytes, result_bytes + result_bytes_length);
  return true;
}

std::optional<std::string> CreateOCSPGetURL(
    const ParsedCertificate *cert, const ParsedCertificate *issuer,
    std::string_view ocsp_responder_url) {
  std::vector<uint8_t> ocsp_request_der;
  if (!CreateOCSPRequest(cert, issuer, &ocsp_request_der)) {

    return std::nullopt;
  }

  size_t len;
  if (!EVP_EncodedLength(&len, ocsp_request_der.size())) {
    return std::nullopt;
  }
  std::vector<uint8_t> encoded(len);
  len = EVP_EncodeBlock(encoded.data(), ocsp_request_der.data(),
                        ocsp_request_der.size());

  std::string b64_encoded(encoded.begin(), encoded.begin() + len);

  b64_encoded = bssl::string_util::FindAndReplace(b64_encoded, "+", "%2B");
  b64_encoded = bssl::string_util::FindAndReplace(b64_encoded, "/", "%2F");
  b64_encoded = bssl::string_util::FindAndReplace(b64_encoded, "=", "%3D");

  return std::string(ocsp_responder_url) + "/" + b64_encoded;
}

BSSL_NAMESPACE_END
