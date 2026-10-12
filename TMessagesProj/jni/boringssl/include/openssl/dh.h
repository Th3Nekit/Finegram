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

#ifndef OPENSSL_HEADER_DH_H
#define OPENSSL_HEADER_DH_H

#include <openssl/base.h>

#include <openssl/thread.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT DH *DH_new(void);

OPENSSL_EXPORT void DH_free(DH *dh);

OPENSSL_EXPORT int DH_up_ref(DH *dh);

#define OPENSSL_DH_MAX_MODULUS_BITS 10000

OPENSSL_EXPORT unsigned DH_bits(const DH *dh);

OPENSSL_EXPORT const BIGNUM *DH_get0_pub_key(const DH *dh);

OPENSSL_EXPORT const BIGNUM *DH_get0_priv_key(const DH *dh);

OPENSSL_EXPORT const BIGNUM *DH_get0_p(const DH *dh);

OPENSSL_EXPORT const BIGNUM *DH_get0_q(const DH *dh);

OPENSSL_EXPORT const BIGNUM *DH_get0_g(const DH *dh);

OPENSSL_EXPORT void DH_get0_key(const DH *dh, const BIGNUM **out_pub_key,
                                const BIGNUM **out_priv_key);

OPENSSL_EXPORT int DH_set0_key(DH *dh, BIGNUM *pub_key, BIGNUM *priv_key);

OPENSSL_EXPORT void DH_get0_pqg(const DH *dh, const BIGNUM **out_p,
                                const BIGNUM **out_q, const BIGNUM **out_g);

OPENSSL_EXPORT int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g);

OPENSSL_EXPORT int DH_set_length(DH *dh, unsigned priv_length);

OPENSSL_EXPORT DH *DH_get_rfc7919_2048(void);

OPENSSL_EXPORT BIGNUM *BN_get_rfc3526_prime_1536(BIGNUM *ret);

OPENSSL_EXPORT BIGNUM *BN_get_rfc3526_prime_2048(BIGNUM *ret);

OPENSSL_EXPORT BIGNUM *BN_get_rfc3526_prime_3072(BIGNUM *ret);

OPENSSL_EXPORT BIGNUM *BN_get_rfc3526_prime_4096(BIGNUM *ret);

OPENSSL_EXPORT BIGNUM *BN_get_rfc3526_prime_6144(BIGNUM *ret);

OPENSSL_EXPORT BIGNUM *BN_get_rfc3526_prime_8192(BIGNUM *ret);

#define DH_GENERATOR_2 2
#define DH_GENERATOR_5 5

OPENSSL_EXPORT int DH_generate_parameters_ex(DH *dh, int prime_bits,
                                             int generator, BN_GENCB *cb);

OPENSSL_EXPORT int DH_generate_key(DH *dh);

OPENSSL_EXPORT int DH_compute_key_padded(uint8_t *out, const BIGNUM *peers_key,
                                         DH *dh);

OPENSSL_EXPORT int DH_compute_key_hashed(DH *dh, uint8_t *out, size_t *out_len,
                                         size_t max_out_len,
                                         const BIGNUM *peers_key,
                                         const EVP_MD *digest);

OPENSSL_EXPORT int DH_size(const DH *dh);

OPENSSL_EXPORT unsigned DH_num_bits(const DH *dh);

#define DH_CHECK_P_NOT_PRIME 0x01
#define DH_CHECK_P_NOT_SAFE_PRIME 0x02
#define DH_CHECK_UNABLE_TO_CHECK_GENERATOR 0x04
#define DH_CHECK_NOT_SUITABLE_GENERATOR 0x08
#define DH_CHECK_Q_NOT_PRIME 0x10
#define DH_CHECK_INVALID_Q_VALUE 0x20

#define DH_NOT_SUITABLE_GENERATOR DH_CHECK_NOT_SUITABLE_GENERATOR
#define DH_UNABLE_TO_CHECK_GENERATOR DH_CHECK_UNABLE_TO_CHECK_GENERATOR

OPENSSL_EXPORT int DH_check(const DH *dh, int *out_flags);

#define DH_CHECK_PUBKEY_TOO_SMALL 0x1
#define DH_CHECK_PUBKEY_TOO_LARGE 0x2
#define DH_CHECK_PUBKEY_INVALID 0x4

OPENSSL_EXPORT int DH_check_pub_key(const DH *dh, const BIGNUM *pub_key,
                                    int *out_flags);

OPENSSL_EXPORT DH *DHparams_dup(const DH *dh);

OPENSSL_EXPORT DH *DH_parse_parameters(CBS *cbs);

OPENSSL_EXPORT int DH_marshal_parameters(CBB *cbb, const DH *dh);

OPENSSL_EXPORT DH *DH_generate_parameters(int prime_len, int generator,
                                          void (*callback)(int, int, void *),
                                          void *cb_arg);

OPENSSL_EXPORT DH *d2i_DHparams(DH **ret, const unsigned char **inp, long len);

OPENSSL_EXPORT int i2d_DHparams(const DH *in, unsigned char **outp);

OPENSSL_EXPORT int DH_compute_key(uint8_t *out, const BIGNUM *peers_key,
                                  DH *dh);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(DH, DH_free)
BORINGSSL_MAKE_UP_REF(DH, DH_up_ref)

BSSL_NAMESPACE_END

}

#endif

#define DH_R_BAD_GENERATOR 100
#define DH_R_INVALID_PUBKEY 101
#define DH_R_MODULUS_TOO_LARGE 102
#define DH_R_NO_PRIVATE_VALUE 103
#define DH_R_DECODE_ERROR 104
#define DH_R_ENCODE_ERROR 105
#define DH_R_INVALID_PARAMETERS 106

#endif
