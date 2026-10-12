/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSL_STREAM_ADAPTER_H_
#define RTC_BASE_OPENSSL_STREAM_ADAPTER_H_

#include <openssl/ossl_typ.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "rtc_base/buffer.h"
#ifdef OPENSSL_IS_BORINGSSL
#include "rtc_base/boringssl_identity.h"
#else
#include "rtc_base/openssl_identity.h"
#endif
#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"
#include "rtc_base/stream.h"
#include "rtc_base/system/rtc_export.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

namespace rtc {

class SSLCertChain;

class OpenSSLStreamAdapter final : public SSLStreamAdapter,
                                   public sigslot::has_slots<> {
 public:
  OpenSSLStreamAdapter(
      std::unique_ptr<StreamInterface> stream,
      absl::AnyInvocable<void(SSLHandshakeError)> handshake_error);
  ~OpenSSLStreamAdapter() override;

  void SetIdentity(std::unique_ptr<SSLIdentity> identity) override;
  SSLIdentity* GetIdentityForTesting() const override;

  void SetServerRole(SSLRole role = SSL_SERVER) override;
  bool SetPeerCertificateDigest(
      absl::string_view digest_alg,
      const unsigned char* digest_val,
      size_t digest_len,
      SSLPeerCertificateDigestError* error = nullptr) override;

  std::unique_ptr<SSLCertChain> GetPeerSSLCertChain() const override;

  int StartSSL() override;
  void SetMode(SSLMode mode) override;
  void SetMaxProtocolVersion(SSLProtocolVersion version) override;
  void SetInitialRetransmissionTimeout(int timeout_ms) override;

  StreamResult Read(rtc::ArrayView<uint8_t> data,
                    size_t& read,
                    int& error) override;
  StreamResult Write(rtc::ArrayView<const uint8_t> data,
                     size_t& written,
                     int& error) override;
  void Close() override;
  StreamState GetState() const override;

  static std::string SslCipherSuiteToName(int crypto_suite);

  bool GetSslCipherSuite(int* cipher) override;

  SSLProtocolVersion GetSslVersion() const override;
  bool GetSslVersionBytes(int* version) const override;

  bool ExportKeyingMaterial(absl::string_view label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override;

  uint16_t GetPeerSignatureAlgorithm() const override;

  bool SetDtlsSrtpCryptoSuites(const std::vector<int>& crypto_suites) override;
  bool GetDtlsSrtpCryptoSuite(int* crypto_suite) override;

  bool IsTlsConnected() override;

  static bool IsBoringSsl();

  static bool IsAcceptableCipher(int cipher, KeyType key_type);
  static bool IsAcceptableCipher(absl::string_view cipher, KeyType key_type);

  static void EnableTimeCallbackForTesting();

 private:
  enum SSLState {

    SSL_NONE,
    SSL_WAIT,
    SSL_CONNECTING,
    SSL_CONNECTED,
    SSL_ERROR,
    SSL_CLOSED
  };

  void OnEvent(StreamInterface* stream, int events, int err);

  void PostEvent(int events, int err);
  void SetTimeout(int delay_ms);

  int BeginSSL();

  int ContinueSSL();

  void Error(absl::string_view context, int err, uint8_t alert, bool signal);
  void Cleanup(uint8_t alert);

  void FlushInput(unsigned int left);

  SSL_CTX* SetupSSLContext();

  bool VerifyPeerCertificate();

#ifdef OPENSSL_IS_BORINGSSL

  static enum ssl_verify_result_t SSLVerifyCallback(SSL* ssl,
                                                    uint8_t* out_alert);
#else

  static int SSLVerifyCallback(X509_STORE_CTX* store, void* arg);
#endif

  bool WaitingToVerifyPeerCertificate() const {
    return GetClientAuthEnabled() && !peer_certificate_verified_;
  }

  bool HasPeerCertificateDigest() const {
    return !peer_certificate_digest_algorithm_.empty() &&
           !peer_certificate_digest_value_.empty();
  }

  const std::unique_ptr<StreamInterface> stream_;
  absl::AnyInvocable<void(SSLHandshakeError)> handshake_error_;

  rtc::Thread* const owner_;
  webrtc::ScopedTaskSafety task_safety_;
  webrtc::RepeatingTaskHandle timeout_task_;

  SSLState state_;
  SSLRole role_;
  int ssl_error_code_;

  bool ssl_read_needs_write_;
  bool ssl_write_needs_read_;

  SSL* ssl_;
  SSL_CTX* ssl_ctx_;

#ifdef OPENSSL_IS_BORINGSSL
  std::unique_ptr<BoringSSLIdentity> identity_;
#else
  std::unique_ptr<OpenSSLIdentity> identity_;
#endif

  std::unique_ptr<SSLCertChain> peer_cert_chain_;
  bool peer_certificate_verified_ = false;

  Buffer peer_certificate_digest_value_;
  std::string peer_certificate_digest_algorithm_;

  std::string srtp_ciphers_;

  SSLMode ssl_mode_;

  SSLProtocolVersion ssl_max_version_;

  int dtls_handshake_timeout_ms_ = 50;
};

}

#endif
