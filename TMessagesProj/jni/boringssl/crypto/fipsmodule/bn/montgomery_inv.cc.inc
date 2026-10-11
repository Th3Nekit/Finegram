// Copyright 2016 Brian Smith.
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

#include "internal.h"
#include "../../internal.h"

static uint64_t bn_neg_inv_mod_r_u64(uint64_t n);

static_assert(BN_MONT_CTX_N0_LIMBS == 1 || BN_MONT_CTX_N0_LIMBS == 2,
              "BN_MONT_CTX_N0_LIMBS value is invalid");
static_assert(sizeof(BN_ULONG) * BN_MONT_CTX_N0_LIMBS == sizeof(uint64_t),
              "uint64_t is insufficient precision for n0");

#define LG_LITTLE_R (BN_MONT_CTX_N0_LIMBS * BN_BITS2)

uint64_t bn_mont_n0(const BIGNUM *n) {

  assert(!BN_is_zero(n));
  assert(!BN_is_negative(n));
  assert(BN_is_odd(n));

  uint64_t n_mod_r = n->d[0];
#if BN_MONT_CTX_N0_LIMBS == 2
  if (n->width > 1) {
    n_mod_r |= (uint64_t)n->d[1] << BN_BITS2;
  }
#endif

  return bn_neg_inv_mod_r_u64(n_mod_r);
}

static uint64_t bn_neg_inv_mod_r_u64(uint64_t n) {
  assert(n % 2 == 1);

  static const uint64_t alpha = UINT64_C(1) << (LG_LITTLE_R - 1);

  const uint64_t beta = n;

  uint64_t u = 1;
  uint64_t v = 0;

  for (size_t i = 0; i < LG_LITTLE_R; ++i) {
#if BN_BITS2 == 64 && defined(BN_ULLONG)
    assert((BN_ULLONG)(1) << (LG_LITTLE_R - i) ==
           ((BN_ULLONG)u * 2 * alpha) - ((BN_ULLONG)v * beta));
#endif

    uint64_t u_is_odd = UINT64_C(0) - (u & 1);

    uint64_t beta_if_u_is_odd = beta & u_is_odd;
    u = ((u ^ beta_if_u_is_odd) >> 1) + (u & beta_if_u_is_odd);

    uint64_t alpha_if_u_is_odd = alpha & u_is_odd;
    v = (v >> 1) + alpha_if_u_is_odd;
  }

#if BN_BITS2 == 64 && defined(BN_ULLONG)
  declassify_assert(1 == ((BN_ULLONG)u * 2 * alpha) - ((BN_ULLONG)v * beta));
#endif

  return v;
}

int bn_mont_ctx_set_RR_consttime(BN_MONT_CTX *mont, BN_CTX *ctx) {
  assert(!BN_is_zero(&mont->N));
  assert(!BN_is_negative(&mont->N));
  assert(BN_is_odd(&mont->N));
  assert(bn_minimal_width(&mont->N) == mont->N.width);

  unsigned n_bits = BN_num_bits(&mont->N);
  assert(n_bits != 0);
  if (n_bits == 1) {
    BN_zero(&mont->RR);
    return bn_resize_words(&mont->RR, mont->N.width);
  }

  unsigned lgBigR = mont->N.width * BN_BITS2;
  assert(lgBigR >= n_bits);

  int threshold = mont->N.width;

  if (!BN_set_bit(&mont->RR, n_bits - 1) ||
      !bn_mod_lshift_consttime(&mont->RR, &mont->RR,
                               threshold + (lgBigR - (n_bits - 1)),
                               &mont->N, ctx)) {
    return 0;
  }

  assert(threshold == mont->N.width);
  for (unsigned i = 0; i < BN_BITS2_LG; i++) {
    if (!BN_mod_mul_montgomery(&mont->RR, &mont->RR, &mont->RR, mont, ctx)) {
      return 0;
    }
  }

  return bn_resize_words(&mont->RR, mont->N.width);
}
