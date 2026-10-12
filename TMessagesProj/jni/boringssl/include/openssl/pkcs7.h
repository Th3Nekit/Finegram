// Copyright 2014 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_PKCS7_H
#define OPENSSL_HEADER_PKCS7_H

#include <openssl/base.h>

#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif

DECLARE_STACK_OF(CRYPTO_BUFFER)
DECLARE_STACK_OF(X509)
DECLARE_STACK_OF(X509_CRL)

OPENSSL_EXPORT int PKCS7_get_raw_certificates(
    STACK_OF(CRYPTO_BUFFER) *out_certs, CBS *cbs, CRYPTO_BUFFER_POOL *pool);

OPENSSL_EXPORT int PKCS7_get_certificates(STACK_OF(X509) *out_certs, CBS *cbs);

OPENSSL_EXPORT int PKCS7_bundle_raw_certificates(
    CBB *out, const STACK_OF(CRYPTO_BUFFER) *certs);

OPENSSL_EXPORT int PKCS7_bundle_certificates(
    CBB *out, const STACK_OF(X509) *certs);

OPENSSL_EXPORT int PKCS7_get_CRLs(STACK_OF(X509_CRL) *out_crls, CBS *cbs);

OPENSSL_EXPORT int PKCS7_bundle_CRLs(CBB *out, const STACK_OF(X509_CRL) *crls);

OPENSSL_EXPORT int PKCS7_get_PEM_certificates(STACK_OF(X509) *out_certs,
                                              BIO *pem_bio);

OPENSSL_EXPORT int PKCS7_get_PEM_CRLs(STACK_OF(X509_CRL) *out_crls,
                                      BIO *pem_bio);

typedef struct {
  STACK_OF(X509) *cert;
  STACK_OF(X509_CRL) *crl;
} PKCS7_SIGNED;

typedef struct {
  STACK_OF(X509) *cert;
  STACK_OF(X509_CRL) *crl;
} PKCS7_SIGN_ENVELOPE;

typedef void PKCS7_ENVELOPE;
typedef void PKCS7_DIGEST;
typedef void PKCS7_ENCRYPT;
typedef void PKCS7_SIGNER_INFO;

typedef struct {
  uint8_t *ber_bytes;
  size_t ber_len;

  ASN1_OBJECT *type;
  union {
    char *ptr;
    ASN1_OCTET_STRING *data;
    PKCS7_SIGNED *sign;
    PKCS7_ENVELOPE *enveloped;
    PKCS7_SIGN_ENVELOPE *signed_and_enveloped;
    PKCS7_DIGEST *digest;
    PKCS7_ENCRYPT *encrypted;
    ASN1_TYPE *other;
  } d;
} PKCS7;

OPENSSL_EXPORT PKCS7 *d2i_PKCS7(PKCS7 **out, const uint8_t **inp,
                                size_t len);

OPENSSL_EXPORT PKCS7 *d2i_PKCS7_bio(BIO *bio, PKCS7 **out);

OPENSSL_EXPORT int i2d_PKCS7(const PKCS7 *p7, uint8_t **out);

OPENSSL_EXPORT int i2d_PKCS7_bio(BIO *bio, const PKCS7 *p7);

OPENSSL_EXPORT void PKCS7_free(PKCS7 *p7);

OPENSSL_EXPORT int PKCS7_type_is_data(const PKCS7 *p7);

OPENSSL_EXPORT int PKCS7_type_is_digest(const PKCS7 *p7);

OPENSSL_EXPORT int PKCS7_type_is_encrypted(const PKCS7 *p7);

OPENSSL_EXPORT int PKCS7_type_is_enveloped(const PKCS7 *p7);

OPENSSL_EXPORT int PKCS7_type_is_signed(const PKCS7 *p7);

OPENSSL_EXPORT int PKCS7_type_is_signedAndEnveloped(const PKCS7 *p7);

#define PKCS7_DETACHED 0x40

#define PKCS7_TEXT 0x1
#define PKCS7_NOCERTS 0x2
#define PKCS7_NOSIGS 0x4
#define PKCS7_NOCHAIN 0x8
#define PKCS7_NOINTERN 0x10
#define PKCS7_NOVERIFY 0x20
#define PKCS7_BINARY 0x80
#define PKCS7_NOATTR 0x100
#define PKCS7_NOSMIMECAP 0x200
#define PKCS7_STREAM 0x1000
#define PKCS7_PARTIAL 0x4000

OPENSSL_EXPORT PKCS7 *PKCS7_sign(X509 *sign_cert, EVP_PKEY *pkey,
                                 STACK_OF(X509) *certs, BIO *data, int flags);

#if defined(__cplusplus)
}

extern "C++" {
BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(PKCS7, PKCS7_free)

BSSL_NAMESPACE_END
}
#endif

#define PKCS7_R_BAD_PKCS7_VERSION 100
#define PKCS7_R_NOT_PKCS7_SIGNED_DATA 101
#define PKCS7_R_NO_CERTIFICATES_INCLUDED 102
#define PKCS7_R_NO_CRLS_INCLUDED 103

#endif
