// Copyright 2014-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2014, Intel Corporation. All Rights Reserved.
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
//
// Originally written by Shay Gueron (1, 2), and Vlad Krasnov (1)
// (1) Intel Corporation, Israel Development Center, Haifa, Israel
// (2) University of Haifa, Israel
//
// Reference:
// S.Gueron and V.Krasnov, "Fast Prime Field Elliptic Curve Cryptography with
//                          256 Bit Primes"

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_EC_P256_NISTZ_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_EC_P256_NISTZ_H

#include <openssl/base.h>

#include <openssl/bn.h>

#include "../bn/internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(OPENSSL_NO_ASM) && \
    (defined(OPENSSL_X86_64) || defined(OPENSSL_AARCH64)) &&   \
    !defined(OPENSSL_SMALL)

#define P256_LIMBS (256 / BN_BITS2)

void ecp_nistz256_neg(BN_ULONG res[P256_LIMBS], const BN_ULONG a[P256_LIMBS]);

#if defined(OPENSSL_X86_64)
void ecp_nistz256_mul_mont_nohw(BN_ULONG res[P256_LIMBS],
                                const BN_ULONG a[P256_LIMBS],
                                const BN_ULONG b[P256_LIMBS]);
void ecp_nistz256_mul_mont_adx(BN_ULONG res[P256_LIMBS],
                               const BN_ULONG a[P256_LIMBS],
                               const BN_ULONG b[P256_LIMBS]);
#else
void ecp_nistz256_mul_mont(BN_ULONG res[P256_LIMBS],
                           const BN_ULONG a[P256_LIMBS],
                           const BN_ULONG b[P256_LIMBS]);
#endif

#if defined(OPENSSL_X86_64)
void ecp_nistz256_sqr_mont_nohw(BN_ULONG res[P256_LIMBS],
                                const BN_ULONG a[P256_LIMBS]);
void ecp_nistz256_sqr_mont_adx(BN_ULONG res[P256_LIMBS],
                               const BN_ULONG a[P256_LIMBS]);
#else
void ecp_nistz256_sqr_mont(BN_ULONG res[P256_LIMBS],
                           const BN_ULONG a[P256_LIMBS]);
#endif

#if defined(OPENSSL_X86_64)
void ecp_nistz256_ord_mul_mont_nohw(BN_ULONG res[P256_LIMBS],
                                    const BN_ULONG a[P256_LIMBS],
                                    const BN_ULONG b[P256_LIMBS]);
void ecp_nistz256_ord_mul_mont_adx(BN_ULONG res[P256_LIMBS],
                                   const BN_ULONG a[P256_LIMBS],
                                   const BN_ULONG b[P256_LIMBS]);
#else
void ecp_nistz256_ord_mul_mont(BN_ULONG res[P256_LIMBS],
                               const BN_ULONG a[P256_LIMBS],
                               const BN_ULONG b[P256_LIMBS]);
#endif

#if defined(OPENSSL_X86_64)
void ecp_nistz256_ord_sqr_mont_nohw(BN_ULONG res[P256_LIMBS],
                                    const BN_ULONG a[P256_LIMBS], BN_ULONG rep);
void ecp_nistz256_ord_sqr_mont_adx(BN_ULONG res[P256_LIMBS],
                                   const BN_ULONG a[P256_LIMBS], BN_ULONG rep);
#else
void ecp_nistz256_ord_sqr_mont(BN_ULONG res[P256_LIMBS],
                               const BN_ULONG a[P256_LIMBS], BN_ULONG rep);
#endif

int beeu_mod_inverse_vartime(BN_ULONG out[P256_LIMBS],
                             const BN_ULONG a[P256_LIMBS],
                             const BN_ULONG p[P256_LIMBS]);

typedef struct {
  BN_ULONG X[P256_LIMBS];
  BN_ULONG Y[P256_LIMBS];
  BN_ULONG Z[P256_LIMBS];
} P256_POINT;

typedef struct {
  BN_ULONG X[P256_LIMBS];
  BN_ULONG Y[P256_LIMBS];
} P256_POINT_AFFINE;

#if defined(OPENSSL_X86_64)
void ecp_nistz256_select_w5_nohw(P256_POINT *val, const P256_POINT in_t[16],
                                 int index);
void ecp_nistz256_select_w5_avx2(P256_POINT *val, const P256_POINT in_t[16],
                                 int index);
#else
void ecp_nistz256_select_w5(P256_POINT *val, const P256_POINT in_t[16],
                            int index);
#endif

#if defined(OPENSSL_X86_64)
void ecp_nistz256_select_w7_nohw(P256_POINT_AFFINE *val,
                                 const P256_POINT_AFFINE in_t[64], int index);
void ecp_nistz256_select_w7_avx2(P256_POINT_AFFINE *val,
                                 const P256_POINT_AFFINE in_t[64], int index);
#else
void ecp_nistz256_select_w7(P256_POINT_AFFINE *val,
                            const P256_POINT_AFFINE in_t[64], int index);
#endif

#if defined(OPENSSL_X86_64)
void ecp_nistz256_point_double_nohw(P256_POINT *r, const P256_POINT *a);
void ecp_nistz256_point_double_adx(P256_POINT *r, const P256_POINT *a);
#else
void ecp_nistz256_point_double(P256_POINT *r, const P256_POINT *a);
#endif

#if defined(OPENSSL_X86_64)
void ecp_nistz256_point_add_nohw(P256_POINT *r, const P256_POINT *a,
                                 const P256_POINT *b);
void ecp_nistz256_point_add_adx(P256_POINT *r, const P256_POINT *a,
                                const P256_POINT *b);
#else
void ecp_nistz256_point_add(P256_POINT *r, const P256_POINT *a,
                            const P256_POINT *b);
#endif

#if defined(OPENSSL_X86_64)
void ecp_nistz256_point_add_affine_adx(P256_POINT *r, const P256_POINT *a,
                                       const P256_POINT_AFFINE *b);
void ecp_nistz256_point_add_affine_nohw(P256_POINT *r, const P256_POINT *a,
                                        const P256_POINT_AFFINE *b);
#else
void ecp_nistz256_point_add_affine(P256_POINT *r, const P256_POINT *a,
                                   const P256_POINT_AFFINE *b);
#endif

#endif

#if defined(__cplusplus)
}
#endif

#endif
