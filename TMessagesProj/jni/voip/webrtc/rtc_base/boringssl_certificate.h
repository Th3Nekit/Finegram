/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BORINGSSL_CERTIFICATE_H_
#define RTC_BASE_BORINGSSL_CERTIFICATE_H_

#include <openssl/ossl_typ.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"

namespace rtc {

class OpenSSLKeyPair;

class BoringSSLCertificate final : public SSLCertificate {
 public:
  explicit BoringSSLCertificate(bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer);

  static std::unique_ptr<BoringSSLCertificate> Generate(
      OpenSSLKeyPair* key_pair,
      const SSLIdentityParams& params);
  static std::unique_ptr<BoringSSLCertificate> FromPEMString(
      absl::string_view pem_string);

  ~BoringSSLCertificate() override;

  BoringSSLCertificate(const BoringSSLCertificate&) = delete;
  BoringSSLCertificate& operator=(const BoringSSLCertificate&) = delete;

  std::unique_ptr<SSLCertificate> Clone() const override;

  CRYPTO_BUFFER* cert_buffer() const { return cert_buffer_.get(); }

  std::string ToPEMString() const override;
  void ToDER(Buffer* der_buffer) const override;
  bool operator==(const BoringSSLCertificate& other) const;
  bool operator!=(const BoringSSLCertificate& other) const;

  bool ComputeDigest(absl::string_view algorithm,
                     unsigned char* digest,
                     size_t size,
                     size_t* length) const override;

  static bool ComputeDigest(const CRYPTO_BUFFER* cert_buffer,
                            absl::string_view algorithm,
                            unsigned char* digest,
                            size_t size,
                            size_t* length);

  bool GetSignatureDigestAlgorithm(std::string* algorithm) const override;

  int64_t CertificateExpirationTime() const override;

 private:

  bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer_;
};

}

#endif
