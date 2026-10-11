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

#if !defined(OPENSSL_HEADER_BSSL_PKI_OCSP_H_)  && defined(__cplusplus)
#define OPENSSL_HEADER_BSSL_PKI_OCSP_H_

#include <openssl/base.h>
#include <string_view>
#include <optional>

BSSL_NAMESPACE_BEGIN

enum class OCSPRevocationStatus {
  GOOD = 0,
  REVOKED = 1,
  UNKNOWN = 2,
  MAX_VALUE = UNKNOWN
};

struct OPENSSL_EXPORT OCSPVerifyResult {
  bool operator==(const OCSPVerifyResult &other) const {
    if (response_status != other.response_status) {
      return false;
    }

    if (response_status == PROVIDED) {

      return revocation_status == other.revocation_status;
    }
    return true;
  }

  enum ResponseStatus {

    NOT_CHECKED = 0,

    MISSING = 1,

    PROVIDED = 2,

    ERROR_RESPONSE = 3,

    BAD_PRODUCED_AT = 4,

    NO_MATCHING_RESPONSE = 5,

    INVALID_DATE = 6,

    PARSE_RESPONSE_ERROR = 7,

    PARSE_RESPONSE_DATA_ERROR = 8,

    UNHANDLED_CRITICAL_EXTENSION = 9,
    RESPONSE_STATUS_MAX = UNHANDLED_CRITICAL_EXTENSION
  };

  ResponseStatus response_status = NOT_CHECKED;

  OCSPRevocationStatus revocation_status = OCSPRevocationStatus::UNKNOWN;
};

[[nodiscard]] OPENSSL_EXPORT OCSPRevocationStatus CheckOCSP(
    std::string_view raw_response, std::string_view certificate_der,
    std::string_view issuer_certificate_der, int64_t verify_time_epoch_seconds,
    std::optional<int64_t> max_age_seconds,
    OCSPVerifyResult::ResponseStatus *response_details);

BSSL_NAMESPACE_END

#endif
