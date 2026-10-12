// Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_ECDSA_H
#define OPENSSL_HEADER_ECDSA_H

#include <openssl/base.h>

#include <openssl/ec_key.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int ECDSA_sign(int type, const uint8_t *digest,
                              size_t digest_len, uint8_t *sig,
                              unsigned int *sig_len, const EC_KEY *key);

OPENSSL_EXPORT int ECDSA_verify(int type, const uint8_t *digest,
                                size_t digest_len, const uint8_t *sig,
                                size_t sig_len, const EC_KEY *key);

OPENSSL_EXPORT size_t ECDSA_size(const EC_KEY *key);

struct ecdsa_sig_st {
  BIGNUM *r;
  BIGNUM *s;
};

OPENSSL_EXPORT ECDSA_SIG *ECDSA_SIG_new(void);

OPENSSL_EXPORT void ECDSA_SIG_free(ECDSA_SIG *sig);

OPENSSL_EXPORT const BIGNUM *ECDSA_SIG_get0_r(const ECDSA_SIG *sig);

OPENSSL_EXPORT const BIGNUM *ECDSA_SIG_get0_s(const ECDSA_SIG *sig);

OPENSSL_EXPORT void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **out_r,
                                   const BIGNUM **out_s);

OPENSSL_EXPORT int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s);

OPENSSL_EXPORT ECDSA_SIG *ECDSA_do_sign(const uint8_t *digest,
                                        size_t digest_len, const EC_KEY *key);

OPENSSL_EXPORT int ECDSA_do_verify(const uint8_t *digest, size_t digest_len,
                                   const ECDSA_SIG *sig, const EC_KEY *key);

OPENSSL_EXPORT ECDSA_SIG *ECDSA_SIG_parse(CBS *cbs);

OPENSSL_EXPORT ECDSA_SIG *ECDSA_SIG_from_bytes(const uint8_t *in,
                                               size_t in_len);

OPENSSL_EXPORT int ECDSA_SIG_marshal(CBB *cbb, const ECDSA_SIG *sig);

OPENSSL_EXPORT int ECDSA_SIG_to_bytes(uint8_t **out_bytes, size_t *out_len,
                                      const ECDSA_SIG *sig);

OPENSSL_EXPORT size_t ECDSA_SIG_max_len(size_t order_len);

OPENSSL_EXPORT ECDSA_SIG *
ECDSA_sign_with_nonce_and_leak_private_key_for_testing(const uint8_t *digest,
                                                       size_t digest_len,
                                                       const EC_KEY *eckey,
                                                       const uint8_t *nonce,
                                                       size_t nonce_len);

OPENSSL_EXPORT ECDSA_SIG *d2i_ECDSA_SIG(ECDSA_SIG **out, const uint8_t **inp,
                                        long len);

OPENSSL_EXPORT int i2d_ECDSA_SIG(const ECDSA_SIG *sig, uint8_t **outp);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(ECDSA_SIG, ECDSA_SIG_free)

BSSL_NAMESPACE_END

}

#endif

#define ECDSA_R_BAD_SIGNATURE 100
#define ECDSA_R_MISSING_PARAMETERS 101
#define ECDSA_R_NEED_NEW_SETUP_VALUES 102
#define ECDSA_R_NOT_IMPLEMENTED 103
#define ECDSA_R_RANDOM_NUMBER_GENERATION_FAILED 104
#define ECDSA_R_ENCODE_ERROR 105
#define ECDSA_R_TOO_MANY_ITERATIONS 106

#endif
