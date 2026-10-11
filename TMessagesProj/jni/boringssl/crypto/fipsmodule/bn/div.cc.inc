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

#include <openssl/err.h>

#include "internal.h"

static inline BN_ULONG bn_div_words(BN_ULONG h, BN_ULONG l, BN_ULONG d) {
  BN_ULONG dh, dl, q, ret = 0, th, tl, t;
  int i, count = 2;

  if (d == 0) {
    return BN_MASK2;
  }

  i = BN_num_bits_word(d);
  assert((i == BN_BITS2) || (h <= (BN_ULONG)1 << i));

  i = BN_BITS2 - i;
  if (h >= d) {
    h -= d;
  }

  if (i) {
    d <<= i;
    h = (h << i) | (l >> (BN_BITS2 - i));
    l <<= i;
  }
  dh = (d & BN_MASK2h) >> BN_BITS4;
  dl = (d & BN_MASK2l);
  for (;;) {
    if ((h >> BN_BITS4) == dh) {
      q = BN_MASK2l;
    } else {
      q = h / dh;
    }

    th = q * dh;
    tl = dl * q;
    for (;;) {
      t = h - th;
      if ((t & BN_MASK2h) ||
          ((tl) <= ((t << BN_BITS4) | ((l & BN_MASK2h) >> BN_BITS4)))) {
        break;
      }
      q--;
      th -= dh;
      tl -= dl;
    }
    t = (tl >> BN_BITS4);
    tl = (tl << BN_BITS4) & BN_MASK2h;
    th += t;

    if (l < tl) {
      th++;
    }
    l -= tl;
    if (h < th) {
      h += d;
      q--;
    }
    h -= th;

    if (--count == 0) {
      break;
    }

    ret = q << BN_BITS4;
    h = (h << BN_BITS4) | (l >> BN_BITS4);
    l = (l & BN_MASK2l) << BN_BITS4;
  }

  ret |= q;
  return ret;
}

static inline void bn_div_rem_words(BN_ULONG *quotient_out, BN_ULONG *rem_out,
                                    BN_ULONG n0, BN_ULONG n1, BN_ULONG d0) {

#if defined(BN_CAN_USE_INLINE_ASM) && defined(OPENSSL_X86)
  __asm__ volatile("divl %4"
                   : "=a"(*quotient_out), "=d"(*rem_out)
                   : "a"(n1), "d"(n0), "rm"(d0)
                   : "cc");
#elif defined(BN_CAN_USE_INLINE_ASM) && defined(OPENSSL_X86_64)
  __asm__ volatile("divq %4"
                   : "=a"(*quotient_out), "=d"(*rem_out)
                   : "a"(n1), "d"(n0), "rm"(d0)
                   : "cc");
#else
#if defined(BN_CAN_DIVIDE_ULLONG)
  BN_ULLONG n = (((BN_ULLONG)n0) << BN_BITS2) | n1;
  *quotient_out = (BN_ULONG)(n / d0);
#else
  *quotient_out = bn_div_words(n0, n1, d0);
#endif
  *rem_out = n1 - (*quotient_out * d0);
#endif
}

int BN_div(BIGNUM *quotient, BIGNUM *rem, const BIGNUM *numerator,
           const BIGNUM *divisor, BN_CTX *ctx) {

  if (BN_is_zero(divisor)) {
    OPENSSL_PUT_ERROR(BN, BN_R_DIV_BY_ZERO);
    return 0;
  }

  bssl::BN_CTXScope scope(ctx);
  BIGNUM *tmp = BN_CTX_get(ctx);
  BIGNUM *snum = BN_CTX_get(ctx);
  BIGNUM *sdiv = BN_CTX_get(ctx);
  BIGNUM *res = quotient == NULL ? BN_CTX_get(ctx) : quotient;
  int norm_shift, num_n, loop, div_n;
  BN_ULONG d0, d1;
  if (tmp == NULL || snum == NULL || sdiv == NULL || res == NULL) {
    return 0;
  }

  norm_shift = BN_BITS2 - (BN_num_bits(divisor) % BN_BITS2);
  if (!BN_lshift(sdiv, divisor, norm_shift) ||
      !BN_lshift(snum, numerator, norm_shift)) {
    return 0;
  }

  bn_set_minimal_width(sdiv);
  bn_set_minimal_width(snum);
  div_n = sdiv->width;
  d0 = sdiv->d[div_n - 1];
  d1 = (div_n == 1) ? 0 : sdiv->d[div_n - 2];
  assert(d0 & (((BN_ULONG)1) << (BN_BITS2 - 1)));

  num_n = snum->width <= div_n ? div_n + 1 : snum->width + 1;
  if (!bn_resize_words(snum, num_n)) {
    return 0;
  }

  loop = num_n - div_n;
  res->neg = snum->neg ^ sdiv->neg;
  if (!bn_wexpand(res, loop) ||
      !bn_wexpand(tmp, div_n + 1)) {
    return 0;
  }
  res->width = loop;

  for (int i = loop - 1; i >= 0; i--) {

    BN_ULONG q, rm = 0;
    BN_ULONG *wnum = snum->d + i;
    BN_ULONG n0 = wnum[div_n];
    BN_ULONG n1 = wnum[div_n - 1];
    if (n0 == d0) {

      q = BN_MASK2;

    } else {

      assert(n0 < d0);
      bn_div_rem_words(&q, &rm, n0, n1, d0);

      if (div_n > 1) {
        BN_ULONG n2 = wnum[div_n - 2];
#ifdef BN_ULLONG
        BN_ULLONG t2 = (BN_ULLONG)d1 * q;
        for (;;) {
          if (t2 <= ((((BN_ULLONG)rm) << BN_BITS2) | n2)) {
            break;
          }
          q--;
          rm += d0;
          if (rm < d0) {

            break;
          }
          t2 -= d1;
        }
#else
        BN_ULONG t2l, t2h;
        BN_UMULT_LOHI(t2l, t2h, d1, q);
        for (;;) {
          if (t2h < rm || (t2h == rm && t2l <= n2)) {
            break;
          }
          q--;
          rm += d0;
          if (rm < d0) {

            break;
          }
          if (t2l < d1) {
            t2h--;
          }
          t2l -= d1;
        }
#endif
      }
    }

    tmp->d[div_n] = bn_mul_words(tmp->d, sdiv->d, div_n, q);
    if (bn_sub_words(wnum, wnum, tmp->d, div_n + 1)) {
      q--;

      wnum[div_n] += bn_add_words(wnum, wnum, sdiv->d, div_n);
    }

    res->d[i] = q;
  }

  bn_set_minimal_width(snum);
  bn_set_minimal_width(res);

  if (rem != NULL && !BN_rshift(rem, snum, norm_shift)) {
    return 0;
  }

  return 1;
}

int BN_nnmod(BIGNUM *r, const BIGNUM *m, const BIGNUM *d, BN_CTX *ctx) {
  if (!(BN_mod(r, m, d, ctx))) {
    return 0;
  }
  if (!r->neg) {
    return 1;
  }

  return BN_usub(r, d, r);
}

BN_ULONG bn_reduce_once(BN_ULONG *r, const BN_ULONG *a, BN_ULONG carry,
                        const BN_ULONG *m, size_t num) {
  assert(r != a);

  carry -= bn_sub_words(r, a, m, num);

  declassify_assert(carry + 1 <= 1);
  bn_select_words(r, carry, a            , r             , num);
  return carry;
}

BN_ULONG bn_reduce_once_in_place(BN_ULONG *r, BN_ULONG carry, const BN_ULONG *m,
                                 BN_ULONG *tmp, size_t num) {

  carry -= bn_sub_words(tmp, r, m, num);
  declassify_assert(carry + 1 <= 1);
  bn_select_words(r, carry, r              , tmp               , num);
  return carry;
}

void bn_mod_sub_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
                      const BN_ULONG *m, BN_ULONG *tmp, size_t num) {

  BN_ULONG borrow = bn_sub_words(r, a, b, num);

  bn_add_words(tmp, r, m, num);
  bn_select_words(r, 0 - borrow, tmp            , r             , num);
}

void bn_mod_add_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
                      const BN_ULONG *m, BN_ULONG *tmp, size_t num) {
  BN_ULONG carry = bn_add_words(r, a, b, num);
  bn_reduce_once_in_place(r, carry, m, tmp, num);
}

int bn_div_consttime(BIGNUM *quotient, BIGNUM *remainder,
                     const BIGNUM *numerator, const BIGNUM *divisor,
                     unsigned divisor_min_bits, BN_CTX *ctx) {
  if (BN_is_negative(numerator) || BN_is_negative(divisor)) {
    OPENSSL_PUT_ERROR(BN, BN_R_NEGATIVE_NUMBER);
    return 0;
  }
  if (BN_is_zero(divisor)) {
    OPENSSL_PUT_ERROR(BN, BN_R_DIV_BY_ZERO);
    return 0;
  }

  bssl::BN_CTXScope scope(ctx);
  BIGNUM *q = quotient, *r = remainder;
  if (quotient == NULL || quotient == numerator || quotient == divisor) {
    q = BN_CTX_get(ctx);
  }
  if (remainder == NULL || remainder == numerator || remainder == divisor) {
    r = BN_CTX_get(ctx);
  }
  BIGNUM *tmp = BN_CTX_get(ctx);
  int initial_words;
  if (q == NULL || r == NULL || tmp == NULL ||
      !bn_wexpand(q, numerator->width) || !bn_wexpand(r, divisor->width) ||
      !bn_wexpand(tmp, divisor->width)) {
    return 0;
  }

  OPENSSL_memset(q->d, 0, numerator->width * sizeof(BN_ULONG));
  q->width = numerator->width;
  q->neg = 0;

  OPENSSL_memset(r->d, 0, divisor->width * sizeof(BN_ULONG));
  r->width = divisor->width;
  r->neg = 0;

  declassify_assert(divisor_min_bits <= BN_num_bits(divisor));
  initial_words = 0;
  if (divisor_min_bits > 0) {
    initial_words = (divisor_min_bits - 1) / BN_BITS2;
    if (initial_words > numerator->width) {
      initial_words = numerator->width;
    }
    OPENSSL_memcpy(r->d, numerator->d + numerator->width - initial_words,
                   initial_words * sizeof(BN_ULONG));
  }

  for (int i = numerator->width - initial_words - 1; i >= 0; i--) {
    for (int bit = BN_BITS2 - 1; bit >= 0; bit--) {

      BN_ULONG carry = bn_add_words(r->d, r->d, r->d, divisor->width);
      r->d[0] |= (numerator->d[i] >> bit) & 1;

      BN_ULONG subtracted = bn_reduce_once_in_place(r->d, carry, divisor->d,
                                                    tmp->d, divisor->width);

      q->d[i] |= (~subtracted & 1) << bit;
    }
  }

  if ((quotient != NULL && !BN_copy(quotient, q)) ||
      (remainder != NULL && !BN_copy(remainder, r))) {
    return 0;
  }

  return 1;
}

static BIGNUM *bn_scratch_space_from_ctx(size_t width, BN_CTX *ctx) {
  BIGNUM *ret = BN_CTX_get(ctx);
  if (ret == NULL || !bn_wexpand(ret, width)) {
    return NULL;
  }
  ret->neg = 0;
  ret->width = (int)width;
  return ret;
}

static const BIGNUM *bn_resized_from_ctx(const BIGNUM *bn, size_t width,
                                         BN_CTX *ctx) {
  if ((size_t)bn->width >= width) {

    assert(bn_fits_in_words(bn, width));
    return bn;
  }
  BIGNUM *ret = bn_scratch_space_from_ctx(width, ctx);
  if (ret == NULL || !BN_copy(ret, bn) || !bn_resize_words(ret, width)) {
    return NULL;
  }
  return ret;
}

int BN_mod_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m,
               BN_CTX *ctx) {
  if (!BN_add(r, a, b)) {
    return 0;
  }
  return BN_nnmod(r, r, m, ctx);
}

int BN_mod_add_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                     const BIGNUM *m) {
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  return ctx != nullptr && bn_mod_add_consttime(r, a, b, m, ctx.get());
}

int bn_mod_add_consttime(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                         const BIGNUM *m, BN_CTX *ctx) {
  bssl::BN_CTXScope scope(ctx);
  a = bn_resized_from_ctx(a, m->width, ctx);
  b = bn_resized_from_ctx(b, m->width, ctx);
  BIGNUM *tmp = bn_scratch_space_from_ctx(m->width, ctx);
  if (a == nullptr || b == nullptr || tmp == nullptr ||
      !bn_wexpand(r, m->width)) {
    return 0;
  }
  bn_mod_add_words(r->d, a->d, b->d, m->d, tmp->d, m->width);
  r->width = m->width;
  r->neg = 0;
  return 1;
}

int BN_mod_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m,
               BN_CTX *ctx) {
  if (!BN_sub(r, a, b)) {
    return 0;
  }
  return BN_nnmod(r, r, m, ctx);
}

int bn_mod_sub_consttime(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                         const BIGNUM *m, BN_CTX *ctx) {
  bssl::BN_CTXScope scope(ctx);
  a = bn_resized_from_ctx(a, m->width, ctx);
  b = bn_resized_from_ctx(b, m->width, ctx);
  BIGNUM *tmp = bn_scratch_space_from_ctx(m->width, ctx);
  if (a == nullptr || b == nullptr || tmp == nullptr ||
      !bn_wexpand(r, m->width)) {
    return 0;
  }
  bn_mod_sub_words(r->d, a->d, b->d, m->d, tmp->d, m->width);
  r->width = m->width;
  r->neg = 0;
  return 1;
}

int BN_mod_sub_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                     const BIGNUM *m) {
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  return ctx != nullptr && bn_mod_sub_consttime(r, a, b, m, ctx.get());
}

int BN_mod_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m,
               BN_CTX *ctx) {
  bssl::BN_CTXScope scope(ctx);
  BIGNUM *t = BN_CTX_get(ctx);
  if (t == NULL) {
    return 0;
  }

  if (a == b) {
    if (!BN_sqr(t, a, ctx)) {
      return 0;
    }
  } else {
    if (!BN_mul(t, a, b, ctx)) {
      return 0;
    }
  }

  if (!BN_nnmod(r, t, m, ctx)) {
    return 0;
  }

  return 1;
}

int BN_mod_sqr(BIGNUM *r, const BIGNUM *a, const BIGNUM *m, BN_CTX *ctx) {
  if (!BN_sqr(r, a, ctx)) {
    return 0;
  }

  return BN_mod(r, r, m, ctx);
}

int BN_mod_lshift(BIGNUM *r, const BIGNUM *a, int n, const BIGNUM *m,
                  BN_CTX *ctx) {
  if (!BN_nnmod(r, a, m, ctx)) {
    return 0;
  }

  bssl::UniquePtr<BIGNUM> abs_m;
  if (m->neg) {
    abs_m.reset(BN_dup(m));
    if (abs_m == nullptr) {
      return 0;
    }
    abs_m->neg = 0;
  }

  return bn_mod_lshift_consttime(r, r, n, (abs_m ? abs_m.get() : m), ctx);
}

int bn_mod_lshift_consttime(BIGNUM *r, const BIGNUM *a, int n, const BIGNUM *m,
                            BN_CTX *ctx) {
  if (!BN_copy(r, a) || !bn_resize_words(r, m->width)) {
    return 0;
  }

  bssl::BN_CTXScope scope(ctx);
  BIGNUM *tmp = bn_scratch_space_from_ctx(m->width, ctx);
  if (tmp == nullptr) {
    return 0;
  }
  for (int i = 0; i < n; i++) {
    bn_mod_add_words(r->d, r->d, r->d, m->d, tmp->d, m->width);
  }
  r->neg = 0;
  return 1;
}

int BN_mod_lshift_quick(BIGNUM *r, const BIGNUM *a, int n, const BIGNUM *m) {
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  return ctx != nullptr && bn_mod_lshift_consttime(r, a, n, m, ctx.get());
}

int BN_mod_lshift1(BIGNUM *r, const BIGNUM *a, const BIGNUM *m, BN_CTX *ctx) {
  if (!BN_lshift1(r, a)) {
    return 0;
  }

  return BN_nnmod(r, r, m, ctx);
}

int bn_mod_lshift1_consttime(BIGNUM *r, const BIGNUM *a, const BIGNUM *m,
                             BN_CTX *ctx) {
  return bn_mod_add_consttime(r, a, a, m, ctx);
}

int BN_mod_lshift1_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *m) {
  bssl::UniquePtr<BN_CTX> ctx(BN_CTX_new());
  return ctx != nullptr && bn_mod_lshift1_consttime(r, a, m, ctx.get());
}

BN_ULONG BN_div_word(BIGNUM *a, BN_ULONG w) {
  BN_ULONG ret = 0;
  int i, j;

  if (!w) {

    return (BN_ULONG)-1;
  }

  if (a->width == 0) {
    return 0;
  }

  j = BN_BITS2 - BN_num_bits_word(w);
  w <<= j;
  if (!BN_lshift(a, a, j)) {
    return (BN_ULONG)-1;
  }

  for (i = a->width - 1; i >= 0; i--) {
    BN_ULONG l = a->d[i];
    BN_ULONG d;
    BN_ULONG unused_rem;
    bn_div_rem_words(&d, &unused_rem, ret, l, w);
    ret = l - (d * w);
    a->d[i] = d;
  }

  bn_set_minimal_width(a);
  ret >>= j;
  return ret;
}

BN_ULONG BN_mod_word(const BIGNUM *a, BN_ULONG w) {
#ifndef BN_CAN_DIVIDE_ULLONG
  BN_ULONG ret = 0;
#else
  BN_ULLONG ret = 0;
#endif
  int i;

  if (w == 0) {
    return (BN_ULONG)-1;
  }

#ifndef BN_CAN_DIVIDE_ULLONG

  if (w > ((BN_ULONG)1 << BN_BITS4)) {
    BIGNUM *tmp = BN_dup(a);
    if (tmp == NULL) {
      return (BN_ULONG)-1;
    }
    ret = BN_div_word(tmp, w);
    BN_free(tmp);
    return ret;
  }
#endif

  for (i = a->width - 1; i >= 0; i--) {
#ifndef BN_CAN_DIVIDE_ULLONG
    ret = ((ret << BN_BITS4) | ((a->d[i] >> BN_BITS4) & BN_MASK2l)) % w;
    ret = ((ret << BN_BITS4) | (a->d[i] & BN_MASK2l)) % w;
#else
    ret = (BN_ULLONG)(((ret << (BN_ULLONG)BN_BITS2) | a->d[i]) % (BN_ULLONG)w);
#endif
  }
  return (BN_ULONG)ret;
}

int BN_mod_pow2(BIGNUM *r, const BIGNUM *a, size_t e) {
  if (e == 0 || a->width == 0) {
    BN_zero(r);
    return 1;
  }

  size_t num_words = 1 + ((e - 1) / BN_BITS2);

  if ((size_t)a->width < num_words) {
    return BN_copy(r, a) != NULL;
  }

  if (!bn_wexpand(r, num_words)) {
    return 0;
  }

  OPENSSL_memcpy(r->d, a->d, num_words * sizeof(BN_ULONG));

  size_t top_word_exponent = e % (sizeof(BN_ULONG) * 8);
  if (top_word_exponent != 0) {
    r->d[num_words - 1] &= (((BN_ULONG)1) << top_word_exponent) - 1;
  }

  r->neg = a->neg;
  r->width = (int)num_words;
  bn_set_minimal_width(r);
  return 1;
}

int BN_nnmod_pow2(BIGNUM *r, const BIGNUM *a, size_t e) {
  if (!BN_mod_pow2(r, a, e)) {
    return 0;
  }

  if (BN_is_zero(r) || !r->neg) {
    return 1;
  }

  size_t num_words = 1 + (e - 1) / BN_BITS2;

  if (!bn_wexpand(r, num_words)) {
    return 0;
  }

  OPENSSL_memset(&r->d[r->width], 0, (num_words - r->width) * BN_BYTES);

  r->neg = 0;
  r->width = (int)num_words;

  for (int i = 0; i < r->width; i++) {
    r->d[i] = ~r->d[i];
  }

  size_t top_word_exponent = e % BN_BITS2;
  if (top_word_exponent != 0) {
    r->d[r->width - 1] &= (((BN_ULONG)1) << top_word_exponent) - 1;
  }

  bn_set_minimal_width(r);

  return BN_add(r, r, BN_value_one());
}
