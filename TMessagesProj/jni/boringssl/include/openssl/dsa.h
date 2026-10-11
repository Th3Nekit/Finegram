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

#ifndef OPENSSL_HEADER_DSA_H
#define OPENSSL_HEADER_DSA_H

#include <openssl/base.h>

#include <openssl/ex_data.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT DSA *DSA_new(void);

OPENSSL_EXPORT void DSA_free(DSA *dsa);

OPENSSL_EXPORT int DSA_up_ref(DSA *dsa);

#define OPENSSL_DSA_MAX_MODULUS_BITS 10000

OPENSSL_EXPORT unsigned DSA_bits(const DSA *dsa);

OPENSSL_EXPORT const BIGNUM *DSA_get0_pub_key(const DSA *dsa);

OPENSSL_EXPORT const BIGNUM *DSA_get0_priv_key(const DSA *dsa);

OPENSSL_EXPORT const BIGNUM *DSA_get0_p(const DSA *dsa);

OPENSSL_EXPORT const BIGNUM *DSA_get0_q(const DSA *dsa);

OPENSSL_EXPORT const BIGNUM *DSA_get0_g(const DSA *dsa);

OPENSSL_EXPORT void DSA_get0_key(const DSA *dsa, const BIGNUM **out_pub_key,
                                 const BIGNUM **out_priv_key);

OPENSSL_EXPORT void DSA_get0_pqg(const DSA *dsa, const BIGNUM **out_p,
                                 const BIGNUM **out_q, const BIGNUM **out_g);

OPENSSL_EXPORT int DSA_set0_key(DSA *dsa, BIGNUM *pub_key, BIGNUM *priv_key);

OPENSSL_EXPORT int DSA_set0_pqg(DSA *dsa, BIGNUM *p, BIGNUM *q, BIGNUM *g);

OPENSSL_EXPORT int DSA_generate_parameters_ex(DSA *dsa, unsigned bits,
                                              const uint8_t *seed,
                                              size_t seed_len, int *out_counter,
                                              unsigned long *out_h,
                                              BN_GENCB *cb);

OPENSSL_EXPORT DSA *DSAparams_dup(const DSA *dsa);

OPENSSL_EXPORT int DSA_generate_key(DSA *dsa);

struct DSA_SIG_st {
  BIGNUM *r, *s;
};

OPENSSL_EXPORT DSA_SIG *DSA_SIG_new(void);

OPENSSL_EXPORT void DSA_SIG_free(DSA_SIG *sig);

OPENSSL_EXPORT void DSA_SIG_get0(const DSA_SIG *sig, const BIGNUM **out_r,
                                 const BIGNUM **out_s);

OPENSSL_EXPORT int DSA_SIG_set0(DSA_SIG *sig, BIGNUM *r, BIGNUM *s);

OPENSSL_EXPORT DSA_SIG *DSA_do_sign(const uint8_t *digest, size_t digest_len,
                                    const DSA *dsa);

OPENSSL_EXPORT int DSA_do_verify(const uint8_t *digest, size_t digest_len,
                                 const DSA_SIG *sig, const DSA *dsa);

OPENSSL_EXPORT int DSA_do_check_signature(int *out_valid, const uint8_t *digest,
                                          size_t digest_len, const DSA_SIG *sig,
                                          const DSA *dsa);

OPENSSL_EXPORT int DSA_sign(int type, const uint8_t *digest, size_t digest_len,
                            uint8_t *out_sig, unsigned int *out_siglen,
                            const DSA *dsa);

OPENSSL_EXPORT int DSA_verify(int type, const uint8_t *digest,
                              size_t digest_len, const uint8_t *sig,
                              size_t sig_len, const DSA *dsa);

OPENSSL_EXPORT int DSA_check_signature(int *out_valid, const uint8_t *digest,
                                       size_t digest_len, const uint8_t *sig,
                                       size_t sig_len, const DSA *dsa);

OPENSSL_EXPORT int DSA_size(const DSA *dsa);

OPENSSL_EXPORT DSA_SIG *DSA_SIG_parse(CBS *cbs);

OPENSSL_EXPORT int DSA_SIG_marshal(CBB *cbb, const DSA_SIG *sig);

OPENSSL_EXPORT DSA *DSA_parse_public_key(CBS *cbs);

OPENSSL_EXPORT int DSA_marshal_public_key(CBB *cbb, const DSA *dsa);

OPENSSL_EXPORT DSA *DSA_parse_private_key(CBS *cbs);

OPENSSL_EXPORT int DSA_marshal_private_key(CBB *cbb, const DSA *dsa);

OPENSSL_EXPORT DSA *DSA_parse_parameters(CBS *cbs);

OPENSSL_EXPORT int DSA_marshal_parameters(CBB *cbb, const DSA *dsa);

OPENSSL_EXPORT DH *DSA_dup_DH(const DSA *dsa);

OPENSSL_EXPORT int DSA_get_ex_new_index(long argl, void *argp,
                                        CRYPTO_EX_unused *unused,
                                        CRYPTO_EX_dup *dup_unused,
                                        CRYPTO_EX_free *free_func);
OPENSSL_EXPORT int DSA_set_ex_data(DSA *dsa, int idx, void *arg);
OPENSSL_EXPORT void *DSA_get_ex_data(const DSA *dsa, int idx);

OPENSSL_EXPORT DSA_SIG *d2i_DSA_SIG(DSA_SIG **out_sig, const uint8_t **inp,
                                    long len);

OPENSSL_EXPORT int i2d_DSA_SIG(const DSA_SIG *in, uint8_t **outp);

OPENSSL_EXPORT DSA *d2i_DSAPublicKey(DSA **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_DSAPublicKey(const DSA *in, uint8_t **outp);

OPENSSL_EXPORT DSA *d2i_DSAPrivateKey(DSA **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_DSAPrivateKey(const DSA *in, uint8_t **outp);

OPENSSL_EXPORT DSA *d2i_DSAparams(DSA **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_DSAparams(const DSA *in, uint8_t **outp);

OPENSSL_EXPORT DSA *DSA_generate_parameters(int bits, unsigned char *seed,
                                            int seed_len, int *counter_ret,
                                            unsigned long *h_ret,
                                            void (*callback)(int, int, void *),
                                            void *cb_arg);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(DSA, DSA_free)
BORINGSSL_MAKE_UP_REF(DSA, DSA_up_ref)
BORINGSSL_MAKE_DELETER(DSA_SIG, DSA_SIG_free)

BSSL_NAMESPACE_END

}

#endif

#define DSA_R_BAD_Q_VALUE 100
#define DSA_R_MISSING_PARAMETERS 101
#define DSA_R_MODULUS_TOO_LARGE 102
#define DSA_R_NEED_NEW_SETUP_VALUES 103
#define DSA_R_BAD_VERSION 104
#define DSA_R_DECODE_ERROR 105
#define DSA_R_ENCODE_ERROR 106
#define DSA_R_INVALID_PARAMETERS 107
#define DSA_R_TOO_MANY_ITERATIONS 108

#endif
