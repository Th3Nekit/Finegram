// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
// Copyright 2005 Nokia. All rights reserved.
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

#ifndef OPENSSL_HEADER_SSL_H
#define OPENSSL_HEADER_SSL_H

#include <openssl/base.h>

#include <openssl/bio.h>
#include <openssl/buf.h>
#include <openssl/pem.h>
#include <openssl/span.h>
#include <openssl/ssl3.h>
#include <openssl/thread.h>
#include <openssl/tls1.h>
#include <openssl/x509.h>

#if !defined(OPENSSL_WINDOWS)
#include <sys/time.h>
#endif

struct timeval;

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT const SSL_METHOD *TLS_method(void);

OPENSSL_EXPORT const SSL_METHOD *DTLS_method(void);

OPENSSL_EXPORT const SSL_METHOD *TLS_with_buffers_method(void);

OPENSSL_EXPORT const SSL_METHOD *DTLS_with_buffers_method(void);

OPENSSL_EXPORT SSL_CTX *SSL_CTX_new(const SSL_METHOD *method);

OPENSSL_EXPORT int SSL_CTX_up_ref(SSL_CTX *ctx);

OPENSSL_EXPORT void SSL_CTX_free(SSL_CTX *ctx);

OPENSSL_EXPORT SSL *SSL_new(SSL_CTX *ctx);

OPENSSL_EXPORT void SSL_free(SSL *ssl);

OPENSSL_EXPORT SSL_CTX *SSL_get_SSL_CTX(const SSL *ssl);

OPENSSL_EXPORT void SSL_set_connect_state(SSL *ssl);

OPENSSL_EXPORT void SSL_set_accept_state(SSL *ssl);

OPENSSL_EXPORT int SSL_is_server(const SSL *ssl);

OPENSSL_EXPORT int SSL_is_dtls(const SSL *ssl);

OPENSSL_EXPORT int SSL_is_quic(const SSL *ssl);

OPENSSL_EXPORT void SSL_set_bio(SSL *ssl, BIO *rbio, BIO *wbio);

OPENSSL_EXPORT void SSL_set0_rbio(SSL *ssl, BIO *rbio);

OPENSSL_EXPORT void SSL_set0_wbio(SSL *ssl, BIO *wbio);

OPENSSL_EXPORT BIO *SSL_get_rbio(const SSL *ssl);

OPENSSL_EXPORT BIO *SSL_get_wbio(const SSL *ssl);

OPENSSL_EXPORT int SSL_get_fd(const SSL *ssl);

OPENSSL_EXPORT int SSL_get_rfd(const SSL *ssl);

OPENSSL_EXPORT int SSL_get_wfd(const SSL *ssl);

#if !defined(OPENSSL_NO_SOCK)

OPENSSL_EXPORT int SSL_set_fd(SSL *ssl, int fd);

OPENSSL_EXPORT int SSL_set_rfd(SSL *ssl, int fd);

OPENSSL_EXPORT int SSL_set_wfd(SSL *ssl, int fd);
#endif

OPENSSL_EXPORT int SSL_do_handshake(SSL *ssl);

OPENSSL_EXPORT int SSL_connect(SSL *ssl);

OPENSSL_EXPORT int SSL_accept(SSL *ssl);

OPENSSL_EXPORT int SSL_read(SSL *ssl, void *buf, int num);

OPENSSL_EXPORT int SSL_peek(SSL *ssl, void *buf, int num);

OPENSSL_EXPORT int SSL_pending(const SSL *ssl);

OPENSSL_EXPORT int SSL_has_pending(const SSL *ssl);

OPENSSL_EXPORT int SSL_write(SSL *ssl, const void *buf, int num);

#define SSL_KEY_UPDATE_REQUESTED 1

#define SSL_KEY_UPDATE_NOT_REQUESTED 0

OPENSSL_EXPORT int SSL_key_update(SSL *ssl, int request_type);

OPENSSL_EXPORT int SSL_shutdown(SSL *ssl);

OPENSSL_EXPORT void SSL_CTX_set_quiet_shutdown(SSL_CTX *ctx, int mode);

OPENSSL_EXPORT int SSL_CTX_get_quiet_shutdown(const SSL_CTX *ctx);

OPENSSL_EXPORT void SSL_set_quiet_shutdown(SSL *ssl, int mode);

OPENSSL_EXPORT int SSL_get_quiet_shutdown(const SSL *ssl);

OPENSSL_EXPORT int SSL_get_error(const SSL *ssl, int ret_code);

#define SSL_ERROR_NONE 0

#define SSL_ERROR_SSL 1

#define SSL_ERROR_WANT_READ 2

#define SSL_ERROR_WANT_WRITE 3

#define SSL_ERROR_WANT_X509_LOOKUP 4

#define SSL_ERROR_SYSCALL 5

#define SSL_ERROR_ZERO_RETURN 6

#define SSL_ERROR_WANT_CONNECT 7

#define SSL_ERROR_WANT_ACCEPT 8

#define SSL_ERROR_WANT_CHANNEL_ID_LOOKUP 9

#define SSL_ERROR_PENDING_SESSION 11

#define SSL_ERROR_PENDING_CERTIFICATE 12

#define SSL_ERROR_WANT_PRIVATE_KEY_OPERATION 13

#define SSL_ERROR_PENDING_TICKET 14

#define SSL_ERROR_EARLY_DATA_REJECTED 15

#define SSL_ERROR_WANT_CERTIFICATE_VERIFY 16

#define SSL_ERROR_HANDOFF 17
#define SSL_ERROR_HANDBACK 18

#define SSL_ERROR_WANT_RENEGOTIATE 19

#define SSL_ERROR_HANDSHAKE_HINTS_READY 20

OPENSSL_EXPORT const char *SSL_error_description(int err);

OPENSSL_EXPORT int SSL_set_mtu(SSL *ssl, unsigned mtu);

OPENSSL_EXPORT void DTLSv1_set_initial_timeout_duration(SSL *ssl,
                                                        uint32_t duration_ms);

OPENSSL_EXPORT int DTLSv1_get_timeout(const SSL *ssl, struct timeval *out);

OPENSSL_EXPORT int DTLSv1_handle_timeout(SSL *ssl);

#define DTLS1_VERSION_MAJOR 0xfe
#define SSL3_VERSION_MAJOR 0x03

#define SSL3_VERSION 0x0300
#define TLS1_VERSION 0x0301
#define TLS1_1_VERSION 0x0302
#define TLS1_2_VERSION 0x0303
#define TLS1_3_VERSION 0x0304

#define DTLS1_VERSION 0xfeff
#define DTLS1_2_VERSION 0xfefd
#define DTLS1_3_VERSION 0xfefc

OPENSSL_EXPORT int SSL_CTX_set_min_proto_version(SSL_CTX *ctx,
                                                 uint16_t version);

OPENSSL_EXPORT int SSL_CTX_set_max_proto_version(SSL_CTX *ctx,
                                                 uint16_t version);

OPENSSL_EXPORT uint16_t SSL_CTX_get_min_proto_version(const SSL_CTX *ctx);

OPENSSL_EXPORT uint16_t SSL_CTX_get_max_proto_version(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_set_min_proto_version(SSL *ssl, uint16_t version);

OPENSSL_EXPORT int SSL_set_max_proto_version(SSL *ssl, uint16_t version);

OPENSSL_EXPORT uint16_t SSL_get_min_proto_version(const SSL *ssl);

OPENSSL_EXPORT uint16_t SSL_get_max_proto_version(const SSL *ssl);

OPENSSL_EXPORT int SSL_version(const SSL *ssl);

#define SSL_OP_NO_QUERY_MTU 0x00001000L

#define SSL_OP_NO_TICKET 0x00004000L

#define SSL_OP_CIPHER_SERVER_PREFERENCE 0x00400000L

#define SSL_OP_NO_TLSv1 0x04000000L
#define SSL_OP_NO_TLSv1_2 0x08000000L
#define SSL_OP_NO_TLSv1_1 0x10000000L
#define SSL_OP_NO_TLSv1_3 0x20000000L
#define SSL_OP_NO_DTLSv1 SSL_OP_NO_TLSv1
#define SSL_OP_NO_DTLSv1_2 SSL_OP_NO_TLSv1_2

OPENSSL_EXPORT uint32_t SSL_CTX_set_options(SSL_CTX *ctx, uint32_t options);

OPENSSL_EXPORT uint32_t SSL_CTX_clear_options(SSL_CTX *ctx, uint32_t options);

OPENSSL_EXPORT uint32_t SSL_CTX_get_options(const SSL_CTX *ctx);

OPENSSL_EXPORT uint32_t SSL_set_options(SSL *ssl, uint32_t options);

OPENSSL_EXPORT uint32_t SSL_clear_options(SSL *ssl, uint32_t options);

OPENSSL_EXPORT uint32_t SSL_get_options(const SSL *ssl);

#define SSL_MODE_ENABLE_PARTIAL_WRITE 0x00000001L

#define SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER 0x00000002L

#define SSL_MODE_NO_AUTO_CHAIN 0x00000008L

#define SSL_MODE_ENABLE_FALSE_START 0x00000080L

#define SSL_MODE_CBC_RECORD_SPLITTING 0x00000100L

#define SSL_MODE_NO_SESSION_CREATION 0x00000200L

#define SSL_MODE_SEND_FALLBACK_SCSV 0x00000400L

OPENSSL_EXPORT uint32_t SSL_CTX_set_mode(SSL_CTX *ctx, uint32_t mode);

OPENSSL_EXPORT uint32_t SSL_CTX_clear_mode(SSL_CTX *ctx, uint32_t mode);

OPENSSL_EXPORT uint32_t SSL_CTX_get_mode(const SSL_CTX *ctx);

OPENSSL_EXPORT uint32_t SSL_set_mode(SSL *ssl, uint32_t mode);

OPENSSL_EXPORT uint32_t SSL_clear_mode(SSL *ssl, uint32_t mode);

OPENSSL_EXPORT uint32_t SSL_get_mode(const SSL *ssl);

OPENSSL_EXPORT void SSL_CTX_set0_buffer_pool(SSL_CTX *ctx,
                                             CRYPTO_BUFFER_POOL *pool);

OPENSSL_EXPORT SSL_CREDENTIAL *SSL_CREDENTIAL_new_x509(void);

OPENSSL_EXPORT void SSL_CREDENTIAL_up_ref(SSL_CREDENTIAL *cred);

OPENSSL_EXPORT void SSL_CREDENTIAL_free(SSL_CREDENTIAL *cred);

OPENSSL_EXPORT int SSL_CREDENTIAL_set1_private_key(SSL_CREDENTIAL *cred,
                                                   EVP_PKEY *key);

OPENSSL_EXPORT int SSL_CREDENTIAL_set1_signing_algorithm_prefs(
    SSL_CREDENTIAL *cred, const uint16_t *prefs, size_t num_prefs);

OPENSSL_EXPORT int SSL_CREDENTIAL_set1_cert_chain(SSL_CREDENTIAL *cred,
                                                  CRYPTO_BUFFER *const *certs,
                                                  size_t num_certs);

OPENSSL_EXPORT int SSL_CREDENTIAL_set1_ocsp_response(SSL_CREDENTIAL *cred,
                                                     CRYPTO_BUFFER *ocsp);

OPENSSL_EXPORT int SSL_CREDENTIAL_set1_certificate_properties(
    SSL_CREDENTIAL *cred, CRYPTO_BUFFER *cert_property_list);

OPENSSL_EXPORT int SSL_CREDENTIAL_set1_signed_cert_timestamp_list(
    SSL_CREDENTIAL *cred, CRYPTO_BUFFER *sct_list);

OPENSSL_EXPORT void SSL_CREDENTIAL_set_must_match_issuer(SSL_CREDENTIAL *cred,
                                                         int match);

OPENSSL_EXPORT int SSL_CTX_add1_credential(SSL_CTX *ctx, SSL_CREDENTIAL *cred);

OPENSSL_EXPORT int SSL_add1_credential(SSL *ssl, SSL_CREDENTIAL *cred);

OPENSSL_EXPORT void SSL_certs_clear(SSL *ssl);

OPENSSL_EXPORT const SSL_CREDENTIAL *SSL_get0_selected_credential(
    const SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_use_certificate(SSL_CTX *ctx, X509 *x509);

OPENSSL_EXPORT int SSL_use_certificate(SSL *ssl, X509 *x509);

OPENSSL_EXPORT int SSL_CTX_use_PrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey);

OPENSSL_EXPORT int SSL_use_PrivateKey(SSL *ssl, EVP_PKEY *pkey);

OPENSSL_EXPORT int SSL_CTX_set0_chain(SSL_CTX *ctx, STACK_OF(X509) *chain);

OPENSSL_EXPORT int SSL_CTX_set1_chain(SSL_CTX *ctx, STACK_OF(X509) *chain);

OPENSSL_EXPORT int SSL_set0_chain(SSL *ssl, STACK_OF(X509) *chain);

OPENSSL_EXPORT int SSL_set1_chain(SSL *ssl, STACK_OF(X509) *chain);

OPENSSL_EXPORT int SSL_CTX_add0_chain_cert(SSL_CTX *ctx, X509 *x509);

OPENSSL_EXPORT int SSL_CTX_add1_chain_cert(SSL_CTX *ctx, X509 *x509);

OPENSSL_EXPORT int SSL_add0_chain_cert(SSL *ssl, X509 *x509);

OPENSSL_EXPORT int SSL_CTX_add_extra_chain_cert(SSL_CTX *ctx, X509 *x509);

OPENSSL_EXPORT int SSL_add1_chain_cert(SSL *ssl, X509 *x509);

OPENSSL_EXPORT int SSL_CTX_clear_chain_certs(SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_clear_extra_chain_certs(SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_clear_chain_certs(SSL *ssl);

OPENSSL_EXPORT void SSL_CTX_set_cert_cb(SSL_CTX *ctx,
                                        int (*cb)(SSL *ssl, void *arg),
                                        void *arg);

OPENSSL_EXPORT void SSL_set_cert_cb(SSL *ssl, int (*cb)(SSL *ssl, void *arg),
                                    void *arg);

OPENSSL_EXPORT size_t SSL_get0_certificate_types(const SSL *ssl,
                                                 const uint8_t **out_types);

OPENSSL_EXPORT size_t
SSL_get0_peer_verify_algorithms(const SSL *ssl, const uint16_t **out_sigalgs);

OPENSSL_EXPORT size_t SSL_get0_peer_delegation_algorithms(
    const SSL *ssl, const uint16_t **out_sigalgs);

OPENSSL_EXPORT X509 *SSL_CTX_get0_certificate(const SSL_CTX *ctx);

OPENSSL_EXPORT X509 *SSL_get_certificate(const SSL *ssl);

OPENSSL_EXPORT EVP_PKEY *SSL_CTX_get0_privatekey(const SSL_CTX *ctx);

OPENSSL_EXPORT EVP_PKEY *SSL_get_privatekey(const SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_get0_chain_certs(const SSL_CTX *ctx,
                                            STACK_OF(X509) **out_chain);

OPENSSL_EXPORT int SSL_CTX_get_extra_chain_certs(const SSL_CTX *ctx,
                                                 STACK_OF(X509) **out_chain);

OPENSSL_EXPORT int SSL_get0_chain_certs(const SSL *ssl,
                                        STACK_OF(X509) **out_chain);

OPENSSL_EXPORT int SSL_CTX_set_signed_cert_timestamp_list(SSL_CTX *ctx,
                                                          const uint8_t *list,
                                                          size_t list_len);

OPENSSL_EXPORT int SSL_set_signed_cert_timestamp_list(SSL *ctx,
                                                      const uint8_t *list,
                                                      size_t list_len);

OPENSSL_EXPORT int SSL_CTX_set_ocsp_response(SSL_CTX *ctx,
                                             const uint8_t *response,
                                             size_t response_len);

OPENSSL_EXPORT int SSL_set_ocsp_response(SSL *ssl, const uint8_t *response,
                                         size_t response_len);

#define SSL_SIGN_RSA_PKCS1_SHA1 0x0201
#define SSL_SIGN_RSA_PKCS1_SHA256 0x0401
#define SSL_SIGN_RSA_PKCS1_SHA384 0x0501
#define SSL_SIGN_RSA_PKCS1_SHA512 0x0601
#define SSL_SIGN_ECDSA_SHA1 0x0203
#define SSL_SIGN_ECDSA_SECP256R1_SHA256 0x0403
#define SSL_SIGN_ECDSA_SECP384R1_SHA384 0x0503
#define SSL_SIGN_ECDSA_SECP521R1_SHA512 0x0603
#define SSL_SIGN_RSA_PSS_RSAE_SHA256 0x0804
#define SSL_SIGN_RSA_PSS_RSAE_SHA384 0x0805
#define SSL_SIGN_RSA_PSS_RSAE_SHA512 0x0806
#define SSL_SIGN_ED25519 0x0807

#define SSL_SIGN_RSA_PKCS1_SHA256_LEGACY 0x0420

#define SSL_SIGN_RSA_PKCS1_MD5_SHA1 0xff01

OPENSSL_EXPORT const char *SSL_get_signature_algorithm_name(uint16_t sigalg,
                                                            int include_curve);

OPENSSL_EXPORT size_t SSL_get_all_signature_algorithm_names(const char **out,
                                                            size_t max_out);

OPENSSL_EXPORT int SSL_get_signature_algorithm_key_type(uint16_t sigalg);

OPENSSL_EXPORT const EVP_MD *SSL_get_signature_algorithm_digest(
    uint16_t sigalg);

OPENSSL_EXPORT int SSL_is_signature_algorithm_rsa_pss(uint16_t sigalg);

OPENSSL_EXPORT int SSL_CTX_set_signing_algorithm_prefs(SSL_CTX *ctx,
                                                       const uint16_t *prefs,
                                                       size_t num_prefs);

OPENSSL_EXPORT int SSL_set_signing_algorithm_prefs(SSL *ssl,
                                                   const uint16_t *prefs,
                                                   size_t num_prefs);

OPENSSL_EXPORT int SSL_CTX_set_chain_and_key(
    SSL_CTX *ctx, CRYPTO_BUFFER *const *certs, size_t num_certs,
    EVP_PKEY *privkey, const SSL_PRIVATE_KEY_METHOD *privkey_method);

OPENSSL_EXPORT int SSL_set_chain_and_key(
    SSL *ssl, CRYPTO_BUFFER *const *certs, size_t num_certs, EVP_PKEY *privkey,
    const SSL_PRIVATE_KEY_METHOD *privkey_method);

OPENSSL_EXPORT const STACK_OF(CRYPTO_BUFFER) *SSL_CTX_get0_chain(
    const SSL_CTX *ctx);

OPENSSL_EXPORT const STACK_OF(CRYPTO_BUFFER) *SSL_get0_chain(const SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_use_RSAPrivateKey(SSL_CTX *ctx, RSA *rsa);

OPENSSL_EXPORT int SSL_use_RSAPrivateKey(SSL *ssl, RSA *rsa);

OPENSSL_EXPORT int SSL_CTX_use_certificate_ASN1(SSL_CTX *ctx, size_t der_len,
                                                const uint8_t *der);
OPENSSL_EXPORT int SSL_use_certificate_ASN1(SSL *ssl, const uint8_t *der,
                                            size_t der_len);

OPENSSL_EXPORT int SSL_CTX_use_PrivateKey_ASN1(int pk, SSL_CTX *ctx,
                                               const uint8_t *der,
                                               size_t der_len);
OPENSSL_EXPORT int SSL_use_PrivateKey_ASN1(int type, SSL *ssl,
                                           const uint8_t *der, size_t der_len);

OPENSSL_EXPORT int SSL_CTX_use_RSAPrivateKey_ASN1(SSL_CTX *ctx,
                                                  const uint8_t *der,
                                                  size_t der_len);
OPENSSL_EXPORT int SSL_use_RSAPrivateKey_ASN1(SSL *ssl, const uint8_t *der,
                                              size_t der_len);

#define SSL_FILETYPE_PEM 1
#define SSL_FILETYPE_ASN1 2

OPENSSL_EXPORT int SSL_CTX_use_RSAPrivateKey_file(SSL_CTX *ctx,
                                                  const char *file, int type);
OPENSSL_EXPORT int SSL_use_RSAPrivateKey_file(SSL *ssl, const char *file,
                                              int type);

OPENSSL_EXPORT int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file,
                                                int type);
OPENSSL_EXPORT int SSL_use_certificate_file(SSL *ssl, const char *file,
                                            int type);

OPENSSL_EXPORT int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file,
                                               int type);
OPENSSL_EXPORT int SSL_use_PrivateKey_file(SSL *ssl, const char *file,
                                           int type);

OPENSSL_EXPORT int SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx,
                                                      const char *file);

OPENSSL_EXPORT void SSL_CTX_set_default_passwd_cb(SSL_CTX *ctx,
                                                  pem_password_cb *cb);

OPENSSL_EXPORT pem_password_cb *SSL_CTX_get_default_passwd_cb(
    const SSL_CTX *ctx);

OPENSSL_EXPORT void SSL_CTX_set_default_passwd_cb_userdata(SSL_CTX *ctx,
                                                           void *data);

OPENSSL_EXPORT void *SSL_CTX_get_default_passwd_cb_userdata(const SSL_CTX *ctx);

enum ssl_private_key_result_t BORINGSSL_ENUM_INT {
  ssl_private_key_success,
  ssl_private_key_retry,
  ssl_private_key_failure,
};

struct ssl_private_key_method_st {

  enum ssl_private_key_result_t (*sign)(SSL *ssl, uint8_t *out, size_t *out_len,
                                        size_t max_out,
                                        uint16_t signature_algorithm,
                                        const uint8_t *in, size_t in_len);

  enum ssl_private_key_result_t (*decrypt)(SSL *ssl, uint8_t *out,
                                           size_t *out_len, size_t max_out,
                                           const uint8_t *in, size_t in_len);

  enum ssl_private_key_result_t (*complete)(SSL *ssl, uint8_t *out,
                                            size_t *out_len, size_t max_out);
};

OPENSSL_EXPORT void SSL_set_private_key_method(
    SSL *ssl, const SSL_PRIVATE_KEY_METHOD *key_method);

OPENSSL_EXPORT void SSL_CTX_set_private_key_method(
    SSL_CTX *ctx, const SSL_PRIVATE_KEY_METHOD *key_method);

OPENSSL_EXPORT int SSL_CREDENTIAL_set_private_key_method(
    SSL_CREDENTIAL *cred, const SSL_PRIVATE_KEY_METHOD *key_method);

OPENSSL_EXPORT int SSL_can_release_private_key(const SSL *ssl);

DEFINE_CONST_STACK_OF(SSL_CIPHER)

OPENSSL_EXPORT const SSL_CIPHER *SSL_get_cipher_by_value(uint16_t value);

OPENSSL_EXPORT uint32_t SSL_CIPHER_get_id(const SSL_CIPHER *cipher);

OPENSSL_EXPORT uint16_t SSL_CIPHER_get_protocol_id(const SSL_CIPHER *cipher);

OPENSSL_EXPORT int SSL_CIPHER_is_aead(const SSL_CIPHER *cipher);

OPENSSL_EXPORT int SSL_CIPHER_is_block_cipher(const SSL_CIPHER *cipher);

OPENSSL_EXPORT int SSL_CIPHER_get_cipher_nid(const SSL_CIPHER *cipher);

OPENSSL_EXPORT int SSL_CIPHER_get_digest_nid(const SSL_CIPHER *cipher);

OPENSSL_EXPORT int SSL_CIPHER_get_kx_nid(const SSL_CIPHER *cipher);

OPENSSL_EXPORT int SSL_CIPHER_get_auth_nid(const SSL_CIPHER *cipher);

OPENSSL_EXPORT const EVP_MD *SSL_CIPHER_get_handshake_digest(
    const SSL_CIPHER *cipher);

OPENSSL_EXPORT int SSL_CIPHER_get_prf_nid(const SSL_CIPHER *cipher);

OPENSSL_EXPORT uint16_t SSL_CIPHER_get_min_version(const SSL_CIPHER *cipher);

OPENSSL_EXPORT uint16_t SSL_CIPHER_get_max_version(const SSL_CIPHER *cipher);

OPENSSL_EXPORT const char *SSL_CIPHER_standard_name(const SSL_CIPHER *cipher);

OPENSSL_EXPORT const char *SSL_CIPHER_get_name(const SSL_CIPHER *cipher);

OPENSSL_EXPORT const char *SSL_CIPHER_get_kx_name(const SSL_CIPHER *cipher);

OPENSSL_EXPORT int SSL_CIPHER_get_bits(const SSL_CIPHER *cipher,
                                       int *out_alg_bits);

OPENSSL_EXPORT size_t SSL_get_all_cipher_names(const char **out,
                                               size_t max_out);

OPENSSL_EXPORT size_t SSL_get_all_standard_cipher_names(const char **out,
                                                        size_t max_out);

#define SSL_DEFAULT_CIPHER_LIST "ALL"

OPENSSL_EXPORT int SSL_CTX_set_strict_cipher_list(SSL_CTX *ctx,
                                                  const char *str);

OPENSSL_EXPORT int SSL_CTX_set_cipher_list(SSL_CTX *ctx, const char *str);

OPENSSL_EXPORT int SSL_set_strict_cipher_list(SSL *ssl, const char *str);

OPENSSL_EXPORT int SSL_set_cipher_list(SSL *ssl, const char *str);

OPENSSL_EXPORT STACK_OF(SSL_CIPHER) *SSL_CTX_get_ciphers(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_cipher_in_group(const SSL_CTX *ctx, size_t i);

OPENSSL_EXPORT STACK_OF(SSL_CIPHER) *SSL_get_ciphers(const SSL *ssl);

OPENSSL_EXPORT int SSL_is_init_finished(const SSL *ssl);

OPENSSL_EXPORT int SSL_in_init(const SSL *ssl);

OPENSSL_EXPORT int SSL_in_false_start(const SSL *ssl);

OPENSSL_EXPORT X509 *SSL_get_peer_certificate(const SSL *ssl);

OPENSSL_EXPORT STACK_OF(X509) *SSL_get_peer_cert_chain(const SSL *ssl);

OPENSSL_EXPORT STACK_OF(X509) *SSL_get_peer_full_cert_chain(const SSL *ssl);

OPENSSL_EXPORT const STACK_OF(CRYPTO_BUFFER) *SSL_get0_peer_certificates(
    const SSL *ssl);

OPENSSL_EXPORT void SSL_get0_signed_cert_timestamp_list(const SSL *ssl,
                                                        const uint8_t **out,
                                                        size_t *out_len);

OPENSSL_EXPORT void SSL_get0_ocsp_response(const SSL *ssl, const uint8_t **out,
                                           size_t *out_len);

OPENSSL_EXPORT int SSL_get_tls_unique(const SSL *ssl, uint8_t *out,
                                      size_t *out_len, size_t max_out);

OPENSSL_EXPORT int SSL_get_extms_support(const SSL *ssl);

OPENSSL_EXPORT const SSL_CIPHER *SSL_get_current_cipher(const SSL *ssl);

OPENSSL_EXPORT int SSL_session_reused(const SSL *ssl);

OPENSSL_EXPORT int SSL_get_secure_renegotiation_support(const SSL *ssl);

OPENSSL_EXPORT int SSL_export_keying_material(const SSL *ssl, uint8_t *out,
                                              size_t out_len, const char *label,
                                              size_t label_len,
                                              const uint8_t *context,
                                              size_t context_len,
                                              int use_context);

DECLARE_PEM_rw(SSL_SESSION, SSL_SESSION)

OPENSSL_EXPORT SSL_SESSION *SSL_SESSION_new(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_SESSION_up_ref(SSL_SESSION *session);

OPENSSL_EXPORT void SSL_SESSION_free(SSL_SESSION *session);

OPENSSL_EXPORT int SSL_SESSION_to_bytes(const SSL_SESSION *in,
                                        uint8_t **out_data, size_t *out_len);

OPENSSL_EXPORT int SSL_SESSION_to_bytes_for_ticket(const SSL_SESSION *in,
                                                   uint8_t **out_data,
                                                   size_t *out_len);

OPENSSL_EXPORT SSL_SESSION *SSL_SESSION_from_bytes(const uint8_t *in,
                                                   size_t in_len,
                                                   const SSL_CTX *ctx);

OPENSSL_EXPORT const char *SSL_SESSION_get_version(const SSL_SESSION *session);

OPENSSL_EXPORT uint16_t
SSL_SESSION_get_protocol_version(const SSL_SESSION *session);

OPENSSL_EXPORT int SSL_SESSION_set_protocol_version(SSL_SESSION *session,
                                                    uint16_t version);

#define SSL_MAX_SSL_SESSION_ID_LENGTH 32

OPENSSL_EXPORT const uint8_t *SSL_SESSION_get_id(const SSL_SESSION *session,
                                                 unsigned *out_len);

OPENSSL_EXPORT int SSL_SESSION_set1_id(SSL_SESSION *session, const uint8_t *sid,
                                       size_t sid_len);

OPENSSL_EXPORT uint64_t SSL_SESSION_get_time(const SSL_SESSION *session);

OPENSSL_EXPORT uint32_t SSL_SESSION_get_timeout(const SSL_SESSION *session);

OPENSSL_EXPORT X509 *SSL_SESSION_get0_peer(const SSL_SESSION *session);

OPENSSL_EXPORT const STACK_OF(CRYPTO_BUFFER) *
SSL_SESSION_get0_peer_certificates(const SSL_SESSION *session);

OPENSSL_EXPORT void SSL_SESSION_get0_signed_cert_timestamp_list(
    const SSL_SESSION *session, const uint8_t **out, size_t *out_len);

OPENSSL_EXPORT void SSL_SESSION_get0_ocsp_response(const SSL_SESSION *session,
                                                   const uint8_t **out,
                                                   size_t *out_len);

#define SSL_MAX_MASTER_KEY_LENGTH 48

OPENSSL_EXPORT size_t SSL_SESSION_get_master_key(const SSL_SESSION *session,
                                                 uint8_t *out, size_t max_out);

OPENSSL_EXPORT uint64_t SSL_SESSION_set_time(SSL_SESSION *session,
                                             uint64_t time);

OPENSSL_EXPORT uint32_t SSL_SESSION_set_timeout(SSL_SESSION *session,
                                                uint32_t timeout);

OPENSSL_EXPORT const uint8_t *SSL_SESSION_get0_id_context(
    const SSL_SESSION *session, unsigned *out_len);

OPENSSL_EXPORT int SSL_SESSION_set1_id_context(SSL_SESSION *session,
                                               const uint8_t *sid_ctx,
                                               size_t sid_ctx_len);

OPENSSL_EXPORT int SSL_SESSION_should_be_single_use(const SSL_SESSION *session);

OPENSSL_EXPORT int SSL_SESSION_is_resumable(const SSL_SESSION *session);

OPENSSL_EXPORT int SSL_SESSION_has_ticket(const SSL_SESSION *session);

OPENSSL_EXPORT void SSL_SESSION_get0_ticket(const SSL_SESSION *session,
                                            const uint8_t **out_ticket,
                                            size_t *out_len);

OPENSSL_EXPORT int SSL_SESSION_set_ticket(SSL_SESSION *session,
                                          const uint8_t *ticket,
                                          size_t ticket_len);

OPENSSL_EXPORT uint32_t
SSL_SESSION_get_ticket_lifetime_hint(const SSL_SESSION *session);

OPENSSL_EXPORT const SSL_CIPHER *SSL_SESSION_get0_cipher(
    const SSL_SESSION *session);

OPENSSL_EXPORT int SSL_SESSION_has_peer_sha256(const SSL_SESSION *session);

OPENSSL_EXPORT void SSL_SESSION_get0_peer_sha256(const SSL_SESSION *session,
                                                 const uint8_t **out_ptr,
                                                 size_t *out_len);

OPENSSL_EXPORT int SSL_SESSION_is_resumable_across_names(
    const SSL_SESSION *session);

#define SSL_SESS_CACHE_OFF 0x0000

#define SSL_SESS_CACHE_CLIENT 0x0001

#define SSL_SESS_CACHE_SERVER 0x0002

#define SSL_SESS_CACHE_BOTH (SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_SERVER)

#define SSL_SESS_CACHE_NO_AUTO_CLEAR 0x0080

#define SSL_SESS_CACHE_NO_INTERNAL_LOOKUP 0x0100

#define SSL_SESS_CACHE_NO_INTERNAL_STORE 0x0200

#define SSL_SESS_CACHE_NO_INTERNAL \
  (SSL_SESS_CACHE_NO_INTERNAL_LOOKUP | SSL_SESS_CACHE_NO_INTERNAL_STORE)

OPENSSL_EXPORT int SSL_CTX_set_session_cache_mode(SSL_CTX *ctx, int mode);

OPENSSL_EXPORT int SSL_CTX_get_session_cache_mode(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_set_session(SSL *ssl, SSL_SESSION *session);

#define SSL_DEFAULT_SESSION_TIMEOUT (2 * 60 * 60)

#define SSL_DEFAULT_SESSION_PSK_DHE_TIMEOUT (2 * 24 * 60 * 60)

#define SSL_DEFAULT_SESSION_AUTH_TIMEOUT (7 * 24 * 60 * 60)

OPENSSL_EXPORT uint32_t SSL_CTX_set_timeout(SSL_CTX *ctx, uint32_t timeout);

OPENSSL_EXPORT void SSL_CTX_set_session_psk_dhe_timeout(SSL_CTX *ctx,
                                                        uint32_t timeout);

OPENSSL_EXPORT uint32_t SSL_CTX_get_timeout(const SSL_CTX *ctx);

#define SSL_MAX_SID_CTX_LENGTH 32

OPENSSL_EXPORT int SSL_CTX_set_session_id_context(SSL_CTX *ctx,
                                                  const uint8_t *sid_ctx,
                                                  size_t sid_ctx_len);

OPENSSL_EXPORT int SSL_set_session_id_context(SSL *ssl, const uint8_t *sid_ctx,
                                              size_t sid_ctx_len);

OPENSSL_EXPORT const uint8_t *SSL_get0_session_id_context(const SSL *ssl,
                                                          size_t *out_len);

#define SSL_SESSION_CACHE_MAX_SIZE_DEFAULT (1024 * 20)

OPENSSL_EXPORT unsigned long SSL_CTX_sess_set_cache_size(SSL_CTX *ctx,
                                                         unsigned long size);

OPENSSL_EXPORT unsigned long SSL_CTX_sess_get_cache_size(const SSL_CTX *ctx);

OPENSSL_EXPORT size_t SSL_CTX_sess_number(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_add_session(SSL_CTX *ctx, SSL_SESSION *session);

OPENSSL_EXPORT int SSL_CTX_remove_session(SSL_CTX *ctx, SSL_SESSION *session);

OPENSSL_EXPORT void SSL_CTX_flush_sessions(SSL_CTX *ctx, uint64_t time);

OPENSSL_EXPORT void SSL_CTX_sess_set_new_cb(
    SSL_CTX *ctx, int (*new_session_cb)(SSL *ssl, SSL_SESSION *session));

OPENSSL_EXPORT int (*SSL_CTX_sess_get_new_cb(SSL_CTX *ctx))(
    SSL *ssl, SSL_SESSION *session);

OPENSSL_EXPORT void SSL_CTX_sess_set_remove_cb(
    SSL_CTX *ctx,
    void (*remove_session_cb)(SSL_CTX *ctx, SSL_SESSION *session));

OPENSSL_EXPORT void (*SSL_CTX_sess_get_remove_cb(SSL_CTX *ctx))(
    SSL_CTX *ctx, SSL_SESSION *session);

OPENSSL_EXPORT void SSL_CTX_sess_set_get_cb(
    SSL_CTX *ctx, SSL_SESSION *(*get_session_cb)(SSL *ssl, const uint8_t *id,
                                                 int id_len, int *out_copy));

OPENSSL_EXPORT SSL_SESSION *(*SSL_CTX_sess_get_get_cb(SSL_CTX *ctx))(
    SSL *ssl, const uint8_t *id, int id_len, int *out_copy);

OPENSSL_EXPORT SSL_SESSION *SSL_magic_pending_session_ptr(void);

OPENSSL_EXPORT void SSL_CTX_set_resumption_across_names_enabled(SSL_CTX *ctx,
                                                                int enabled);

OPENSSL_EXPORT void SSL_set_resumption_across_names_enabled(SSL *ssl,
                                                            int enabled);

#define SSL_DEFAULT_TICKET_KEY_ROTATION_INTERVAL (2 * 24 * 60 * 60)

OPENSSL_EXPORT int SSL_CTX_get_tlsext_ticket_keys(SSL_CTX *ctx, void *out,
                                                  size_t len);

OPENSSL_EXPORT int SSL_CTX_set_tlsext_ticket_keys(SSL_CTX *ctx, const void *in,
                                                  size_t len);

#define SSL_TICKET_KEY_NAME_LEN 16

OPENSSL_EXPORT int SSL_CTX_set_tlsext_ticket_key_cb(
    SSL_CTX *ctx,
    int (*callback)(SSL *ssl, uint8_t *key_name, uint8_t *iv,
                    EVP_CIPHER_CTX *ctx, HMAC_CTX *hmac_ctx, int encrypt));

enum ssl_ticket_aead_result_t BORINGSSL_ENUM_INT {

  ssl_ticket_aead_success,

  ssl_ticket_aead_retry,

  ssl_ticket_aead_ignore_ticket,

  ssl_ticket_aead_error,
};

struct ssl_ticket_aead_method_st {

  size_t (*max_overhead)(SSL *ssl);

  int (*seal)(SSL *ssl, uint8_t *out, size_t *out_len, size_t max_out_len,
              const uint8_t *in, size_t in_len);

  enum ssl_ticket_aead_result_t (*open)(SSL *ssl, uint8_t *out, size_t *out_len,
                                        size_t max_out_len, const uint8_t *in,
                                        size_t in_len);
};

OPENSSL_EXPORT void SSL_CTX_set_ticket_aead_method(
    SSL_CTX *ctx, const SSL_TICKET_AEAD_METHOD *aead_method);

OPENSSL_EXPORT SSL_SESSION *SSL_process_tls13_new_session_ticket(
    SSL *ssl, const uint8_t *buf, size_t buf_len);

OPENSSL_EXPORT int SSL_CTX_set_num_tickets(SSL_CTX *ctx, size_t num_tickets);

OPENSSL_EXPORT size_t SSL_CTX_get_num_tickets(const SSL_CTX *ctx);

#define SSL_GROUP_SECP224R1 21
#define SSL_GROUP_SECP256R1 23
#define SSL_GROUP_SECP384R1 24
#define SSL_GROUP_SECP521R1 25
#define SSL_GROUP_X25519 29
#define SSL_GROUP_X25519_MLKEM768 0x11ec
#define SSL_GROUP_X25519_KYBER768_DRAFT00 0x6399

OPENSSL_EXPORT int SSL_CTX_set1_group_ids(SSL_CTX *ctx,
                                          const uint16_t *group_ids,
                                          size_t num_group_ids);

OPENSSL_EXPORT int SSL_set1_group_ids(SSL *ssl, const uint16_t *group_ids,
                                      size_t num_group_ids);

OPENSSL_EXPORT uint16_t SSL_get_group_id(const SSL *ssl);

OPENSSL_EXPORT const char *SSL_get_group_name(uint16_t group_id);

OPENSSL_EXPORT size_t SSL_get_all_group_names(const char **out, size_t max_out);

OPENSSL_EXPORT int SSL_CTX_set1_groups(SSL_CTX *ctx, const int *groups,
                                       size_t num_groups);

OPENSSL_EXPORT int SSL_set1_groups(SSL *ssl, const int *groups,
                                   size_t num_groups);

OPENSSL_EXPORT int SSL_CTX_set1_groups_list(SSL_CTX *ctx, const char *groups);

OPENSSL_EXPORT int SSL_set1_groups_list(SSL *ssl, const char *groups);

OPENSSL_EXPORT int SSL_get_negotiated_group(const SSL *ssl);

#define SSL_VERIFY_NONE 0x00

#define SSL_VERIFY_PEER 0x01

#define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 0x02

OPENSSL_EXPORT void SSL_CTX_set_verify(
    SSL_CTX *ctx, int mode, int (*callback)(int ok, X509_STORE_CTX *store_ctx));

OPENSSL_EXPORT void SSL_set_verify(SSL *ssl, int mode,
                                   int (*callback)(int ok,
                                                   X509_STORE_CTX *store_ctx));

enum ssl_verify_result_t BORINGSSL_ENUM_INT {
  ssl_verify_ok,
  ssl_verify_invalid,
  ssl_verify_retry,
};

OPENSSL_EXPORT void SSL_CTX_set_custom_verify(
    SSL_CTX *ctx, int mode,
    enum ssl_verify_result_t (*callback)(SSL *ssl, uint8_t *out_alert));

OPENSSL_EXPORT void SSL_set_custom_verify(
    SSL *ssl, int mode,
    enum ssl_verify_result_t (*callback)(SSL *ssl, uint8_t *out_alert));

OPENSSL_EXPORT int SSL_CTX_get_verify_mode(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_get_verify_mode(const SSL *ssl);

OPENSSL_EXPORT int (*SSL_CTX_get_verify_callback(const SSL_CTX *ctx))(
    int ok, X509_STORE_CTX *store_ctx);

OPENSSL_EXPORT int (*SSL_get_verify_callback(const SSL *ssl))(
    int ok, X509_STORE_CTX *store_ctx);

OPENSSL_EXPORT int SSL_set1_host(SSL *ssl, const char *hostname);

OPENSSL_EXPORT void SSL_set_hostflags(SSL *ssl, unsigned flags);

OPENSSL_EXPORT void SSL_CTX_set_verify_depth(SSL_CTX *ctx, int depth);

OPENSSL_EXPORT void SSL_set_verify_depth(SSL *ssl, int depth);

OPENSSL_EXPORT int SSL_CTX_get_verify_depth(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_get_verify_depth(const SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_set1_param(SSL_CTX *ctx,
                                      const X509_VERIFY_PARAM *param);

OPENSSL_EXPORT int SSL_set1_param(SSL *ssl, const X509_VERIFY_PARAM *param);

OPENSSL_EXPORT X509_VERIFY_PARAM *SSL_CTX_get0_param(SSL_CTX *ctx);

OPENSSL_EXPORT X509_VERIFY_PARAM *SSL_get0_param(SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_set_purpose(SSL_CTX *ctx, int purpose);

OPENSSL_EXPORT int SSL_set_purpose(SSL *ssl, int purpose);

OPENSSL_EXPORT int SSL_CTX_set_trust(SSL_CTX *ctx, int trust);

OPENSSL_EXPORT int SSL_set_trust(SSL *ssl, int trust);

OPENSSL_EXPORT void SSL_CTX_set_cert_store(SSL_CTX *ctx, X509_STORE *store);

OPENSSL_EXPORT X509_STORE *SSL_CTX_get_cert_store(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_set_default_verify_paths(SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_load_verify_locations(SSL_CTX *ctx,
                                                 const char *ca_file,
                                                 const char *ca_dir);

OPENSSL_EXPORT long SSL_get_verify_result(const SSL *ssl);

OPENSSL_EXPORT int SSL_alert_from_verify_result(long result);

OPENSSL_EXPORT int SSL_get_ex_data_X509_STORE_CTX_idx(void);

OPENSSL_EXPORT void SSL_CTX_set_cert_verify_callback(
    SSL_CTX *ctx, int (*callback)(X509_STORE_CTX *store_ctx, void *arg),
    void *arg);

OPENSSL_EXPORT void SSL_enable_signed_cert_timestamps(SSL *ssl);

OPENSSL_EXPORT void SSL_CTX_enable_signed_cert_timestamps(SSL_CTX *ctx);

OPENSSL_EXPORT void SSL_enable_ocsp_stapling(SSL *ssl);

OPENSSL_EXPORT void SSL_CTX_enable_ocsp_stapling(SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_set0_verify_cert_store(SSL_CTX *ctx,
                                                  X509_STORE *store);

OPENSSL_EXPORT int SSL_CTX_set1_verify_cert_store(SSL_CTX *ctx,
                                                  X509_STORE *store);

OPENSSL_EXPORT int SSL_set0_verify_cert_store(SSL *ssl, X509_STORE *store);

OPENSSL_EXPORT int SSL_set1_verify_cert_store(SSL *ssl, X509_STORE *store);

OPENSSL_EXPORT int SSL_CTX_set_verify_algorithm_prefs(SSL_CTX *ctx,
                                                      const uint16_t *prefs,
                                                      size_t num_prefs);

OPENSSL_EXPORT int SSL_set_verify_algorithm_prefs(SSL *ssl,
                                                  const uint16_t *prefs,
                                                  size_t num_prefs);

OPENSSL_EXPORT void SSL_set_client_CA_list(SSL *ssl,
                                           STACK_OF(X509_NAME) *name_list);

OPENSSL_EXPORT void SSL_CTX_set_client_CA_list(SSL_CTX *ctx,
                                               STACK_OF(X509_NAME) *name_list);

OPENSSL_EXPORT void SSL_set0_client_CAs(SSL *ssl,
                                        STACK_OF(CRYPTO_BUFFER) *name_list);

OPENSSL_EXPORT void SSL_set0_CA_names(SSL *ssl,
                                      STACK_OF(CRYPTO_BUFFER) *name_list);

OPENSSL_EXPORT void SSL_CTX_set0_client_CAs(SSL_CTX *ctx,
                                            STACK_OF(CRYPTO_BUFFER) *name_list);

OPENSSL_EXPORT STACK_OF(X509_NAME) *SSL_get_client_CA_list(const SSL *ssl);

OPENSSL_EXPORT const STACK_OF(CRYPTO_BUFFER) *SSL_get0_server_requested_CAs(
    const SSL *ssl);

OPENSSL_EXPORT STACK_OF(X509_NAME) *SSL_CTX_get_client_CA_list(
    const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_add_client_CA(SSL *ssl, X509 *x509);

OPENSSL_EXPORT int SSL_CTX_add_client_CA(SSL_CTX *ctx, X509 *x509);

OPENSSL_EXPORT STACK_OF(X509_NAME) *SSL_load_client_CA_file(const char *file);

OPENSSL_EXPORT STACK_OF(X509_NAME) *SSL_dup_CA_list(STACK_OF(X509_NAME) *list);

OPENSSL_EXPORT int SSL_add_file_cert_subjects_to_stack(STACK_OF(X509_NAME) *out,
                                                       const char *file);

OPENSSL_EXPORT int SSL_add_bio_cert_subjects_to_stack(STACK_OF(X509_NAME) *out,
                                                      BIO *bio);

OPENSSL_EXPORT int SSL_CREDENTIAL_set1_trust_anchor_id(SSL_CREDENTIAL *cred,
                                                       const uint8_t *id,
                                                       size_t id_len);

OPENSSL_EXPORT int SSL_CTX_set1_requested_trust_anchors(SSL_CTX *ctx,
                                                        const uint8_t *ids,
                                                        size_t ids_len);

OPENSSL_EXPORT int SSL_set1_requested_trust_anchors(SSL *ssl,
                                                    const uint8_t *ids,
                                                    size_t ids_len);

OPENSSL_EXPORT int SSL_peer_matched_trust_anchor(const SSL *ssl);

OPENSSL_EXPORT void SSL_get0_peer_available_trust_anchors(const SSL *ssl,
                                                          const uint8_t **out,
                                                          size_t *out_len);

#define TLSEXT_NAMETYPE_host_name 0

OPENSSL_EXPORT int SSL_set_tlsext_host_name(SSL *ssl, const char *name);

OPENSSL_EXPORT const char *SSL_get_servername(const SSL *ssl, const int type);

OPENSSL_EXPORT int SSL_get_servername_type(const SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_set_tlsext_servername_callback(
    SSL_CTX *ctx, int (*callback)(SSL *ssl, int *out_alert, void *arg));

OPENSSL_EXPORT int SSL_CTX_set_tlsext_servername_arg(SSL_CTX *ctx, void *arg);

#define SSL_TLSEXT_ERR_OK 0
#define SSL_TLSEXT_ERR_ALERT_WARNING 1
#define SSL_TLSEXT_ERR_ALERT_FATAL 2
#define SSL_TLSEXT_ERR_NOACK 3

OPENSSL_EXPORT SSL_CTX *SSL_set_SSL_CTX(SSL *ssl, SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_set_alpn_protos(SSL_CTX *ctx, const uint8_t *protos,
                                           size_t protos_len);

OPENSSL_EXPORT int SSL_set_alpn_protos(SSL *ssl, const uint8_t *protos,
                                       size_t protos_len);

OPENSSL_EXPORT void SSL_CTX_set_alpn_select_cb(
    SSL_CTX *ctx,
    int (*cb)(SSL *ssl, const uint8_t **out, uint8_t *out_len,
              const uint8_t *in, unsigned in_len, void *arg),
    void *arg);

OPENSSL_EXPORT void SSL_get0_alpn_selected(const SSL *ssl,
                                           const uint8_t **out_data,
                                           unsigned *out_len);

OPENSSL_EXPORT void SSL_CTX_set_allow_unknown_alpn_protos(SSL_CTX *ctx,
                                                          int enabled);

OPENSSL_EXPORT int SSL_add_application_settings(SSL *ssl, const uint8_t *proto,
                                                size_t proto_len,
                                                const uint8_t *settings,
                                                size_t settings_len);

OPENSSL_EXPORT void SSL_get0_peer_application_settings(const SSL *ssl,
                                                       const uint8_t **out_data,
                                                       size_t *out_len);

OPENSSL_EXPORT int SSL_has_application_settings(const SSL *ssl);

OPENSSL_EXPORT void SSL_set_alps_use_new_codepoint(SSL *ssl, int use_new);

typedef int (*ssl_cert_compression_func_t)(SSL *ssl, CBB *out,
                                           const uint8_t *in, size_t in_len);

typedef int (*ssl_cert_decompression_func_t)(SSL *ssl, CRYPTO_BUFFER **out,
                                             size_t uncompressed_len,
                                             const uint8_t *in, size_t in_len);

OPENSSL_EXPORT int SSL_CTX_add_cert_compression_alg(
    SSL_CTX *ctx, uint16_t alg_id, ssl_cert_compression_func_t compress,
    ssl_cert_decompression_func_t decompress);

OPENSSL_EXPORT void SSL_CTX_set_next_protos_advertised_cb(
    SSL_CTX *ctx,
    int (*cb)(SSL *ssl, const uint8_t **out, unsigned *out_len, void *arg),
    void *arg);

OPENSSL_EXPORT void SSL_CTX_set_next_proto_select_cb(
    SSL_CTX *ctx,
    int (*cb)(SSL *ssl, uint8_t **out, uint8_t *out_len, const uint8_t *in,
              unsigned in_len, void *arg),
    void *arg);

OPENSSL_EXPORT void SSL_get0_next_proto_negotiated(const SSL *ssl,
                                                   const uint8_t **out_data,
                                                   unsigned *out_len);

OPENSSL_EXPORT int SSL_select_next_proto(uint8_t **out, uint8_t *out_len,
                                         const uint8_t *peer, unsigned peer_len,
                                         const uint8_t *supported,
                                         unsigned supported_len);

#define OPENSSL_NPN_UNSUPPORTED 0
#define OPENSSL_NPN_NEGOTIATED 1
#define OPENSSL_NPN_NO_OVERLAP 2

OPENSSL_EXPORT void SSL_CTX_set_tls_channel_id_enabled(SSL_CTX *ctx,
                                                       int enabled);

OPENSSL_EXPORT void SSL_set_tls_channel_id_enabled(SSL *ssl, int enabled);

OPENSSL_EXPORT int SSL_CTX_set1_tls_channel_id(SSL_CTX *ctx,
                                               EVP_PKEY *private_key);

OPENSSL_EXPORT int SSL_set1_tls_channel_id(SSL *ssl, EVP_PKEY *private_key);

OPENSSL_EXPORT size_t SSL_get_tls_channel_id(SSL *ssl, uint8_t *out,
                                             size_t max_out);

struct srtp_protection_profile_st {
  const char *name;
  unsigned long id;
}                              ;

DEFINE_CONST_STACK_OF(SRTP_PROTECTION_PROFILE)

#define SRTP_AES128_CM_SHA1_80 0x0001
#define SRTP_AES128_CM_SHA1_32 0x0002
#define SRTP_AES128_F8_SHA1_80 0x0003
#define SRTP_AES128_F8_SHA1_32 0x0004
#define SRTP_NULL_SHA1_80 0x0005
#define SRTP_NULL_SHA1_32 0x0006
#define SRTP_AEAD_AES_128_GCM 0x0007
#define SRTP_AEAD_AES_256_GCM 0x0008

OPENSSL_EXPORT int SSL_CTX_set_srtp_profiles(SSL_CTX *ctx,
                                             const char *profiles);

OPENSSL_EXPORT int SSL_set_srtp_profiles(SSL *ssl, const char *profiles);

OPENSSL_EXPORT const STACK_OF(SRTP_PROTECTION_PROFILE) *SSL_get_srtp_profiles(
    const SSL *ssl);

OPENSSL_EXPORT const SRTP_PROTECTION_PROFILE *SSL_get_selected_srtp_profile(
    SSL *ssl);

#define PSK_MAX_IDENTITY_LEN 128

#define PSK_MAX_PSK_LEN 256

OPENSSL_EXPORT void SSL_CTX_set_psk_client_callback(
    SSL_CTX *ctx, unsigned (*cb)(SSL *ssl, const char *hint, char *identity,
                                 unsigned max_identity_len, uint8_t *psk,
                                 unsigned max_psk_len));

OPENSSL_EXPORT void SSL_set_psk_client_callback(
    SSL *ssl, unsigned (*cb)(SSL *ssl, const char *hint, char *identity,
                             unsigned max_identity_len, uint8_t *psk,
                             unsigned max_psk_len));

OPENSSL_EXPORT void SSL_CTX_set_psk_server_callback(
    SSL_CTX *ctx, unsigned (*cb)(SSL *ssl, const char *identity, uint8_t *psk,
                                 unsigned max_psk_len));

OPENSSL_EXPORT void SSL_set_psk_server_callback(
    SSL *ssl, unsigned (*cb)(SSL *ssl, const char *identity, uint8_t *psk,
                             unsigned max_psk_len));

OPENSSL_EXPORT int SSL_CTX_use_psk_identity_hint(SSL_CTX *ctx,
                                                 const char *identity_hint);

OPENSSL_EXPORT int SSL_use_psk_identity_hint(SSL *ssl,
                                             const char *identity_hint);

OPENSSL_EXPORT const char *SSL_get_psk_identity_hint(const SSL *ssl);

OPENSSL_EXPORT const char *SSL_get_psk_identity(const SSL *ssl);

OPENSSL_EXPORT SSL_CREDENTIAL *SSL_CREDENTIAL_new_delegated(void);

OPENSSL_EXPORT int SSL_CREDENTIAL_set1_delegated_credential(
    SSL_CREDENTIAL *cred, CRYPTO_BUFFER *dc);

#define SSL_PAKE_SPAKE2PLUSV1 0x7d96

OPENSSL_EXPORT int SSL_spake2plusv1_register(
    uint8_t out_w0[32], uint8_t out_w1[32], uint8_t out_registration_record[65],
    const uint8_t *password, size_t password_len,
    const uint8_t *client_identity, size_t client_identity_len,
    const uint8_t *server_identity, size_t server_identity_len);

OPENSSL_EXPORT SSL_CREDENTIAL *SSL_CREDENTIAL_new_spake2plusv1_client(
    const uint8_t *context, size_t context_len, const uint8_t *client_identity,
    size_t client_identity_len, const uint8_t *server_identity,
    size_t server_identity_len, uint32_t error_limit, const uint8_t *w0,
    size_t w0_len, const uint8_t *w1, size_t w1_len);

OPENSSL_EXPORT SSL_CREDENTIAL *SSL_CREDENTIAL_new_spake2plusv1_server(
    const uint8_t *context, size_t context_len, const uint8_t *client_identity,
    size_t client_identity_len, const uint8_t *server_identity,
    size_t server_identity_len, uint32_t rate_limit, const uint8_t *w0,
    size_t w0_len, const uint8_t *registration_record,
    size_t registration_record_len);

enum ssl_encryption_level_t BORINGSSL_ENUM_INT {
  ssl_encryption_initial = 0,
  ssl_encryption_early_data = 1,
  ssl_encryption_handshake = 2,
  ssl_encryption_application = 3,
};

struct ssl_quic_method_st {

  int (*set_read_secret)(SSL *ssl, enum ssl_encryption_level_t level,
                         const SSL_CIPHER *cipher, const uint8_t *secret,
                         size_t secret_len);

  int (*set_write_secret)(SSL *ssl, enum ssl_encryption_level_t level,
                          const SSL_CIPHER *cipher, const uint8_t *secret,
                          size_t secret_len);

  int (*add_handshake_data)(SSL *ssl, enum ssl_encryption_level_t level,
                            const uint8_t *data, size_t len);

  int (*flush_flight)(SSL *ssl);

  int (*send_alert)(SSL *ssl, enum ssl_encryption_level_t level, uint8_t alert);
};

OPENSSL_EXPORT size_t SSL_quic_max_handshake_flight_len(
    const SSL *ssl, enum ssl_encryption_level_t level);

OPENSSL_EXPORT enum ssl_encryption_level_t SSL_quic_read_level(const SSL *ssl);

OPENSSL_EXPORT enum ssl_encryption_level_t SSL_quic_write_level(const SSL *ssl);

OPENSSL_EXPORT int SSL_provide_quic_data(SSL *ssl,
                                         enum ssl_encryption_level_t level,
                                         const uint8_t *data, size_t len);

OPENSSL_EXPORT int SSL_process_quic_post_handshake(SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_set_quic_method(SSL_CTX *ctx,
                                           const SSL_QUIC_METHOD *quic_method);

OPENSSL_EXPORT int SSL_set_quic_method(SSL *ssl,
                                       const SSL_QUIC_METHOD *quic_method);

OPENSSL_EXPORT int SSL_set_quic_transport_params(SSL *ssl,
                                                 const uint8_t *params,
                                                 size_t params_len);

OPENSSL_EXPORT void SSL_get_peer_quic_transport_params(
    const SSL *ssl, const uint8_t **out_params, size_t *out_params_len);

OPENSSL_EXPORT void SSL_set_quic_use_legacy_codepoint(SSL *ssl, int use_legacy);

OPENSSL_EXPORT int SSL_set_quic_early_data_context(SSL *ssl,
                                                   const uint8_t *context,
                                                   size_t context_len);

OPENSSL_EXPORT void SSL_CTX_set_early_data_enabled(SSL_CTX *ctx, int enabled);

OPENSSL_EXPORT void SSL_set_early_data_enabled(SSL *ssl, int enabled);

OPENSSL_EXPORT int SSL_in_early_data(const SSL *ssl);

OPENSSL_EXPORT int SSL_SESSION_early_data_capable(const SSL_SESSION *session);

OPENSSL_EXPORT SSL_SESSION *SSL_SESSION_copy_without_early_data(
    SSL_SESSION *session);

OPENSSL_EXPORT int SSL_early_data_accepted(const SSL *ssl);

OPENSSL_EXPORT void SSL_reset_early_data_reject(SSL *ssl);

OPENSSL_EXPORT int32_t SSL_get_ticket_age_skew(const SSL *ssl);

enum ssl_early_data_reason_t BORINGSSL_ENUM_INT {

  ssl_early_data_unknown = 0,

  ssl_early_data_disabled = 1,

  ssl_early_data_accepted = 2,

  ssl_early_data_protocol_version = 3,

  ssl_early_data_peer_declined = 4,

  ssl_early_data_no_session_offered = 5,

  ssl_early_data_session_not_resumed = 6,

  ssl_early_data_unsupported_for_session = 7,

  ssl_early_data_hello_retry_request = 8,

  ssl_early_data_alpn_mismatch = 9,

  ssl_early_data_channel_id = 10,

  ssl_early_data_ticket_age_skew = 12,

  ssl_early_data_quic_parameter_mismatch = 13,

  ssl_early_data_alps_mismatch = 14,

  ssl_early_data_reason_max_value = ssl_early_data_alps_mismatch,
};

OPENSSL_EXPORT enum ssl_early_data_reason_t SSL_get_early_data_reason(
    const SSL *ssl);

OPENSSL_EXPORT const char *SSL_early_data_reason_string(
    enum ssl_early_data_reason_t reason);

OPENSSL_EXPORT void SSL_set_enable_ech_grease(SSL *ssl, int enable);

OPENSSL_EXPORT int SSL_set1_ech_config_list(SSL *ssl,
                                            const uint8_t *ech_config_list,
                                            size_t ech_config_list_len);

OPENSSL_EXPORT void SSL_get0_ech_name_override(const SSL *ssl,
                                               const char **out_name,
                                               size_t *out_name_len);

OPENSSL_EXPORT void SSL_get0_ech_retry_configs(
    const SSL *ssl, const uint8_t **out_retry_configs,
    size_t *out_retry_configs_len);

OPENSSL_EXPORT int SSL_marshal_ech_config(uint8_t **out, size_t *out_len,
                                          uint8_t config_id,
                                          const EVP_HPKE_KEY *key,
                                          const char *public_name,
                                          size_t max_name_len);

OPENSSL_EXPORT SSL_ECH_KEYS *SSL_ECH_KEYS_new(void);

OPENSSL_EXPORT void SSL_ECH_KEYS_up_ref(SSL_ECH_KEYS *keys);

OPENSSL_EXPORT void SSL_ECH_KEYS_free(SSL_ECH_KEYS *keys);

OPENSSL_EXPORT int SSL_ECH_KEYS_add(SSL_ECH_KEYS *keys, int is_retry_config,
                                    const uint8_t *ech_config,
                                    size_t ech_config_len,
                                    const EVP_HPKE_KEY *key);

OPENSSL_EXPORT int SSL_ECH_KEYS_has_duplicate_config_id(
    const SSL_ECH_KEYS *keys);

OPENSSL_EXPORT int SSL_ECH_KEYS_marshal_retry_configs(const SSL_ECH_KEYS *keys,
                                                      uint8_t **out,
                                                      size_t *out_len);

OPENSSL_EXPORT int SSL_CTX_set1_ech_keys(SSL_CTX *ctx, SSL_ECH_KEYS *keys);

OPENSSL_EXPORT int SSL_ech_accepted(const SSL *ssl);

#define SSL_AD_REASON_OFFSET 1000

#define SSL_AD_CLOSE_NOTIFY SSL3_AD_CLOSE_NOTIFY
#define SSL_AD_UNEXPECTED_MESSAGE SSL3_AD_UNEXPECTED_MESSAGE
#define SSL_AD_BAD_RECORD_MAC SSL3_AD_BAD_RECORD_MAC
#define SSL_AD_DECRYPTION_FAILED TLS1_AD_DECRYPTION_FAILED
#define SSL_AD_RECORD_OVERFLOW TLS1_AD_RECORD_OVERFLOW
#define SSL_AD_DECOMPRESSION_FAILURE SSL3_AD_DECOMPRESSION_FAILURE
#define SSL_AD_HANDSHAKE_FAILURE SSL3_AD_HANDSHAKE_FAILURE
#define SSL_AD_NO_CERTIFICATE SSL3_AD_NO_CERTIFICATE  // Legacy SSL 3.0 value
#define SSL_AD_BAD_CERTIFICATE SSL3_AD_BAD_CERTIFICATE
#define SSL_AD_UNSUPPORTED_CERTIFICATE SSL3_AD_UNSUPPORTED_CERTIFICATE
#define SSL_AD_CERTIFICATE_REVOKED SSL3_AD_CERTIFICATE_REVOKED
#define SSL_AD_CERTIFICATE_EXPIRED SSL3_AD_CERTIFICATE_EXPIRED
#define SSL_AD_CERTIFICATE_UNKNOWN SSL3_AD_CERTIFICATE_UNKNOWN
#define SSL_AD_ILLEGAL_PARAMETER SSL3_AD_ILLEGAL_PARAMETER
#define SSL_AD_UNKNOWN_CA TLS1_AD_UNKNOWN_CA
#define SSL_AD_ACCESS_DENIED TLS1_AD_ACCESS_DENIED
#define SSL_AD_DECODE_ERROR TLS1_AD_DECODE_ERROR
#define SSL_AD_DECRYPT_ERROR TLS1_AD_DECRYPT_ERROR
#define SSL_AD_EXPORT_RESTRICTION TLS1_AD_EXPORT_RESTRICTION
#define SSL_AD_PROTOCOL_VERSION TLS1_AD_PROTOCOL_VERSION
#define SSL_AD_INSUFFICIENT_SECURITY TLS1_AD_INSUFFICIENT_SECURITY
#define SSL_AD_INTERNAL_ERROR TLS1_AD_INTERNAL_ERROR
#define SSL_AD_INAPPROPRIATE_FALLBACK SSL3_AD_INAPPROPRIATE_FALLBACK
#define SSL_AD_USER_CANCELLED TLS1_AD_USER_CANCELLED
#define SSL_AD_NO_RENEGOTIATION TLS1_AD_NO_RENEGOTIATION
#define SSL_AD_MISSING_EXTENSION TLS1_AD_MISSING_EXTENSION
#define SSL_AD_UNSUPPORTED_EXTENSION TLS1_AD_UNSUPPORTED_EXTENSION
#define SSL_AD_CERTIFICATE_UNOBTAINABLE TLS1_AD_CERTIFICATE_UNOBTAINABLE
#define SSL_AD_UNRECOGNIZED_NAME TLS1_AD_UNRECOGNIZED_NAME
#define SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE \
  TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE
#define SSL_AD_BAD_CERTIFICATE_HASH_VALUE TLS1_AD_BAD_CERTIFICATE_HASH_VALUE
#define SSL_AD_UNKNOWN_PSK_IDENTITY TLS1_AD_UNKNOWN_PSK_IDENTITY
#define SSL_AD_CERTIFICATE_REQUIRED TLS1_AD_CERTIFICATE_REQUIRED
#define SSL_AD_NO_APPLICATION_PROTOCOL TLS1_AD_NO_APPLICATION_PROTOCOL
#define SSL_AD_ECH_REQUIRED TLS1_AD_ECH_REQUIRED

OPENSSL_EXPORT const char *SSL_alert_type_string_long(int value);

OPENSSL_EXPORT const char *SSL_alert_desc_string_long(int value);

OPENSSL_EXPORT int SSL_send_fatal_alert(SSL *ssl, uint8_t alert);

OPENSSL_EXPORT int SSL_set_ex_data(SSL *ssl, int idx, void *data);
OPENSSL_EXPORT void *SSL_get_ex_data(const SSL *ssl, int idx);
OPENSSL_EXPORT int SSL_get_ex_new_index(long argl, void *argp,
                                        CRYPTO_EX_unused *unused,
                                        CRYPTO_EX_dup *dup_unused,
                                        CRYPTO_EX_free *free_func);

OPENSSL_EXPORT int SSL_SESSION_set_ex_data(SSL_SESSION *session, int idx,
                                           void *data);
OPENSSL_EXPORT void *SSL_SESSION_get_ex_data(const SSL_SESSION *session,
                                             int idx);
OPENSSL_EXPORT int SSL_SESSION_get_ex_new_index(long argl, void *argp,
                                                CRYPTO_EX_unused *unused,
                                                CRYPTO_EX_dup *dup_unused,
                                                CRYPTO_EX_free *free_func);

OPENSSL_EXPORT int SSL_CTX_set_ex_data(SSL_CTX *ctx, int idx, void *data);
OPENSSL_EXPORT void *SSL_CTX_get_ex_data(const SSL_CTX *ctx, int idx);
OPENSSL_EXPORT int SSL_CTX_get_ex_new_index(long argl, void *argp,
                                            CRYPTO_EX_unused *unused,
                                            CRYPTO_EX_dup *dup_unused,
                                            CRYPTO_EX_free *free_func);

OPENSSL_EXPORT int SSL_CREDENTIAL_set_ex_data(SSL_CREDENTIAL *cred, int idx,
                                              void *data);
OPENSSL_EXPORT void *SSL_CREDENTIAL_get_ex_data(const SSL_CREDENTIAL *cred,
                                                int idx);
OPENSSL_EXPORT int SSL_CREDENTIAL_get_ex_new_index(long argl, void *argp,
                                                   CRYPTO_EX_unused *unused,
                                                   CRYPTO_EX_dup *dup_unused,
                                                   CRYPTO_EX_free *free_func);

OPENSSL_EXPORT int SSL_get_ivs(const SSL *ssl, const uint8_t **out_read_iv,
                               const uint8_t **out_write_iv,
                               size_t *out_iv_len);

OPENSSL_EXPORT size_t SSL_get_key_block_len(const SSL *ssl);

OPENSSL_EXPORT int SSL_generate_key_block(const SSL *ssl, uint8_t *out,
                                          size_t out_len);

OPENSSL_EXPORT uint64_t SSL_get_read_sequence(const SSL *ssl);

OPENSSL_EXPORT uint64_t SSL_get_write_sequence(const SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_set_record_protocol_version(SSL_CTX *ctx,
                                                       int version);

OPENSSL_EXPORT int SSL_serialize_capabilities(const SSL *ssl, CBB *out);

OPENSSL_EXPORT int SSL_request_handshake_hints(SSL *ssl,
                                               const uint8_t *client_hello,
                                               size_t client_hello_len,
                                               const uint8_t *capabilities,
                                               size_t capabilities_len);

OPENSSL_EXPORT int SSL_serialize_handshake_hints(const SSL *ssl, CBB *out);

OPENSSL_EXPORT int SSL_set_handshake_hints(SSL *ssl, const uint8_t *hints,
                                           size_t hints_len);

OPENSSL_EXPORT void SSL_CTX_set_msg_callback(
    SSL_CTX *ctx, void (*cb)(int is_write, int version, int content_type,
                             const void *buf, size_t len, SSL *ssl, void *arg));

OPENSSL_EXPORT void SSL_CTX_set_msg_callback_arg(SSL_CTX *ctx, void *arg);

OPENSSL_EXPORT void SSL_set_msg_callback(
    SSL *ssl, void (*cb)(int write_p, int version, int content_type,
                         const void *buf, size_t len, SSL *ssl, void *arg));

OPENSSL_EXPORT void SSL_set_msg_callback_arg(SSL *ssl, void *arg);

OPENSSL_EXPORT void SSL_CTX_set_keylog_callback(SSL_CTX *ctx,
                                                void (*cb)(const SSL *ssl,
                                                           const char *line));

OPENSSL_EXPORT void (*SSL_CTX_get_keylog_callback(const SSL_CTX *ctx))(
    const SSL *ssl, const char *line);

OPENSSL_EXPORT void SSL_CTX_set_current_time_cb(
    SSL_CTX *ctx, void (*cb)(const SSL *ssl, struct timeval *out_clock));

OPENSSL_EXPORT void SSL_set_shed_handshake_config(SSL *ssl, int enable);

enum ssl_renegotiate_mode_t BORINGSSL_ENUM_INT {
  ssl_renegotiate_never = 0,
  ssl_renegotiate_once,
  ssl_renegotiate_freely,
  ssl_renegotiate_ignore,
  ssl_renegotiate_explicit,
};

OPENSSL_EXPORT void SSL_set_renegotiate_mode(SSL *ssl,
                                             enum ssl_renegotiate_mode_t mode);

OPENSSL_EXPORT int SSL_renegotiate(SSL *ssl);

OPENSSL_EXPORT int SSL_renegotiate_pending(SSL *ssl);

OPENSSL_EXPORT int SSL_total_renegotiations(const SSL *ssl);

#define SSL_MAX_CERT_LIST_DEFAULT (1024 * 100)

OPENSSL_EXPORT size_t SSL_CTX_get_max_cert_list(const SSL_CTX *ctx);

OPENSSL_EXPORT void SSL_CTX_set_max_cert_list(SSL_CTX *ctx,
                                              size_t max_cert_list);

OPENSSL_EXPORT size_t SSL_get_max_cert_list(const SSL *ssl);

OPENSSL_EXPORT void SSL_set_max_cert_list(SSL *ssl, size_t max_cert_list);

OPENSSL_EXPORT int SSL_CTX_set_max_send_fragment(SSL_CTX *ctx,
                                                 size_t max_send_fragment);

OPENSSL_EXPORT int SSL_set_max_send_fragment(SSL *ssl,
                                             size_t max_send_fragment);

struct ssl_early_callback_ctx {
  SSL *ssl;
  const uint8_t *client_hello;
  size_t client_hello_len;
  uint16_t version;
  const uint8_t *random;
  size_t random_len;
  const uint8_t *session_id;
  size_t session_id_len;
  const uint8_t *dtls_cookie;
  size_t dtls_cookie_len;
  const uint8_t *cipher_suites;
  size_t cipher_suites_len;
  const uint8_t *compression_methods;
  size_t compression_methods_len;
  const uint8_t *extensions;
  size_t extensions_len;
}                       ;

enum ssl_select_cert_result_t BORINGSSL_ENUM_INT {

  ssl_select_cert_success = 1,

  ssl_select_cert_retry = 0,

  ssl_select_cert_error = -1,

  ssl_select_cert_disable_ech = -2,
};

OPENSSL_EXPORT int SSL_early_callback_ctx_extension_get(
    const SSL_CLIENT_HELLO *client_hello, uint16_t extension_type,
    const uint8_t **out_data, size_t *out_len);

OPENSSL_EXPORT void SSL_CTX_set_select_certificate_cb(
    SSL_CTX *ctx,
    enum ssl_select_cert_result_t (*cb)(const SSL_CLIENT_HELLO *));

OPENSSL_EXPORT void SSL_CTX_set_dos_protection_cb(
    SSL_CTX *ctx, int (*cb)(const SSL_CLIENT_HELLO *));

OPENSSL_EXPORT void SSL_CTX_set_reverify_on_resume(SSL_CTX *ctx, int enabled);

OPENSSL_EXPORT void SSL_set_enforce_rsa_key_usage(SSL *ssl, int enabled);

OPENSSL_EXPORT int SSL_was_key_usage_invalid(const SSL *ssl);

#define SSL_ST_CONNECT 0x1000
#define SSL_ST_ACCEPT 0x2000
#define SSL_ST_MASK 0x0FFF
#define SSL_ST_INIT (SSL_ST_CONNECT | SSL_ST_ACCEPT)
#define SSL_ST_OK 0x03
#define SSL_ST_RENEGOTIATE (0x04 | SSL_ST_INIT)
#define SSL_ST_BEFORE (0x05 | SSL_ST_INIT)

#define TLS_ST_OK SSL_ST_OK
#define TLS_ST_BEFORE SSL_ST_BEFORE

#define SSL_CB_LOOP 0x01
#define SSL_CB_EXIT 0x02
#define SSL_CB_READ 0x04
#define SSL_CB_WRITE 0x08
#define SSL_CB_ALERT 0x4000
#define SSL_CB_READ_ALERT (SSL_CB_ALERT | SSL_CB_READ)
#define SSL_CB_WRITE_ALERT (SSL_CB_ALERT | SSL_CB_WRITE)
#define SSL_CB_ACCEPT_LOOP (SSL_ST_ACCEPT | SSL_CB_LOOP)
#define SSL_CB_ACCEPT_EXIT (SSL_ST_ACCEPT | SSL_CB_EXIT)
#define SSL_CB_CONNECT_LOOP (SSL_ST_CONNECT | SSL_CB_LOOP)
#define SSL_CB_CONNECT_EXIT (SSL_ST_CONNECT | SSL_CB_EXIT)
#define SSL_CB_HANDSHAKE_START 0x10
#define SSL_CB_HANDSHAKE_DONE 0x20

OPENSSL_EXPORT void SSL_CTX_set_info_callback(SSL_CTX *ctx,
                                              void (*cb)(const SSL *ssl,
                                                         int type, int value));

OPENSSL_EXPORT void (*SSL_CTX_get_info_callback(SSL_CTX *ctx))(const SSL *ssl,
                                                               int type,
                                                               int value);

OPENSSL_EXPORT void SSL_set_info_callback(SSL *ssl,
                                          void (*cb)(const SSL *ssl, int type,
                                                     int value));

OPENSSL_EXPORT void (*SSL_get_info_callback(const SSL *ssl))(const SSL *ssl,
                                                             int type,
                                                             int value);

OPENSSL_EXPORT const char *SSL_state_string_long(const SSL *ssl);

#define SSL_SENT_SHUTDOWN 1
#define SSL_RECEIVED_SHUTDOWN 2

OPENSSL_EXPORT int SSL_get_shutdown(const SSL *ssl);

OPENSSL_EXPORT uint16_t SSL_get_peer_signature_algorithm(const SSL *ssl);

OPENSSL_EXPORT size_t SSL_get_client_random(const SSL *ssl, uint8_t *out,
                                            size_t max_out);

OPENSSL_EXPORT size_t SSL_get_server_random(const SSL *ssl, uint8_t *out,
                                            size_t max_out);

OPENSSL_EXPORT const SSL_CIPHER *SSL_get_pending_cipher(const SSL *ssl);

OPENSSL_EXPORT void SSL_set_retain_only_sha256_of_client_certs(SSL *ssl,
                                                               int enable);

OPENSSL_EXPORT void SSL_CTX_set_retain_only_sha256_of_client_certs(SSL_CTX *ctx,
                                                                   int enable);

OPENSSL_EXPORT void SSL_CTX_set_grease_enabled(SSL_CTX *ctx, int enabled);

OPENSSL_EXPORT void SSL_CTX_set_permute_extensions(SSL_CTX *ctx, int enabled);

OPENSSL_EXPORT void SSL_set_permute_extensions(SSL *ssl, int enabled);

OPENSSL_EXPORT size_t SSL_max_seal_overhead(const SSL *ssl);

OPENSSL_EXPORT void SSL_CTX_set_false_start_allowed_without_alpn(SSL_CTX *ctx,
                                                                 int allowed);

OPENSSL_EXPORT int SSL_used_hello_retry_request(const SSL *ssl);

OPENSSL_EXPORT void SSL_set_jdk11_workaround(SSL *ssl, int enable);

OPENSSL_EXPORT int SSL_parse_client_hello(const SSL *ssl, SSL_CLIENT_HELLO *out,
                                          const uint8_t *in, size_t len);

OPENSSL_EXPORT int SSL_library_init(void);

OPENSSL_EXPORT const char *SSL_CIPHER_description(const SSL_CIPHER *cipher,
                                                  char *buf, int len);

OPENSSL_EXPORT const char *SSL_CIPHER_get_version(const SSL_CIPHER *cipher);

typedef void COMP_METHOD;
typedef struct ssl_comp_st SSL_COMP;

OPENSSL_EXPORT STACK_OF(SSL_COMP) *SSL_COMP_get_compression_methods(void);

OPENSSL_EXPORT int SSL_COMP_add_compression_method(int id, COMP_METHOD *cm);

OPENSSL_EXPORT const char *SSL_COMP_get_name(const COMP_METHOD *comp);

OPENSSL_EXPORT const char *SSL_COMP_get0_name(const SSL_COMP *comp);

OPENSSL_EXPORT int SSL_COMP_get_id(const SSL_COMP *comp);

OPENSSL_EXPORT void SSL_COMP_free_compression_methods(void);

OPENSSL_EXPORT const SSL_METHOD *SSLv23_method(void);

OPENSSL_EXPORT const SSL_METHOD *TLSv1_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_1_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_2_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_2_method(void);

OPENSSL_EXPORT const SSL_METHOD *TLS_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLS_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *SSLv23_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *SSLv23_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_1_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_1_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_2_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *TLSv1_2_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLS_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLS_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_client_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_2_server_method(void);
OPENSSL_EXPORT const SSL_METHOD *DTLSv1_2_client_method(void);

OPENSSL_EXPORT int SSL_clear(SSL *ssl);

OPENSSL_EXPORT void SSL_CTX_set_tmp_rsa_callback(
    SSL_CTX *ctx, RSA *(*cb)(SSL *ssl, int is_export, int keylength));

OPENSSL_EXPORT void SSL_set_tmp_rsa_callback(SSL *ssl,
                                             RSA *(*cb)(SSL *ssl, int is_export,
                                                        int keylength));

OPENSSL_EXPORT int SSL_CTX_sess_connect(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_connect_good(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_connect_renegotiate(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_accept(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_accept_renegotiate(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_accept_good(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_hits(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_cb_hits(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_misses(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_timeouts(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_sess_cache_full(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_cutthrough_complete(const SSL *ssl);

OPENSSL_EXPORT int SSL_num_renegotiations(const SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_need_tmp_RSA(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_need_tmp_RSA(const SSL *ssl);

OPENSSL_EXPORT int SSL_CTX_set_tmp_rsa(SSL_CTX *ctx, const RSA *rsa);

OPENSSL_EXPORT int SSL_set_tmp_rsa(SSL *ssl, const RSA *rsa);

OPENSSL_EXPORT int SSL_CTX_get_read_ahead(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_CTX_set_read_ahead(SSL_CTX *ctx, int yes);

OPENSSL_EXPORT int SSL_get_read_ahead(const SSL *ssl);

OPENSSL_EXPORT int SSL_set_read_ahead(SSL *ssl, int yes);

OPENSSL_EXPORT void SSL_set_state(SSL *ssl, int state);

OPENSSL_EXPORT char *SSL_get_shared_ciphers(const SSL *ssl, char *buf, int len);

OPENSSL_EXPORT int SSL_get_shared_sigalgs(SSL *ssl, int idx, int *psign,
                                          int *phash, int *psignandhash,
                                          uint8_t *rsig, uint8_t *rhash);

#define SSL_MODE_HANDSHAKE_CUTTHROUGH SSL_MODE_ENABLE_FALSE_START

OPENSSL_EXPORT int i2d_SSL_SESSION(SSL_SESSION *in, uint8_t **pp);

OPENSSL_EXPORT SSL_SESSION *d2i_SSL_SESSION(SSL_SESSION **a, const uint8_t **pp,
                                            long length);

OPENSSL_EXPORT int i2d_SSL_SESSION_bio(BIO *bio, const SSL_SESSION *session);

OPENSSL_EXPORT SSL_SESSION *d2i_SSL_SESSION_bio(BIO *bio, SSL_SESSION **out);

OPENSSL_EXPORT void ERR_load_SSL_strings(void);

OPENSSL_EXPORT void SSL_load_error_strings(void);

OPENSSL_EXPORT int SSL_CTX_set_tlsext_use_srtp(SSL_CTX *ctx,
                                               const char *profiles);

OPENSSL_EXPORT int SSL_set_tlsext_use_srtp(SSL *ssl, const char *profiles);

OPENSSL_EXPORT const COMP_METHOD *SSL_get_current_compression(SSL *ssl);

OPENSSL_EXPORT const COMP_METHOD *SSL_get_current_expansion(SSL *ssl);

OPENSSL_EXPORT int SSL_get_server_tmp_key(SSL *ssl, EVP_PKEY **out_key);

OPENSSL_EXPORT int SSL_CTX_set_tmp_dh(SSL_CTX *ctx, const DH *dh);

OPENSSL_EXPORT int SSL_set_tmp_dh(SSL *ssl, const DH *dh);

OPENSSL_EXPORT void SSL_CTX_set_tmp_dh_callback(
    SSL_CTX *ctx, DH *(*cb)(SSL *ssl, int is_export, int keylength));

OPENSSL_EXPORT void SSL_set_tmp_dh_callback(SSL *ssl,
                                            DH *(*cb)(SSL *ssl, int is_export,
                                                      int keylength));

OPENSSL_EXPORT int SSL_CTX_set1_sigalgs(SSL_CTX *ctx, const int *values,
                                        size_t num_values);

OPENSSL_EXPORT int SSL_set1_sigalgs(SSL *ssl, const int *values,
                                    size_t num_values);

OPENSSL_EXPORT int SSL_CTX_set1_sigalgs_list(SSL_CTX *ctx, const char *str);

OPENSSL_EXPORT int SSL_set1_sigalgs_list(SSL *ssl, const char *str);

#define SSL_set_app_data(s, arg) (SSL_set_ex_data(s, 0, (char *)(arg)))
#define SSL_get_app_data(s) (SSL_get_ex_data(s, 0))
#define SSL_SESSION_set_app_data(s, a) \
  (SSL_SESSION_set_ex_data(s, 0, (char *)(a)))
#define SSL_SESSION_get_app_data(s) (SSL_SESSION_get_ex_data(s, 0))
#define SSL_CTX_get_app_data(ctx) (SSL_CTX_get_ex_data(ctx, 0))
#define SSL_CTX_set_app_data(ctx, arg) \
  (SSL_CTX_set_ex_data(ctx, 0, (char *)(arg)))

#define OpenSSL_add_ssl_algorithms() SSL_library_init()
#define SSLeay_add_ssl_algorithms() SSL_library_init()

#define SSL_get_cipher(ssl) SSL_CIPHER_get_name(SSL_get_current_cipher(ssl))
#define SSL_get_cipher_bits(ssl, out_alg_bits) \
  SSL_CIPHER_get_bits(SSL_get_current_cipher(ssl), out_alg_bits)
#define SSL_get_cipher_version(ssl) \
  SSL_CIPHER_get_version(SSL_get_current_cipher(ssl))
#define SSL_get_cipher_name(ssl) \
  SSL_CIPHER_get_name(SSL_get_current_cipher(ssl))
#define SSL_get_time(session) SSL_SESSION_get_time(session)
#define SSL_set_time(session, time) SSL_SESSION_set_time((session), (time))
#define SSL_get_timeout(session) SSL_SESSION_get_timeout(session)
#define SSL_set_timeout(session, timeout) \
  SSL_SESSION_set_timeout((session), (timeout))

struct ssl_comp_st {
  int id;
  const char *name;
  char *method;
};

DEFINE_STACK_OF(SSL_COMP)

#define SSL_MODE_AUTO_RETRY 0
#define SSL_MODE_RELEASE_BUFFERS 0
#define SSL_MODE_SEND_CLIENTHELLO_TIME 0
#define SSL_MODE_SEND_SERVERHELLO_TIME 0
#define SSL_OP_ALL 0
#define SSL_OP_ALLOW_UNSAFE_LEGACY_RENEGOTIATION 0
#define SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS 0
#define SSL_OP_EPHEMERAL_RSA 0
#define SSL_OP_LEGACY_SERVER_CONNECT 0
#define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER 0
#define SSL_OP_MICROSOFT_SESS_ID_BUG 0
#define SSL_OP_MSIE_SSLV2_RSA_PADDING 0
#define SSL_OP_NETSCAPE_CA_DN_BUG 0
#define SSL_OP_NETSCAPE_CHALLENGE_BUG 0
#define SSL_OP_NETSCAPE_DEMO_CIPHER_CHANGE_BUG 0
#define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG 0
#define SSL_OP_NO_COMPRESSION 0
#define SSL_OP_NO_RENEGOTIATION 0  // ssl_renegotiate_never is the default
#define SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION 0
#define SSL_OP_NO_SSLv2 0
#define SSL_OP_NO_SSLv3 0
#define SSL_OP_PKCS1_CHECK_1 0
#define SSL_OP_PKCS1_CHECK_2 0
#define SSL_OP_SINGLE_DH_USE 0
#define SSL_OP_SINGLE_ECDH_USE 0
#define SSL_OP_SSLEAY_080_CLIENT_DH_BUG 0
#define SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG 0
#define SSL_OP_TLS_BLOCK_PADDING_BUG 0
#define SSL_OP_TLS_D5_BUG 0
#define SSL_OP_TLS_ROLLBACK_BUG 0
#define SSL_VERIFY_CLIENT_ONCE 0

OPENSSL_EXPORT int SSL_cache_hit(SSL *ssl);

OPENSSL_EXPORT long SSL_get_default_timeout(const SSL *ssl);

OPENSSL_EXPORT const char *SSL_get_version(const SSL *ssl);

OPENSSL_EXPORT size_t SSL_get_all_version_names(const char **out,
                                                size_t max_out);

OPENSSL_EXPORT const char *SSL_get_cipher_list(const SSL *ssl, int n);

OPENSSL_EXPORT void SSL_CTX_set_client_cert_cb(
    SSL_CTX *ctx, int (*cb)(SSL *ssl, X509 **out_x509, EVP_PKEY **out_pkey));

#define SSL_NOTHING SSL_ERROR_NONE
#define SSL_WRITING SSL_ERROR_WANT_WRITE
#define SSL_READING SSL_ERROR_WANT_READ

OPENSSL_EXPORT int SSL_want(const SSL *ssl);

#define SSL_want_read(ssl) (SSL_want(ssl) == SSL_READING)
#define SSL_want_write(ssl) (SSL_want(ssl) == SSL_WRITING)

OPENSSL_EXPORT size_t SSL_get_finished(const SSL *ssl, void *buf, size_t count);

OPENSSL_EXPORT size_t SSL_get_peer_finished(const SSL *ssl, void *buf,
                                            size_t count);

OPENSSL_EXPORT const char *SSL_alert_type_string(int value);

OPENSSL_EXPORT const char *SSL_alert_desc_string(int value);

OPENSSL_EXPORT const char *SSL_state_string(const SSL *ssl);

#define SSL_TXT_MEDIUM "MEDIUM"
#define SSL_TXT_HIGH "HIGH"
#define SSL_TXT_FIPS "FIPS"
#define SSL_TXT_kRSA "kRSA"
#define SSL_TXT_kDHE "kDHE"
#define SSL_TXT_kEDH "kEDH"
#define SSL_TXT_kECDHE "kECDHE"
#define SSL_TXT_kEECDH "kEECDH"
#define SSL_TXT_kPSK "kPSK"
#define SSL_TXT_aRSA "aRSA"
#define SSL_TXT_aECDSA "aECDSA"
#define SSL_TXT_aPSK "aPSK"
#define SSL_TXT_DH "DH"
#define SSL_TXT_DHE "DHE"
#define SSL_TXT_EDH "EDH"
#define SSL_TXT_RSA "RSA"
#define SSL_TXT_ECDH "ECDH"
#define SSL_TXT_ECDHE "ECDHE"
#define SSL_TXT_EECDH "EECDH"
#define SSL_TXT_ECDSA "ECDSA"
#define SSL_TXT_PSK "PSK"
#define SSL_TXT_3DES "3DES"
#define SSL_TXT_RC4 "RC4"
#define SSL_TXT_AES128 "AES128"
#define SSL_TXT_AES256 "AES256"
#define SSL_TXT_AES "AES"
#define SSL_TXT_AES_GCM "AESGCM"
#define SSL_TXT_CHACHA20 "CHACHA20"
#define SSL_TXT_MD5 "MD5"
#define SSL_TXT_SHA1 "SHA1"
#define SSL_TXT_SHA "SHA"
#define SSL_TXT_SHA256 "SHA256"
#define SSL_TXT_SHA384 "SHA384"
#define SSL_TXT_SSLV3 "SSLv3"
#define SSL_TXT_TLSV1 "TLSv1"
#define SSL_TXT_TLSV1_1 "TLSv1.1"
#define SSL_TXT_TLSV1_2 "TLSv1.2"
#define SSL_TXT_TLSV1_3 "TLSv1.3"
#define SSL_TXT_ALL "ALL"
#define SSL_TXT_CMPDEF "COMPLEMENTOFDEFAULT"

typedef struct ssl_conf_ctx_st SSL_CONF_CTX;

OPENSSL_EXPORT int SSL_state(const SSL *ssl);

#define SSL_get_state(ssl) SSL_state(ssl)

OPENSSL_EXPORT void SSL_set_shutdown(SSL *ssl, int mode);

OPENSSL_EXPORT int SSL_CTX_set_tmp_ecdh(SSL_CTX *ctx, const EC_KEY *ec_key);

OPENSSL_EXPORT int SSL_set_tmp_ecdh(SSL *ssl, const EC_KEY *ec_key);

#if !defined(OPENSSL_NO_FILESYSTEM)

OPENSSL_EXPORT int SSL_add_dir_cert_subjects_to_stack(STACK_OF(X509_NAME) *out,
                                                      const char *dir);
#endif

OPENSSL_EXPORT int SSL_CTX_enable_tls_channel_id(SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_enable_tls_channel_id(SSL *ssl);

OPENSSL_EXPORT const BIO_METHOD *BIO_f_ssl(void);

OPENSSL_EXPORT long BIO_set_ssl(BIO *bio, SSL *ssl, int take_owership);

#define SSL_CTX_set_ecdh_auto(ctx, onoff) 1

#define SSL_set_ecdh_auto(ssl, onoff) 1

OPENSSL_EXPORT SSL_SESSION *SSL_get_session(const SSL *ssl);

#define SSL_get0_session SSL_get_session

OPENSSL_EXPORT SSL_SESSION *SSL_get1_session(SSL *ssl);

#define OPENSSL_INIT_NO_LOAD_SSL_STRINGS 0
#define OPENSSL_INIT_LOAD_SSL_STRINGS 0
#define OPENSSL_INIT_SSL_DEFAULT 0

OPENSSL_EXPORT int OPENSSL_init_ssl(uint64_t opts,
                                    const OPENSSL_INIT_SETTINGS *settings);

#define SSL_SIGN_RSA_PSS_SHA256 SSL_SIGN_RSA_PSS_RSAE_SHA256
#define SSL_SIGN_RSA_PSS_SHA384 SSL_SIGN_RSA_PSS_RSAE_SHA384
#define SSL_SIGN_RSA_PSS_SHA512 SSL_SIGN_RSA_PSS_RSAE_SHA512

OPENSSL_EXPORT int SSL_set_tlsext_status_type(SSL *ssl, int type);

OPENSSL_EXPORT int SSL_get_tlsext_status_type(const SSL *ssl);

OPENSSL_EXPORT int SSL_set_tlsext_status_ocsp_resp(SSL *ssl, uint8_t *resp,
                                                   size_t resp_len);

OPENSSL_EXPORT size_t SSL_get_tlsext_status_ocsp_resp(const SSL *ssl,
                                                      const uint8_t **out);

OPENSSL_EXPORT int SSL_CTX_set_tlsext_status_cb(SSL_CTX *ctx,
                                                int (*callback)(SSL *ssl,
                                                                void *arg));

OPENSSL_EXPORT int SSL_CTX_set_tlsext_status_arg(SSL_CTX *ctx, void *arg);

#define SSL_R_TLSV1_UNSUPPORTED_EXTENSION \
  SSL_R_TLSV1_ALERT_UNSUPPORTED_EXTENSION
#define SSL_R_TLSV1_CERTIFICATE_UNOBTAINABLE \
  SSL_R_TLSV1_ALERT_CERTIFICATE_UNOBTAINABLE
#define SSL_R_TLSV1_UNRECOGNIZED_NAME SSL_R_TLSV1_ALERT_UNRECOGNIZED_NAME
#define SSL_R_TLSV1_BAD_CERTIFICATE_STATUS_RESPONSE \
  SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE
#define SSL_R_TLSV1_BAD_CERTIFICATE_HASH_VALUE \
  SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_HASH_VALUE
#define SSL_R_TLSV1_CERTIFICATE_REQUIRED SSL_R_TLSV1_ALERT_CERTIFICATE_REQUIRED

#define SSL_CURVE_SECP224R1 SSL_GROUP_SECP224R1
#define SSL_CURVE_SECP256R1 SSL_GROUP_SECP256R1
#define SSL_CURVE_SECP384R1 SSL_GROUP_SECP384R1
#define SSL_CURVE_SECP521R1 SSL_GROUP_SECP521R1
#define SSL_CURVE_X25519 SSL_GROUP_X25519
#define SSL_CURVE_X25519_KYBER768_DRAFT00 SSL_GROUP_X25519_KYBER768_DRAFT00

OPENSSL_EXPORT uint16_t SSL_get_curve_id(const SSL *ssl);

OPENSSL_EXPORT const char *SSL_get_curve_name(uint16_t curve_id);

OPENSSL_EXPORT size_t SSL_get_all_curve_names(const char **out, size_t max_out);

OPENSSL_EXPORT int SSL_CTX_set1_curves(SSL_CTX *ctx, const int *curves,
                                       size_t num_curves);

OPENSSL_EXPORT int SSL_set1_curves(SSL *ssl, const int *curves,
                                   size_t num_curves);

OPENSSL_EXPORT int SSL_CTX_set1_curves_list(SSL_CTX *ctx, const char *curves);

OPENSSL_EXPORT int SSL_set1_curves_list(SSL *ssl, const char *curves);

#define TLSEXT_nid_unknown 0x1000000

OPENSSL_EXPORT int SSL_CTX_check_private_key(const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_check_private_key(const SSL *ssl);

enum ssl_compliance_policy_t BORINGSSL_ENUM_INT {

  ssl_compliance_policy_none,

  ssl_compliance_policy_fips_202205,

  ssl_compliance_policy_wpa3_192_202304,

  ssl_compliance_policy_cnsa_202407,
};

OPENSSL_EXPORT int SSL_CTX_set_compliance_policy(
    SSL_CTX *ctx, enum ssl_compliance_policy_t policy);

OPENSSL_EXPORT enum ssl_compliance_policy_t SSL_CTX_get_compliance_policy(
    const SSL_CTX *ctx);

OPENSSL_EXPORT int SSL_set_compliance_policy(
    SSL *ssl, enum ssl_compliance_policy_t policy);

OPENSSL_EXPORT enum ssl_compliance_policy_t SSL_get_compliance_policy(
    const SSL *ssl);

#define SSLerr(function, reason) \
  ERR_put_error(ERR_LIB_SSL, 0, reason, __FILE__, __LINE__)

#define DTLS_CTRL_GET_TIMEOUT doesnt_exist
#define DTLS_CTRL_HANDLE_TIMEOUT doesnt_exist
#define SSL_CTRL_CHAIN doesnt_exist
#define SSL_CTRL_CHAIN_CERT doesnt_exist
#define SSL_CTRL_CHANNEL_ID doesnt_exist
#define SSL_CTRL_CLEAR_EXTRA_CHAIN_CERTS doesnt_exist
#define SSL_CTRL_CLEAR_MODE doesnt_exist
#define SSL_CTRL_CLEAR_OPTIONS doesnt_exist
#define SSL_CTRL_EXTRA_CHAIN_CERT doesnt_exist
#define SSL_CTRL_GET_CHAIN_CERTS doesnt_exist
#define SSL_CTRL_GET_CHANNEL_ID doesnt_exist
#define SSL_CTRL_GET_CLIENT_CERT_TYPES doesnt_exist
#define SSL_CTRL_GET_EXTRA_CHAIN_CERTS doesnt_exist
#define SSL_CTRL_GET_MAX_CERT_LIST doesnt_exist
#define SSL_CTRL_GET_NEGOTIATED_GROUP doesnt_exist
#define SSL_CTRL_GET_NUM_RENEGOTIATIONS doesnt_exist
#define SSL_CTRL_GET_READ_AHEAD doesnt_exist
#define SSL_CTRL_GET_RI_SUPPORT doesnt_exist
#define SSL_CTRL_GET_SERVER_TMP_KEY doesnt_exist
#define SSL_CTRL_GET_SESSION_REUSED doesnt_exist
#define SSL_CTRL_GET_SESS_CACHE_MODE doesnt_exist
#define SSL_CTRL_GET_SESS_CACHE_SIZE doesnt_exist
#define SSL_CTRL_GET_TLSEXT_TICKET_KEYS doesnt_exist
#define SSL_CTRL_GET_TOTAL_RENEGOTIATIONS doesnt_exist
#define SSL_CTRL_MODE doesnt_exist
#define SSL_CTRL_NEED_TMP_RSA doesnt_exist
#define SSL_CTRL_OPTIONS doesnt_exist
#define SSL_CTRL_SESS_NUMBER doesnt_exist
#define SSL_CTRL_SET_CURVES doesnt_exist
#define SSL_CTRL_SET_CURVES_LIST doesnt_exist
#define SSL_CTRL_SET_GROUPS doesnt_exist
#define SSL_CTRL_SET_GROUPS_LIST doesnt_exist
#define SSL_CTRL_SET_ECDH_AUTO doesnt_exist
#define SSL_CTRL_SET_MAX_CERT_LIST doesnt_exist
#define SSL_CTRL_SET_MAX_SEND_FRAGMENT doesnt_exist
#define SSL_CTRL_SET_MSG_CALLBACK doesnt_exist
#define SSL_CTRL_SET_MSG_CALLBACK_ARG doesnt_exist
#define SSL_CTRL_SET_MTU doesnt_exist
#define SSL_CTRL_SET_READ_AHEAD doesnt_exist
#define SSL_CTRL_SET_SESS_CACHE_MODE doesnt_exist
#define SSL_CTRL_SET_SESS_CACHE_SIZE doesnt_exist
#define SSL_CTRL_SET_TLSEXT_HOSTNAME doesnt_exist
#define SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG doesnt_exist
#define SSL_CTRL_SET_TLSEXT_SERVERNAME_CB doesnt_exist
#define SSL_CTRL_SET_TLSEXT_TICKET_KEYS doesnt_exist
#define SSL_CTRL_SET_TLSEXT_TICKET_KEY_CB doesnt_exist
#define SSL_CTRL_SET_TMP_DH doesnt_exist
#define SSL_CTRL_SET_TMP_DH_CB doesnt_exist
#define SSL_CTRL_SET_TMP_ECDH doesnt_exist
#define SSL_CTRL_SET_TMP_ECDH_CB doesnt_exist
#define SSL_CTRL_SET_TMP_RSA doesnt_exist
#define SSL_CTRL_SET_TMP_RSA_CB doesnt_exist

#if !defined(BORINGSSL_PREFIX)

#define DTLSv1_get_timeout DTLSv1_get_timeout
#define DTLSv1_handle_timeout DTLSv1_handle_timeout
#define SSL_CTX_add0_chain_cert SSL_CTX_add0_chain_cert
#define SSL_CTX_add1_chain_cert SSL_CTX_add1_chain_cert
#define SSL_CTX_add_extra_chain_cert SSL_CTX_add_extra_chain_cert
#define SSL_CTX_clear_extra_chain_certs SSL_CTX_clear_extra_chain_certs
#define SSL_CTX_clear_chain_certs SSL_CTX_clear_chain_certs
#define SSL_CTX_clear_mode SSL_CTX_clear_mode
#define SSL_CTX_clear_options SSL_CTX_clear_options
#define SSL_CTX_get0_chain_certs SSL_CTX_get0_chain_certs
#define SSL_CTX_get_extra_chain_certs SSL_CTX_get_extra_chain_certs
#define SSL_CTX_get_max_cert_list SSL_CTX_get_max_cert_list
#define SSL_CTX_get_mode SSL_CTX_get_mode
#define SSL_CTX_get_options SSL_CTX_get_options
#define SSL_CTX_get_read_ahead SSL_CTX_get_read_ahead
#define SSL_CTX_get_session_cache_mode SSL_CTX_get_session_cache_mode
#define SSL_CTX_get_tlsext_ticket_keys SSL_CTX_get_tlsext_ticket_keys
#define SSL_CTX_need_tmp_RSA SSL_CTX_need_tmp_RSA
#define SSL_CTX_sess_get_cache_size SSL_CTX_sess_get_cache_size
#define SSL_CTX_sess_number SSL_CTX_sess_number
#define SSL_CTX_sess_set_cache_size SSL_CTX_sess_set_cache_size
#define SSL_CTX_set0_chain SSL_CTX_set0_chain
#define SSL_CTX_set1_chain SSL_CTX_set1_chain
#define SSL_CTX_set1_curves SSL_CTX_set1_curves
#define SSL_CTX_set1_groups SSL_CTX_set1_groups
#define SSL_CTX_set_max_cert_list SSL_CTX_set_max_cert_list
#define SSL_CTX_set_max_send_fragment SSL_CTX_set_max_send_fragment
#define SSL_CTX_set_mode SSL_CTX_set_mode
#define SSL_CTX_set_msg_callback_arg SSL_CTX_set_msg_callback_arg
#define SSL_CTX_set_options SSL_CTX_set_options
#define SSL_CTX_set_read_ahead SSL_CTX_set_read_ahead
#define SSL_CTX_set_session_cache_mode SSL_CTX_set_session_cache_mode
#define SSL_CTX_set_tlsext_servername_arg SSL_CTX_set_tlsext_servername_arg
#define SSL_CTX_set_tlsext_servername_callback \
  SSL_CTX_set_tlsext_servername_callback
#define SSL_CTX_set_tlsext_ticket_key_cb SSL_CTX_set_tlsext_ticket_key_cb
#define SSL_CTX_set_tlsext_ticket_keys SSL_CTX_set_tlsext_ticket_keys
#define SSL_CTX_set_tmp_dh SSL_CTX_set_tmp_dh
#define SSL_CTX_set_tmp_ecdh SSL_CTX_set_tmp_ecdh
#define SSL_CTX_set_tmp_rsa SSL_CTX_set_tmp_rsa
#define SSL_add0_chain_cert SSL_add0_chain_cert
#define SSL_add1_chain_cert SSL_add1_chain_cert
#define SSL_clear_chain_certs SSL_clear_chain_certs
#define SSL_clear_mode SSL_clear_mode
#define SSL_clear_options SSL_clear_options
#define SSL_get0_certificate_types SSL_get0_certificate_types
#define SSL_get0_chain_certs SSL_get0_chain_certs
#define SSL_get_max_cert_list SSL_get_max_cert_list
#define SSL_get_mode SSL_get_mode
#define SSL_get_negotiated_group SSL_get_negotiated_group
#define SSL_get_options SSL_get_options
#define SSL_get_secure_renegotiation_support \
  SSL_get_secure_renegotiation_support
#define SSL_need_tmp_RSA SSL_need_tmp_RSA
#define SSL_num_renegotiations SSL_num_renegotiations
#define SSL_session_reused SSL_session_reused
#define SSL_set0_chain SSL_set0_chain
#define SSL_set1_chain SSL_set1_chain
#define SSL_set1_curves SSL_set1_curves
#define SSL_set1_groups SSL_set1_groups
#define SSL_set_max_cert_list SSL_set_max_cert_list
#define SSL_set_max_send_fragment SSL_set_max_send_fragment
#define SSL_set_mode SSL_set_mode
#define SSL_set_msg_callback_arg SSL_set_msg_callback_arg
#define SSL_set_mtu SSL_set_mtu
#define SSL_set_options SSL_set_options
#define SSL_set_tlsext_host_name SSL_set_tlsext_host_name
#define SSL_set_tmp_dh SSL_set_tmp_dh
#define SSL_set_tmp_ecdh SSL_set_tmp_ecdh
#define SSL_set_tmp_rsa SSL_set_tmp_rsa
#define SSL_total_renegotiations SSL_total_renegotiations

#endif

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(SSL, SSL_free)
BORINGSSL_MAKE_DELETER(SSL_CREDENTIAL, SSL_CREDENTIAL_free)
BORINGSSL_MAKE_UP_REF(SSL_CREDENTIAL, SSL_CREDENTIAL_up_ref)
BORINGSSL_MAKE_DELETER(SSL_CTX, SSL_CTX_free)
BORINGSSL_MAKE_UP_REF(SSL_CTX, SSL_CTX_up_ref)
BORINGSSL_MAKE_DELETER(SSL_ECH_KEYS, SSL_ECH_KEYS_free)
BORINGSSL_MAKE_UP_REF(SSL_ECH_KEYS, SSL_ECH_KEYS_up_ref)
BORINGSSL_MAKE_DELETER(SSL_SESSION, SSL_SESSION_free)
BORINGSSL_MAKE_UP_REF(SSL_SESSION, SSL_SESSION_up_ref)

OPENSSL_EXPORT void SSL_CTX_set_handoff_mode(SSL_CTX *ctx, bool on);
OPENSSL_EXPORT void SSL_set_handoff_mode(SSL *SSL, bool on);
OPENSSL_EXPORT bool SSL_serialize_handoff(const SSL *ssl, CBB *out,
                                          SSL_CLIENT_HELLO *out_hello);
OPENSSL_EXPORT bool SSL_decline_handoff(SSL *ssl);
OPENSSL_EXPORT bool SSL_apply_handoff(SSL *ssl, Span<const uint8_t> handoff);
OPENSSL_EXPORT bool SSL_serialize_handback(const SSL *ssl, CBB *out);
OPENSSL_EXPORT bool SSL_apply_handback(SSL *ssl, Span<const uint8_t> handback);

OPENSSL_EXPORT bool SSL_get_traffic_secrets(
    const SSL *ssl, Span<const uint8_t> *out_read_traffic_secret,
    Span<const uint8_t> *out_write_traffic_secret);

OPENSSL_EXPORT void SSL_CTX_set_aes_hw_override_for_testing(
    SSL_CTX *ctx, bool override_value);

OPENSSL_EXPORT void SSL_set_aes_hw_override_for_testing(SSL *ssl,
                                                        bool override_value);

BSSL_NAMESPACE_END

}

#endif

#endif

#define SSL_R_APP_DATA_IN_HANDSHAKE 100
#define SSL_R_ATTEMPT_TO_REUSE_SESSION_IN_DIFFERENT_CONTEXT 101
#define SSL_R_BAD_ALERT 102
#define SSL_R_BAD_CHANGE_CIPHER_SPEC 103
#define SSL_R_BAD_DATA_RETURNED_BY_CALLBACK 104
#define SSL_R_BAD_DH_P_LENGTH 105
#define SSL_R_BAD_DIGEST_LENGTH 106
#define SSL_R_BAD_ECC_CERT 107
#define SSL_R_BAD_ECPOINT 108
#define SSL_R_BAD_HANDSHAKE_RECORD 109
#define SSL_R_BAD_HELLO_REQUEST 110
#define SSL_R_BAD_LENGTH 111
#define SSL_R_BAD_PACKET_LENGTH 112
#define SSL_R_BAD_RSA_ENCRYPT 113
#define SSL_R_BAD_SIGNATURE 114
#define SSL_R_BAD_SRTP_MKI_VALUE 115
#define SSL_R_BAD_SRTP_PROTECTION_PROFILE_LIST 116
#define SSL_R_BAD_SSL_FILETYPE 117
#define SSL_R_BAD_WRITE_RETRY 118
#define SSL_R_BIO_NOT_SET 119
#define SSL_R_BN_LIB 120
#define SSL_R_BUFFER_TOO_SMALL 121
#define SSL_R_CA_DN_LENGTH_MISMATCH 122
#define SSL_R_CA_DN_TOO_LONG 123
#define SSL_R_CCS_RECEIVED_EARLY 124
#define SSL_R_CERTIFICATE_VERIFY_FAILED 125
#define SSL_R_CERT_CB_ERROR 126
#define SSL_R_CERT_LENGTH_MISMATCH 127
#define SSL_R_CHANNEL_ID_NOT_P256 128
#define SSL_R_CHANNEL_ID_SIGNATURE_INVALID 129
#define SSL_R_CIPHER_OR_HASH_UNAVAILABLE 130
#define SSL_R_CLIENTHELLO_PARSE_FAILED 131
#define SSL_R_CLIENTHELLO_TLSEXT 132
#define SSL_R_CONNECTION_REJECTED 133
#define SSL_R_CONNECTION_TYPE_NOT_SET 134
#define SSL_R_CUSTOM_EXTENSION_ERROR 135
#define SSL_R_DATA_LENGTH_TOO_LONG 136
#define SSL_R_DECODE_ERROR 137
#define SSL_R_DECRYPTION_FAILED 138
#define SSL_R_DECRYPTION_FAILED_OR_BAD_RECORD_MAC 139
#define SSL_R_DH_PUBLIC_VALUE_LENGTH_IS_WRONG 140
#define SSL_R_DH_P_TOO_LONG 141
#define SSL_R_DIGEST_CHECK_FAILED 142
#define SSL_R_DTLS_MESSAGE_TOO_BIG 143
#define SSL_R_ECC_CERT_NOT_FOR_SIGNING 144
#define SSL_R_EMS_STATE_INCONSISTENT 145
#define SSL_R_ENCRYPTED_LENGTH_TOO_LONG 146
#define SSL_R_ERROR_ADDING_EXTENSION 147
#define SSL_R_ERROR_IN_RECEIVED_CIPHER_LIST 148
#define SSL_R_ERROR_PARSING_EXTENSION 149
#define SSL_R_EXCESSIVE_MESSAGE_SIZE 150
#define SSL_R_EXTRA_DATA_IN_MESSAGE 151
#define SSL_R_FRAGMENT_MISMATCH 152
#define SSL_R_GOT_NEXT_PROTO_WITHOUT_EXTENSION 153
#define SSL_R_HANDSHAKE_FAILURE_ON_CLIENT_HELLO 154
#define SSL_R_HTTPS_PROXY_REQUEST 155
#define SSL_R_HTTP_REQUEST 156
#define SSL_R_INAPPROPRIATE_FALLBACK 157
#define SSL_R_INVALID_COMMAND 158
#define SSL_R_INVALID_MESSAGE 159
#define SSL_R_INVALID_SSL_SESSION 160
#define SSL_R_INVALID_TICKET_KEYS_LENGTH 161
#define SSL_R_LENGTH_MISMATCH 162
#define SSL_R_MISSING_EXTENSION 164
#define SSL_R_MISSING_RSA_CERTIFICATE 165
#define SSL_R_MISSING_TMP_DH_KEY 166
#define SSL_R_MISSING_TMP_ECDH_KEY 167
#define SSL_R_MIXED_SPECIAL_OPERATOR_WITH_GROUPS 168
#define SSL_R_MTU_TOO_SMALL 169
#define SSL_R_NEGOTIATED_BOTH_NPN_AND_ALPN 170
#define SSL_R_NESTED_GROUP 171
#define SSL_R_NO_CERTIFICATES_RETURNED 172
#define SSL_R_NO_CERTIFICATE_ASSIGNED 173
#define SSL_R_NO_CERTIFICATE_SET 174
#define SSL_R_NO_CIPHERS_AVAILABLE 175
#define SSL_R_NO_CIPHERS_PASSED 176
#define SSL_R_NO_CIPHER_MATCH 177
#define SSL_R_NO_COMPRESSION_SPECIFIED 178
#define SSL_R_NO_METHOD_SPECIFIED 179
#define SSL_R_NO_PRIVATE_KEY_ASSIGNED 181
#define SSL_R_NO_RENEGOTIATION 182
#define SSL_R_NO_REQUIRED_DIGEST 183
#define SSL_R_NO_SHARED_CIPHER 184
#define SSL_R_NULL_SSL_CTX 185
#define SSL_R_NULL_SSL_METHOD_PASSED 186
#define SSL_R_OLD_SESSION_CIPHER_NOT_RETURNED 187
#define SSL_R_OLD_SESSION_VERSION_NOT_RETURNED 188
#define SSL_R_OUTPUT_ALIASES_INPUT 189
#define SSL_R_PARSE_TLSEXT 190
#define SSL_R_PATH_TOO_LONG 191
#define SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE 192
#define SSL_R_PEER_ERROR_UNSUPPORTED_CERTIFICATE_TYPE 193
#define SSL_R_PROTOCOL_IS_SHUTDOWN 194
#define SSL_R_PSK_IDENTITY_NOT_FOUND 195
#define SSL_R_PSK_NO_CLIENT_CB 196
#define SSL_R_PSK_NO_SERVER_CB 197
#define SSL_R_READ_TIMEOUT_EXPIRED 198
#define SSL_R_RECORD_LENGTH_MISMATCH 199
#define SSL_R_RECORD_TOO_LARGE 200
#define SSL_R_RENEGOTIATION_ENCODING_ERR 201
#define SSL_R_RENEGOTIATION_MISMATCH 202
#define SSL_R_REQUIRED_CIPHER_MISSING 203
#define SSL_R_RESUMED_EMS_SESSION_WITHOUT_EMS_EXTENSION 204
#define SSL_R_RESUMED_NON_EMS_SESSION_WITH_EMS_EXTENSION 205
#define SSL_R_SCSV_RECEIVED_WHEN_RENEGOTIATING 206
#define SSL_R_SERVERHELLO_TLSEXT 207
#define SSL_R_SESSION_ID_CONTEXT_UNINITIALIZED 208
#define SSL_R_SESSION_MAY_NOT_BE_CREATED 209
#define SSL_R_SIGNATURE_ALGORITHMS_EXTENSION_SENT_BY_SERVER 210
#define SSL_R_SRTP_COULD_NOT_ALLOCATE_PROFILES 211
#define SSL_R_SRTP_UNKNOWN_PROTECTION_PROFILE 212
#define SSL_R_SSL3_EXT_INVALID_SERVERNAME 213
#define SSL_R_SSL_CTX_HAS_NO_DEFAULT_SSL_VERSION 214
#define SSL_R_SSL_HANDSHAKE_FAILURE 215
#define SSL_R_SSL_SESSION_ID_CONTEXT_TOO_LONG 216
#define SSL_R_TLS_PEER_DID_NOT_RESPOND_WITH_CERTIFICATE_LIST 217
#define SSL_R_TLS_RSA_ENCRYPTED_VALUE_LENGTH_IS_WRONG 218
#define SSL_R_TOO_MANY_EMPTY_FRAGMENTS 219
#define SSL_R_TOO_MANY_WARNING_ALERTS 220
#define SSL_R_UNABLE_TO_FIND_ECDH_PARAMETERS 221
#define SSL_R_UNEXPECTED_EXTENSION 222
#define SSL_R_UNEXPECTED_MESSAGE 223
#define SSL_R_UNEXPECTED_OPERATOR_IN_GROUP 224
#define SSL_R_UNEXPECTED_RECORD 225
#define SSL_R_UNINITIALIZED 226
#define SSL_R_UNKNOWN_ALERT_TYPE 227
#define SSL_R_UNKNOWN_CERTIFICATE_TYPE 228
#define SSL_R_UNKNOWN_CIPHER_RETURNED 229
#define SSL_R_UNKNOWN_CIPHER_TYPE 230
#define SSL_R_UNKNOWN_DIGEST 231
#define SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE 232
#define SSL_R_UNKNOWN_PROTOCOL 233
#define SSL_R_UNKNOWN_SSL_VERSION 234
#define SSL_R_UNKNOWN_STATE 235
#define SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED 236
#define SSL_R_UNSUPPORTED_CIPHER 237
#define SSL_R_UNSUPPORTED_COMPRESSION_ALGORITHM 238
#define SSL_R_UNSUPPORTED_ELLIPTIC_CURVE 239
#define SSL_R_UNSUPPORTED_PROTOCOL 240
#define SSL_R_WRONG_CERTIFICATE_TYPE 241
#define SSL_R_WRONG_CIPHER_RETURNED 242
#define SSL_R_WRONG_CURVE 243
#define SSL_R_WRONG_MESSAGE_TYPE 244
#define SSL_R_WRONG_SIGNATURE_TYPE 245
#define SSL_R_WRONG_SSL_VERSION 246
#define SSL_R_WRONG_VERSION_NUMBER 247
#define SSL_R_X509_LIB 248
#define SSL_R_X509_VERIFICATION_SETUP_PROBLEMS 249
#define SSL_R_SHUTDOWN_WHILE_IN_INIT 250
#define SSL_R_INVALID_OUTER_RECORD_TYPE 251
#define SSL_R_UNSUPPORTED_PROTOCOL_FOR_CUSTOM_KEY 252
#define SSL_R_NO_COMMON_SIGNATURE_ALGORITHMS 253
#define SSL_R_DOWNGRADE_DETECTED 254
#define SSL_R_EXCESS_HANDSHAKE_DATA 255
#define SSL_R_INVALID_COMPRESSION_LIST 256
#define SSL_R_DUPLICATE_EXTENSION 257
#define SSL_R_MISSING_KEY_SHARE 258
#define SSL_R_INVALID_ALPN_PROTOCOL 259
#define SSL_R_TOO_MANY_KEY_UPDATES 260
#define SSL_R_BLOCK_CIPHER_PAD_IS_WRONG 261
#define SSL_R_NO_CIPHERS_SPECIFIED 262
#define SSL_R_RENEGOTIATION_EMS_MISMATCH 263
#define SSL_R_DUPLICATE_KEY_SHARE 264
#define SSL_R_NO_GROUPS_SPECIFIED 265
#define SSL_R_NO_SHARED_GROUP 266
#define SSL_R_PRE_SHARED_KEY_MUST_BE_LAST 267
#define SSL_R_OLD_SESSION_PRF_HASH_MISMATCH 268
#define SSL_R_INVALID_SCT_LIST 269
#define SSL_R_TOO_MUCH_SKIPPED_EARLY_DATA 270
#define SSL_R_PSK_IDENTITY_BINDER_COUNT_MISMATCH 271
#define SSL_R_CANNOT_PARSE_LEAF_CERT 272
#define SSL_R_SERVER_CERT_CHANGED 273
#define SSL_R_CERTIFICATE_AND_PRIVATE_KEY_MISMATCH 274
#define SSL_R_CANNOT_HAVE_BOTH_PRIVKEY_AND_METHOD 275
#define SSL_R_TICKET_ENCRYPTION_FAILED 276
#define SSL_R_ALPN_MISMATCH_ON_EARLY_DATA 277
#define SSL_R_WRONG_VERSION_ON_EARLY_DATA 278
#define SSL_R_UNEXPECTED_EXTENSION_ON_EARLY_DATA 279
#define SSL_R_NO_SUPPORTED_VERSIONS_ENABLED 280
#define SSL_R_EMPTY_HELLO_RETRY_REQUEST 282
#define SSL_R_EARLY_DATA_NOT_IN_USE 283
#define SSL_R_HANDSHAKE_NOT_COMPLETE 284
#define SSL_R_NEGOTIATED_TB_WITHOUT_EMS_OR_RI 285
#define SSL_R_SERVER_ECHOED_INVALID_SESSION_ID 286
#define SSL_R_PRIVATE_KEY_OPERATION_FAILED 287
#define SSL_R_SECOND_SERVERHELLO_VERSION_MISMATCH 288
#define SSL_R_OCSP_CB_ERROR 289
#define SSL_R_SSL_SESSION_ID_TOO_LONG 290
#define SSL_R_APPLICATION_DATA_ON_SHUTDOWN 291
#define SSL_R_CERT_DECOMPRESSION_FAILED 292
#define SSL_R_UNCOMPRESSED_CERT_TOO_LARGE 293
#define SSL_R_UNKNOWN_CERT_COMPRESSION_ALG 294
#define SSL_R_INVALID_SIGNATURE_ALGORITHM 295
#define SSL_R_DUPLICATE_SIGNATURE_ALGORITHM 296
#define SSL_R_TLS13_DOWNGRADE 297
#define SSL_R_QUIC_INTERNAL_ERROR 298
#define SSL_R_WRONG_ENCRYPTION_LEVEL_RECEIVED 299
#define SSL_R_TOO_MUCH_READ_EARLY_DATA 300
#define SSL_R_INVALID_DELEGATED_CREDENTIAL 301
#define SSL_R_KEY_USAGE_BIT_INCORRECT 302
#define SSL_R_INCONSISTENT_CLIENT_HELLO 303
#define SSL_R_CIPHER_MISMATCH_ON_EARLY_DATA 304
#define SSL_R_QUIC_TRANSPORT_PARAMETERS_MISCONFIGURED 305
#define SSL_R_UNEXPECTED_COMPATIBILITY_MODE 306
#define SSL_R_NO_APPLICATION_PROTOCOL 307
#define SSL_R_NEGOTIATED_ALPS_WITHOUT_ALPN 308
#define SSL_R_ALPS_MISMATCH_ON_EARLY_DATA 309
#define SSL_R_ECH_SERVER_CONFIG_AND_PRIVATE_KEY_MISMATCH 310
#define SSL_R_ECH_SERVER_CONFIG_UNSUPPORTED_EXTENSION 311
#define SSL_R_UNSUPPORTED_ECH_SERVER_CONFIG 312
#define SSL_R_ECH_SERVER_WOULD_HAVE_NO_RETRY_CONFIGS 313
#define SSL_R_INVALID_CLIENT_HELLO_INNER 314
#define SSL_R_INVALID_ALPN_PROTOCOL_LIST 315
#define SSL_R_COULD_NOT_PARSE_HINTS 316
#define SSL_R_INVALID_ECH_PUBLIC_NAME 317
#define SSL_R_INVALID_ECH_CONFIG_LIST 318
#define SSL_R_ECH_REJECTED 319
#define SSL_R_INVALID_OUTER_EXTENSION 320
#define SSL_R_INCONSISTENT_ECH_NEGOTIATION 321
#define SSL_R_INVALID_ALPS_CODEPOINT 322
#define SSL_R_NO_MATCHING_ISSUER 323
#define SSL_R_INVALID_SPAKE2PLUSV1_VALUE 324
#define SSL_R_PAKE_EXHAUSTED 325
#define SSL_R_PEER_PAKE_MISMATCH 326
#define SSL_R_UNSUPPORTED_CREDENTIAL_LIST 327
#define SSL_R_INVALID_TRUST_ANCHOR_LIST 328
#define SSL_R_INVALID_CERTIFICATE_PROPERTY_LIST 329
#define SSL_R_SSLV3_ALERT_CLOSE_NOTIFY 1000
#define SSL_R_SSLV3_ALERT_UNEXPECTED_MESSAGE 1010
#define SSL_R_SSLV3_ALERT_BAD_RECORD_MAC 1020
#define SSL_R_TLSV1_ALERT_DECRYPTION_FAILED 1021
#define SSL_R_TLSV1_ALERT_RECORD_OVERFLOW 1022
#define SSL_R_SSLV3_ALERT_DECOMPRESSION_FAILURE 1030
#define SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE 1040
#define SSL_R_SSLV3_ALERT_NO_CERTIFICATE 1041
#define SSL_R_SSLV3_ALERT_BAD_CERTIFICATE 1042
#define SSL_R_SSLV3_ALERT_UNSUPPORTED_CERTIFICATE 1043
#define SSL_R_SSLV3_ALERT_CERTIFICATE_REVOKED 1044
#define SSL_R_SSLV3_ALERT_CERTIFICATE_EXPIRED 1045
#define SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN 1046
#define SSL_R_SSLV3_ALERT_ILLEGAL_PARAMETER 1047
#define SSL_R_TLSV1_ALERT_UNKNOWN_CA 1048
#define SSL_R_TLSV1_ALERT_ACCESS_DENIED 1049
#define SSL_R_TLSV1_ALERT_DECODE_ERROR 1050
#define SSL_R_TLSV1_ALERT_DECRYPT_ERROR 1051
#define SSL_R_TLSV1_ALERT_EXPORT_RESTRICTION 1060
#define SSL_R_TLSV1_ALERT_PROTOCOL_VERSION 1070
#define SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY 1071
#define SSL_R_TLSV1_ALERT_INTERNAL_ERROR 1080
#define SSL_R_TLSV1_ALERT_INAPPROPRIATE_FALLBACK 1086
#define SSL_R_TLSV1_ALERT_USER_CANCELLED 1090
#define SSL_R_TLSV1_ALERT_NO_RENEGOTIATION 1100
#define SSL_R_TLSV1_ALERT_UNSUPPORTED_EXTENSION 1110
#define SSL_R_TLSV1_ALERT_CERTIFICATE_UNOBTAINABLE 1111
#define SSL_R_TLSV1_ALERT_UNRECOGNIZED_NAME 1112
#define SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE 1113
#define SSL_R_TLSV1_ALERT_BAD_CERTIFICATE_HASH_VALUE 1114
#define SSL_R_TLSV1_ALERT_UNKNOWN_PSK_IDENTITY 1115
#define SSL_R_TLSV1_ALERT_CERTIFICATE_REQUIRED 1116
#define SSL_R_TLSV1_ALERT_NO_APPLICATION_PROTOCOL 1120
#define SSL_R_TLSV1_ALERT_ECH_REQUIRED 1121
#define SSL_R_PAKE_AND_KEY_SHARE_NOT_ALLOWED 1122

#endif
