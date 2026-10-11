/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Generic interface for SSL Certificates, used in both the SSLAdapter
// for TLS TURN connections and the SSLStreamAdapter for DTLS Peer to Peer
// Connections for SRTP Key negotiation and SCTP encryption.

#ifndef RTC_BASE_SSL_CERTIFICATE_H_
#define RTC_BASE_SSL_CERTIFICATE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "rtc_base/buffer.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

struct RTC_EXPORT SSLCertificateStats {
  SSLCertificateStats(std::string&& fingerprint,
                      std::string&& fingerprint_algorithm,
                      std::string&& base64_certificate,
                      std::unique_ptr<SSLCertificateStats> issuer);
  ~SSLCertificateStats();
  std::string fingerprint;
  std::string fingerprint_algorithm;
  std::string base64_certificate;
  std::unique_ptr<SSLCertificateStats> issuer;

  std::unique_ptr<SSLCertificateStats> Copy() const;
};

class RTC_EXPORT SSLCertificate {
 public:

  static std::unique_ptr<SSLCertificate> FromPEMString(
      absl::string_view pem_string);
  virtual ~SSLCertificate() = default;

  virtual std::unique_ptr<SSLCertificate> Clone() const = 0;

  virtual std::string ToPEMString() const = 0;

  virtual void ToDER(Buffer* der_buffer) const = 0;

  virtual bool GetSignatureDigestAlgorithm(std::string* algorithm) const = 0;

  virtual bool ComputeDigest(absl::string_view algorithm,
                             unsigned char* digest,
                             size_t size,
                             size_t* length) const = 0;

  virtual int64_t CertificateExpirationTime() const = 0;

  std::unique_ptr<SSLCertificateStats> GetStats() const;
};

class RTC_EXPORT SSLCertChain final {
 public:
  explicit SSLCertChain(std::unique_ptr<SSLCertificate> single_cert);
  explicit SSLCertChain(std::vector<std::unique_ptr<SSLCertificate>> certs);

  SSLCertChain(SSLCertChain&&);
  SSLCertChain& operator=(SSLCertChain&&);

  ~SSLCertChain();

  SSLCertChain(const SSLCertChain&) = delete;
  SSLCertChain& operator=(const SSLCertChain&) = delete;

  size_t GetSize() const { return certs_.size(); }

  const SSLCertificate& Get(size_t pos) const { return *(certs_[pos]); }

  std::unique_ptr<SSLCertChain> Clone() const;

  std::unique_ptr<SSLCertificateStats> GetStats() const;

 private:
  std::vector<std::unique_ptr<SSLCertificate>> certs_;
};

class SSLCertificateVerifier {
 public:
  virtual ~SSLCertificateVerifier() = default;

  virtual bool Verify(const SSLCertificate& certificate) = 0;
};

}

#endif
