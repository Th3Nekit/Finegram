// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OPENSSL_HEADER_BN_H
#define OPENSSL_HEADER_BN_H

#include <openssl/base.h>
#include <openssl/thread.h>

#include <inttypes.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(OPENSSL_64_BIT)
typedef uint64_t BN_ULONG;
#define BN_BITS2 64
#define BN_DEC_FMT1 "%" PRIu64
#define BN_HEX_FMT1 "%" PRIx64
#define BN_HEX_FMT2 "%016" PRIx64
#elif defined(OPENSSL_32_BIT)
typedef uint32_t BN_ULONG;
#define BN_BITS2 32
#define BN_DEC_FMT1 "%" PRIu32
#define BN_HEX_FMT1 "%" PRIx32
#define BN_HEX_FMT2 "%08" PRIx32
#else
#error "Must define either OPENSSL_32_BIT or OPENSSL_64_BIT"
#endif

OPENSSL_EXPORT BIGNUM *BN_new(void);

OPENSSL_EXPORT void BN_init(BIGNUM *bn);

OPENSSL_EXPORT void BN_free(BIGNUM *bn);

OPENSSL_EXPORT void BN_clear_free(BIGNUM *bn);

OPENSSL_EXPORT BIGNUM *BN_dup(const BIGNUM *src);

OPENSSL_EXPORT BIGNUM *BN_copy(BIGNUM *dest, const BIGNUM *src);

OPENSSL_EXPORT void BN_clear(BIGNUM *bn);

OPENSSL_EXPORT const BIGNUM *BN_value_one(void);

OPENSSL_EXPORT unsigned BN_num_bits(const BIGNUM *bn);

OPENSSL_EXPORT unsigned BN_num_bytes(const BIGNUM *bn);

OPENSSL_EXPORT void BN_zero(BIGNUM *bn);

OPENSSL_EXPORT int BN_one(BIGNUM *bn);

OPENSSL_EXPORT int BN_set_word(BIGNUM *bn, BN_ULONG value);

OPENSSL_EXPORT int BN_set_u64(BIGNUM *bn, uint64_t value);

OPENSSL_EXPORT void BN_set_negative(BIGNUM *bn, int sign);

OPENSSL_EXPORT int BN_is_negative(const BIGNUM *bn);

OPENSSL_EXPORT BIGNUM *BN_bin2bn(const uint8_t *in, size_t len, BIGNUM *ret);

OPENSSL_EXPORT size_t BN_bn2bin(const BIGNUM *in, uint8_t *out);

OPENSSL_EXPORT BIGNUM *BN_lebin2bn(const uint8_t *in, size_t len, BIGNUM *ret);

OPENSSL_EXPORT int BN_bn2le_padded(uint8_t *out, size_t len, const BIGNUM *in);

OPENSSL_EXPORT int BN_bn2bin_padded(uint8_t *out, size_t len, const BIGNUM *in);

OPENSSL_EXPORT int BN_bn2cbb_padded(CBB *out, size_t len, const BIGNUM *in);

OPENSSL_EXPORT char *BN_bn2hex(const BIGNUM *bn);

OPENSSL_EXPORT int BN_hex2bn(BIGNUM **outp, const char *in);

OPENSSL_EXPORT char *BN_bn2dec(const BIGNUM *a);

OPENSSL_EXPORT int BN_dec2bn(BIGNUM **outp, const char *in);

OPENSSL_EXPORT int BN_asc2bn(BIGNUM **outp, const char *in);

OPENSSL_EXPORT int BN_print(BIO *bio, const BIGNUM *a);

OPENSSL_EXPORT int BN_print_fp(FILE *fp, const BIGNUM *a);

OPENSSL_EXPORT BN_ULONG BN_get_word(const BIGNUM *bn);

OPENSSL_EXPORT int BN_get_u64(const BIGNUM *bn, uint64_t *out);

OPENSSL_EXPORT int BN_parse_asn1_unsigned(CBS *cbs, BIGNUM *ret);

OPENSSL_EXPORT int BN_marshal_asn1(CBB *cbb, const BIGNUM *bn);

OPENSSL_EXPORT BN_CTX *BN_CTX_new(void);

OPENSSL_EXPORT void BN_CTX_free(BN_CTX *ctx);

OPENSSL_EXPORT void BN_CTX_start(BN_CTX *ctx);

OPENSSL_EXPORT BIGNUM *BN_CTX_get(BN_CTX *ctx);

OPENSSL_EXPORT void BN_CTX_end(BN_CTX *ctx);

OPENSSL_EXPORT int BN_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);

OPENSSL_EXPORT int BN_uadd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);

OPENSSL_EXPORT int BN_add_word(BIGNUM *a, BN_ULONG w);

OPENSSL_EXPORT int BN_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);

OPENSSL_EXPORT int BN_usub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);

OPENSSL_EXPORT int BN_sub_word(BIGNUM *a, BN_ULONG w);

OPENSSL_EXPORT int BN_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                          BN_CTX *ctx);

OPENSSL_EXPORT int BN_mul_word(BIGNUM *bn, BN_ULONG w);

OPENSSL_EXPORT int BN_sqr(BIGNUM *r, const BIGNUM *a, BN_CTX *ctx);

OPENSSL_EXPORT int BN_div(BIGNUM *quotient, BIGNUM *rem,
                          const BIGNUM *numerator, const BIGNUM *divisor,
                          BN_CTX *ctx);

OPENSSL_EXPORT BN_ULONG BN_div_word(BIGNUM *numerator, BN_ULONG divisor);

OPENSSL_EXPORT int BN_sqrt(BIGNUM *out_sqrt, const BIGNUM *in, BN_CTX *ctx);

OPENSSL_EXPORT int BN_cmp(const BIGNUM *a, const BIGNUM *b);

OPENSSL_EXPORT int BN_cmp_word(const BIGNUM *a, BN_ULONG b);

OPENSSL_EXPORT int BN_ucmp(const BIGNUM *a, const BIGNUM *b);

OPENSSL_EXPORT int BN_equal_consttime(const BIGNUM *a, const BIGNUM *b);

OPENSSL_EXPORT int BN_abs_is_word(const BIGNUM *bn, BN_ULONG w);

OPENSSL_EXPORT int BN_is_zero(const BIGNUM *bn);

OPENSSL_EXPORT int BN_is_one(const BIGNUM *bn);

OPENSSL_EXPORT int BN_is_word(const BIGNUM *bn, BN_ULONG w);

OPENSSL_EXPORT int BN_is_odd(const BIGNUM *bn);

OPENSSL_EXPORT int BN_is_pow2(const BIGNUM *a);

OPENSSL_EXPORT int BN_lshift(BIGNUM *r, const BIGNUM *a, int n);

OPENSSL_EXPORT int BN_lshift1(BIGNUM *r, const BIGNUM *a);

OPENSSL_EXPORT int BN_rshift(BIGNUM *r, const BIGNUM *a, int n);

OPENSSL_EXPORT int BN_rshift1(BIGNUM *r, const BIGNUM *a);

OPENSSL_EXPORT int BN_set_bit(BIGNUM *a, int n);

OPENSSL_EXPORT int BN_clear_bit(BIGNUM *a, int n);

OPENSSL_EXPORT int BN_is_bit_set(const BIGNUM *a, int n);

OPENSSL_EXPORT int BN_mask_bits(BIGNUM *a, int n);

OPENSSL_EXPORT int BN_count_low_zero_bits(const BIGNUM *bn);

OPENSSL_EXPORT BN_ULONG BN_mod_word(const BIGNUM *a, BN_ULONG w);

OPENSSL_EXPORT int BN_mod_pow2(BIGNUM *r, const BIGNUM *a, size_t e);

OPENSSL_EXPORT int BN_nnmod_pow2(BIGNUM *r, const BIGNUM *a, size_t e);

#define BN_mod(rem, numerator, divisor, ctx) \
  BN_div(NULL, (rem), (numerator), (divisor), (ctx))

OPENSSL_EXPORT int BN_nnmod(BIGNUM *rem, const BIGNUM *numerator,
                            const BIGNUM *divisor, BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                              const BIGNUM *m, BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_add_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                                    const BIGNUM *m);

OPENSSL_EXPORT int BN_mod_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                              const BIGNUM *m, BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_sub_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                                    const BIGNUM *m);

OPENSSL_EXPORT int BN_mod_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                              const BIGNUM *m, BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_sqr(BIGNUM *r, const BIGNUM *a, const BIGNUM *m,
                              BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_lshift(BIGNUM *r, const BIGNUM *a, int n,
                                 const BIGNUM *m, BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_lshift_quick(BIGNUM *r, const BIGNUM *a, int n,
                                       const BIGNUM *m);

OPENSSL_EXPORT int BN_mod_lshift1(BIGNUM *r, const BIGNUM *a, const BIGNUM *m,
                                  BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_lshift1_quick(BIGNUM *r, const BIGNUM *a,
                                        const BIGNUM *m);

OPENSSL_EXPORT BIGNUM *BN_mod_sqrt(BIGNUM *in, const BIGNUM *a, const BIGNUM *p,
                                   BN_CTX *ctx);

#define BN_RAND_TOP_ANY    (-1)
#define BN_RAND_TOP_ONE     0
#define BN_RAND_TOP_TWO     1

#define BN_RAND_BOTTOM_ANY  0
#define BN_RAND_BOTTOM_ODD  1

OPENSSL_EXPORT int BN_rand(BIGNUM *rnd, int bits, int top, int bottom);

OPENSSL_EXPORT int BN_pseudo_rand(BIGNUM *rnd, int bits, int top, int bottom);

OPENSSL_EXPORT int BN_rand_range(BIGNUM *rnd, const BIGNUM *range);

OPENSSL_EXPORT int BN_rand_range_ex(BIGNUM *r, BN_ULONG min_inclusive,
                                    const BIGNUM *max_exclusive);

OPENSSL_EXPORT int BN_pseudo_rand_range(BIGNUM *rnd, const BIGNUM *range);

#define BN_GENCB_GENERATED 0
#define BN_GENCB_PRIME_TEST 1

struct bn_gencb_st {
  void *arg;
  int (*callback)(int event, int n, struct bn_gencb_st *);
};

OPENSSL_EXPORT BN_GENCB *BN_GENCB_new(void);

OPENSSL_EXPORT void BN_GENCB_free(BN_GENCB *callback);

OPENSSL_EXPORT void BN_GENCB_set(BN_GENCB *callback,
                                 int (*f)(int event, int n, BN_GENCB *),
                                 void *arg);

OPENSSL_EXPORT int BN_GENCB_call(BN_GENCB *callback, int event, int n);

OPENSSL_EXPORT void *BN_GENCB_get_arg(const BN_GENCB *callback);

OPENSSL_EXPORT int BN_generate_prime_ex(BIGNUM *ret, int bits, int safe,
                                        const BIGNUM *add, const BIGNUM *rem,
                                        BN_GENCB *cb);

#define BN_prime_checks_for_validation 64

#define BN_prime_checks_for_generation 0

enum bn_primality_result_t {
  bn_probably_prime,
  bn_composite,
  bn_non_prime_power_composite,
};

OPENSSL_EXPORT int BN_enhanced_miller_rabin_primality_test(
    enum bn_primality_result_t *out_result, const BIGNUM *w, int checks,
    BN_CTX *ctx, BN_GENCB *cb);

OPENSSL_EXPORT int BN_primality_test(int *is_probably_prime,
                                     const BIGNUM *candidate, int checks,
                                     BN_CTX *ctx, int do_trial_division,
                                     BN_GENCB *cb);

OPENSSL_EXPORT int BN_is_prime_fasttest_ex(const BIGNUM *candidate, int checks,
                                           BN_CTX *ctx, int do_trial_division,
                                           BN_GENCB *cb);

OPENSSL_EXPORT int BN_is_prime_ex(const BIGNUM *candidate, int checks,
                                  BN_CTX *ctx, BN_GENCB *cb);

OPENSSL_EXPORT int BN_gcd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                          BN_CTX *ctx);

OPENSSL_EXPORT BIGNUM *BN_mod_inverse(BIGNUM *out, const BIGNUM *a,
                                      const BIGNUM *n, BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_inverse_blinded(BIGNUM *out, int *out_no_inverse,
                                          const BIGNUM *a,
                                          const BN_MONT_CTX *mont, BN_CTX *ctx);

int BN_mod_inverse_odd(BIGNUM *out, int *out_no_inverse, const BIGNUM *a,
                       const BIGNUM *n, BN_CTX *ctx);

OPENSSL_EXPORT BN_MONT_CTX *BN_MONT_CTX_new_for_modulus(const BIGNUM *mod,
                                                        BN_CTX *ctx);

OPENSSL_EXPORT BN_MONT_CTX *BN_MONT_CTX_new_consttime(const BIGNUM *mod,
                                                      BN_CTX *ctx);

OPENSSL_EXPORT void BN_MONT_CTX_free(BN_MONT_CTX *mont);

OPENSSL_EXPORT BN_MONT_CTX *BN_MONT_CTX_copy(BN_MONT_CTX *to,
                                             const BN_MONT_CTX *from);

OPENSSL_EXPORT int BN_to_montgomery(BIGNUM *ret, const BIGNUM *a,
                                    const BN_MONT_CTX *mont, BN_CTX *ctx);

OPENSSL_EXPORT int BN_from_montgomery(BIGNUM *ret, const BIGNUM *a,
                                      const BN_MONT_CTX *mont, BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_mul_montgomery(BIGNUM *r, const BIGNUM *a,
                                         const BIGNUM *b,
                                         const BN_MONT_CTX *mont, BN_CTX *ctx);

OPENSSL_EXPORT int BN_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                          BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                              const BIGNUM *m, BN_CTX *ctx);

OPENSSL_EXPORT int BN_mod_exp_mont(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
                                   const BIGNUM *m, BN_CTX *ctx,
                                   const BN_MONT_CTX *mont);

OPENSSL_EXPORT int BN_mod_exp_mont_consttime(BIGNUM *rr, const BIGNUM *a,
                                             const BIGNUM *p, const BIGNUM *m,
                                             BN_CTX *ctx,
                                             const BN_MONT_CTX *mont);

OPENSSL_EXPORT size_t BN_bn2mpi(const BIGNUM *in, uint8_t *out);

OPENSSL_EXPORT BIGNUM *BN_mpi2bn(const uint8_t *in, size_t len, BIGNUM *out);

OPENSSL_EXPORT int BN_mod_exp_mont_word(BIGNUM *r, BN_ULONG a, const BIGNUM *p,
                                        const BIGNUM *m, BN_CTX *ctx,
                                        const BN_MONT_CTX *mont);

OPENSSL_EXPORT int BN_mod_exp2_mont(BIGNUM *r, const BIGNUM *a1,
                                    const BIGNUM *p1, const BIGNUM *a2,
                                    const BIGNUM *p2, const BIGNUM *m,
                                    BN_CTX *ctx, const BN_MONT_CTX *mont);

OPENSSL_EXPORT BN_MONT_CTX *BN_MONT_CTX_new(void);

OPENSSL_EXPORT int BN_MONT_CTX_set(BN_MONT_CTX *mont, const BIGNUM *mod,
                                   BN_CTX *ctx);

OPENSSL_EXPORT int BN_bn2binpad(const BIGNUM *in, uint8_t *out, int len);

OPENSSL_EXPORT int BN_bn2lebinpad(const BIGNUM *in, uint8_t *out, int len);

#define BN_prime_checks BN_prime_checks_for_validation

OPENSSL_EXPORT BIGNUM *BN_secure_new(void);

OPENSSL_EXPORT BIGNUM *BN_le2bn(const uint8_t *in, size_t len, BIGNUM *ret);

struct bignum_st {

  BN_ULONG *d;

  int width;

  int dmax;

  int neg;

  int flags;
};

struct bn_mont_ctx_st {

  BIGNUM RR;

  BIGNUM N;
  BN_ULONG n0[2];
};

OPENSSL_EXPORT unsigned BN_num_bits_word(BN_ULONG l);

#define BN_FLG_MALLOCED 0x01
#define BN_FLG_STATIC_DATA 0x02

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(BIGNUM, BN_free)
BORINGSSL_MAKE_DELETER(BN_CTX, BN_CTX_free)
BORINGSSL_MAKE_DELETER(BN_MONT_CTX, BN_MONT_CTX_free)

class BN_CTXScope {
 public:
  BN_CTXScope(BN_CTX *ctx) : ctx_(ctx) { BN_CTX_start(ctx_); }
  ~BN_CTXScope() { BN_CTX_end(ctx_); }

 private:
  BN_CTX *ctx_;

  BN_CTXScope(BN_CTXScope &) = delete;
  BN_CTXScope &operator=(BN_CTXScope &) = delete;
};

BSSL_NAMESPACE_END

}
#endif

#endif

#define BN_R_ARG2_LT_ARG3 100
#define BN_R_BAD_RECIPROCAL 101
#define BN_R_BIGNUM_TOO_LONG 102
#define BN_R_BITS_TOO_SMALL 103
#define BN_R_CALLED_WITH_EVEN_MODULUS 104
#define BN_R_DIV_BY_ZERO 105
#define BN_R_EXPAND_ON_STATIC_BIGNUM_DATA 106
#define BN_R_INPUT_NOT_REDUCED 107
#define BN_R_INVALID_RANGE 108
#define BN_R_NEGATIVE_NUMBER 109
#define BN_R_NOT_A_SQUARE 110
#define BN_R_NOT_INITIALIZED 111
#define BN_R_NO_INVERSE 112
#define BN_R_PRIVATE_KEY_TOO_LARGE 113
#define BN_R_P_IS_NOT_PRIME 114
#define BN_R_TOO_MANY_ITERATIONS 115
#define BN_R_TOO_MANY_TEMPORARY_VARIABLES 116
#define BN_R_BAD_ENCODING 117
#define BN_R_ENCODE_ERROR 118
#define BN_R_INVALID_INPUT 119

#endif
