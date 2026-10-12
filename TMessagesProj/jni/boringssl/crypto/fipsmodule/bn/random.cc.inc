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

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <openssl/err.h>

#include "../../internal.h"
#include "../bcm_interface.h"
#include "../service_indicator/internal.h"
#include "internal.h"

int BN_rand(BIGNUM *rnd, int bits, int top, int bottom) {
  if (rnd == NULL) {
    return 0;
  }

  if (top != BN_RAND_TOP_ANY && top != BN_RAND_TOP_ONE &&
      top != BN_RAND_TOP_TWO) {
    OPENSSL_PUT_ERROR(BN, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  if (bottom != BN_RAND_BOTTOM_ANY && bottom != BN_RAND_BOTTOM_ODD) {
    OPENSSL_PUT_ERROR(BN, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    return 0;
  }

  if (bits == 0) {
    BN_zero(rnd);
    return 1;
  }

  if (bits > INT_MAX - (BN_BITS2 - 1)) {
    OPENSSL_PUT_ERROR(BN, BN_R_BIGNUM_TOO_LONG);
    return 0;
  }

  int words = (bits + BN_BITS2 - 1) / BN_BITS2;
  int bit = (bits - 1) % BN_BITS2;
  const BN_ULONG kOne = 1;
  const BN_ULONG kThree = 3;
  BN_ULONG mask = bit < BN_BITS2 - 1 ? (kOne << (bit + 1)) - 1 : BN_MASK2;
  if (!bn_wexpand(rnd, words)) {
    return 0;
  }

  FIPS_service_indicator_lock_state();
  BCM_rand_bytes((uint8_t *)rnd->d, words * sizeof(BN_ULONG));
  FIPS_service_indicator_unlock_state();

  rnd->d[words - 1] &= mask;
  if (top != BN_RAND_TOP_ANY) {
    if (top == BN_RAND_TOP_TWO && bits > 1) {
      if (bit == 0) {
        rnd->d[words - 1] |= 1;
        rnd->d[words - 2] |= kOne << (BN_BITS2 - 1);
      } else {
        rnd->d[words - 1] |= kThree << (bit - 1);
      }
    } else {
      rnd->d[words - 1] |= kOne << bit;
    }
  }
  if (bottom == BN_RAND_BOTTOM_ODD) {
    rnd->d[0] |= 1;
  }

  rnd->neg = 0;
  rnd->width = words;
  return 1;
}

int BN_pseudo_rand(BIGNUM *rnd, int bits, int top, int bottom) {
  return BN_rand(rnd, bits, top, bottom);
}

static crypto_word_t bn_less_than_word_mask(const BN_ULONG *a, size_t len,
                                            BN_ULONG b) {
  if (b == 0) {
    return CONSTTIME_FALSE_W;
  }
  if (len == 0) {
    return CONSTTIME_TRUE_W;
  }

  static_assert(sizeof(BN_ULONG) <= sizeof(crypto_word_t),
                "crypto_word_t is too small");
  crypto_word_t mask = 0;
  for (size_t i = 1; i < len; i++) {
    mask |= a[i];
  }

  mask = constant_time_is_zero_w(mask);
  mask &= constant_time_lt_w(a[0], b);
  return mask;
}

int bn_in_range_words(const BN_ULONG *a, BN_ULONG min_inclusive,
                      const BN_ULONG *max_exclusive, size_t len) {
  crypto_word_t mask = ~bn_less_than_word_mask(a, len, min_inclusive);
  return mask & bn_less_than_words(a, max_exclusive, len);
}

static int bn_range_to_mask(size_t *out_words, BN_ULONG *out_mask,
                            size_t min_inclusive, const BN_ULONG *max_exclusive,
                            size_t len) {

  size_t words = len;
  while (words > 0 && max_exclusive[words - 1] == 0) {
    words--;
  }
  if (words == 0 || (words == 1 && max_exclusive[0] <= min_inclusive)) {
    OPENSSL_PUT_ERROR(BN, BN_R_INVALID_RANGE);
    return 0;
  }
  BN_ULONG mask = max_exclusive[words - 1];

  mask |= mask >> 1;
  mask |= mask >> 2;
  mask |= mask >> 4;
  mask |= mask >> 8;
  mask |= mask >> 16;
#if defined(OPENSSL_64_BIT)
  mask |= mask >> 32;
#endif

  *out_words = words;
  *out_mask = mask;
  return 1;
}

int bn_rand_range_words(BN_ULONG *out, BN_ULONG min_inclusive,
                        const BN_ULONG *max_exclusive, size_t len,
                        const uint8_t additional_data[32]) {

  size_t words;
  BN_ULONG mask;
  if (!bn_range_to_mask(&words, &mask, min_inclusive, max_exclusive, len)) {
    return 0;
  }

  OPENSSL_memset(out + words, 0, (len - words) * sizeof(BN_ULONG));

  unsigned count = 100;
  do {
    if (!--count) {
      OPENSSL_PUT_ERROR(BN, BN_R_TOO_MANY_ITERATIONS);
      return 0;
    }

    FIPS_service_indicator_lock_state();
    BCM_rand_bytes_with_additional_data(
        (uint8_t *)out, words * sizeof(BN_ULONG), additional_data);
    FIPS_service_indicator_unlock_state();
    out[words - 1] &= mask;

  } while (!constant_time_declassify_int(
      bn_in_range_words(out, min_inclusive, max_exclusive, words)));
  return 1;
}

int BN_rand_range_ex(BIGNUM *r, BN_ULONG min_inclusive,
                     const BIGNUM *max_exclusive) {
  static const uint8_t kDefaultAdditionalData[32] = {0};
  if (!bn_wexpand(r, max_exclusive->width) ||
      !bn_rand_range_words(r->d, min_inclusive, max_exclusive->d,
                           max_exclusive->width, kDefaultAdditionalData)) {
    return 0;
  }

  r->neg = 0;
  r->width = max_exclusive->width;
  return 1;
}

int bn_rand_secret_range(BIGNUM *r, int *out_is_uniform, BN_ULONG min_inclusive,
                         const BIGNUM *max_exclusive) {
  size_t words;
  BN_ULONG mask;
  if (!bn_range_to_mask(&words, &mask, min_inclusive, max_exclusive->d,
                        max_exclusive->width) ||
      !bn_wexpand(r, words)) {
    return 0;
  }

  assert(words > 0);
  assert(mask != 0);

  if (words == 1 && min_inclusive > mask >> 1) {
    OPENSSL_PUT_ERROR(BN, BN_R_INVALID_RANGE);
    return 0;
  }

  FIPS_service_indicator_lock_state();
  BCM_rand_bytes((uint8_t *)r->d, words * sizeof(BN_ULONG));
  FIPS_service_indicator_unlock_state();
  r->d[words - 1] &= mask;

  *out_is_uniform =
      bn_in_range_words(r->d, min_inclusive, max_exclusive->d, words);
  crypto_word_t in_range = *out_is_uniform;
  in_range = 0 - in_range;

  r->d[0] |= constant_time_select_w(in_range, 0, min_inclusive);
  r->d[words - 1] &= constant_time_select_w(in_range, BN_MASK2, mask >> 1);
  declassify_assert(
      bn_in_range_words(r->d, min_inclusive, max_exclusive->d, words));

  r->neg = 0;
  r->width = (int)words;
  return 1;
}

int BN_rand_range(BIGNUM *r, const BIGNUM *range) {
  return BN_rand_range_ex(r, 0, range);
}

int BN_pseudo_rand_range(BIGNUM *r, const BIGNUM *range) {
  return BN_rand_range(r, range);
}
