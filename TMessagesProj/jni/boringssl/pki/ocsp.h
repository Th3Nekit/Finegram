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

#ifndef BSSL_PKI_OCSP_H_
#define BSSL_PKI_OCSP_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <openssl/base.h>
#include <openssl/pki/ocsp.h>

#include "input.h"
#include "parse_values.h"
#include "parser.h"
#include "signature_algorithm.h"

BSSL_NAMESPACE_BEGIN

class ParsedCertificate;

struct OPENSSL_EXPORT OCSPCertID {
  OCSPCertID();
  ~OCSPCertID();

  DigestAlgorithm hash_algorithm;
  der::Input issuer_name_hash;
  der::Input issuer_key_hash;
  der::Input serial_number;
};

struct OCSPCertStatus {

  enum class RevocationReason {
    UNSPECIFIED = 0,
    KEY_COMPROMISE = 1,
    CA_COMPROMISE = 2,
    AFFILIATION_CHANGED = 3,
    SUPERSEDED = 4,
    CESSATION_OF_OPERATION = 5,
    CERTIFICATE_HOLD = 6,
    UNUSED = 7,
    REMOVE_FROM_CRL = 8,
    PRIVILEGE_WITHDRAWN = 9,
    AA_COMPROMISE = 10,

    LAST = AA_COMPROMISE,
  };

  OCSPRevocationStatus status;
  der::GeneralizedTime revocation_time;
  bool has_reason;
  RevocationReason revocation_reason;
};

struct OPENSSL_EXPORT OCSPSingleResponse {
  OCSPSingleResponse();
  ~OCSPSingleResponse();

  der::Input cert_id_tlv;
  OCSPCertStatus cert_status;
  der::GeneralizedTime this_update;
  bool has_next_update;
  der::GeneralizedTime next_update;
  bool has_extensions;
  der::Input extensions;
};

struct OPENSSL_EXPORT OCSPResponseData {
  enum class ResponderType { NAME, KEY_HASH };

  struct ResponderID {
    ResponderType type;
    der::Input name;
    der::Input key_hash;
  };

  OCSPResponseData();
  ~OCSPResponseData();

  uint8_t version;
  OCSPResponseData::ResponderID responder_id;
  der::GeneralizedTime produced_at;
  std::vector<der::Input> responses;
  bool has_extensions;
  der::Input extensions;
};

struct OPENSSL_EXPORT OCSPResponse {

  enum class ResponseStatus {
    SUCCESSFUL = 0,
    MALFORMED_REQUEST = 1,
    INTERNAL_ERROR = 2,
    TRY_LATER = 3,
    UNUSED = 4,
    SIG_REQUIRED = 5,
    UNAUTHORIZED = 6,

    LAST = UNAUTHORIZED,
  };

  OCSPResponse();
  ~OCSPResponse();

  ResponseStatus status;
  der::Input data;
  SignatureAlgorithm signature_algorithm;
  der::BitString signature;
  bool has_certs;
  std::vector<der::Input> certs;
};

inline constexpr uint8_t kBasicOCSPResponseOid[] = {
    0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01};

OPENSSL_EXPORT bool ParseOCSPCertID(der::Input raw_tlv, OCSPCertID *out);

OPENSSL_EXPORT bool ParseOCSPSingleResponse(der::Input raw_tlv,
                                            OCSPSingleResponse *out);

OPENSSL_EXPORT bool ParseOCSPResponseData(der::Input raw_tlv,
                                          OCSPResponseData *out);

OPENSSL_EXPORT bool ParseOCSPResponse(der::Input raw_tlv, OCSPResponse *out);

[[nodiscard]] OPENSSL_EXPORT OCSPRevocationStatus CheckOCSP(
    std::string_view raw_response, const ParsedCertificate *certificate,
    const ParsedCertificate *issuer_certificate,
    int64_t verify_time_epoch_seconds, std::optional<int64_t> max_age_seconds,
    OCSPVerifyResult::ResponseStatus *response_details);

OPENSSL_EXPORT bool CreateOCSPRequest(const ParsedCertificate *cert,
                                      const ParsedCertificate *issuer,
                                      std::vector<uint8_t> *request_der);

OPENSSL_EXPORT std::optional<std::string> CreateOCSPGetURL(
    const ParsedCertificate *cert, const ParsedCertificate *issuer,
    std::string_view ocsp_responder_url);

BSSL_NAMESPACE_END

#endif
