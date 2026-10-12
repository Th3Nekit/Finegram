/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SSL_STREAM_ADAPTER_H_
#define RTC_BASE_SSL_STREAM_ADAPTER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/stream.h"

namespace rtc {

constexpr int kTlsNullWithNullNull = 0;
constexpr int kSslCipherSuiteMaxValue = 0xFFFF;

constexpr int kSrtpInvalidCryptoSuite = 0;
constexpr int kSrtpAes128CmSha1_80 = 0x0001;
constexpr int kSrtpAes128CmSha1_32 = 0x0002;
constexpr int kSrtpAeadAes128Gcm = 0x0007;
constexpr int kSrtpAeadAes256Gcm = 0x0008;
constexpr int kSrtpCryptoSuiteMaxValue = 0xFFFF;

constexpr int kSslSignatureAlgorithmUnknown = 0;
constexpr int kSslSignatureAlgorithmMaxValue = 0xFFFF;

extern const char kCsAesCm128HmacSha1_80[];

extern const char kCsAesCm128HmacSha1_32[];

extern const char kCsAeadAes128Gcm[];

extern const char kCsAeadAes256Gcm[];

std::string SrtpCryptoSuiteToName(int crypto_suite);

int SrtpCryptoSuiteFromName(absl::string_view crypto_suite);

bool GetSrtpKeyAndSaltLengths(int crypto_suite,
                              int* key_length,
                              int* salt_length);

bool IsGcmCryptoSuite(int crypto_suite);

bool IsGcmCryptoSuiteName(absl::string_view crypto_suite);

enum SSLRole { SSL_CLIENT, SSL_SERVER };
enum SSLMode { SSL_MODE_TLS, SSL_MODE_DTLS };

enum SSLProtocolVersion {
  SSL_PROTOCOL_NOT_GIVEN = -1,
  SSL_PROTOCOL_TLS_10 = 0,
  SSL_PROTOCOL_TLS_11,
  SSL_PROTOCOL_TLS_12,
  SSL_PROTOCOL_DTLS_10 = SSL_PROTOCOL_TLS_11,
  SSL_PROTOCOL_DTLS_12 = SSL_PROTOCOL_TLS_12,
};
enum class SSLPeerCertificateDigestError {
  NONE,
  UNKNOWN_ALGORITHM,
  INVALID_LENGTH,
  VERIFICATION_FAILED,
};

enum { SSE_MSG_TRUNC = 0xff0001 };

enum class SSLHandshakeError { UNKNOWN, INCOMPATIBLE_CIPHERSUITE, MAX_VALUE };

class SSLStreamAdapter : public StreamInterface {
 public:

  static std::unique_ptr<SSLStreamAdapter> Create(
      std::unique_ptr<StreamInterface> stream,
      absl::AnyInvocable<void(SSLHandshakeError)> handshake_error = nullptr);

  SSLStreamAdapter() = default;
  ~SSLStreamAdapter() override = default;

  virtual void SetIdentity(std::unique_ptr<SSLIdentity> identity) = 0;
  virtual SSLIdentity* GetIdentityForTesting() const = 0;

  virtual void SetServerRole(SSLRole role = SSL_SERVER) = 0;

  virtual void SetMode(SSLMode mode) = 0;

  virtual void SetMaxProtocolVersion(SSLProtocolVersion version) = 0;

  virtual void SetInitialRetransmissionTimeout(int timeout_ms) = 0;

  virtual int StartSSL() = 0;

  virtual bool SetPeerCertificateDigest(
      absl::string_view digest_alg,
      const unsigned char* digest_val,
      size_t digest_len,
      SSLPeerCertificateDigestError* error = nullptr) = 0;

  virtual std::unique_ptr<SSLCertChain> GetPeerSSLCertChain() const = 0;

  virtual bool GetSslCipherSuite(int* cipher_suite);

  virtual SSLProtocolVersion GetSslVersion() const = 0;

  virtual bool GetSslVersionBytes(int* version) const = 0;

  virtual bool ExportKeyingMaterial(absl::string_view label,
                                    const uint8_t* context,
                                    size_t context_len,
                                    bool use_context,
                                    uint8_t* result,
                                    size_t result_len);

  virtual uint16_t GetPeerSignatureAlgorithm() const = 0;

  virtual bool SetDtlsSrtpCryptoSuites(const std::vector<int>& crypto_suites);
  virtual bool GetDtlsSrtpCryptoSuite(int* crypto_suite);

  virtual bool IsTlsConnected() = 0;

  static bool IsBoringSsl();

  static bool IsAcceptableCipher(int cipher, KeyType key_type);
  static bool IsAcceptableCipher(absl::string_view cipher, KeyType key_type);

  static std::string SslCipherSuiteToName(int cipher_suite);

  static void EnableTimeCallbackForTesting();

  void SetClientAuthEnabledForTesting(bool enabled) {
    client_auth_enabled_ = enabled;
  }

  bool GetClientAuthEnabled() const { return client_auth_enabled_; }

 private:

  bool client_auth_enabled_ = true;
};

}

#endif
