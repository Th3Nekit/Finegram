/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_FAKE_SSL_IDENTITY_H_
#define RTC_BASE_FAKE_SSL_IDENTITY_H_

#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"

namespace rtc {

class FakeSSLCertificate : public SSLCertificate {
 public:

  explicit FakeSSLCertificate(absl::string_view pem_string);

  FakeSSLCertificate(const FakeSSLCertificate&);
  ~FakeSSLCertificate() override;

  std::unique_ptr<SSLCertificate> Clone() const override;
  std::string ToPEMString() const override;
  void ToDER(Buffer* der_buffer) const override;
  int64_t CertificateExpirationTime() const override;
  bool GetSignatureDigestAlgorithm(std::string* algorithm) const override;
  bool ComputeDigest(absl::string_view algorithm,
                     unsigned char* digest,
                     size_t size,
                     size_t* length) const override;

  void SetCertificateExpirationTime(int64_t expiration_time);

  void set_digest_algorithm(absl::string_view algorithm);

 private:
  std::string pem_string_;
  std::string digest_algorithm_;

  int64_t expiration_time_;
};

class FakeSSLIdentity : public SSLIdentity {
 public:
  explicit FakeSSLIdentity(absl::string_view pem_string);

  explicit FakeSSLIdentity(const std::vector<std::string>& pem_strings);
  explicit FakeSSLIdentity(const FakeSSLCertificate& cert);

  explicit FakeSSLIdentity(const FakeSSLIdentity& o);

  ~FakeSSLIdentity() override;

  const SSLCertificate& certificate() const override;
  const SSLCertChain& cert_chain() const override;

  std::string PrivateKeyToPEMString() const override;

  std::string PublicKeyToPEMString() const override;

  virtual bool operator==(const SSLIdentity& other) const;

 private:
  std::unique_ptr<SSLIdentity> CloneInternal() const override;

  std::unique_ptr<SSLCertChain> cert_chain_;
};

}

#endif
