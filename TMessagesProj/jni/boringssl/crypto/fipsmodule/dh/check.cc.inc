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

#include <openssl/dh.h>

#include <openssl/bn.h>
#include <openssl/err.h>

#include "../bn/internal.h"
#include "internal.h"

static_assert(OPENSSL_DH_MAX_MODULUS_BITS <=
                  BN_MONTGOMERY_MAX_WORDS * BN_BITS2,
              "Max DH size too big for Montgomery arithmetic");

int dh_check_params_fast(const DH *dh) {

  if (BN_is_negative(dh->p) || !BN_is_odd(dh->p) ||
      BN_num_bits(dh->p) > OPENSSL_DH_MAX_MODULUS_BITS) {
    OPENSSL_PUT_ERROR(DH, DH_R_INVALID_PARAMETERS);
    return 0;
  }

  if (dh->q != NULL && (BN_is_negative(dh->q) || BN_ucmp(dh->q, dh->p) > 0)) {
    OPENSSL_PUT_ERROR(DH, DH_R_INVALID_PARAMETERS);
    return 0;
  }

  if (BN_is_negative(dh->g) || BN_is_zero(dh->g) ||
      BN_ucmp(dh->g, dh->p) >= 0) {
    OPENSSL_PUT_ERROR(DH, DH_R_INVALID_PARAMETERS);
    return 0;
  }

  return 1;
}

int DH_check_pub_key(const DH *dh, const BIGNUM *pub_key, int *out_flags) {
  *out_flags = 0;
  if (!dh_check_params_fast(dh)) {
    return 0;
  }

  BN_CTX *ctx = BN_CTX_new();
  if (ctx == NULL) {
    return 0;
  }
  BN_CTX_start(ctx);

  int ok = 0;

  if (BN_cmp(pub_key, BN_value_one()) <= 0) {
    *out_flags |= DH_CHECK_PUBKEY_TOO_SMALL;
  }

  BIGNUM *tmp = BN_CTX_get(ctx);
  if (tmp == NULL ||
      !BN_copy(tmp, dh->p) ||
      !BN_sub_word(tmp, 1)) {
    goto err;
  }
  if (BN_cmp(pub_key, tmp) >= 0) {
    *out_flags |= DH_CHECK_PUBKEY_TOO_LARGE;
  }

  if (dh->q != NULL) {

    if (!BN_mod_exp_mont(tmp, pub_key, dh->q, dh->p, ctx, NULL)) {
      goto err;
    }
    if (!BN_is_one(tmp)) {
      *out_flags |= DH_CHECK_PUBKEY_INVALID;
    }
  }

  ok = 1;

err:
  BN_CTX_end(ctx);
  BN_CTX_free(ctx);
  return ok;
}

int DH_check(const DH *dh, int *out_flags) {
  *out_flags = 0;
  if (!dh_check_params_fast(dh)) {
    return 0;
  }

  int ok = 0, r;
  BN_CTX *ctx = NULL;
  BN_ULONG l;
  BIGNUM *t1 = NULL, *t2 = NULL;

  ctx = BN_CTX_new();
  if (ctx == NULL) {
    goto err;
  }
  BN_CTX_start(ctx);
  t1 = BN_CTX_get(ctx);
  if (t1 == NULL) {
    goto err;
  }
  t2 = BN_CTX_get(ctx);
  if (t2 == NULL) {
    goto err;
  }

  if (dh->q) {
    if (BN_cmp(dh->g, BN_value_one()) <= 0) {
      *out_flags |= DH_CHECK_NOT_SUITABLE_GENERATOR;
    } else if (BN_cmp(dh->g, dh->p) >= 0) {
      *out_flags |= DH_CHECK_NOT_SUITABLE_GENERATOR;
    } else {

      if (!BN_mod_exp_mont(t1, dh->g, dh->q, dh->p, ctx, NULL)) {
        goto err;
      }
      if (!BN_is_one(t1)) {
        *out_flags |= DH_CHECK_NOT_SUITABLE_GENERATOR;
      }
    }
    r = BN_is_prime_ex(dh->q, BN_prime_checks_for_validation, ctx, NULL);
    if (r < 0) {
      goto err;
    }
    if (!r) {
      *out_flags |= DH_CHECK_Q_NOT_PRIME;
    }

    if (!BN_div(t1, t2, dh->p, dh->q, ctx)) {
      goto err;
    }
    if (!BN_is_one(t2)) {
      *out_flags |= DH_CHECK_INVALID_Q_VALUE;
    }
  } else if (BN_is_word(dh->g, DH_GENERATOR_2)) {
    l = BN_mod_word(dh->p, 24);
    if (l == (BN_ULONG)-1) {
      goto err;
    }
    if (l != 11) {
      *out_flags |= DH_CHECK_NOT_SUITABLE_GENERATOR;
    }
  } else if (BN_is_word(dh->g, DH_GENERATOR_5)) {
    l = BN_mod_word(dh->p, 10);
    if (l == (BN_ULONG)-1) {
      goto err;
    }
    if (l != 3 && l != 7) {
      *out_flags |= DH_CHECK_NOT_SUITABLE_GENERATOR;
    }
  } else {
    *out_flags |= DH_CHECK_UNABLE_TO_CHECK_GENERATOR;
  }

  r = BN_is_prime_ex(dh->p, BN_prime_checks_for_validation, ctx, NULL);
  if (r < 0) {
    goto err;
  }
  if (!r) {
    *out_flags |= DH_CHECK_P_NOT_PRIME;
  } else if (!dh->q) {
    if (!BN_rshift1(t1, dh->p)) {
      goto err;
    }
    r = BN_is_prime_ex(t1, BN_prime_checks_for_validation, ctx, NULL);
    if (r < 0) {
      goto err;
    }
    if (!r) {
      *out_flags |= DH_CHECK_P_NOT_SAFE_PRIME;
    }
  }
  ok = 1;

err:
  if (ctx != NULL) {
    BN_CTX_end(ctx);
    BN_CTX_free(ctx);
  }
  return ok;
}
