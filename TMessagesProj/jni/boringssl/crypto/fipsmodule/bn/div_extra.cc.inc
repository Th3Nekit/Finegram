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

#include "internal.h"

static uint16_t mod_u16(uint32_t n, uint16_t d, uint32_t p, uint32_t m) {

  uint32_t q = ((uint64_t)m * n) >> 32;

  uint32_t t = ((n - q) >> 1) + q;
  t = t >> (p - 1);

  n -= d * t;
  declassify_assert(n < d);
  return n;
}

static uint16_t shift_and_add_mod_u16(uint16_t r, uint32_t a, uint16_t d,
                                      uint32_t p, uint32_t m) {

  uint32_t t = r;
  t <<= 16;
  t |= a >> 16;
  t = mod_u16(t, d, p, m);

  t <<= 16;
  t |= a & 0xffff;
  t = mod_u16(t, d, p, m);
  return t;
}

uint16_t bn_mod_u16_consttime(const BIGNUM *bn, uint16_t d) {
  if (d <= 1) {
    return 0;
  }

  uint32_t p = BN_num_bits_word(d - 1);

  assert(p <= 16);
  uint32_t m = (uint32_t)(((UINT64_C(1) << (32 + p)) + d - 1) / d);

  uint16_t ret = 0;
  for (int i = bn->width - 1; i >= 0; i--) {
#if BN_BITS2 == 32
    ret = shift_and_add_mod_u16(ret, bn->d[i], d, p, m);
#elif BN_BITS2 == 64
    ret = shift_and_add_mod_u16(ret, bn->d[i] >> 32, d, p, m);
    ret = shift_and_add_mod_u16(ret, bn->d[i] & 0xffffffff, d, p, m);
#else
#error "Unknown BN_ULONG size"
#endif
  }
  return ret;
}
