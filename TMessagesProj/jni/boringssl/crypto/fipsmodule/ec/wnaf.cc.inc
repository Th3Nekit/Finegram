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

#include <openssl/ec.h>

#include <assert.h>
#include <string.h>

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/mem.h>
#include <openssl/thread.h>

#include "../../internal.h"
#include "../bn/internal.h"
#include "internal.h"

void ec_compute_wNAF(const EC_GROUP *group, int8_t *out,
                     const EC_SCALAR *scalar, size_t bits, int w) {

  assert(0 < w && w <= 7);
  assert(bits != 0);
  int bit = 1 << w;
  int next_bit = bit << 1;
  int mask = next_bit - 1;

  int window_val = scalar->words[0] & mask;
  for (size_t j = 0; j < bits + 1; j++) {
    assert(0 <= window_val && window_val <= next_bit);
    int digit = 0;
    if (window_val & 1) {
      assert(0 < window_val && window_val < next_bit);
      if (window_val & bit) {
        digit = window_val - next_bit;

        if (j + w + 1 >= bits) {

          digit = window_val & (mask >> 1);

        }
      } else {
        digit = window_val;

      }

      window_val -= digit;

      assert(window_val == 0 || window_val == next_bit || window_val == bit);
      assert(-bit < digit && digit < bit);

      assert(digit & 1);
    }

    out[j] = digit;

    window_val >>= 1;
    window_val += bit * bn_is_bit_set_words(scalar->words, group->order.N.width,
                                            j + w + 1);
    assert(window_val <= next_bit);
  }

  assert(window_val == 0);
}

static void compute_precomp(const EC_GROUP *group, EC_JACOBIAN *out,
                            const EC_JACOBIAN *p, size_t len) {
  ec_GFp_simple_point_copy(&out[0], p);
  EC_JACOBIAN two_p;
  ec_GFp_mont_dbl(group, &two_p, p);
  for (size_t i = 1; i < len; i++) {
    ec_GFp_mont_add(group, &out[i], &out[i - 1], &two_p);
  }
}

static void lookup_precomp(const EC_GROUP *group, EC_JACOBIAN *out,
                           const EC_JACOBIAN *precomp, int digit) {
  if (digit < 0) {
    digit = -digit;
    ec_GFp_simple_point_copy(out, &precomp[digit >> 1]);
    ec_GFp_simple_invert(group, out);
  } else {
    ec_GFp_simple_point_copy(out, &precomp[digit >> 1]);
  }
}

#define EC_WNAF_WINDOW_BITS 4

#define EC_WNAF_TABLE_SIZE (1 << (EC_WNAF_WINDOW_BITS - 1))

#define EC_WNAF_STACK 3

int ec_GFp_mont_mul_public_batch(const EC_GROUP *group, EC_JACOBIAN *r,
                                 const EC_SCALAR *g_scalar,
                                 const EC_JACOBIAN *points,
                                 const EC_SCALAR *scalars, size_t num) {
  size_t bits = EC_GROUP_order_bits(group);
  size_t wNAF_len = bits + 1;

  int8_t wNAF_stack[EC_WNAF_STACK][EC_MAX_BYTES * 8 + 1];
  EC_JACOBIAN precomp_stack[EC_WNAF_STACK][EC_WNAF_TABLE_SIZE];

  EC_JACOBIAN(*precomp_alloc)[EC_WNAF_TABLE_SIZE] = NULL;
  int8_t(*wNAF_alloc)[EC_MAX_BYTES * 8 + 1] = NULL;

  int8_t(*wNAF)[EC_MAX_BYTES * 8 + 1];
  EC_JACOBIAN(*precomp)[EC_WNAF_TABLE_SIZE];

  if (num <= EC_WNAF_STACK) {
    wNAF = wNAF_stack;
    precomp = precomp_stack;
  } else {
    wNAF_alloc = reinterpret_cast<decltype(wNAF_alloc)>(
        OPENSSL_calloc(num, sizeof(wNAF_alloc[0])));
    if (wNAF_alloc == NULL) {
      return 0;
    }
    precomp_alloc = reinterpret_cast<decltype(precomp_alloc)>(
        OPENSSL_calloc(num, sizeof(precomp_alloc[0])));
    if (precomp_alloc == NULL) {
      OPENSSL_free(wNAF_alloc);
      return 0;
    }

    wNAF = wNAF_alloc;
    precomp = precomp_alloc;
  }

  int8_t g_wNAF[EC_MAX_BYTES * 8 + 1];
  EC_JACOBIAN g_precomp[EC_WNAF_TABLE_SIZE];
  assert(wNAF_len <= OPENSSL_ARRAY_SIZE(g_wNAF));
  const EC_JACOBIAN *g = &group->generator.raw;
  if (g_scalar != NULL) {
    ec_compute_wNAF(group, g_wNAF, g_scalar, bits, EC_WNAF_WINDOW_BITS);
    compute_precomp(group, g_precomp, g, EC_WNAF_TABLE_SIZE);
  }

  for (size_t i = 0; i < num; i++) {
    assert(wNAF_len <= OPENSSL_ARRAY_SIZE(wNAF[i]));
    ec_compute_wNAF(group, wNAF[i], &scalars[i], bits, EC_WNAF_WINDOW_BITS);
    compute_precomp(group, precomp[i], &points[i], EC_WNAF_TABLE_SIZE);
  }

  EC_JACOBIAN tmp;
  int r_is_at_infinity = 1;
  for (size_t k = wNAF_len - 1; k < wNAF_len; k--) {
    if (!r_is_at_infinity) {
      ec_GFp_mont_dbl(group, r, r);
    }

    if (g_scalar != NULL && g_wNAF[k] != 0) {
      lookup_precomp(group, &tmp, g_precomp, g_wNAF[k]);
      if (r_is_at_infinity) {
        ec_GFp_simple_point_copy(r, &tmp);
        r_is_at_infinity = 0;
      } else {
        ec_GFp_mont_add(group, r, r, &tmp);
      }
    }

    for (size_t i = 0; i < num; i++) {
      if (wNAF[i][k] != 0) {
        lookup_precomp(group, &tmp, precomp[i], wNAF[i][k]);
        if (r_is_at_infinity) {
          ec_GFp_simple_point_copy(r, &tmp);
          r_is_at_infinity = 0;
        } else {
          ec_GFp_mont_add(group, r, r, &tmp);
        }
      }
    }
  }

  if (r_is_at_infinity) {
    ec_GFp_simple_point_set_to_infinity(group, r);
  }

  OPENSSL_free(wNAF_alloc);
  OPENSSL_free(precomp_alloc);
  return 1;
}
