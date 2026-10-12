/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_OPENSSL_ADAPTER_H_
#define RTC_BASE_OPENSSL_ADAPTER_H_

#include <openssl/ossl_typ.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/buffer.h"
#ifdef OPENSSL_IS_BORINGSSL
#include "rtc_base/boringssl_identity.h"
#else
#include "rtc_base/openssl_identity.h"
#endif
#include "rtc_base/openssl_session_cache.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/ssl_stream_adapter.h"

namespace rtc {

class OpenSSLAdapter final : public SSLAdapter {
 public:
  static bool InitializeSSL();
  static bool CleanupSSL();

  explicit OpenSSLAdapter(Socket* socket,
                          OpenSSLSessionCache* ssl_session_cache = nullptr,
                          SSLCertificateVerifier* ssl_cert_verifier = nullptr);
  ~OpenSSLAdapter() override;

  void SetIgnoreBadCert(bool ignore) override;
  void SetAlpnProtocols(const std::vector<std::string>& protos) override;
  void SetEllipticCurves(const std::vector<std::string>& curves) override;
  void SetMode(SSLMode mode) override;
  void SetCertVerifier(SSLCertificateVerifier* ssl_cert_verifier) override;
  void SetIdentity(std::unique_ptr<SSLIdentity> identity) override;
  void SetRole(SSLRole role) override;
  int StartSSL(absl::string_view hostname) override;
  int Send(const void* pv, size_t cb) override;
  int SendTo(const void* pv, size_t cb, const SocketAddress& addr) override;
  int Recv(void* pv, size_t cb, int64_t* timestamp) override;
  int RecvFrom(void* pv,
               size_t cb,
               SocketAddress* paddr,
               int64_t* timestamp) override;
  int Close() override;

  ConnState GetState() const override;
  bool IsResumedSession() override;

  static SSL_CTX* CreateContext(SSLMode mode, bool enable_cache);

 protected:
  void OnConnectEvent(Socket* socket) override;
  void OnReadEvent(Socket* socket) override;
  void OnWriteEvent(Socket* socket) override;
  void OnCloseEvent(Socket* socket, int err) override;

 private:
  class EarlyExitCatcher {
   public:
    EarlyExitCatcher(OpenSSLAdapter& adapter_ptr);
    void disable();
    ~EarlyExitCatcher();

   private:
    bool disabled_ = false;
    OpenSSLAdapter& adapter_ptr_;
  };
  enum SSLState {
    SSL_NONE,
    SSL_WAIT,
    SSL_CONNECTING,
    SSL_CONNECTED,
    SSL_ERROR
  };

  int BeginSSL();
  int ContinueSSL();
  void Error(absl::string_view context, int err, bool signal = true);
  void Cleanup();
  void OnTimeout();

  int DoSslWrite(const void* pv, size_t cb, int* error);
  bool SSLPostConnectionCheck(SSL* ssl, absl::string_view host);

  static void SSLInfoCallback(const SSL* ssl, int where, int ret);

#if defined(OPENSSL_IS_BORINGSSL) && \
    defined(WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS)
  static enum ssl_verify_result_t SSLVerifyCallback(SSL* ssl,
                                                    uint8_t* out_alert);
  enum ssl_verify_result_t SSLVerifyInternal(SSL* ssl, uint8_t* out_alert);
#else
  static int SSLVerifyCallback(int ok, X509_STORE_CTX* store);

  int SSLVerifyInternal(int status_on_error, SSL* ssl, X509_STORE_CTX* store);
#endif
  friend class OpenSSLStreamAdapter;

  static int NewSSLSessionCallback(SSL* ssl, SSL_SESSION* session);

  OpenSSLSessionCache* ssl_session_cache_ = nullptr;

  SSLCertificateVerifier* ssl_cert_verifier_ = nullptr;

  SSLState state_;

#ifdef OPENSSL_IS_BORINGSSL
  std::unique_ptr<BoringSSLIdentity> identity_;
#else
  std::unique_ptr<OpenSSLIdentity> identity_;
#endif

  SSLRole role_;
  bool ssl_read_needs_write_;
  bool ssl_write_needs_read_;

  Buffer pending_data_;
  SSL* ssl_;

  SSL_CTX* ssl_ctx_;

  std::string ssl_host_name_;

  SSLMode ssl_mode_;

  bool ignore_bad_cert_;

  std::vector<std::string> alpn_protocols_;

  std::vector<std::string> elliptic_curves_;

  bool custom_cert_verifier_status_;

  webrtc::ScopedTaskSafety timer_;
};

class OpenSSLAdapterFactory : public SSLAdapterFactory {
 public:
  OpenSSLAdapterFactory();
  ~OpenSSLAdapterFactory() override;

  void SetMode(SSLMode mode) override;

  void SetCertVerifier(SSLCertificateVerifier* ssl_cert_verifier) override;

  void SetIdentity(std::unique_ptr<SSLIdentity> identity) override;

  void SetRole(SSLRole role) override;

  void SetIgnoreBadCert(bool ignore) override;

  OpenSSLAdapter* CreateAdapter(Socket* socket) override;

 private:

  SSLMode ssl_mode_ = SSL_MODE_TLS;
  SSLRole ssl_role_ = SSL_CLIENT;
  bool ignore_bad_cert_ = false;

  std::unique_ptr<SSLIdentity> identity_;

  std::unique_ptr<OpenSSLSessionCache> ssl_session_cache_;

  SSLCertificateVerifier* ssl_cert_verifier_ = nullptr;

  friend class OpenSSLAdapter;
};

std::string TransformAlpnProtocols(const std::vector<std::string>& protos);

}

#endif
