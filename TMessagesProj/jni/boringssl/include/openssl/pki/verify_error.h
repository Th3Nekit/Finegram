// Copyright 2024 The BoringSSL Authors
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

#if !defined(OPENSSL_HEADER_BSSL_PKI_VERIFY_ERROR_H_) && defined(__cplusplus)
#define OPENSSL_HEADER_BSSL_PKI_VERIFY_ERROR_H_

#include <openssl/base.h>

#include <string>

BSSL_NAMESPACE_BEGIN

class OPENSSL_EXPORT VerifyError {
 public:
  VerifyError() = default;
  VerifyError(const VerifyError &other) = default;
  VerifyError &operator=(const VerifyError &other) = default;

  enum class StatusCode {

    PATH_VERIFIED,

    CERTIFICATE_INVALID_SIGNATURE,

    CERTIFICATE_UNSUPPORTED_KEY,

    CERTIFICATE_UNSUPPORTED_SIGNATURE_ALGORITHM,

    CERTIFICATE_REVOKED,

    CERTIFICATE_NO_REVOCATION_MECHANISM,

    CERTIFICATE_UNABLE_TO_CHECK_REVOCATION,

    CERTIFICATE_EXPIRED,

    CERTIFICATE_NOT_YET_VALID,

    CERTIFICATE_NO_MATCHING_EKU,

    CERTIFICATE_INVALID,

    PATH_NOT_FOUND,

    PATH_ITERATION_COUNT_EXCEEDED,

    PATH_DEADLINE_EXCEEDED,

    PATH_DEPTH_LIMIT_REACHED,

    PATH_MULTIPLE_ERRORS,

    VERIFICATION_FAILURE,
  };

  VerifyError(StatusCode code, ptrdiff_t offset, std::string diagnostic);

  StatusCode Code() const;

  ptrdiff_t Index() const;

  const std::string &DiagnosticString() const;

 private:
  ptrdiff_t offset_ = -1;
  StatusCode code_ = StatusCode::VERIFICATION_FAILURE;
  std::string diagnostic_;
};

BSSL_NAMESPACE_END

#endif
