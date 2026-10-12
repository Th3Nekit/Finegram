// Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_EVP_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_EVP_INTERNAL_H

#include <openssl/base.h>

#include <openssl/rsa.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct evp_pkey_asn1_method_st EVP_PKEY_ASN1_METHOD;
typedef struct evp_pkey_method_st EVP_PKEY_METHOD;

struct evp_pkey_asn1_method_st {
  int pkey_id;
  uint8_t oid[9];
  uint8_t oid_len;

  const EVP_PKEY_METHOD *pkey_method;

  int (*pub_decode)(EVP_PKEY *out, CBS *params, CBS *key);

  int (*pub_encode)(CBB *out, const EVP_PKEY *key);

  int (*pub_cmp)(const EVP_PKEY *a, const EVP_PKEY *b);

  int (*priv_decode)(EVP_PKEY *out, CBS *params, CBS *key);

  int (*priv_encode)(CBB *out, const EVP_PKEY *key);

  int (*set_priv_raw)(EVP_PKEY *pkey, const uint8_t *in, size_t len);
  int (*set_pub_raw)(EVP_PKEY *pkey, const uint8_t *in, size_t len);
  int (*get_priv_raw)(const EVP_PKEY *pkey, uint8_t *out, size_t *out_len);
  int (*get_pub_raw)(const EVP_PKEY *pkey, uint8_t *out, size_t *out_len);

  int (*set1_tls_encodedpoint)(EVP_PKEY *pkey, const uint8_t *in, size_t len);
  size_t (*get1_tls_encodedpoint)(const EVP_PKEY *pkey, uint8_t **out_ptr);

  int (*pkey_opaque)(const EVP_PKEY *pk);

  int (*pkey_size)(const EVP_PKEY *pk);
  int (*pkey_bits)(const EVP_PKEY *pk);

  int (*param_missing)(const EVP_PKEY *pk);
  int (*param_copy)(EVP_PKEY *to, const EVP_PKEY *from);
  int (*param_cmp)(const EVP_PKEY *a, const EVP_PKEY *b);

  void (*pkey_free)(EVP_PKEY *pkey);
}                           ;

struct evp_pkey_st {
  CRYPTO_refcount_t references;

  int type;

  void *pkey;

  const EVP_PKEY_ASN1_METHOD *ameth;
}               ;

#define EVP_PKEY_OP_UNDEFINED 0
#define EVP_PKEY_OP_KEYGEN (1 << 2)
#define EVP_PKEY_OP_SIGN (1 << 3)
#define EVP_PKEY_OP_VERIFY (1 << 4)
#define EVP_PKEY_OP_VERIFYRECOVER (1 << 5)
#define EVP_PKEY_OP_ENCRYPT (1 << 6)
#define EVP_PKEY_OP_DECRYPT (1 << 7)
#define EVP_PKEY_OP_DERIVE (1 << 8)
#define EVP_PKEY_OP_PARAMGEN (1 << 9)

#define EVP_PKEY_OP_TYPE_SIG \
  (EVP_PKEY_OP_SIGN | EVP_PKEY_OP_VERIFY | EVP_PKEY_OP_VERIFYRECOVER)

#define EVP_PKEY_OP_TYPE_CRYPT (EVP_PKEY_OP_ENCRYPT | EVP_PKEY_OP_DECRYPT)

#define EVP_PKEY_OP_TYPE_NOGEN \
  (EVP_PKEY_OP_SIG | EVP_PKEY_OP_CRYPT | EVP_PKEY_OP_DERIVE)

#define EVP_PKEY_OP_TYPE_GEN (EVP_PKEY_OP_KEYGEN | EVP_PKEY_OP_PARAMGEN)

OPENSSL_EXPORT int EVP_PKEY_CTX_ctrl(EVP_PKEY_CTX *ctx, int keytype, int optype,
                                     int cmd, int p1, void *p2);

#define EVP_PKEY_CTRL_MD 1
#define EVP_PKEY_CTRL_GET_MD 2

#define EVP_PKEY_CTRL_PEER_KEY 3

#define EVP_PKEY_ALG_CTRL 0x1000

#define EVP_PKEY_CTRL_RSA_PADDING (EVP_PKEY_ALG_CTRL + 1)
#define EVP_PKEY_CTRL_GET_RSA_PADDING (EVP_PKEY_ALG_CTRL + 2)
#define EVP_PKEY_CTRL_RSA_PSS_SALTLEN (EVP_PKEY_ALG_CTRL + 3)
#define EVP_PKEY_CTRL_GET_RSA_PSS_SALTLEN (EVP_PKEY_ALG_CTRL + 4)
#define EVP_PKEY_CTRL_RSA_KEYGEN_BITS (EVP_PKEY_ALG_CTRL + 5)
#define EVP_PKEY_CTRL_RSA_KEYGEN_PUBEXP (EVP_PKEY_ALG_CTRL + 6)
#define EVP_PKEY_CTRL_RSA_OAEP_MD (EVP_PKEY_ALG_CTRL + 7)
#define EVP_PKEY_CTRL_GET_RSA_OAEP_MD (EVP_PKEY_ALG_CTRL + 8)
#define EVP_PKEY_CTRL_RSA_MGF1_MD (EVP_PKEY_ALG_CTRL + 9)
#define EVP_PKEY_CTRL_GET_RSA_MGF1_MD (EVP_PKEY_ALG_CTRL + 10)
#define EVP_PKEY_CTRL_RSA_OAEP_LABEL (EVP_PKEY_ALG_CTRL + 11)
#define EVP_PKEY_CTRL_GET_RSA_OAEP_LABEL (EVP_PKEY_ALG_CTRL + 12)
#define EVP_PKEY_CTRL_EC_PARAMGEN_CURVE_NID (EVP_PKEY_ALG_CTRL + 13)
#define EVP_PKEY_CTRL_HKDF_MODE (EVP_PKEY_ALG_CTRL + 14)
#define EVP_PKEY_CTRL_HKDF_MD (EVP_PKEY_ALG_CTRL + 15)
#define EVP_PKEY_CTRL_HKDF_KEY (EVP_PKEY_ALG_CTRL + 16)
#define EVP_PKEY_CTRL_HKDF_SALT (EVP_PKEY_ALG_CTRL + 17)
#define EVP_PKEY_CTRL_HKDF_INFO (EVP_PKEY_ALG_CTRL + 18)
#define EVP_PKEY_CTRL_DH_PAD (EVP_PKEY_ALG_CTRL + 19)

struct evp_pkey_ctx_st {

  const EVP_PKEY_METHOD *pmeth;

  ENGINE *engine;

  EVP_PKEY *pkey;

  EVP_PKEY *peerkey;

  int operation;

  void *data;
}                   ;

struct evp_pkey_method_st {
  int pkey_id;

  int (*init)(EVP_PKEY_CTX *ctx);
  int (*copy)(EVP_PKEY_CTX *dst, EVP_PKEY_CTX *src);
  void (*cleanup)(EVP_PKEY_CTX *ctx);

  int (*keygen)(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey);

  int (*sign)(EVP_PKEY_CTX *ctx, uint8_t *sig, size_t *siglen,
              const uint8_t *tbs, size_t tbslen);

  int (*sign_message)(EVP_PKEY_CTX *ctx, uint8_t *sig, size_t *siglen,
                      const uint8_t *tbs, size_t tbslen);

  int (*verify)(EVP_PKEY_CTX *ctx, const uint8_t *sig, size_t siglen,
                const uint8_t *tbs, size_t tbslen);

  int (*verify_message)(EVP_PKEY_CTX *ctx, const uint8_t *sig, size_t siglen,
                        const uint8_t *tbs, size_t tbslen);

  int (*verify_recover)(EVP_PKEY_CTX *ctx, uint8_t *out, size_t *out_len,
                        const uint8_t *sig, size_t sig_len);

  int (*encrypt)(EVP_PKEY_CTX *ctx, uint8_t *out, size_t *outlen,
                 const uint8_t *in, size_t inlen);

  int (*decrypt)(EVP_PKEY_CTX *ctx, uint8_t *out, size_t *outlen,
                 const uint8_t *in, size_t inlen);

  int (*derive)(EVP_PKEY_CTX *ctx, uint8_t *key, size_t *keylen);

  int (*paramgen)(EVP_PKEY_CTX *ctx, EVP_PKEY *pkey);

  int (*ctrl)(EVP_PKEY_CTX *ctx, int type, int p1, void *p2);
}                      ;

typedef struct {

  uint8_t key[64];
  char has_private;
} ED25519_KEY;

#define ED25519_PUBLIC_KEY_OFFSET 32

typedef struct {
  uint8_t pub[32];
  uint8_t priv[32];
  char has_private;
} X25519_KEY;

extern const EVP_PKEY_ASN1_METHOD dsa_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD ec_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD rsa_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD ed25519_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD x25519_asn1_meth;
extern const EVP_PKEY_ASN1_METHOD dh_asn1_meth;

extern const EVP_PKEY_METHOD rsa_pkey_meth;
extern const EVP_PKEY_METHOD ec_pkey_meth;
extern const EVP_PKEY_METHOD ed25519_pkey_meth;
extern const EVP_PKEY_METHOD x25519_pkey_meth;
extern const EVP_PKEY_METHOD hkdf_pkey_meth;
extern const EVP_PKEY_METHOD dh_pkey_meth;

void evp_pkey_set_method(EVP_PKEY *pkey, const EVP_PKEY_ASN1_METHOD *method);

#if defined(__cplusplus)
}
#endif

#endif
