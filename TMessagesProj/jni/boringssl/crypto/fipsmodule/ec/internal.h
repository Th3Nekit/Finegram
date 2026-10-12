// Copyright 2001-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_EC_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_EC_INTERNAL_H

#include <openssl/base.h>

#include <assert.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ex_data.h>

#include "../bn/internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define EC_MAX_BYTES 66
#define EC_MAX_WORDS ((EC_MAX_BYTES + BN_BYTES - 1) / BN_BYTES)
#define EC_MAX_COMPRESSED (EC_MAX_BYTES + 1)
#define EC_MAX_UNCOMPRESSED (2 * EC_MAX_BYTES + 1)

static_assert(EC_MAX_WORDS <= BN_SMALL_MAX_WORDS,
              "bn_*_small functions not usable");

typedef struct {
  BN_ULONG words[EC_MAX_WORDS];
} EC_SCALAR;

OPENSSL_EXPORT int ec_bignum_to_scalar(const EC_GROUP *group, EC_SCALAR *out,
                                       const BIGNUM *in);

OPENSSL_EXPORT void ec_scalar_to_bytes(const EC_GROUP *group, uint8_t *out,
                                       size_t *out_len, const EC_SCALAR *in);

OPENSSL_EXPORT int ec_scalar_from_bytes(const EC_GROUP *group, EC_SCALAR *out,
                                        const uint8_t *in, size_t len);

void ec_scalar_reduce(const EC_GROUP *group, EC_SCALAR *out,
                      const BN_ULONG *words, size_t num);

int ec_random_scalar(const EC_GROUP *group, EC_SCALAR *out,
                     const uint8_t additional_data[32]);

int ec_random_nonzero_scalar(const EC_GROUP *group, EC_SCALAR *out,
                             const uint8_t additional_data[32]);

int ec_scalar_equal_vartime(const EC_GROUP *group, const EC_SCALAR *a,
                            const EC_SCALAR *b);

int ec_scalar_is_zero(const EC_GROUP *group, const EC_SCALAR *a);

void ec_scalar_add(const EC_GROUP *group, EC_SCALAR *r, const EC_SCALAR *a,
                   const EC_SCALAR *b);

void ec_scalar_sub(const EC_GROUP *group, EC_SCALAR *r, const EC_SCALAR *a,
                   const EC_SCALAR *b);

void ec_scalar_neg(const EC_GROUP *group, EC_SCALAR *r, const EC_SCALAR *a);

void ec_scalar_to_montgomery(const EC_GROUP *group, EC_SCALAR *r,
                             const EC_SCALAR *a);

void ec_scalar_from_montgomery(const EC_GROUP *group, EC_SCALAR *r,
                               const EC_SCALAR *a);

void ec_scalar_mul_montgomery(const EC_GROUP *group, EC_SCALAR *r,
                              const EC_SCALAR *a, const EC_SCALAR *b);

void ec_scalar_inv0_montgomery(const EC_GROUP *group, EC_SCALAR *r,
                               const EC_SCALAR *a);

int ec_scalar_to_montgomery_inv_vartime(const EC_GROUP *group, EC_SCALAR *r,
                                        const EC_SCALAR *a);

void ec_scalar_select(const EC_GROUP *group, EC_SCALAR *out, BN_ULONG mask,
                      const EC_SCALAR *a, const EC_SCALAR *b);

typedef struct {
  BN_ULONG words[EC_MAX_WORDS];
} EC_FELEM;

const EC_FELEM *ec_felem_one(const EC_GROUP *group);

int ec_bignum_to_felem(const EC_GROUP *group, EC_FELEM *out, const BIGNUM *in);

int ec_felem_to_bignum(const EC_GROUP *group, BIGNUM *out, const EC_FELEM *in);

void ec_felem_to_bytes(const EC_GROUP *group, uint8_t *out, size_t *out_len,
                       const EC_FELEM *in);

int ec_felem_from_bytes(const EC_GROUP *group, EC_FELEM *out, const uint8_t *in,
                        size_t len);

void ec_felem_neg(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a);

void ec_felem_add(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a,
                  const EC_FELEM *b);

void ec_felem_sub(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a,
                  const EC_FELEM *b);

BN_ULONG ec_felem_non_zero_mask(const EC_GROUP *group, const EC_FELEM *a);

void ec_felem_select(const EC_GROUP *group, EC_FELEM *out, BN_ULONG mask,
                     const EC_FELEM *a, const EC_FELEM *b);

int ec_felem_equal(const EC_GROUP *group, const EC_FELEM *a, const EC_FELEM *b);

typedef struct {

  EC_FELEM X, Y, Z;
} EC_JACOBIAN;

typedef struct {
  EC_FELEM X, Y;
} EC_AFFINE;

void ec_affine_to_jacobian(const EC_GROUP *group, EC_JACOBIAN *out,
                           const EC_AFFINE *p);

OPENSSL_EXPORT int ec_jacobian_to_affine(const EC_GROUP *group, EC_AFFINE *out,
                                         const EC_JACOBIAN *p);

int ec_jacobian_to_affine_batch(const EC_GROUP *group, EC_AFFINE *out,
                                const EC_JACOBIAN *in, size_t num);

int ec_point_set_affine_coordinates(const EC_GROUP *group, EC_AFFINE *out,
                                    const EC_FELEM *x, const EC_FELEM *y);

int ec_point_mul_no_self_test(const EC_GROUP *group, EC_POINT *r,
                              const BIGNUM *g_scalar, const EC_POINT *p,
                              const BIGNUM *p_scalar, BN_CTX *ctx);

int ec_point_mul_scalar(const EC_GROUP *group, EC_JACOBIAN *r,
                        const EC_JACOBIAN *p, const EC_SCALAR *scalar);

int ec_point_mul_scalar_base(const EC_GROUP *group, EC_JACOBIAN *r,
                             const EC_SCALAR *scalar);

int ec_point_mul_scalar_batch(const EC_GROUP *group, EC_JACOBIAN *r,
                              const EC_JACOBIAN *p0, const EC_SCALAR *scalar0,
                              const EC_JACOBIAN *p1, const EC_SCALAR *scalar1,
                              const EC_JACOBIAN *p2, const EC_SCALAR *scalar2);

#define EC_MONT_PRECOMP_COMB_SIZE 5

typedef union {
  EC_AFFINE comb[(1 << EC_MONT_PRECOMP_COMB_SIZE) - 1];
} EC_PRECOMP;

int ec_init_precomp(const EC_GROUP *group, EC_PRECOMP *out,
                    const EC_JACOBIAN *p);

int ec_point_mul_scalar_precomp(const EC_GROUP *group, EC_JACOBIAN *r,
                                const EC_PRECOMP *p0, const EC_SCALAR *scalar0,
                                const EC_PRECOMP *p1, const EC_SCALAR *scalar1,
                                const EC_PRECOMP *p2, const EC_SCALAR *scalar2);

OPENSSL_EXPORT int ec_point_mul_scalar_public(const EC_GROUP *group,
                                              EC_JACOBIAN *r,
                                              const EC_SCALAR *g_scalar,
                                              const EC_JACOBIAN *p,
                                              const EC_SCALAR *p_scalar);

int ec_point_mul_scalar_public_batch(const EC_GROUP *group, EC_JACOBIAN *r,
                                     const EC_SCALAR *g_scalar,
                                     const EC_JACOBIAN *points,
                                     const EC_SCALAR *scalars, size_t num);

void ec_point_select(const EC_GROUP *group, EC_JACOBIAN *out, BN_ULONG mask,
                     const EC_JACOBIAN *a, const EC_JACOBIAN *b);

void ec_affine_select(const EC_GROUP *group, EC_AFFINE *out, BN_ULONG mask,
                      const EC_AFFINE *a, const EC_AFFINE *b);

void ec_precomp_select(const EC_GROUP *group, EC_PRECOMP *out, BN_ULONG mask,
                       const EC_PRECOMP *a, const EC_PRECOMP *b);

int ec_cmp_x_coordinate(const EC_GROUP *group, const EC_JACOBIAN *p,
                        const EC_SCALAR *r);

int ec_get_x_coordinate_as_scalar(const EC_GROUP *group, EC_SCALAR *out,
                                  const EC_JACOBIAN *p);

int ec_get_x_coordinate_as_bytes(const EC_GROUP *group, uint8_t *out,
                                 size_t *out_len, size_t max_out,
                                 const EC_JACOBIAN *p);

size_t ec_point_byte_len(const EC_GROUP *group, point_conversion_form_t form);

size_t ec_point_to_bytes(const EC_GROUP *group, const EC_AFFINE *point,
                         point_conversion_form_t form, uint8_t *buf,
                         size_t max_out);

int ec_point_from_uncompressed(const EC_GROUP *group, EC_AFFINE *out,
                               const uint8_t *in, size_t len);

void ec_set_to_safe_point(const EC_GROUP *group, EC_JACOBIAN *out);

int ec_affine_jacobian_equal(const EC_GROUP *group, const EC_AFFINE *a,
                             const EC_JACOBIAN *b);

struct ec_method_st {

  int (*point_get_affine_coordinates)(const EC_GROUP *, const EC_JACOBIAN *p,
                                      EC_FELEM *x, EC_FELEM *y);

  int (*jacobian_to_affine_batch)(const EC_GROUP *group, EC_AFFINE *out,
                                  const EC_JACOBIAN *in, size_t num);

  void (*add)(const EC_GROUP *group, EC_JACOBIAN *r, const EC_JACOBIAN *a,
              const EC_JACOBIAN *b);

  void (*dbl)(const EC_GROUP *group, EC_JACOBIAN *r, const EC_JACOBIAN *a);

  void (*mul)(const EC_GROUP *group, EC_JACOBIAN *r, const EC_JACOBIAN *p,
              const EC_SCALAR *scalar);

  void (*mul_base)(const EC_GROUP *group, EC_JACOBIAN *r,
                   const EC_SCALAR *scalar);

  void (*mul_batch)(const EC_GROUP *group, EC_JACOBIAN *r,
                    const EC_JACOBIAN *p0, const EC_SCALAR *scalar0,
                    const EC_JACOBIAN *p1, const EC_SCALAR *scalar1,
                    const EC_JACOBIAN *p2, const EC_SCALAR *scalar2);

  void (*mul_public)(const EC_GROUP *group, EC_JACOBIAN *r,
                     const EC_SCALAR *g_scalar, const EC_JACOBIAN *p,
                     const EC_SCALAR *p_scalar);

  int (*mul_public_batch)(const EC_GROUP *group, EC_JACOBIAN *r,
                          const EC_SCALAR *g_scalar, const EC_JACOBIAN *points,
                          const EC_SCALAR *scalars, size_t num);

  int (*init_precomp)(const EC_GROUP *group, EC_PRECOMP *out,
                      const EC_JACOBIAN *p);

  void (*mul_precomp)(const EC_GROUP *group, EC_JACOBIAN *r,
                      const EC_PRECOMP *p0, const EC_SCALAR *scalar0,
                      const EC_PRECOMP *p1, const EC_SCALAR *scalar1,
                      const EC_PRECOMP *p2, const EC_SCALAR *scalar2);

  void (*felem_mul)(const EC_GROUP *, EC_FELEM *r, const EC_FELEM *a,
                    const EC_FELEM *b);
  void (*felem_sqr)(const EC_GROUP *, EC_FELEM *r, const EC_FELEM *a);

  void (*felem_to_bytes)(const EC_GROUP *group, uint8_t *out, size_t *out_len,
                         const EC_FELEM *in);
  int (*felem_from_bytes)(const EC_GROUP *group, EC_FELEM *out,
                          const uint8_t *in, size_t len);

  void (*felem_reduce)(const EC_GROUP *group, EC_FELEM *out,
                       const BN_ULONG *words, size_t num);

  void (*felem_exp)(const EC_GROUP *group, EC_FELEM *out, const EC_FELEM *a,
                    const BN_ULONG *exp, size_t num_exp);

  void (*scalar_inv0_montgomery)(const EC_GROUP *group, EC_SCALAR *out,
                                 const EC_SCALAR *in);

  int (*scalar_to_montgomery_inv_vartime)(const EC_GROUP *group, EC_SCALAR *out,
                                          const EC_SCALAR *in);

  int (*cmp_x_coordinate)(const EC_GROUP *group, const EC_JACOBIAN *p,
                          const EC_SCALAR *r);
}                ;

const EC_METHOD *EC_GFp_mont_method(void);

struct ec_point_st {

  EC_GROUP *group;

  EC_JACOBIAN raw;
}               ;

struct ec_group_st {
  const EC_METHOD *meth;

  EC_POINT generator;

  BN_MONT_CTX order;
  BN_MONT_CTX field;

  EC_FELEM a, b;

  const char *comment;

  int curve_name;
  uint8_t oid[9];
  uint8_t oid_len;

  int a_is_minus3;

  int has_order;

  int field_greater_than_order;

  CRYPTO_refcount_t references;
}               ;

EC_GROUP *ec_group_new(const EC_METHOD *meth, const BIGNUM *p, const BIGNUM *a,
                       const BIGNUM *b, BN_CTX *ctx);

void ec_GFp_mont_mul(const EC_GROUP *group, EC_JACOBIAN *r,
                     const EC_JACOBIAN *p, const EC_SCALAR *scalar);
void ec_GFp_mont_mul_base(const EC_GROUP *group, EC_JACOBIAN *r,
                          const EC_SCALAR *scalar);
void ec_GFp_mont_mul_batch(const EC_GROUP *group, EC_JACOBIAN *r,
                           const EC_JACOBIAN *p0, const EC_SCALAR *scalar0,
                           const EC_JACOBIAN *p1, const EC_SCALAR *scalar1,
                           const EC_JACOBIAN *p2, const EC_SCALAR *scalar2);
int ec_GFp_mont_init_precomp(const EC_GROUP *group, EC_PRECOMP *out,
                             const EC_JACOBIAN *p);
void ec_GFp_mont_mul_precomp(const EC_GROUP *group, EC_JACOBIAN *r,
                             const EC_PRECOMP *p0, const EC_SCALAR *scalar0,
                             const EC_PRECOMP *p1, const EC_SCALAR *scalar1,
                             const EC_PRECOMP *p2, const EC_SCALAR *scalar2);
void ec_GFp_mont_felem_reduce(const EC_GROUP *group, EC_FELEM *out,
                              const BN_ULONG *words, size_t num);
void ec_GFp_mont_felem_exp(const EC_GROUP *group, EC_FELEM *out,
                           const EC_FELEM *a, const BN_ULONG *exp,
                           size_t num_exp);

void ec_compute_wNAF(const EC_GROUP *group, int8_t *out,
                     const EC_SCALAR *scalar, size_t bits, int w);

int ec_GFp_mont_mul_public_batch(const EC_GROUP *group, EC_JACOBIAN *r,
                                 const EC_SCALAR *g_scalar,
                                 const EC_JACOBIAN *points,
                                 const EC_SCALAR *scalars, size_t num);

int ec_GFp_simple_group_set_curve(EC_GROUP *, const BIGNUM *p, const BIGNUM *a,
                                  const BIGNUM *b, BN_CTX *);
int ec_GFp_simple_group_get_curve(const EC_GROUP *, BIGNUM *p, BIGNUM *a,
                                  BIGNUM *b);
void ec_GFp_simple_point_init(EC_JACOBIAN *);
void ec_GFp_simple_point_copy(EC_JACOBIAN *, const EC_JACOBIAN *);
void ec_GFp_simple_point_set_to_infinity(const EC_GROUP *, EC_JACOBIAN *);
void ec_GFp_mont_add(const EC_GROUP *, EC_JACOBIAN *r, const EC_JACOBIAN *a,
                     const EC_JACOBIAN *b);
void ec_GFp_mont_dbl(const EC_GROUP *, EC_JACOBIAN *r, const EC_JACOBIAN *a);
void ec_GFp_simple_invert(const EC_GROUP *, EC_JACOBIAN *);
int ec_GFp_simple_is_at_infinity(const EC_GROUP *, const EC_JACOBIAN *);
int ec_GFp_simple_is_on_curve(const EC_GROUP *, const EC_JACOBIAN *);
int ec_GFp_simple_points_equal(const EC_GROUP *, const EC_JACOBIAN *a,
                               const EC_JACOBIAN *b);
void ec_simple_scalar_inv0_montgomery(const EC_GROUP *group, EC_SCALAR *r,
                                      const EC_SCALAR *a);

int ec_simple_scalar_to_montgomery_inv_vartime(const EC_GROUP *group,
                                               EC_SCALAR *r,
                                               const EC_SCALAR *a);

int ec_GFp_simple_cmp_x_coordinate(const EC_GROUP *group, const EC_JACOBIAN *p,
                                   const EC_SCALAR *r);

void ec_GFp_simple_felem_to_bytes(const EC_GROUP *group, uint8_t *out,
                                  size_t *out_len, const EC_FELEM *in);
int ec_GFp_simple_felem_from_bytes(const EC_GROUP *group, EC_FELEM *out,
                                   const uint8_t *in, size_t len);

void ec_GFp_mont_felem_mul(const EC_GROUP *, EC_FELEM *r, const EC_FELEM *a,
                           const EC_FELEM *b);
void ec_GFp_mont_felem_sqr(const EC_GROUP *, EC_FELEM *r, const EC_FELEM *a);

void ec_GFp_mont_felem_to_bytes(const EC_GROUP *group, uint8_t *out,
                                size_t *out_len, const EC_FELEM *in);
int ec_GFp_mont_felem_from_bytes(const EC_GROUP *group, EC_FELEM *out,
                                 const uint8_t *in, size_t len);

void ec_GFp_nistp_recode_scalar_bits(crypto_word_t *sign, crypto_word_t *digit,
                                     crypto_word_t in);

const EC_METHOD *EC_GFp_nistp224_method(void);
const EC_METHOD *EC_GFp_nistp256_method(void);

const EC_METHOD *EC_GFp_nistz256_method(void);

typedef struct {
  BIGNUM bignum;
  EC_SCALAR scalar;
} EC_WRAPPED_SCALAR;

struct ec_key_st {
  EC_GROUP *group;

  EC_POINT *pub_key;
  EC_WRAPPED_SCALAR *priv_key;

  unsigned int enc_flag;
  point_conversion_form_t conv_form;

  CRYPTO_refcount_t references;

  ECDSA_METHOD *ecdsa_meth;

  CRYPTO_EX_DATA ex_data;
}             ;

#if defined(__cplusplus)
}
#endif

#endif
