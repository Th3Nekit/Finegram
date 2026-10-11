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

#include <openssl/bn.h>

#include <openssl/err.h>

#include "internal.h"

int BN_mod_inverse_odd(BIGNUM *out, int *out_no_inverse, const BIGNUM *a,
                       const BIGNUM *n, BN_CTX *ctx) {
  *out_no_inverse = 0;

  if (!BN_is_odd(n)) {
    OPENSSL_PUT_ERROR(BN, BN_R_CALLED_WITH_EVEN_MODULUS);
    return 0;
  }

  if (BN_is_negative(a) || BN_cmp(a, n) >= 0) {
    OPENSSL_PUT_ERROR(BN, BN_R_INPUT_NOT_REDUCED);
    return 0;
  }

  BIGNUM *A, *B, *X, *Y;
  int ret = 0;
  int sign;

  BN_CTX_start(ctx);
  A = BN_CTX_get(ctx);
  B = BN_CTX_get(ctx);
  X = BN_CTX_get(ctx);
  Y = BN_CTX_get(ctx);
  BIGNUM *R = out;
  if (Y == NULL) {
    goto err;
  }

  BN_zero(Y);
  if (!BN_one(X) || BN_copy(B, a) == NULL || BN_copy(A, n) == NULL) {
    goto err;
  }
  A->neg = 0;
  sign = -1;

  int shift;

  while (!BN_is_zero(B)) {

    shift = 0;
    while (!BN_is_bit_set(B, shift)) {

      shift++;

      if (BN_is_odd(X)) {
        if (!BN_uadd(X, X, n)) {
          goto err;
        }
      }

      if (!BN_rshift1(X, X)) {
        goto err;
      }
    }
    if (shift > 0) {
      if (!BN_rshift(B, B, shift)) {
        goto err;
      }
    }

    shift = 0;
    while (!BN_is_bit_set(A, shift)) {

      shift++;

      if (BN_is_odd(Y)) {
        if (!BN_uadd(Y, Y, n)) {
          goto err;
        }
      }

      if (!BN_rshift1(Y, Y)) {
        goto err;
      }
    }
    if (shift > 0) {
      if (!BN_rshift(A, A, shift)) {
        goto err;
      }
    }

    if (BN_ucmp(B, A) >= 0) {

      if (!BN_uadd(X, X, Y)) {
        goto err;
      }

      if (!BN_usub(B, B, A)) {
        goto err;
      }
    } else {

      if (!BN_uadd(Y, Y, X)) {
        goto err;
      }

      if (!BN_usub(A, A, B)) {
        goto err;
      }
    }
  }

  if (!BN_is_one(A)) {
    *out_no_inverse = 1;
    OPENSSL_PUT_ERROR(BN, BN_R_NO_INVERSE);
    goto err;
  }

  if (sign < 0) {
    if (!BN_sub(Y, n, Y)) {
      goto err;
    }
  }

  if (Y->neg || BN_ucmp(Y, n) >= 0) {
    if (!BN_nnmod(Y, Y, n, ctx)) {
      goto err;
    }
  }
  if (!BN_copy(R, Y)) {
    goto err;
  }

  ret = 1;

err:
  BN_CTX_end(ctx);
  return ret;
}

BIGNUM *BN_mod_inverse(BIGNUM *out, const BIGNUM *a, const BIGNUM *n,
                       BN_CTX *ctx) {
  BIGNUM *new_out = NULL;
  if (out == NULL) {
    new_out = BN_new();
    if (new_out == NULL) {
      return NULL;
    }
    out = new_out;
  }

  int ok = 0;
  BIGNUM *a_reduced = NULL;
  if (a->neg || BN_ucmp(a, n) >= 0) {
    a_reduced = BN_dup(a);
    if (a_reduced == NULL) {
      goto err;
    }
    if (!BN_nnmod(a_reduced, a_reduced, n, ctx)) {
      goto err;
    }
    a = a_reduced;
  }

  int no_inverse;
  if (!BN_is_odd(n)) {
    if (!bn_mod_inverse_consttime(out, &no_inverse, a, n, ctx)) {
      goto err;
    }
  } else if (!BN_mod_inverse_odd(out, &no_inverse, a, n, ctx)) {
    goto err;
  }

  ok = 1;

err:
  if (!ok) {
    BN_free(new_out);
    out = NULL;
  }
  BN_free(a_reduced);
  return out;
}

int BN_mod_inverse_blinded(BIGNUM *out, int *out_no_inverse, const BIGNUM *a,
                           const BN_MONT_CTX *mont, BN_CTX *ctx) {
  *out_no_inverse = 0;

  if (BN_is_negative(a) ||
      constant_time_declassify_int(BN_cmp(a, &mont->N) >= 0)) {
    OPENSSL_PUT_ERROR(BN, BN_R_INPUT_NOT_REDUCED);
    return 0;
  }

  int ret = 0;
  BIGNUM blinding_factor;
  BN_init(&blinding_factor);

  if (!BN_rand_range_ex(&blinding_factor, 1, &mont->N)) {
    goto err;
  }
  bn_secret(&blinding_factor);
  if (!BN_mod_mul_montgomery(out, &blinding_factor, a, mont, ctx)) {
    goto err;
  }

  bn_declassify(out);
  if (!BN_mod_inverse_odd(out, out_no_inverse, out, &mont->N, ctx) ||
      !BN_mod_mul_montgomery(out, &blinding_factor, out, mont, ctx)) {
    goto err;
  }

  ret = 1;

err:
  BN_free(&blinding_factor);
  return ret;
}

int bn_mod_inverse_prime(BIGNUM *out, const BIGNUM *a, const BIGNUM *p,
                         BN_CTX *ctx, const BN_MONT_CTX *mont_p) {
  BN_CTX_start(ctx);
  BIGNUM *p_minus_2 = BN_CTX_get(ctx);
  int ok = p_minus_2 != NULL && BN_copy(p_minus_2, p) &&
           BN_sub_word(p_minus_2, 2) &&
           BN_mod_exp_mont(out, a, p_minus_2, p, ctx, mont_p);
  BN_CTX_end(ctx);
  return ok;
}

int bn_mod_inverse_secret_prime(BIGNUM *out, const BIGNUM *a, const BIGNUM *p,
                                BN_CTX *ctx, const BN_MONT_CTX *mont_p) {
  BN_CTX_start(ctx);
  BIGNUM *p_minus_2 = BN_CTX_get(ctx);
  int ok = p_minus_2 != NULL && BN_copy(p_minus_2, p) &&
           BN_sub_word(p_minus_2, 2) &&
           BN_mod_exp_mont_consttime(out, a, p_minus_2, p, ctx, mont_p);
  BN_CTX_end(ctx);
  return ok;
}
