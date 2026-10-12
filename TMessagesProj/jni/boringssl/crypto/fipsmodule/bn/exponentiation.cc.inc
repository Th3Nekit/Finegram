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
#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/mem.h>

#include "internal.h"
#include "rsaz_exp.h"

#if defined(OPENSSL_BN_ASM_MONT5)

static void bn_mul_mont_gather5(BN_ULONG *rp, const BN_ULONG *ap,
                                const BN_ULONG *table, const BN_ULONG *np,
                                const BN_ULONG *n0, int num, int power) {
  if (bn_mulx4x_mont_gather5_capable(num)) {
    bn_mulx4x_mont_gather5(rp, ap, table, np, n0, num, power);
  } else if (bn_mul4x_mont_gather5_capable(num)) {
    bn_mul4x_mont_gather5(rp, ap, table, np, n0, num, power);
  } else {
    bn_mul_mont_gather5_nohw(rp, ap, table, np, n0, num, power);
  }
}

static void bn_power5(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *table,
                      const BN_ULONG *np, const BN_ULONG *n0, int num,
                      int power) {
  assert(bn_power5_capable(num));
  if (bn_powerx5_capable(num)) {
    bn_powerx5(rp, ap, table, np, n0, num, power);
  } else {
    bn_power5_nohw(rp, ap, table, np, n0, num, power);
  }
}

#endif

static int BN_window_bits_for_exponent_size(size_t b) {
  if (b > 671) {
    return 6;
  }
  if (b > 239) {
    return 5;
  }
  if (b > 79) {
    return 4;
  }
  if (b > 23) {
    return 3;
  }
  return 1;
}

#define TABLE_SIZE 32

#define TABLE_BITS_SMALL 5

#define TABLE_SIZE_SMALL (1 << (TABLE_BITS_SMALL - 1))

int BN_mod_exp_mont(BIGNUM *rr, const BIGNUM *a, const BIGNUM *p,
                    const BIGNUM *m, BN_CTX *ctx, const BN_MONT_CTX *mont) {
  if (!BN_is_odd(m)) {
    OPENSSL_PUT_ERROR(BN, BN_R_CALLED_WITH_EVEN_MODULUS);
    return 0;
  }
  if (m->neg) {
    OPENSSL_PUT_ERROR(BN, BN_R_NEGATIVE_NUMBER);
    return 0;
  }

  if (a->neg || constant_time_declassify_int(BN_ucmp(a, m)) >= 0) {
    OPENSSL_PUT_ERROR(BN, BN_R_INPUT_NOT_REDUCED);
    return 0;
  }

  int bits = BN_num_bits(p);
  if (bits == 0) {

    if (BN_abs_is_word(m, 1)) {
      BN_zero(rr);
      return 1;
    }
    return BN_one(rr);
  }

  BIGNUM *val[TABLE_SIZE];

  bssl::BN_CTXScope scope(ctx);
  BIGNUM *r = BN_CTX_get(ctx);
  val[0] = BN_CTX_get(ctx);
  if (r == NULL || val[0] == NULL) {
    return 0;
  }

  bssl::UniquePtr<BN_MONT_CTX> new_mont;
  if (mont == nullptr) {
    new_mont.reset(BN_MONT_CTX_new_consttime(m, ctx));
    if (new_mont == nullptr) {
      return 0;
    }
    mont = new_mont.get();
  }

  int window = BN_window_bits_for_exponent_size(bits);
  if (!BN_to_montgomery(val[0], a, mont, ctx)) {
    return 0;
  }
  if (window > 1) {
    BIGNUM *d = BN_CTX_get(ctx);
    if (d == NULL || !BN_mod_mul_montgomery(d, val[0], val[0], mont, ctx)) {
      return 0;
    }
    for (int i = 1; i < 1 << (window - 1); i++) {
      val[i] = BN_CTX_get(ctx);
      if (val[i] == NULL ||
          !BN_mod_mul_montgomery(val[i], val[i - 1], d, mont, ctx)) {
        return 0;
      }
    }
  }

  int r_is_one = 1;
  int wstart = bits - 1;
  for (;;) {
    if (!BN_is_bit_set(p, wstart)) {
      if (!r_is_one && !BN_mod_mul_montgomery(r, r, r, mont, ctx)) {
        return 0;
      }
      if (wstart == 0) {
        break;
      }
      wstart--;
      continue;
    }

    int wvalue = 1;
    int wsize = 0;
    for (int i = 1; i < window && i <= wstart; i++) {
      if (BN_is_bit_set(p, wstart - i)) {
        wvalue <<= (i - wsize);
        wvalue |= 1;
        wsize = i;
      }
    }

    if (!r_is_one) {
      for (int i = 0; i < wsize + 1; i++) {
        if (!BN_mod_mul_montgomery(r, r, r, mont, ctx)) {
          return 0;
        }
      }
    }

    assert(wvalue & 1);
    assert(wvalue < (1 << window));
    if (r_is_one) {
      if (!BN_copy(r, val[wvalue >> 1])) {
        return 0;
      }
    } else if (!BN_mod_mul_montgomery(r, r, val[wvalue >> 1], mont, ctx)) {
      return 0;
    }

    r_is_one = 0;
    if (wstart == wsize) {
      break;
    }
    wstart -= wsize + 1;
  }

  assert(!r_is_one);

  return BN_from_montgomery(rr, r, mont, ctx);
}

void bn_mod_exp_mont_small(BN_ULONG *r, const BN_ULONG *a, size_t num,
                           const BN_ULONG *p, size_t num_p,
                           const BN_MONT_CTX *mont) {
  if (num != (size_t)mont->N.width || num > BN_SMALL_MAX_WORDS ||
      num_p > SIZE_MAX / BN_BITS2) {
    abort();
  }
  assert(BN_is_odd(&mont->N));

  while (num_p != 0 && p[num_p - 1] == 0) {
    num_p--;
  }
  if (num_p == 0) {
    bn_from_montgomery_small(r, num, mont->RR.d, num, mont);
    return;
  }
  size_t bits = BN_num_bits_word(p[num_p - 1]) + (num_p - 1) * BN_BITS2;
  assert(bits != 0);

  unsigned window = BN_window_bits_for_exponent_size(bits);
  if (window > TABLE_BITS_SMALL) {
    window = TABLE_BITS_SMALL;
  }
  BN_ULONG val[TABLE_SIZE_SMALL][BN_SMALL_MAX_WORDS];
  OPENSSL_memcpy(val[0], a, num * sizeof(BN_ULONG));
  if (window > 1) {
    BN_ULONG d[BN_SMALL_MAX_WORDS];
    bn_mod_mul_montgomery_small(d, val[0], val[0], num, mont);
    for (unsigned i = 1; i < 1u << (window - 1); i++) {
      bn_mod_mul_montgomery_small(val[i], val[i - 1], d, num, mont);
    }
  }

  int r_is_one = 1;
  size_t wstart = bits - 1;
  for (;;) {
    if (!bn_is_bit_set_words(p, num_p, wstart)) {
      if (!r_is_one) {
        bn_mod_mul_montgomery_small(r, r, r, num, mont);
      }
      if (wstart == 0) {
        break;
      }
      wstart--;
      continue;
    }

    unsigned wvalue = 1;
    unsigned wsize = 0;
    for (unsigned i = 1; i < window && i <= wstart; i++) {
      if (bn_is_bit_set_words(p, num_p, wstart - i)) {
        wvalue <<= (i - wsize);
        wvalue |= 1;
        wsize = i;
      }
    }

    if (!r_is_one) {
      for (unsigned i = 0; i < wsize + 1; i++) {
        bn_mod_mul_montgomery_small(r, r, r, num, mont);
      }
    }

    assert(wvalue & 1);
    assert(wvalue < (1u << window));
    if (r_is_one) {
      OPENSSL_memcpy(r, val[wvalue >> 1], num * sizeof(BN_ULONG));
    } else {
      bn_mod_mul_montgomery_small(r, r, val[wvalue >> 1], num, mont);
    }
    r_is_one = 0;
    if (wstart == wsize) {
      break;
    }
    wstart -= wsize + 1;
  }

  assert(!r_is_one);
  OPENSSL_cleanse(val, sizeof(val));
}

void bn_mod_inverse0_prime_mont_small(BN_ULONG *r, const BN_ULONG *a,
                                      size_t num, const BN_MONT_CTX *mont) {
  if (num != (size_t)mont->N.width || num > BN_SMALL_MAX_WORDS) {
    abort();
  }

  BN_ULONG p_minus_two[BN_SMALL_MAX_WORDS];
  const BN_ULONG *p = mont->N.d;
  OPENSSL_memcpy(p_minus_two, p, num * sizeof(BN_ULONG));
  if (p_minus_two[0] >= 2) {
    p_minus_two[0] -= 2;
  } else {
    p_minus_two[0] -= 2;
    for (size_t i = 1; i < num; i++) {
      if (p_minus_two[i]-- != 0) {
        break;
      }
    }
  }

  bn_mod_exp_mont_small(r, a, num, p_minus_two, num, mont);
}

static void copy_to_prebuf(const BIGNUM *b, int top, BN_ULONG *table, int idx,
                           int window) {
  int ret = bn_copy_words(table + idx * top, top, b);
  assert(ret);
  (void)ret;
}

static int copy_from_prebuf(BIGNUM *b, int top, const BN_ULONG *table, int idx,
                            int window) {
  if (!bn_wexpand(b, top)) {
    return 0;
  }

  OPENSSL_memset(b->d, 0, sizeof(BN_ULONG) * top);
  const int width = 1 << window;
  for (int i = 0; i < width; i++, table += top) {

    BN_ULONG mask = value_barrier_w(constant_time_eq_int(i, idx));
    for (int j = 0; j < top; j++) {
      b->d[j] |= table[j] & mask;
    }
  }

  b->width = top;
  return 1;
}

#define BN_window_bits_for_ctime_exponent_size(b) \
  ((b) > 937 ? 6 : (b) > 306 ? 5 : (b) > 89 ? 4 : (b) > 22 ? 3 : 1)
#define BN_MAX_MOD_EXP_CTIME_WINDOW (6)

int BN_mod_exp_mont_consttime(BIGNUM *rr, const BIGNUM *a, const BIGNUM *p,
                              const BIGNUM *m, BN_CTX *ctx,
                              const BN_MONT_CTX *mont) {
  int i, ret = 0, wvalue;
  BN_MONT_CTX *new_mont = NULL;

  void *powerbuf_free = NULL;
  size_t powerbuf_len = 0;
  BN_ULONG *powerbuf = NULL;

  if (!BN_is_odd(m)) {
    OPENSSL_PUT_ERROR(BN, BN_R_CALLED_WITH_EVEN_MODULUS);
    return 0;
  }
  if (m->neg) {
    OPENSSL_PUT_ERROR(BN, BN_R_NEGATIVE_NUMBER);
    return 0;
  }

  if (a->neg || constant_time_declassify_int(BN_ucmp(a, m) >= 0)) {
    OPENSSL_PUT_ERROR(BN, BN_R_INPUT_NOT_REDUCED);
    return 0;
  }

  int max_bits = p->width * BN_BITS2;
  int bits = max_bits;
  if (bits == 0) {

    if (BN_abs_is_word(m, 1)) {
      BN_zero(rr);
      return 1;
    }
    return BN_one(rr);
  }

  int top, num_powers, window;
  if (mont == NULL) {
    new_mont = BN_MONT_CTX_new_consttime(m, ctx);
    if (new_mont == NULL) {
      goto err;
    }
    mont = new_mont;
  }

  top = mont->N.width;

#if defined(OPENSSL_BN_ASM_MONT5) || defined(RSAZ_ENABLED)

  alignas(MOD_EXP_CTIME_ALIGN) BN_ULONG storage[MOD_EXP_CTIME_STORAGE_LEN];
#endif
#if defined(RSAZ_ENABLED)

  if (a->width == 16 && p->width == 16 && BN_num_bits(m) == 1024 &&
      rsaz_avx2_preferred()) {
    if (!bn_wexpand(rr, 16)) {
      goto err;
    }
    RSAZ_1024_mod_exp_avx2(rr->d, a->d, p->d, m->d, mont->RR.d, mont->n0[0],
                           storage);
    rr->width = 16;
    rr->neg = 0;
    ret = 1;
    goto err;
  }
#endif

  window = BN_window_bits_for_ctime_exponent_size(bits);
  assert(window <= BN_MAX_MOD_EXP_CTIME_WINDOW);

  assert((size_t)top <= BN_MONTGOMERY_MAX_WORDS);
  static_assert(
      BN_MONTGOMERY_MAX_WORDS <=
          INT_MAX / sizeof(BN_ULONG) / ((1 << BN_MAX_MOD_EXP_CTIME_WINDOW) + 3),
      "powerbuf_len may overflow");

#if defined(OPENSSL_BN_ASM_MONT5)
  if (window >= 5) {
    window = 5;

    powerbuf_len += top * sizeof(mont->N.d[0]);
  }
#endif

  num_powers = 1 << window;
  powerbuf_len += sizeof(m->d[0]) * top * (num_powers + 2);

#if defined(OPENSSL_BN_ASM_MONT5)
  if (powerbuf_len <= sizeof(storage)) {
    powerbuf = storage;
  }

  assert(powerbuf != NULL || top * BN_BITS2 > 1024);
#endif
  if (powerbuf == NULL) {
    powerbuf_free = OPENSSL_malloc(powerbuf_len + MOD_EXP_CTIME_ALIGN);
    if (powerbuf_free == NULL) {
      goto err;
    }
    powerbuf = reinterpret_cast<BN_ULONG *>(
        align_pointer(powerbuf_free, MOD_EXP_CTIME_ALIGN));
  }
  OPENSSL_memset(powerbuf, 0, powerbuf_len);

  BIGNUM tmp, am;
  tmp.d = powerbuf + top * num_powers;
  am.d = tmp.d + top;
  tmp.width = am.width = 0;
  tmp.dmax = am.dmax = top;
  tmp.neg = am.neg = 0;
  tmp.flags = am.flags = BN_FLG_STATIC_DATA;

  if (!bn_one_to_montgomery(&tmp, mont, ctx) || !bn_resize_words(&tmp, top)) {
    goto err;
  }

  assert(!a->neg);
  declassify_assert(BN_ucmp(a, m) < 0);
  if (!BN_to_montgomery(&am, a, mont, ctx) || !bn_resize_words(&am, top)) {
    goto err;
  }

#if defined(OPENSSL_BN_ASM_MONT5)

  if (window == 5 && top > 1) {

    BN_ULONG *np = am.d + top;
    for (i = 0; i < top; i++) {
      np[i] = mont->N.d[i];
    }

    const BN_ULONG *n0 = mont->n0;
    bn_scatter5(tmp.d, top, powerbuf, 0);
    bn_scatter5(am.d, am.width, powerbuf, 1);
    bn_mul_mont(tmp.d, am.d, am.d, np, n0, top);
    bn_scatter5(tmp.d, top, powerbuf, 2);

    for (i = 4; i < 32; i *= 2) {
      bn_mul_mont(tmp.d, tmp.d, tmp.d, np, n0, top);
      bn_scatter5(tmp.d, top, powerbuf, i);
    }

    for (i = 3; i < 32; i += 2) {
      bn_mul_mont_gather5(tmp.d, am.d, powerbuf, np, n0, top, i - 1);
      bn_scatter5(tmp.d, top, powerbuf, i);
      for (int j = 2 * i; j < 32; j *= 2) {
        bn_mul_mont(tmp.d, tmp.d, tmp.d, np, n0, top);
        bn_scatter5(tmp.d, top, powerbuf, j);
      }
    }

    bits--;
    for (wvalue = 0, i = bits % 5; i >= 0; i--, bits--) {
      wvalue = (wvalue << 1) + BN_is_bit_set(p, bits);
    }
    bn_gather5(tmp.d, top, powerbuf, wvalue);

    assert(bits >= -1 && (bits == -1 || bits % 5 == 4));

    if (!bn_power5_capable(top)) {
      while (bits >= 0) {
        for (wvalue = 0, i = 0; i < 5; i++, bits--) {
          wvalue = (wvalue << 1) + BN_is_bit_set(p, bits);
        }

        bn_mul_mont(tmp.d, tmp.d, tmp.d, np, n0, top);
        bn_mul_mont(tmp.d, tmp.d, tmp.d, np, n0, top);
        bn_mul_mont(tmp.d, tmp.d, tmp.d, np, n0, top);
        bn_mul_mont(tmp.d, tmp.d, tmp.d, np, n0, top);
        bn_mul_mont(tmp.d, tmp.d, tmp.d, np, n0, top);
        bn_mul_mont_gather5(tmp.d, tmp.d, powerbuf, np, n0, top, wvalue);
      }
    } else {
      const uint8_t *p_bytes = (const uint8_t *)p->d;
      assert(bits < max_bits);

      assert(max_bits >= 64);

      if (bits - 4 >= max_bits - 8) {

        wvalue = p_bytes[p->width * BN_BYTES - 1];
        wvalue >>= (bits - 4) & 7;
        wvalue &= 0x1f;
        bits -= 5;
        bn_power5(tmp.d, tmp.d, powerbuf, np, n0, top, wvalue);
      }
      while (bits >= 0) {

        int first_bit = bits - 4;
        uint16_t val;
        OPENSSL_memcpy(&val, p_bytes + (first_bit >> 3), sizeof(val));
        val >>= first_bit & 7;
        val &= 0x1f;
        bits -= 5;
        bn_power5(tmp.d, tmp.d, powerbuf, np, n0, top, val);
      }
    }

  } else
#endif
  {
    copy_to_prebuf(&tmp, top, powerbuf, 0, window);
    copy_to_prebuf(&am, top, powerbuf, 1, window);

    if (window > 1) {
      if (!BN_mod_mul_montgomery(&tmp, &am, &am, mont, ctx)) {
        goto err;
      }

      copy_to_prebuf(&tmp, top, powerbuf, 2, window);

      for (i = 3; i < num_powers; i++) {

        if (!BN_mod_mul_montgomery(&tmp, &am, &tmp, mont, ctx)) {
          goto err;
        }

        copy_to_prebuf(&tmp, top, powerbuf, i, window);
      }
    }

    bits--;
    for (wvalue = 0, i = bits % window; i >= 0; i--, bits--) {
      wvalue = (wvalue << 1) + BN_is_bit_set(p, bits);
    }
    if (!copy_from_prebuf(&tmp, top, powerbuf, wvalue, window)) {
      goto err;
    }

    while (bits >= 0) {
      wvalue = 0;

      for (i = 0; i < window; i++, bits--) {
        if (!BN_mod_mul_montgomery(&tmp, &tmp, &tmp, mont, ctx)) {
          goto err;
        }
        wvalue = (wvalue << 1) + BN_is_bit_set(p, bits);
      }

      if (!copy_from_prebuf(&am, top, powerbuf, wvalue, window)) {
        goto err;
      }

      if (!BN_mod_mul_montgomery(&tmp, &tmp, &am, mont, ctx)) {
        goto err;
      }
    }
  }

  if (!BN_from_montgomery(rr, &tmp, mont, ctx)) {
    goto err;
  }
  ret = 1;

err:
  BN_MONT_CTX_free(new_mont);
  if (powerbuf != NULL && powerbuf_free == NULL) {
    OPENSSL_cleanse(powerbuf, powerbuf_len);
  }
  OPENSSL_free(powerbuf_free);
  return ret;
}
