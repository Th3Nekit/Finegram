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

#ifndef OPENSSL_HEADER_RSA_H
#define OPENSSL_HEADER_RSA_H

#include <openssl/base.h>

#include <openssl/engine.h>
#include <openssl/ex_data.h>
#include <openssl/thread.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT RSA *RSA_new_public_key(const BIGNUM *n, const BIGNUM *e);

OPENSSL_EXPORT RSA *RSA_new_private_key(const BIGNUM *n, const BIGNUM *e,
                                        const BIGNUM *d, const BIGNUM *p,
                                        const BIGNUM *q, const BIGNUM *dmp1,
                                        const BIGNUM *dmq1, const BIGNUM *iqmp);

OPENSSL_EXPORT RSA *RSA_new(void);

OPENSSL_EXPORT RSA *RSA_new_method(const ENGINE *engine);

OPENSSL_EXPORT void RSA_free(RSA *rsa);

OPENSSL_EXPORT int RSA_up_ref(RSA *rsa);

#define OPENSSL_RSA_MAX_MODULUS_BITS 16384

OPENSSL_EXPORT unsigned RSA_bits(const RSA *rsa);

OPENSSL_EXPORT const BIGNUM *RSA_get0_n(const RSA *rsa);

OPENSSL_EXPORT const BIGNUM *RSA_get0_e(const RSA *rsa);

OPENSSL_EXPORT const BIGNUM *RSA_get0_d(const RSA *rsa);

OPENSSL_EXPORT const BIGNUM *RSA_get0_p(const RSA *rsa);

OPENSSL_EXPORT const BIGNUM *RSA_get0_q(const RSA *rsa);

OPENSSL_EXPORT const BIGNUM *RSA_get0_dmp1(const RSA *rsa);

OPENSSL_EXPORT const BIGNUM *RSA_get0_dmq1(const RSA *rsa);

OPENSSL_EXPORT const BIGNUM *RSA_get0_iqmp(const RSA *rsa);

OPENSSL_EXPORT void RSA_get0_key(const RSA *rsa, const BIGNUM **out_n,
                                 const BIGNUM **out_e, const BIGNUM **out_d);

OPENSSL_EXPORT void RSA_get0_factors(const RSA *rsa, const BIGNUM **out_p,
                                     const BIGNUM **out_q);

OPENSSL_EXPORT void RSA_get0_crt_params(const RSA *rsa, const BIGNUM **out_dmp1,
                                        const BIGNUM **out_dmq1,
                                        const BIGNUM **out_iqmp);

OPENSSL_EXPORT int RSA_set0_key(RSA *rsa, BIGNUM *n, BIGNUM *e, BIGNUM *d);

OPENSSL_EXPORT int RSA_set0_factors(RSA *rsa, BIGNUM *p, BIGNUM *q);

OPENSSL_EXPORT int RSA_set0_crt_params(RSA *rsa, BIGNUM *dmp1, BIGNUM *dmq1,
                                       BIGNUM *iqmp);

OPENSSL_EXPORT int RSA_generate_key_ex(RSA *rsa, int bits, const BIGNUM *e,
                                       BN_GENCB *cb);

OPENSSL_EXPORT int RSA_generate_key_fips(RSA *rsa, int bits, BN_GENCB *cb);

#define RSA_PKCS1_PADDING 1

#define RSA_NO_PADDING 3

#define RSA_PKCS1_OAEP_PADDING 4

#define RSA_PKCS1_PSS_PADDING 6

OPENSSL_EXPORT int RSA_encrypt(RSA *rsa, size_t *out_len, uint8_t *out,
                               size_t max_out, const uint8_t *in, size_t in_len,
                               int padding);

OPENSSL_EXPORT int RSA_decrypt(RSA *rsa, size_t *out_len, uint8_t *out,
                               size_t max_out, const uint8_t *in, size_t in_len,
                               int padding);

OPENSSL_EXPORT int RSA_public_encrypt(size_t flen, const uint8_t *from,
                                      uint8_t *to, RSA *rsa, int padding);

OPENSSL_EXPORT int RSA_private_decrypt(size_t flen, const uint8_t *from,
                                       uint8_t *to, RSA *rsa, int padding);

OPENSSL_EXPORT int RSA_sign(int hash_nid, const uint8_t *digest,
                            size_t digest_len, uint8_t *out, unsigned *out_len,
                            RSA *rsa);

OPENSSL_EXPORT int RSA_sign_pss_mgf1(RSA *rsa, size_t *out_len, uint8_t *out,
                                     size_t max_out, const uint8_t *digest,
                                     size_t digest_len, const EVP_MD *md,
                                     const EVP_MD *mgf1_md, int salt_len);

OPENSSL_EXPORT int RSA_sign_raw(RSA *rsa, size_t *out_len, uint8_t *out,
                                size_t max_out, const uint8_t *in,
                                size_t in_len, int padding);

OPENSSL_EXPORT int RSA_verify(int hash_nid, const uint8_t *digest,
                              size_t digest_len, const uint8_t *sig,
                              size_t sig_len, RSA *rsa);

OPENSSL_EXPORT int RSA_verify_pss_mgf1(RSA *rsa, const uint8_t *digest,
                                       size_t digest_len, const EVP_MD *md,
                                       const EVP_MD *mgf1_md, int salt_len,
                                       const uint8_t *sig, size_t sig_len);

OPENSSL_EXPORT int RSA_verify_raw(RSA *rsa, size_t *out_len, uint8_t *out,
                                  size_t max_out, const uint8_t *in,
                                  size_t in_len, int padding);

OPENSSL_EXPORT int RSA_private_encrypt(size_t flen, const uint8_t *from,
                                       uint8_t *to, RSA *rsa, int padding);

OPENSSL_EXPORT int RSA_public_decrypt(size_t flen, const uint8_t *from,
                                      uint8_t *to, RSA *rsa, int padding);

OPENSSL_EXPORT unsigned RSA_size(const RSA *rsa);

OPENSSL_EXPORT int RSA_is_opaque(const RSA *rsa);

OPENSSL_EXPORT RSA *RSAPublicKey_dup(const RSA *rsa);

OPENSSL_EXPORT RSA *RSAPrivateKey_dup(const RSA *rsa);

OPENSSL_EXPORT int RSA_check_key(const RSA *rsa);

OPENSSL_EXPORT int RSA_check_fips(RSA *key);

OPENSSL_EXPORT int RSA_verify_PKCS1_PSS_mgf1(const RSA *rsa,
                                             const uint8_t *mHash,
                                             const EVP_MD *Hash,
                                             const EVP_MD *mgf1Hash,
                                             const uint8_t *EM, int sLen);

OPENSSL_EXPORT int RSA_padding_add_PKCS1_PSS_mgf1(const RSA *rsa, uint8_t *EM,
                                                  const uint8_t *mHash,
                                                  const EVP_MD *Hash,
                                                  const EVP_MD *mgf1Hash,
                                                  int sLen);

OPENSSL_EXPORT int RSA_padding_add_PKCS1_OAEP_mgf1(
    uint8_t *to, size_t to_len, const uint8_t *from, size_t from_len,
    const uint8_t *param, size_t param_len, const EVP_MD *md,
    const EVP_MD *mgf1md);

OPENSSL_EXPORT int RSA_add_pkcs1_prefix(uint8_t **out_msg, size_t *out_msg_len,
                                        int *is_alloced, int hash_nid,
                                        const uint8_t *digest,
                                        size_t digest_len);

OPENSSL_EXPORT RSA *RSA_parse_public_key(CBS *cbs);

OPENSSL_EXPORT RSA *RSA_public_key_from_bytes(const uint8_t *in, size_t in_len);

OPENSSL_EXPORT int RSA_marshal_public_key(CBB *cbb, const RSA *rsa);

OPENSSL_EXPORT int RSA_public_key_to_bytes(uint8_t **out_bytes, size_t *out_len,
                                           const RSA *rsa);

OPENSSL_EXPORT RSA *RSA_parse_private_key(CBS *cbs);

OPENSSL_EXPORT RSA *RSA_private_key_from_bytes(const uint8_t *in,
                                               size_t in_len);

OPENSSL_EXPORT int RSA_marshal_private_key(CBB *cbb, const RSA *rsa);

OPENSSL_EXPORT int RSA_private_key_to_bytes(uint8_t **out_bytes,
                                            size_t *out_len, const RSA *rsa);

OPENSSL_EXPORT RSA *RSA_new_private_key_no_crt(const BIGNUM *n, const BIGNUM *e,
                                               const BIGNUM *d);

OPENSSL_EXPORT RSA *RSA_new_private_key_no_e(const BIGNUM *n, const BIGNUM *d);

OPENSSL_EXPORT RSA *RSA_new_public_key_large_e(const BIGNUM *n,
                                               const BIGNUM *e);

OPENSSL_EXPORT RSA *RSA_new_private_key_large_e(
    const BIGNUM *n, const BIGNUM *e, const BIGNUM *d, const BIGNUM *p,
    const BIGNUM *q, const BIGNUM *dmp1, const BIGNUM *dmq1,
    const BIGNUM *iqmp);

OPENSSL_EXPORT int RSA_get_ex_new_index(long argl, void *argp,
                                        CRYPTO_EX_unused *unused,
                                        CRYPTO_EX_dup *dup_unused,
                                        CRYPTO_EX_free *free_func);
OPENSSL_EXPORT int RSA_set_ex_data(RSA *rsa, int idx, void *arg);
OPENSSL_EXPORT void *RSA_get_ex_data(const RSA *rsa, int idx);

#define RSA_FLAG_OPAQUE 1

#define RSA_FLAG_NO_BLINDING 8

#define RSA_FLAG_EXT_PKEY 0x20

#define RSA_FLAG_NO_PUBLIC_EXPONENT 0x40

#define RSA_FLAG_LARGE_PUBLIC_EXPONENT 0x80

#define RSA_3 0x3
#define RSA_F4 0x10001

#define RSA_METHOD_FLAG_NO_CHECK RSA_FLAG_OPAQUE

OPENSSL_EXPORT int RSA_flags(const RSA *rsa);

OPENSSL_EXPORT int RSA_test_flags(const RSA *rsa, int flags);

OPENSSL_EXPORT int RSA_blinding_on(RSA *rsa, BN_CTX *ctx);

OPENSSL_EXPORT void RSA_blinding_off(RSA *rsa);

OPENSSL_EXPORT RSA *RSA_generate_key(int bits, uint64_t e, void *callback,
                                     void *cb_arg);

OPENSSL_EXPORT RSA *d2i_RSAPublicKey(RSA **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_RSAPublicKey(const RSA *in, uint8_t **outp);

OPENSSL_EXPORT RSA *d2i_RSAPrivateKey(RSA **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_RSAPrivateKey(const RSA *in, uint8_t **outp);

OPENSSL_EXPORT int RSA_padding_add_PKCS1_PSS(const RSA *rsa, uint8_t *EM,
                                             const uint8_t *mHash,
                                             const EVP_MD *Hash, int sLen);

OPENSSL_EXPORT int RSA_verify_PKCS1_PSS(const RSA *rsa, const uint8_t *mHash,
                                        const EVP_MD *Hash, const uint8_t *EM,
                                        int sLen);

OPENSSL_EXPORT int RSA_padding_add_PKCS1_OAEP(uint8_t *to, size_t to_len,
                                              const uint8_t *from,
                                              size_t from_len,
                                              const uint8_t *param,
                                              size_t param_len);

OPENSSL_EXPORT int RSA_print(BIO *bio, const RSA *rsa, int indent);

OPENSSL_EXPORT const RSA_PSS_PARAMS *RSA_get0_pss_params(const RSA *rsa);

OPENSSL_EXPORT RSA *RSA_new_method_no_e(const ENGINE *engine, const BIGNUM *n);

struct rsa_meth_st {
  struct openssl_method_common_st common;

  void *app_data;

  int (*init)(RSA *rsa);
  int (*finish)(RSA *rsa);

  int (*sign)(int type, const uint8_t *m, unsigned int m_length,
              uint8_t *sigret, unsigned int *siglen, const RSA *rsa);

  int (*sign_raw)(RSA *rsa, size_t *out_len, uint8_t *out, size_t max_out,
                  const uint8_t *in, size_t in_len, int padding);
  int (*decrypt)(RSA *rsa, size_t *out_len, uint8_t *out, size_t max_out,
                 const uint8_t *in, size_t in_len, int padding);

  int (*private_transform)(RSA *rsa, uint8_t *out, const uint8_t *in,
                           size_t len);

  int flags;
};

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(RSA, RSA_free)
BORINGSSL_MAKE_UP_REF(RSA, RSA_up_ref)

BSSL_NAMESPACE_END

}

#endif

#define RSA_R_BAD_ENCODING 100
#define RSA_R_BAD_E_VALUE 101
#define RSA_R_BAD_FIXED_HEADER_DECRYPT 102
#define RSA_R_BAD_PAD_BYTE_COUNT 103
#define RSA_R_BAD_RSA_PARAMETERS 104
#define RSA_R_BAD_SIGNATURE 105
#define RSA_R_BAD_VERSION 106
#define RSA_R_BLOCK_TYPE_IS_NOT_01 107
#define RSA_R_BN_NOT_INITIALIZED 108
#define RSA_R_CANNOT_RECOVER_MULTI_PRIME_KEY 109
#define RSA_R_CRT_PARAMS_ALREADY_GIVEN 110
#define RSA_R_CRT_VALUES_INCORRECT 111
#define RSA_R_DATA_LEN_NOT_EQUAL_TO_MOD_LEN 112
#define RSA_R_DATA_TOO_LARGE 113
#define RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE 114
#define RSA_R_DATA_TOO_LARGE_FOR_MODULUS 115
#define RSA_R_DATA_TOO_SMALL 116
#define RSA_R_DATA_TOO_SMALL_FOR_KEY_SIZE 117
#define RSA_R_DIGEST_TOO_BIG_FOR_RSA_KEY 118
#define RSA_R_D_E_NOT_CONGRUENT_TO_1 119
#define RSA_R_EMPTY_PUBLIC_KEY 120
#define RSA_R_ENCODE_ERROR 121
#define RSA_R_FIRST_OCTET_INVALID 122
#define RSA_R_INCONSISTENT_SET_OF_CRT_VALUES 123
#define RSA_R_INTERNAL_ERROR 124
#define RSA_R_INVALID_MESSAGE_LENGTH 125
#define RSA_R_KEY_SIZE_TOO_SMALL 126
#define RSA_R_LAST_OCTET_INVALID 127
#define RSA_R_MODULUS_TOO_LARGE 128
#define RSA_R_MUST_HAVE_AT_LEAST_TWO_PRIMES 129
#define RSA_R_NO_PUBLIC_EXPONENT 130
#define RSA_R_NULL_BEFORE_BLOCK_MISSING 131
#define RSA_R_N_NOT_EQUAL_P_Q 132
#define RSA_R_OAEP_DECODING_ERROR 133
#define RSA_R_ONLY_ONE_OF_P_Q_GIVEN 134
#define RSA_R_OUTPUT_BUFFER_TOO_SMALL 135
#define RSA_R_PADDING_CHECK_FAILED 136
#define RSA_R_PKCS_DECODING_ERROR 137
#define RSA_R_SLEN_CHECK_FAILED 138
#define RSA_R_SLEN_RECOVERY_FAILED 139
#define RSA_R_TOO_LONG 140
#define RSA_R_TOO_MANY_ITERATIONS 141
#define RSA_R_UNKNOWN_ALGORITHM_TYPE 142
#define RSA_R_UNKNOWN_PADDING_TYPE 143
#define RSA_R_VALUE_MISSING 144
#define RSA_R_WRONG_SIGNATURE_LENGTH 145
#define RSA_R_PUBLIC_KEY_VALIDATION_FAILED 146
#define RSA_R_D_OUT_OF_RANGE 147
#define RSA_R_BLOCK_TYPE_IS_NOT_02 148

#endif
