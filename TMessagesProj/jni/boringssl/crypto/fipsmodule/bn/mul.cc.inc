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
#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/mem.h>

#include "../../internal.h"
#include "internal.h"

#define BN_SQR_STACK_WORDS 16

static void bn_mul_normal(BN_ULONG *r, const BN_ULONG *a, size_t na,
                          const BN_ULONG *b, size_t nb) {
  if (na < nb) {
    size_t itmp = na;
    na = nb;
    nb = itmp;
    const BN_ULONG *ltmp = a;
    a = b;
    b = ltmp;
  }
  BN_ULONG *rr = &(r[na]);
  if (nb == 0) {
    OPENSSL_memset(r, 0, na * sizeof(BN_ULONG));
    return;
  }
  rr[0] = bn_mul_words(r, a, na, b[0]);

  for (;;) {
    if (--nb == 0) {
      return;
    }
    rr[1] = bn_mul_add_words(&(r[1]), a, na, b[1]);
    if (--nb == 0) {
      return;
    }
    rr[2] = bn_mul_add_words(&(r[2]), a, na, b[2]);
    if (--nb == 0) {
      return;
    }
    rr[3] = bn_mul_add_words(&(r[3]), a, na, b[3]);
    if (--nb == 0) {
      return;
    }
    rr[4] = bn_mul_add_words(&(r[4]), a, na, b[4]);
    rr += 4;
    r += 4;
    b += 4;
  }
}

static BN_ULONG bn_sub_part_words(BN_ULONG *r, const BN_ULONG *a,
                                  const BN_ULONG *b, int cl, int dl) {
  assert(cl >= 0);
  BN_ULONG borrow = bn_sub_words(r, a, b, cl);
  if (dl == 0) {
    return borrow;
  }

  r += cl;
  a += cl;
  b += cl;

  if (dl < 0) {

    dl = -dl;
    for (int i = 0; i < dl; i++) {
      r[i] = CRYPTO_subc_w(0, b[i], borrow, &borrow);
    }
  } else {

    for (int i = 0; i < dl; i++) {
      r[i] = CRYPTO_subc_w(a[i], 0, borrow, &borrow);
    }
  }

  return borrow;
}

static BN_ULONG bn_abs_sub_part_words(BN_ULONG *r, const BN_ULONG *a,
                                      const BN_ULONG *b, int cl, int dl,
                                      BN_ULONG *tmp) {
  BN_ULONG borrow = bn_sub_part_words(tmp, a, b, cl, dl);
  bn_sub_part_words(r, b, a, cl, -dl);
  int r_len = cl + (dl < 0 ? -dl : dl);
  borrow = 0 - borrow;
  bn_select_words(r, borrow, r              , tmp               , r_len);
  return borrow;
}

int bn_abs_sub_consttime(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                         BN_CTX *ctx) {
  int cl = a->width < b->width ? a->width : b->width;
  int dl = a->width - b->width;
  int r_len = a->width < b->width ? b->width : a->width;
  BN_CTX_start(ctx);
  BIGNUM *tmp = BN_CTX_get(ctx);
  int ok = tmp != NULL && bn_wexpand(r, r_len) && bn_wexpand(tmp, r_len);
  if (ok) {
    bn_abs_sub_part_words(r->d, a->d, b->d, cl, dl, tmp->d);
    r->width = r_len;
  }
  BN_CTX_end(ctx);
  return ok;
}

static int bn_mul_impl(BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
                       BN_CTX *ctx) {
  int al = a->width;
  int bl = b->width;
  if (al == 0 || bl == 0) {
    BN_zero(r);
    return 1;
  }

  int ret = 0, i, top;
  BIGNUM *rr;
  BN_CTX_start(ctx);
  if (r == a || r == b) {
    rr = BN_CTX_get(ctx);
    if (rr == NULL) {
      goto err;
    }
  } else {
    rr = r;
  }
  rr->neg = a->neg ^ b->neg;

  i = al - bl;
  if (i == 0) {
    if (al == 8) {
      if (!bn_wexpand(rr, 16)) {
        goto err;
      }
      rr->width = 16;
      bn_mul_comba8(rr->d, a->d, b->d);
      goto end;
    }
  }

  top = al + bl;
  if (!bn_wexpand(rr, top)) {
    goto err;
  }
  rr->width = top;
  bn_mul_normal(rr->d, a->d, al, b->d, bl);

end:
  if (r != rr && !BN_copy(r, rr)) {
    goto err;
  }
  ret = 1;

err:
  BN_CTX_end(ctx);
  return ret;
}

int BN_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx) {
  if (!bn_mul_impl(r, a, b, ctx)) {
    return 0;
  }

  bn_set_minimal_width(r);
  return 1;
}

int bn_mul_consttime(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx) {

  if (a->neg || b->neg) {
    OPENSSL_PUT_ERROR(BN, BN_R_NEGATIVE_NUMBER);
    return 0;
  }

  return bn_mul_impl(r, a, b, ctx);
}

void bn_mul_small(BN_ULONG *r, size_t num_r, const BN_ULONG *a, size_t num_a,
                  const BN_ULONG *b, size_t num_b) {
  if (num_r != num_a + num_b) {
    abort();
  }

  if (num_a == 8 && num_b == 8) {
    bn_mul_comba8(r, a, b);
  } else {
    bn_mul_normal(r, a, num_a, b, num_b);
  }
}

static void bn_sqr_normal(BN_ULONG *r, const BN_ULONG *a, size_t n,
                          BN_ULONG *tmp) {
  if (n == 0) {
    return;
  }

  size_t max = n * 2;
  const BN_ULONG *ap = a;
  BN_ULONG *rp = r;
  rp[0] = rp[max - 1] = 0;
  rp++;

  if (n > 1) {
    ap++;
    rp[n - 1] = bn_mul_words(rp, ap, n - 1, ap[-1]);
    rp += 2;
  }
  if (n > 2) {
    for (size_t i = n - 2; i > 0; i--) {
      ap++;
      rp[i] = bn_mul_add_words(rp, ap, i, ap[-1]);
      rp += 2;
    }
  }

  bn_add_words(r, r, r, max);

  bn_sqr_words(tmp, a, n);
  bn_add_words(r, r, tmp, max);
}

int BN_mul_word(BIGNUM *bn, BN_ULONG w) {
  if (!bn->width) {
    return 1;
  }

  if (w == 0) {
    BN_zero(bn);
    return 1;
  }

  BN_ULONG ll = bn_mul_words(bn->d, bn->d, bn->width, w);
  if (ll) {
    if (!bn_wexpand(bn, bn->width + 1)) {
      return 0;
    }
    bn->d[bn->width++] = ll;
  }

  return 1;
}

int bn_sqr_consttime(BIGNUM *r, const BIGNUM *a, BN_CTX *ctx) {
  int al = a->width;
  if (al <= 0) {
    r->width = 0;
    r->neg = 0;
    return 1;
  }

  int ret = 0, max;
  BN_CTX_start(ctx);
  BIGNUM *rr = (a != r) ? r : BN_CTX_get(ctx);
  BIGNUM *tmp = BN_CTX_get(ctx);
  if (!rr || !tmp) {
    goto err;
  }

  max = 2 * al;
  if (!bn_wexpand(rr, max)) {
    goto err;
  }

  if (al == 4) {
    bn_sqr_comba4(rr->d, a->d);
  } else if (al == 8) {
    bn_sqr_comba8(rr->d, a->d);
  } else {
    if (al < BN_SQR_STACK_WORDS) {
      BN_ULONG t[BN_SQR_STACK_WORDS * 2];
      bn_sqr_normal(rr->d, a->d, al, t);
    } else {
      if (!bn_wexpand(tmp, max)) {
        goto err;
      }
      bn_sqr_normal(rr->d, a->d, al, tmp->d);
    }
  }

  rr->neg = 0;
  rr->width = max;

  if (rr != r && !BN_copy(r, rr)) {
    goto err;
  }
  ret = 1;

err:
  BN_CTX_end(ctx);
  return ret;
}

int BN_sqr(BIGNUM *r, const BIGNUM *a, BN_CTX *ctx) {
  if (!bn_sqr_consttime(r, a, ctx)) {
    return 0;
  }

  bn_set_minimal_width(r);
  return 1;
}

void bn_sqr_small(BN_ULONG *r, size_t num_r, const BN_ULONG *a, size_t num_a) {
  if (num_r != 2 * num_a || num_a > BN_SMALL_MAX_WORDS) {
    abort();
  }
  if (num_a == 4) {
    bn_sqr_comba4(r, a);
  } else if (num_a == 8) {
    bn_sqr_comba8(r, a);
  } else {
    BN_ULONG tmp[2 * BN_SMALL_MAX_WORDS];
    bn_sqr_normal(r, a, num_a, tmp);
    OPENSSL_cleanse(tmp, 2 * num_a * sizeof(BN_ULONG));
  }
}
