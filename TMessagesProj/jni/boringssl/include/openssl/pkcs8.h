// Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_PKCS8_H
#define OPENSSL_HEADER_PKCS8_H

#include <openssl/base.h>
#include <openssl/x509.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT X509_SIG *PKCS8_encrypt(int pbe_nid, const EVP_CIPHER *cipher,
                                       const char *pass, int pass_len,
                                       const uint8_t *salt, size_t salt_len,
                                       int iterations,
                                       PKCS8_PRIV_KEY_INFO *p8inf);

OPENSSL_EXPORT int PKCS8_marshal_encrypted_private_key(
    CBB *out, int pbe_nid, const EVP_CIPHER *cipher, const char *pass,
    size_t pass_len, const uint8_t *salt, size_t salt_len, int iterations,
    const EVP_PKEY *pkey);

OPENSSL_EXPORT PKCS8_PRIV_KEY_INFO *PKCS8_decrypt(X509_SIG *pkcs8,
                                                  const char *pass,
                                                  int pass_len);

OPENSSL_EXPORT EVP_PKEY *PKCS8_parse_encrypted_private_key(CBS *cbs,
                                                           const char *pass,
                                                           size_t pass_len);

OPENSSL_EXPORT int PKCS12_get_key_and_certs(EVP_PKEY **out_key,
                                            STACK_OF(X509) *out_certs,
                                            CBS *in, const char *password);

OPENSSL_EXPORT void PKCS12_PBE_add(void);

OPENSSL_EXPORT PKCS12 *d2i_PKCS12(PKCS12 **out_p12, const uint8_t **ber_bytes,
                                  size_t ber_len);

OPENSSL_EXPORT PKCS12* d2i_PKCS12_bio(BIO *bio, PKCS12 **out_p12);

OPENSSL_EXPORT PKCS12* d2i_PKCS12_fp(FILE *fp, PKCS12 **out_p12);

OPENSSL_EXPORT int i2d_PKCS12(const PKCS12 *p12, uint8_t **out);

OPENSSL_EXPORT int i2d_PKCS12_bio(BIO *bio, const PKCS12 *p12);

OPENSSL_EXPORT int i2d_PKCS12_fp(FILE *fp, const PKCS12 *p12);

OPENSSL_EXPORT int PKCS12_parse(const PKCS12 *p12, const char *password,
                                EVP_PKEY **out_pkey, X509 **out_cert,
                                STACK_OF(X509) **out_ca_certs);

OPENSSL_EXPORT int PKCS12_verify_mac(const PKCS12 *p12, const char *password,
                                     int password_len);

#define PKCS12_DEFAULT_ITER 2048

OPENSSL_EXPORT PKCS12 *PKCS12_create(const char *password, const char *name,
                                     const EVP_PKEY *pkey, X509 *cert,
                                     const STACK_OF(X509) *chain, int key_nid,
                                     int cert_nid, int iterations,
                                     int mac_iterations, int key_type);

OPENSSL_EXPORT void PKCS12_free(PKCS12 *p12);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(PKCS12, PKCS12_free)
BORINGSSL_MAKE_DELETER(PKCS8_PRIV_KEY_INFO, PKCS8_PRIV_KEY_INFO_free)

BSSL_NAMESPACE_END

}

#endif

#define PKCS8_R_BAD_PKCS12_DATA 100
#define PKCS8_R_BAD_PKCS12_VERSION 101
#define PKCS8_R_CIPHER_HAS_NO_OBJECT_IDENTIFIER 102
#define PKCS8_R_CRYPT_ERROR 103
#define PKCS8_R_DECODE_ERROR 104
#define PKCS8_R_ENCODE_ERROR 105
#define PKCS8_R_ENCRYPT_ERROR 106
#define PKCS8_R_ERROR_SETTING_CIPHER_PARAMS 107
#define PKCS8_R_INCORRECT_PASSWORD 108
#define PKCS8_R_KEYGEN_FAILURE 109
#define PKCS8_R_KEY_GEN_ERROR 110
#define PKCS8_R_METHOD_NOT_SUPPORTED 111
#define PKCS8_R_MISSING_MAC 112
#define PKCS8_R_MULTIPLE_PRIVATE_KEYS_IN_PKCS12 113
#define PKCS8_R_PKCS12_PUBLIC_KEY_INTEGRITY_NOT_SUPPORTED 114
#define PKCS8_R_PKCS12_TOO_DEEPLY_NESTED 115
#define PKCS8_R_PRIVATE_KEY_DECODE_ERROR 116
#define PKCS8_R_PRIVATE_KEY_ENCODE_ERROR 117
#define PKCS8_R_TOO_LONG 118
#define PKCS8_R_UNKNOWN_ALGORITHM 119
#define PKCS8_R_UNKNOWN_CIPHER 120
#define PKCS8_R_UNKNOWN_CIPHER_ALGORITHM 121
#define PKCS8_R_UNKNOWN_DIGEST 122
#define PKCS8_R_UNKNOWN_HASH 123
#define PKCS8_R_UNSUPPORTED_PRIVATE_KEY_ALGORITHM 124
#define PKCS8_R_UNSUPPORTED_KEYLENGTH 125
#define PKCS8_R_UNSUPPORTED_SALT_TYPE 126
#define PKCS8_R_UNSUPPORTED_CIPHER 127
#define PKCS8_R_UNSUPPORTED_KEY_DERIVATION_FUNCTION 128
#define PKCS8_R_BAD_ITERATION_COUNT 129
#define PKCS8_R_UNSUPPORTED_PRF 130
#define PKCS8_R_INVALID_CHARACTERS 131
#define PKCS8_R_UNSUPPORTED_OPTIONS 132
#define PKCS8_R_AMBIGUOUS_FRIENDLY_NAME 133

#endif
