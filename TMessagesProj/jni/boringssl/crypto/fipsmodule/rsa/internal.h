// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_RSA_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_RSA_INTERNAL_H

#include <openssl/base.h>

#include <openssl/bn.h>
#include <openssl/rsa.h>

#include "../../internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct bn_blinding_st BN_BLINDING;

struct rsa_st {
  RSA_METHOD *meth;

  BIGNUM *n;
  BIGNUM *e;
  BIGNUM *d;
  BIGNUM *p;
  BIGNUM *q;
  BIGNUM *dmp1;
  BIGNUM *dmq1;
  BIGNUM *iqmp;

  CRYPTO_EX_DATA ex_data;
  CRYPTO_refcount_t references;
  int flags;

  CRYPTO_MUTEX lock;

  BN_MONT_CTX *mont_n;
  BN_MONT_CTX *mont_p;
  BN_MONT_CTX *mont_q;

  BIGNUM *d_fixed, *dmp1_fixed, *dmq1_fixed;

  BIGNUM *iqmp_mont;

  size_t num_blindings;

  BN_BLINDING **blindings;
  unsigned char *blindings_inuse;
  uint64_t blinding_fork_generation;

  unsigned private_key_frozen:1;
};

#define RSA_PKCS1_PADDING_SIZE 11

const RSA_METHOD *RSA_default_method(void);

int rsa_default_sign_raw(RSA *rsa, size_t *out_len, uint8_t *out,
                         size_t max_out, const uint8_t *in, size_t in_len,
                         int padding);
int rsa_default_private_transform(RSA *rsa, uint8_t *out, const uint8_t *in,
                                  size_t len);

BN_BLINDING *BN_BLINDING_new(void);
void BN_BLINDING_free(BN_BLINDING *b);
void BN_BLINDING_invalidate(BN_BLINDING *b);
int BN_BLINDING_convert(BIGNUM *n, BN_BLINDING *b, const BIGNUM *e,
                        const BN_MONT_CTX *mont_ctx, BN_CTX *ctx);
int BN_BLINDING_invert(BIGNUM *n, const BN_BLINDING *b, BN_MONT_CTX *mont_ctx,
                       BN_CTX *ctx);

int PKCS1_MGF1(uint8_t *out, size_t len, const uint8_t *seed, size_t seed_len,
               const EVP_MD *md);
int RSA_padding_add_PKCS1_type_1(uint8_t *to, size_t to_len,
                                 const uint8_t *from, size_t from_len);
int RSA_padding_check_PKCS1_type_1(uint8_t *out, size_t *out_len,
                                   size_t max_out, const uint8_t *from,
                                   size_t from_len);
int RSA_padding_add_none(uint8_t *to, size_t to_len, const uint8_t *from,
                         size_t from_len);

int rsa_check_public_key(const RSA *rsa);

int rsa_private_transform_no_self_test(RSA *rsa, uint8_t *out,
                                       const uint8_t *in, size_t len);

int rsa_private_transform(RSA *rsa, uint8_t *out, const uint8_t *in,
                          size_t len);

void rsa_invalidate_key(RSA *rsa);

extern const BN_ULONG kBoringSSLRSASqrtTwo[];
extern const size_t kBoringSSLRSASqrtTwoLen;

int rsa_verify_no_self_test(int hash_nid, const uint8_t *digest,
                            size_t digest_len, const uint8_t *sig,
                            size_t sig_len, RSA *rsa);

int rsa_verify_raw_no_self_test(RSA *rsa, size_t *out_len, uint8_t *out,
                                size_t max_out, const uint8_t *in,
                                size_t in_len, int padding);

int rsa_sign_no_self_test(int hash_nid, const uint8_t *digest,
                          size_t digest_len, uint8_t *out, unsigned *out_len,
                          RSA *rsa);

#if defined(__cplusplus)
}
#endif

#endif
