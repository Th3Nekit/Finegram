// Copyright 2023 The BoringSSL Authors
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

#if !defined(OPENSSL_HEADER_BSSL_PKI_CERTIFICATE_H_) && defined(__cplusplus)
#define OPENSSL_HEADER_BSSL_PKI_CERTIFICATE_H_

#include <memory>
#include <string>
#include <string_view>

#include <openssl/base.h>
#include <openssl/span.h>

BSSL_NAMESPACE_BEGIN

struct CertificateInternals;

class OPENSSL_EXPORT Certificate {
 public:
  Certificate(Certificate&& other);
  Certificate(const Certificate& other) = delete;
  ~Certificate();
  Certificate& operator=(const Certificate& other) = delete;

  static std::unique_ptr<Certificate> FromDER(
      bssl::Span<const uint8_t> der, std::string *out_diagnostic);

  static std::unique_ptr<Certificate> FromPEM(
      std::string_view pem, std::string *out_diagnostic);

  bool IsSelfIssued() const;

  struct Validity {
    int64_t not_before;
    int64_t not_after;
  };

  Validity GetValidity() const;

  bssl::Span<const uint8_t> GetSerialNumber() const;

 private:
  explicit Certificate(std::unique_ptr<CertificateInternals> internals);

  std::unique_ptr<CertificateInternals> internals_;
};

BSSL_NAMESPACE_END

#endif
