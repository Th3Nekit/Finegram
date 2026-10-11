// Copyright 2019 The BoringSSL Authors
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

#include <openssl/base.h>

#include "../../internal.h"
#include "internal.h"

#if !defined(BORINGSSL_HAS_UINT128) && defined(OPENSSL_SSE2)
#include <emmintrin.h>
#endif

#if defined(BORINGSSL_HAS_UINT128)

static void gcm_mul64_nohw(uint64_t *out_lo, uint64_t *out_hi, uint64_t a,
                           uint64_t b) {

  uint64_t a0 = a & UINT64_C(0x1111111111111110);
  uint64_t a1 = a & UINT64_C(0x2222222222222220);
  uint64_t a2 = a & UINT64_C(0x4444444444444440);
  uint64_t a3 = a & UINT64_C(0x8888888888888880);

  uint64_t b0 = b & UINT64_C(0x1111111111111111);
  uint64_t b1 = b & UINT64_C(0x2222222222222222);
  uint64_t b2 = b & UINT64_C(0x4444444444444444);
  uint64_t b3 = b & UINT64_C(0x8888888888888888);

  uint128_t c0 = (a0 * (uint128_t)b0) ^ (a1 * (uint128_t)b3) ^
                 (a2 * (uint128_t)b2) ^ (a3 * (uint128_t)b1);
  uint128_t c1 = (a0 * (uint128_t)b1) ^ (a1 * (uint128_t)b0) ^
                 (a2 * (uint128_t)b3) ^ (a3 * (uint128_t)b2);
  uint128_t c2 = (a0 * (uint128_t)b2) ^ (a1 * (uint128_t)b1) ^
                 (a2 * (uint128_t)b0) ^ (a3 * (uint128_t)b3);
  uint128_t c3 = (a0 * (uint128_t)b3) ^ (a1 * (uint128_t)b2) ^
                 (a2 * (uint128_t)b1) ^ (a3 * (uint128_t)b0);

  uint64_t a0_mask = UINT64_C(0) - (a & 1);
  uint64_t a1_mask = UINT64_C(0) - ((a >> 1) & 1);
  uint64_t a2_mask = UINT64_C(0) - ((a >> 2) & 1);
  uint64_t a3_mask = UINT64_C(0) - ((a >> 3) & 1);
  uint128_t extra = (a0_mask & b) ^ ((uint128_t)(a1_mask & b) << 1) ^
                    ((uint128_t)(a2_mask & b) << 2) ^
                    ((uint128_t)(a3_mask & b) << 3);

  *out_lo = (((uint64_t)c0) & UINT64_C(0x1111111111111111)) ^
            (((uint64_t)c1) & UINT64_C(0x2222222222222222)) ^
            (((uint64_t)c2) & UINT64_C(0x4444444444444444)) ^
            (((uint64_t)c3) & UINT64_C(0x8888888888888888)) ^ ((uint64_t)extra);
  *out_hi = (((uint64_t)(c0 >> 64)) & UINT64_C(0x1111111111111111)) ^
            (((uint64_t)(c1 >> 64)) & UINT64_C(0x2222222222222222)) ^
            (((uint64_t)(c2 >> 64)) & UINT64_C(0x4444444444444444)) ^
            (((uint64_t)(c3 >> 64)) & UINT64_C(0x8888888888888888)) ^
            ((uint64_t)(extra >> 64));
}

#elif defined(OPENSSL_SSE2)

static __m128i gcm_mul32_nohw(uint32_t a, uint32_t b) {

  __m128i aa = _mm_setr_epi32(a, 0, a, 0);
  __m128i bb = _mm_setr_epi32(b, 0, b, 0);

  __m128i a0a0 =
      _mm_and_si128(aa, _mm_setr_epi32(0x11111111, 0, 0x11111111, 0));
  __m128i a2a2 =
      _mm_and_si128(aa, _mm_setr_epi32(0x44444444, 0, 0x44444444, 0));
  __m128i b0b1 =
      _mm_and_si128(bb, _mm_setr_epi32(0x11111111, 0, 0x22222222, 0));
  __m128i b2b3 =
      _mm_and_si128(bb, _mm_setr_epi32(0x44444444, 0, 0x88888888, 0));

  __m128i c0c1 =
      _mm_xor_si128(_mm_mul_epu32(a0a0, b0b1), _mm_mul_epu32(a2a2, b2b3));
  __m128i c2c3 =
      _mm_xor_si128(_mm_mul_epu32(a2a2, b0b1), _mm_mul_epu32(a0a0, b2b3));

  __m128i a1a1 =
      _mm_and_si128(aa, _mm_setr_epi32(0x22222222, 0, 0x22222222, 0));
  __m128i a3a3 =
      _mm_and_si128(aa, _mm_setr_epi32(0x88888888, 0, 0x88888888, 0));
  __m128i b3b0 =
      _mm_and_si128(bb, _mm_setr_epi32(0x88888888, 0, 0x11111111, 0));
  __m128i b1b2 =
      _mm_and_si128(bb, _mm_setr_epi32(0x22222222, 0, 0x44444444, 0));

  c0c1 = _mm_xor_si128(c0c1, _mm_mul_epu32(a1a1, b3b0));
  c0c1 = _mm_xor_si128(c0c1, _mm_mul_epu32(a3a3, b1b2));
  c2c3 = _mm_xor_si128(c2c3, _mm_mul_epu32(a3a3, b3b0));
  c2c3 = _mm_xor_si128(c2c3, _mm_mul_epu32(a1a1, b1b2));

  c0c1 = _mm_and_si128(
      c0c1, _mm_setr_epi32(0x11111111, 0x11111111, 0x22222222, 0x22222222));
  c2c3 = _mm_and_si128(
      c2c3, _mm_setr_epi32(0x44444444, 0x44444444, 0x88888888, 0x88888888));

  c0c1 = _mm_xor_si128(c0c1, c2c3);

  c0c1 = _mm_xor_si128(c0c1, _mm_srli_si128(c0c1, 8));
  return c0c1;
}

static void gcm_mul64_nohw(uint64_t *out_lo, uint64_t *out_hi, uint64_t a,
                           uint64_t b) {
  uint32_t a0 = a & 0xffffffff;
  uint32_t a1 = a >> 32;
  uint32_t b0 = b & 0xffffffff;
  uint32_t b1 = b >> 32;

  __m128i lo = gcm_mul32_nohw(a0, b0);
  __m128i hi = gcm_mul32_nohw(a1, b1);
  __m128i mid = gcm_mul32_nohw(a0 ^ a1, b0 ^ b1);
  mid = _mm_xor_si128(mid, lo);
  mid = _mm_xor_si128(mid, hi);
  __m128i ret = _mm_unpacklo_epi64(lo, hi);
  mid = _mm_slli_si128(mid, 4);
  mid = _mm_and_si128(mid, _mm_setr_epi32(0, 0xffffffff, 0xffffffff, 0));
  ret = _mm_xor_si128(ret, mid);
  memcpy(out_lo, &ret, 8);
  memcpy(out_hi, ((char*)&ret) + 8, 8);
}

#else

static uint64_t gcm_mul32_nohw(uint32_t a, uint32_t b) {

  uint32_t a0 = a & 0x11111111;
  uint32_t a1 = a & 0x22222222;
  uint32_t a2 = a & 0x44444444;
  uint32_t a3 = a & 0x88888888;

  uint32_t b0 = b & 0x11111111;
  uint32_t b1 = b & 0x22222222;
  uint32_t b2 = b & 0x44444444;
  uint32_t b3 = b & 0x88888888;

  uint64_t c0 = (a0 * (uint64_t)b0) ^ (a1 * (uint64_t)b3) ^
                (a2 * (uint64_t)b2) ^ (a3 * (uint64_t)b1);
  uint64_t c1 = (a0 * (uint64_t)b1) ^ (a1 * (uint64_t)b0) ^
                (a2 * (uint64_t)b3) ^ (a3 * (uint64_t)b2);
  uint64_t c2 = (a0 * (uint64_t)b2) ^ (a1 * (uint64_t)b1) ^
                (a2 * (uint64_t)b0) ^ (a3 * (uint64_t)b3);
  uint64_t c3 = (a0 * (uint64_t)b3) ^ (a1 * (uint64_t)b2) ^
                (a2 * (uint64_t)b1) ^ (a3 * (uint64_t)b0);

  return (c0 & UINT64_C(0x1111111111111111)) |
         (c1 & UINT64_C(0x2222222222222222)) |
         (c2 & UINT64_C(0x4444444444444444)) |
         (c3 & UINT64_C(0x8888888888888888));
}

static void gcm_mul64_nohw(uint64_t *out_lo, uint64_t *out_hi, uint64_t a,
                           uint64_t b) {
  uint32_t a0 = a & 0xffffffff;
  uint32_t a1 = a >> 32;
  uint32_t b0 = b & 0xffffffff;
  uint32_t b1 = b >> 32;

  uint64_t lo = gcm_mul32_nohw(a0, b0);
  uint64_t hi = gcm_mul32_nohw(a1, b1);
  uint64_t mid = gcm_mul32_nohw(a0 ^ a1, b0 ^ b1) ^ lo ^ hi;
  *out_lo = lo ^ (mid << 32);
  *out_hi = hi ^ (mid >> 32);
}

#endif

void gcm_init_nohw(u128 Htable[16], const uint64_t Xi[2]) {

  Htable[0].lo = Xi[1];
  Htable[0].hi = Xi[0];

  uint64_t carry = Htable[0].hi >> 63;
  carry = 0u - carry;

  Htable[0].hi <<= 1;
  Htable[0].hi |= Htable[0].lo >> 63;
  Htable[0].lo <<= 1;

  Htable[0].lo ^= carry & 1;
  Htable[0].hi ^= carry & UINT64_C(0xc200000000000000);

}

static void gcm_polyval_nohw(uint64_t Xi[2], const u128 *H) {

  uint64_t r0, r1;
  gcm_mul64_nohw(&r0, &r1, Xi[0], H->lo);
  uint64_t r2, r3;
  gcm_mul64_nohw(&r2, &r3, Xi[1], H->hi);
  uint64_t mid0, mid1;
  gcm_mul64_nohw(&mid0, &mid1, Xi[0] ^ Xi[1], H->hi ^ H->lo);
  mid0 ^= r0 ^ r2;
  mid1 ^= r1 ^ r3;
  r2 ^= mid1;
  r1 ^= mid0;

  r1 ^= (r0 << 63) ^ (r0 << 62) ^ (r0 << 57);

  r2 ^= r0;
  r3 ^= r1;

  r2 ^= r0 >> 1;
  r2 ^= r1 << 63;
  r3 ^= r1 >> 1;

  r2 ^= r0 >> 2;
  r2 ^= r1 << 62;
  r3 ^= r1 >> 2;

  r2 ^= r0 >> 7;
  r2 ^= r1 << 57;
  r3 ^= r1 >> 7;

  Xi[0] = r2;
  Xi[1] = r3;
}

void gcm_gmult_nohw(uint8_t Xi[16], const u128 Htable[16]) {
  uint64_t swapped[2];
  swapped[0] = CRYPTO_load_u64_be(Xi + 8);
  swapped[1] = CRYPTO_load_u64_be(Xi);
  gcm_polyval_nohw(swapped, &Htable[0]);
  CRYPTO_store_u64_be(Xi, swapped[1]);
  CRYPTO_store_u64_be(Xi + 8, swapped[0]);
}

void gcm_ghash_nohw(uint8_t Xi[16], const u128 Htable[16], const uint8_t *inp,
                    size_t len) {
  uint64_t swapped[2];
  swapped[0] = CRYPTO_load_u64_be(Xi + 8);
  swapped[1] = CRYPTO_load_u64_be(Xi);

  while (len >= 16) {
    swapped[0] ^= CRYPTO_load_u64_be(inp + 8);
    swapped[1] ^= CRYPTO_load_u64_be(inp);
    gcm_polyval_nohw(swapped, &Htable[0]);
    inp += 16;
    len -= 16;
  }

  CRYPTO_store_u64_be(Xi, swapped[1]);
  CRYPTO_store_u64_be(Xi + 8, swapped[0]);
}
