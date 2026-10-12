// Copyright 2018 The BoringSSL Authors
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

#include <openssl/bn.h>

#include <assert.h>

#include <openssl/err.h>

#include "internal.h"

static BN_ULONG word_is_odd_mask(BN_ULONG a) { return (BN_ULONG)0 - (a & 1); }

static void maybe_rshift1_words(BN_ULONG *a, BN_ULONG mask, BN_ULONG *tmp,
                                size_t num) {
  bn_rshift1_words(tmp, a, num);
  bn_select_words(a, mask, tmp, a, num);
}

static void maybe_rshift1_words_carry(BN_ULONG *a, BN_ULONG carry,
                                      BN_ULONG mask, BN_ULONG *tmp,
                                      size_t num) {
  maybe_rshift1_words(a, mask, tmp, num);
  if (num != 0) {
    carry &= mask;
    a[num - 1] |= carry << (BN_BITS2 - 1);
  }
}

static BN_ULONG maybe_add_words(BN_ULONG *a, BN_ULONG mask, const BN_ULONG *b,
                                BN_ULONG *tmp, size_t num) {
  BN_ULONG carry = bn_add_words(tmp, a, b, num);
  bn_select_words(a, mask, tmp, a, num);
  return carry & mask;
}

static int bn_gcd_consttime(BIGNUM *r, unsigned *out_shift, const BIGNUM *x,
                            const BIGNUM *y, BN_CTX *ctx) {
  size_t width = x->width > y->width ? x->width : y->width;
  if (width == 0) {
    *out_shift = 0;
    BN_zero(r);
    return 1;
  }

  int ret = 0;
  BN_CTX_start(ctx);
  BIGNUM *u = BN_CTX_get(ctx);
  BIGNUM *v = BN_CTX_get(ctx);
  BIGNUM *tmp = BN_CTX_get(ctx);
  unsigned x_bits, y_bits, num_iters, shift;
  if (u == NULL || v == NULL || tmp == NULL ||
      !BN_copy(u, x) ||
      !BN_copy(v, y) ||
      !bn_resize_words(u, width) ||
      !bn_resize_words(v, width) ||
      !bn_resize_words(tmp, width)) {
    goto err;
  }

  x_bits = x->width * BN_BITS2;
  y_bits = y->width * BN_BITS2;
  num_iters = x_bits + y_bits;
  if (num_iters < x_bits) {
    OPENSSL_PUT_ERROR(BN, BN_R_BIGNUM_TOO_LONG);
    goto err;
  }

  shift = 0;
  for (unsigned i = 0; i < num_iters; i++) {
    BN_ULONG both_odd = word_is_odd_mask(u->d[0]) & word_is_odd_mask(v->d[0]);

    BN_ULONG u_less_than_v =
        (BN_ULONG)0 - bn_sub_words(tmp->d, u->d, v->d, width);
    bn_select_words(u->d, both_odd & ~u_less_than_v, tmp->d, u->d, width);
    bn_sub_words(tmp->d, v->d, u->d, width);
    bn_select_words(v->d, both_odd & u_less_than_v, tmp->d, v->d, width);

    BN_ULONG u_is_odd = word_is_odd_mask(u->d[0]);
    BN_ULONG v_is_odd = word_is_odd_mask(v->d[0]);
    declassify_assert(!(u_is_odd & v_is_odd));

    shift += 1 & (~u_is_odd & ~v_is_odd);

    maybe_rshift1_words(u->d, ~u_is_odd, tmp->d, width);
    maybe_rshift1_words(v->d, ~v_is_odd, tmp->d, width);
  }

  declassify_assert(BN_is_zero(u) | BN_is_zero(v));
  for (size_t i = 0; i < width; i++) {
    v->d[i] |= u->d[i];
  }

  *out_shift = shift;
  ret = bn_set_words(r, v->d, width);

err:
  BN_CTX_end(ctx);
  return ret;
}

int BN_gcd(BIGNUM *r, const BIGNUM *x, const BIGNUM *y, BN_CTX *ctx) {
  unsigned shift;
  return bn_gcd_consttime(r, &shift, x, y, ctx) && BN_lshift(r, r, shift);
}

int bn_is_relatively_prime(int *out_relatively_prime, const BIGNUM *x,
                           const BIGNUM *y, BN_CTX *ctx) {
  int ret = 0;
  BN_CTX_start(ctx);
  unsigned shift;
  BIGNUM *gcd = BN_CTX_get(ctx);
  if (gcd == NULL || !bn_gcd_consttime(gcd, &shift, x, y, ctx)) {
    goto err;
  }

  if (gcd->width == 0) {
    *out_relatively_prime = 0;
  } else {
    BN_ULONG mask = shift | (gcd->d[0] ^ 1);
    for (int i = 1; i < gcd->width; i++) {
      mask |= gcd->d[i];
    }
    *out_relatively_prime = mask == 0;
  }
  ret = 1;

err:
  BN_CTX_end(ctx);
  return ret;
}

int bn_lcm_consttime(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx) {
  BN_CTX_start(ctx);
  unsigned shift;
  BIGNUM *gcd = BN_CTX_get(ctx);
  int ret = gcd != NULL &&
            bn_mul_consttime(r, a, b, ctx) &&
            bn_gcd_consttime(gcd, &shift, a, b, ctx) &&

            bn_div_consttime(r, NULL, r, gcd,                      0, ctx) &&
            bn_rshift_secret_shift(r, r, shift, ctx);
  BN_CTX_end(ctx);
  return ret;
}

int bn_mod_inverse_consttime(BIGNUM *r, int *out_no_inverse, const BIGNUM *a,
                             const BIGNUM *n, BN_CTX *ctx) {
  *out_no_inverse = 0;
  if (BN_is_negative(a) || BN_ucmp(a, n) >= 0) {
    OPENSSL_PUT_ERROR(BN, BN_R_INPUT_NOT_REDUCED);
    return 0;
  }
  if (BN_is_zero(a)) {
    if (BN_is_one(n)) {
      BN_zero(r);
      return 1;
    }
    *out_no_inverse = 1;
    OPENSSL_PUT_ERROR(BN, BN_R_NO_INVERSE);
    return 0;
  }

  if (!BN_is_odd(a) && !BN_is_odd(n)) {
    *out_no_inverse = 1;
    OPENSSL_PUT_ERROR(BN, BN_R_NO_INVERSE);
    return 0;
  }

  size_t n_width = n->width, a_width = a->width;
  if (a_width > n_width) {
    a_width = n_width;
  }

  int ret = 0;
  BN_CTX_start(ctx);
  BIGNUM *u = BN_CTX_get(ctx);
  BIGNUM *v = BN_CTX_get(ctx);
  BIGNUM *A = BN_CTX_get(ctx);
  BIGNUM *B = BN_CTX_get(ctx);
  BIGNUM *C = BN_CTX_get(ctx);
  BIGNUM *D = BN_CTX_get(ctx);
  BIGNUM *tmp = BN_CTX_get(ctx);
  BIGNUM *tmp2 = BN_CTX_get(ctx);
  size_t a_bits, num_iters, n_bits;
  if (u == NULL ||
      v == NULL ||
      A == NULL ||
      B == NULL ||
      C == NULL ||
      D == NULL ||
      tmp == NULL ||
      tmp2 == NULL ||
      !BN_copy(u, a) ||
      !BN_copy(v, n) ||
      !BN_one(A) ||
      !BN_one(D) ||

      !bn_resize_words(u, n_width) ||
      !bn_resize_words(v, n_width) ||

      !bn_resize_words(A, n_width) ||
      !bn_resize_words(C, n_width) ||

      !bn_resize_words(B, a_width) ||
      !bn_resize_words(D, a_width) ||

      !bn_resize_words(tmp, n_width) ||
      !bn_resize_words(tmp2, n_width)) {
    goto err;
  }

  a_bits = a_width * BN_BITS2;
  n_bits = n_width * BN_BITS2;
  num_iters = a_bits + n_bits;
  if (num_iters < a_bits) {
    OPENSSL_PUT_ERROR(BN, BN_R_BIGNUM_TOO_LONG);
    goto err;
  }

  for (size_t i = 0; i < num_iters; i++) {
    BN_ULONG both_odd = word_is_odd_mask(u->d[0]) & word_is_odd_mask(v->d[0]);

    BN_ULONG v_less_than_u =
        (BN_ULONG)0 - bn_sub_words(tmp->d, v->d, u->d, n_width);
    bn_select_words(v->d, both_odd & ~v_less_than_u, tmp->d, v->d, n_width);
    bn_sub_words(tmp->d, u->d, v->d, n_width);
    bn_select_words(u->d, both_odd & v_less_than_u, tmp->d, u->d, n_width);

    BN_ULONG carry = bn_add_words(tmp->d, A->d, C->d, n_width);
    carry -= bn_sub_words(tmp2->d, tmp->d, n->d, n_width);
    bn_select_words(tmp->d, carry, tmp->d, tmp2->d, n_width);
    bn_select_words(A->d, both_odd & v_less_than_u, tmp->d, A->d, n_width);
    bn_select_words(C->d, both_odd & ~v_less_than_u, tmp->d, C->d, n_width);

    bn_add_words(tmp->d, B->d, D->d, a_width);
    bn_sub_words(tmp2->d, tmp->d, a->d, a_width);
    bn_select_words(tmp->d, carry, tmp->d, tmp2->d, a_width);
    bn_select_words(B->d, both_odd & v_less_than_u, tmp->d, B->d, a_width);
    bn_select_words(D->d, both_odd & ~v_less_than_u, tmp->d, D->d, a_width);

    BN_ULONG u_is_even = ~word_is_odd_mask(u->d[0]);
    BN_ULONG v_is_even = ~word_is_odd_mask(v->d[0]);
    declassify_assert(u_is_even != v_is_even);

    maybe_rshift1_words(u->d, u_is_even, tmp->d, n_width);
    BN_ULONG A_or_B_is_odd =
        word_is_odd_mask(A->d[0]) | word_is_odd_mask(B->d[0]);
    BN_ULONG A_carry =
        maybe_add_words(A->d, A_or_B_is_odd & u_is_even, n->d, tmp->d, n_width);
    BN_ULONG B_carry =
        maybe_add_words(B->d, A_or_B_is_odd & u_is_even, a->d, tmp->d, a_width);
    maybe_rshift1_words_carry(A->d, A_carry, u_is_even, tmp->d, n_width);
    maybe_rshift1_words_carry(B->d, B_carry, u_is_even, tmp->d, a_width);

    maybe_rshift1_words(v->d, v_is_even, tmp->d, n_width);
    BN_ULONG C_or_D_is_odd =
        word_is_odd_mask(C->d[0]) | word_is_odd_mask(D->d[0]);
    BN_ULONG C_carry =
        maybe_add_words(C->d, C_or_D_is_odd & v_is_even, n->d, tmp->d, n_width);
    BN_ULONG D_carry =
        maybe_add_words(D->d, C_or_D_is_odd & v_is_even, a->d, tmp->d, a_width);
    maybe_rshift1_words_carry(C->d, C_carry, v_is_even, tmp->d, n_width);
    maybe_rshift1_words_carry(D->d, D_carry, v_is_even, tmp->d, a_width);
  }

  declassify_assert(BN_is_zero(v));

  if (constant_time_declassify_int(!BN_is_one(u))) {
    *out_no_inverse = 1;
    OPENSSL_PUT_ERROR(BN, BN_R_NO_INVERSE);
    goto err;
  }

  ret = BN_copy(r, A) != NULL;

err:
  BN_CTX_end(ctx);
  return ret;
}
