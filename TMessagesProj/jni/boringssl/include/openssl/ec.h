// Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_EC_H
#define OPENSSL_HEADER_EC_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {

  POINT_CONVERSION_COMPRESSED = 2,

  POINT_CONVERSION_UNCOMPRESSED = 4,

  POINT_CONVERSION_HYBRID = 6,
} point_conversion_form_t;

OPENSSL_EXPORT const EC_GROUP *EC_group_p224(void);

OPENSSL_EXPORT const EC_GROUP *EC_group_p256(void);

OPENSSL_EXPORT const EC_GROUP *EC_group_p384(void);

OPENSSL_EXPORT const EC_GROUP *EC_group_p521(void);

OPENSSL_EXPORT EC_GROUP *EC_GROUP_new_by_curve_name(int nid);

OPENSSL_EXPORT int EC_GROUP_cmp(const EC_GROUP *a, const EC_GROUP *b,
                                BN_CTX *ignored);

OPENSSL_EXPORT const EC_POINT *EC_GROUP_get0_generator(const EC_GROUP *group);

OPENSSL_EXPORT const BIGNUM *EC_GROUP_get0_order(const EC_GROUP *group);

OPENSSL_EXPORT int EC_GROUP_order_bits(const EC_GROUP *group);

OPENSSL_EXPORT int EC_GROUP_get_cofactor(const EC_GROUP *group,
                                         BIGNUM *cofactor, BN_CTX *ctx);

OPENSSL_EXPORT int EC_GROUP_get_curve_GFp(const EC_GROUP *group, BIGNUM *out_p,
                                          BIGNUM *out_a, BIGNUM *out_b,
                                          BN_CTX *ctx);

OPENSSL_EXPORT int EC_GROUP_get_curve_name(const EC_GROUP *group);

OPENSSL_EXPORT unsigned EC_GROUP_get_degree(const EC_GROUP *group);

OPENSSL_EXPORT const char *EC_curve_nid2nist(int nid);

OPENSSL_EXPORT int EC_curve_nist2nid(const char *name);

OPENSSL_EXPORT EC_POINT *EC_POINT_new(const EC_GROUP *group);

OPENSSL_EXPORT void EC_POINT_free(EC_POINT *point);

OPENSSL_EXPORT int EC_POINT_copy(EC_POINT *dest, const EC_POINT *src);

OPENSSL_EXPORT EC_POINT *EC_POINT_dup(const EC_POINT *src,
                                      const EC_GROUP *group);

OPENSSL_EXPORT int EC_POINT_set_to_infinity(const EC_GROUP *group,
                                            EC_POINT *point);

OPENSSL_EXPORT int EC_POINT_is_at_infinity(const EC_GROUP *group,
                                           const EC_POINT *point);

OPENSSL_EXPORT int EC_POINT_is_on_curve(const EC_GROUP *group,
                                        const EC_POINT *point, BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_cmp(const EC_GROUP *group, const EC_POINT *a,
                                const EC_POINT *b, BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_get_affine_coordinates_GFp(const EC_GROUP *group,
                                                       const EC_POINT *point,
                                                       BIGNUM *x, BIGNUM *y,
                                                       BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_get_affine_coordinates(const EC_GROUP *group,
                                                   const EC_POINT *point,
                                                   BIGNUM *x, BIGNUM *y,
                                                   BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_set_affine_coordinates_GFp(const EC_GROUP *group,
                                                       EC_POINT *point,
                                                       const BIGNUM *x,
                                                       const BIGNUM *y,
                                                       BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_set_affine_coordinates(const EC_GROUP *group,
                                                   EC_POINT *point,
                                                   const BIGNUM *x,
                                                   const BIGNUM *y,
                                                   BN_CTX *ctx);

OPENSSL_EXPORT size_t EC_POINT_point2oct(const EC_GROUP *group,
                                         const EC_POINT *point,
                                         point_conversion_form_t form,
                                         uint8_t *buf, size_t max_out,
                                         BN_CTX *ctx);

OPENSSL_EXPORT size_t EC_POINT_point2buf(const EC_GROUP *group,
                                         const EC_POINT *point,
                                         point_conversion_form_t form,
                                         uint8_t **out_buf, BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_point2cbb(CBB *out, const EC_GROUP *group,
                                      const EC_POINT *point,
                                      point_conversion_form_t form,
                                      BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_oct2point(const EC_GROUP *group, EC_POINT *point,
                                      const uint8_t *buf, size_t len,
                                      BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_set_compressed_coordinates_GFp(
    const EC_GROUP *group, EC_POINT *point, const BIGNUM *x, int y_bit,
    BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_add(const EC_GROUP *group, EC_POINT *r,
                                const EC_POINT *a, const EC_POINT *b,
                                BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_dbl(const EC_GROUP *group, EC_POINT *r,
                                const EC_POINT *a, BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_invert(const EC_GROUP *group, EC_POINT *a,
                                   BN_CTX *ctx);

OPENSSL_EXPORT int EC_POINT_mul(const EC_GROUP *group, EC_POINT *r,
                                const BIGNUM *n, const EC_POINT *q,
                                const BIGNUM *m, BN_CTX *ctx);

OPENSSL_EXPORT int EC_hash_to_curve_p256_xmd_sha256_sswu(
    const EC_GROUP *group, EC_POINT *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int EC_hash_to_curve_p384_xmd_sha384_sswu(
    const EC_GROUP *group, EC_POINT *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT void EC_GROUP_free(EC_GROUP *group);

OPENSSL_EXPORT EC_GROUP *EC_GROUP_dup(const EC_GROUP *group);

OPENSSL_EXPORT EC_GROUP *EC_GROUP_new_curve_GFp(const BIGNUM *p,
                                                const BIGNUM *a,
                                                const BIGNUM *b, BN_CTX *ctx);

OPENSSL_EXPORT int EC_GROUP_set_generator(EC_GROUP *group,
                                          const EC_POINT *generator,
                                          const BIGNUM *order,
                                          const BIGNUM *cofactor);

OPENSSL_EXPORT int EC_GROUP_get_order(const EC_GROUP *group, BIGNUM *order,
                                      BN_CTX *ctx);

#define OPENSSL_EC_EXPLICIT_CURVE 0
#define OPENSSL_EC_NAMED_CURVE 1

OPENSSL_EXPORT void EC_GROUP_set_asn1_flag(EC_GROUP *group, int flag);

OPENSSL_EXPORT int EC_GROUP_get_asn1_flag(const EC_GROUP *group);

typedef struct ec_method_st EC_METHOD;

OPENSSL_EXPORT const EC_METHOD *EC_GROUP_method_of(const EC_GROUP *group);

OPENSSL_EXPORT int EC_METHOD_get_field_type(const EC_METHOD *meth);

OPENSSL_EXPORT void EC_GROUP_set_point_conversion_form(
    EC_GROUP *group, point_conversion_form_t form);

typedef struct {
  int nid;
  const char *comment;
} EC_builtin_curve;

OPENSSL_EXPORT size_t EC_get_builtin_curves(EC_builtin_curve *out_curves,
                                            size_t max_num_curves);

OPENSSL_EXPORT void EC_POINT_clear_free(EC_POINT *point);

#if defined(__cplusplus)
}
#endif

#include <openssl/ec_key.h>

#if defined(__cplusplus)
extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(EC_POINT, EC_POINT_free)
BORINGSSL_MAKE_DELETER(EC_GROUP, EC_GROUP_free)

BSSL_NAMESPACE_END

}

#endif

#define EC_R_BUFFER_TOO_SMALL 100
#define EC_R_COORDINATES_OUT_OF_RANGE 101
#define EC_R_D2I_ECPKPARAMETERS_FAILURE 102
#define EC_R_EC_GROUP_NEW_BY_NAME_FAILURE 103
#define EC_R_GROUP2PKPARAMETERS_FAILURE 104
#define EC_R_I2D_ECPKPARAMETERS_FAILURE 105
#define EC_R_INCOMPATIBLE_OBJECTS 106
#define EC_R_INVALID_COMPRESSED_POINT 107
#define EC_R_INVALID_COMPRESSION_BIT 108
#define EC_R_INVALID_ENCODING 109
#define EC_R_INVALID_FIELD 110
#define EC_R_INVALID_FORM 111
#define EC_R_INVALID_GROUP_ORDER 112
#define EC_R_INVALID_PRIVATE_KEY 113
#define EC_R_MISSING_PARAMETERS 114
#define EC_R_MISSING_PRIVATE_KEY 115
#define EC_R_NON_NAMED_CURVE 116
#define EC_R_NOT_INITIALIZED 117
#define EC_R_PKPARAMETERS2GROUP_FAILURE 118
#define EC_R_POINT_AT_INFINITY 119
#define EC_R_POINT_IS_NOT_ON_CURVE 120
#define EC_R_SLOT_FULL 121
#define EC_R_UNDEFINED_GENERATOR 122
#define EC_R_UNKNOWN_GROUP 123
#define EC_R_UNKNOWN_ORDER 124
#define EC_R_WRONG_ORDER 125
#define EC_R_BIGNUM_OUT_OF_RANGE 126
#define EC_R_WRONG_CURVE_PARAMETERS 127
#define EC_R_DECODE_ERROR 128
#define EC_R_ENCODE_ERROR 129
#define EC_R_GROUP_MISMATCH 130
#define EC_R_INVALID_COFACTOR 131
#define EC_R_PUBLIC_KEY_VALIDATION_FAILED 132
#define EC_R_INVALID_SCALAR 133

#endif
