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

#include <openssl/hrss.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>

#include <openssl/bn.h>
#include <openssl/hmac.h>
#include <openssl/mem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "../internal.h"
#include "internal.h"

#if defined(OPENSSL_SSE2)
#include <emmintrin.h>
#endif

#if (defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)) && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

#if defined(OPENSSL_SSE2) && (defined(__clang__) || !defined(_MSC_VER))

#define HRSS_HAVE_VECTOR_UNIT
typedef __m128i vec_t;

static int vec_capable(void) { return 1; }

static inline vec_t vec_add(vec_t a, vec_t b) { return _mm_add_epi16(a, b); }

static inline vec_t vec_sub(vec_t a, vec_t b) { return _mm_sub_epi16(a, b); }

static inline vec_t vec_mul(vec_t a, uint16_t b) {
  return _mm_mullo_epi16(a, _mm_set1_epi16(b));
}

static inline vec_t vec_fma(vec_t a, vec_t b, uint16_t c) {
  return _mm_add_epi16(a, _mm_mullo_epi16(b, _mm_set1_epi16(c)));
}

static inline void vec3_rshift_word(vec_t v[3]) {

  const __m128i carry0 = _mm_srli_si128(v[0], 14);
  v[0] = _mm_slli_si128(v[0], 2);

  const __m128i carry1 = _mm_srli_si128(v[1], 14);
  v[1] = _mm_slli_si128(v[1], 2);
  v[1] |= carry0;

  v[2] = _mm_slli_si128(v[2], 2);
  v[2] |= carry1;
}

static inline void vec4_rshift_word(vec_t v[4]) {

  const __m128i carry0 = _mm_srli_si128(v[0], 14);
  v[0] = _mm_slli_si128(v[0], 2);

  const __m128i carry1 = _mm_srli_si128(v[1], 14);
  v[1] = _mm_slli_si128(v[1], 2);
  v[1] |= carry0;

  const __m128i carry2 = _mm_srli_si128(v[2], 14);
  v[2] = _mm_slli_si128(v[2], 2);
  v[2] |= carry1;

  v[3] = _mm_slli_si128(v[3], 2);
  v[3] |= carry2;
}

static inline vec_t vec_merge_3_5(vec_t left, vec_t right) {
  return _mm_srli_si128(left, 10) | _mm_slli_si128(right, 6);
}

static inline void poly3_vec_lshift1(vec_t a_s[6], vec_t a_a[6]) {
  vec_t carry_s = {0};
  vec_t carry_a = {0};

  for (int i = 0; i < 6; i++) {
    vec_t next_carry_s = _mm_srli_epi64(a_s[i], 63);
    a_s[i] = _mm_slli_epi64(a_s[i], 1);
    a_s[i] |= _mm_slli_si128(next_carry_s, 8);
    a_s[i] |= carry_s;
    carry_s = _mm_srli_si128(next_carry_s, 8);

    vec_t next_carry_a = _mm_srli_epi64(a_a[i], 63);
    a_a[i] = _mm_slli_epi64(a_a[i], 1);
    a_a[i] |= _mm_slli_si128(next_carry_a, 8);
    a_a[i] |= carry_a;
    carry_a = _mm_srli_si128(next_carry_a, 8);
  }
}

static inline void poly3_vec_rshift1(vec_t a_s[6], vec_t a_a[6]) {
  vec_t carry_s = {0};
  vec_t carry_a = {0};

  for (int i = 5; i >= 0; i--) {
    const vec_t next_carry_s = _mm_slli_epi64(a_s[i], 63);
    a_s[i] = _mm_srli_epi64(a_s[i], 1);
    a_s[i] |= _mm_srli_si128(next_carry_s, 8);
    a_s[i] |= carry_s;
    carry_s = _mm_slli_si128(next_carry_s, 8);

    const vec_t next_carry_a = _mm_slli_epi64(a_a[i], 63);
    a_a[i] = _mm_srli_epi64(a_a[i], 1);
    a_a[i] |= _mm_srli_si128(next_carry_a, 8);
    a_a[i] |= carry_a;
    carry_a = _mm_slli_si128(next_carry_a, 8);
  }
}

static inline vec_t vec_broadcast_bit(vec_t a) {
  return _mm_shuffle_epi32(_mm_srai_epi32(_mm_slli_epi64(a, 63), 31),
                           0b01010101);
}

#define vec_get_word(v, i) _mm_extract_epi16(v, i)

#elif (defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)) && defined(__ARM_NEON)

#define HRSS_HAVE_VECTOR_UNIT
typedef uint16x8_t vec_t;

static int vec_capable(void) { return CRYPTO_is_NEON_capable(); }

static inline vec_t vec_add(vec_t a, vec_t b) { return a + b; }

static inline vec_t vec_sub(vec_t a, vec_t b) { return a - b; }

static inline vec_t vec_mul(vec_t a, uint16_t b) { return vmulq_n_u16(a, b); }

static inline vec_t vec_fma(vec_t a, vec_t b, uint16_t c) {
  return vmlaq_n_u16(a, b, c);
}

static inline void vec3_rshift_word(vec_t v[3]) {
  const uint16x8_t kZero = {0};
  v[2] = vextq_u16(v[1], v[2], 7);
  v[1] = vextq_u16(v[0], v[1], 7);
  v[0] = vextq_u16(kZero, v[0], 7);
}

static inline void vec4_rshift_word(vec_t v[4]) {
  const uint16x8_t kZero = {0};
  v[3] = vextq_u16(v[2], v[3], 7);
  v[2] = vextq_u16(v[1], v[2], 7);
  v[1] = vextq_u16(v[0], v[1], 7);
  v[0] = vextq_u16(kZero, v[0], 7);
}

static inline vec_t vec_merge_3_5(vec_t left, vec_t right) {
  return vextq_u16(left, right, 5);
}

static inline uint16_t vec_get_word(vec_t v, unsigned i) { return v[i]; }

#if !defined(OPENSSL_AARCH64)

static inline vec_t vec_broadcast_bit(vec_t a) {
  a = (vec_t)vshrq_n_s16(((int16x8_t)a) << 15, 15);
  return vdupq_lane_u16(vget_low_u16(a), 0);
}

static inline void poly3_vec_lshift1(vec_t a_s[6], vec_t a_a[6]) {
  vec_t carry_s = {0};
  vec_t carry_a = {0};
  const vec_t kZero = {0};

  for (int i = 0; i < 6; i++) {
    vec_t next_carry_s = a_s[i] >> 15;
    a_s[i] <<= 1;
    a_s[i] |= vextq_u16(kZero, next_carry_s, 7);
    a_s[i] |= carry_s;
    carry_s = vextq_u16(next_carry_s, kZero, 7);

    vec_t next_carry_a = a_a[i] >> 15;
    a_a[i] <<= 1;
    a_a[i] |= vextq_u16(kZero, next_carry_a, 7);
    a_a[i] |= carry_a;
    carry_a = vextq_u16(next_carry_a, kZero, 7);
  }
}

static inline void poly3_vec_rshift1(vec_t a_s[6], vec_t a_a[6]) {
  vec_t carry_s = {0};
  vec_t carry_a = {0};
  const vec_t kZero = {0};

  for (int i = 5; i >= 0; i--) {
    vec_t next_carry_s = a_s[i] << 15;
    a_s[i] >>= 1;
    a_s[i] |= vextq_u16(next_carry_s, kZero, 1);
    a_s[i] |= carry_s;
    carry_s = vextq_u16(kZero, next_carry_s, 1);

    vec_t next_carry_a = a_a[i] << 15;
    a_a[i] >>= 1;
    a_a[i] |= vextq_u16(next_carry_a, kZero, 1);
    a_a[i] |= carry_a;
    carry_a = vextq_u16(kZero, next_carry_a, 1);
  }
}

#endif

#endif

static void poly2_zero(struct poly2 *p) {
  OPENSSL_memset(&p->v[0], 0, sizeof(crypto_word_t) * WORDS_PER_POLY);
}

static crypto_word_t word_reverse(crypto_word_t in) {
#if defined(OPENSSL_64_BIT)
  static const crypto_word_t kMasks[6] = {
      UINT64_C(0x5555555555555555), UINT64_C(0x3333333333333333),
      UINT64_C(0x0f0f0f0f0f0f0f0f), UINT64_C(0x00ff00ff00ff00ff),
      UINT64_C(0x0000ffff0000ffff), UINT64_C(0x00000000ffffffff),
  };
#else
  static const crypto_word_t kMasks[5] = {
      0x55555555, 0x33333333, 0x0f0f0f0f, 0x00ff00ff, 0x0000ffff,
  };
#endif

  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(kMasks); i++) {
    in = ((in >> (1 << i)) & kMasks[i]) | ((in & kMasks[i]) << (1 << i));
  }

  return in;
}

static crypto_word_t lsb_to_all(crypto_word_t v) { return 0u - (v & 1); }

static void poly2_mod_phiN(struct poly2 *p) {

  const crypto_word_t m =
      lsb_to_all(p->v[WORDS_PER_POLY - 1] >> (BITS_IN_LAST_WORD - 1));
  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    p->v[i] ^= m;
  }
  p->v[WORDS_PER_POLY - 1] &= (UINT64_C(1) << (BITS_IN_LAST_WORD - 1)) - 1;
}

static void poly2_reverse_700(struct poly2 *out, const struct poly2 *in) {
  struct poly2 t;
  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    t.v[i] = word_reverse(in->v[i]);
  }

  static const size_t shift = BITS_PER_WORD - ((N - 1) % BITS_PER_WORD);
  for (size_t i = 0; i < WORDS_PER_POLY - 1; i++) {
    out->v[i] = t.v[WORDS_PER_POLY - 1 - i] >> shift;
    out->v[i] |= t.v[WORDS_PER_POLY - 2 - i] << (BITS_PER_WORD - shift);
  }
  out->v[WORDS_PER_POLY - 1] = t.v[0] >> shift;
}

static void poly2_cswap(struct poly2 *a, struct poly2 *b, crypto_word_t swap) {
  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    const crypto_word_t sum = swap & (a->v[i] ^ b->v[i]);
    a->v[i] ^= sum;
    b->v[i] ^= sum;
  }
}

static void poly2_fmadd(struct poly2 *out, const struct poly2 *in,
                        crypto_word_t m) {
  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    out->v[i] ^= in->v[i] & m;
  }
}

static void poly2_lshift1(struct poly2 *p) {
  crypto_word_t carry = 0;
  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    const crypto_word_t next_carry = p->v[i] >> (BITS_PER_WORD - 1);
    p->v[i] <<= 1;
    p->v[i] |= carry;
    carry = next_carry;
  }
}

static void poly2_rshift1(struct poly2 *p) {
  crypto_word_t carry = 0;
  for (size_t i = WORDS_PER_POLY - 1; i < WORDS_PER_POLY; i--) {
    const crypto_word_t next_carry = p->v[i] & 1;
    p->v[i] >>= 1;
    p->v[i] |= carry << (BITS_PER_WORD - 1);
    carry = next_carry;
  }
}

static void poly2_clear_top_bits(struct poly2 *p) {
  p->v[WORDS_PER_POLY - 1] &= (UINT64_C(1) << BITS_IN_LAST_WORD) - 1;
}

static void poly3_zero(struct poly3 *p) {
  poly2_zero(&p->s);
  poly2_zero(&p->a);
}

static void poly3_reverse_700(struct poly3 *out, const struct poly3 *in) {
  poly2_reverse_700(&out->a, &in->a);
  poly2_reverse_700(&out->s, &in->s);
}

static void poly3_word_mul(crypto_word_t *out_s, crypto_word_t *out_a,
                           const crypto_word_t s1, const crypto_word_t a1,
                           const crypto_word_t s2, const crypto_word_t a2) {
  *out_a = a1 & a2;
  *out_s = (s1 ^ s2) & *out_a;
}

static void poly3_word_add(crypto_word_t *out_s, crypto_word_t *out_a,
                           const crypto_word_t s1, const crypto_word_t a1,
                           const crypto_word_t s2, const crypto_word_t a2) {
  const crypto_word_t t = s1 ^ a2;
  *out_s = t & (s2 ^ a1);
  *out_a = (a1 ^ a2) | (t ^ s2);
}

static void poly3_word_sub(crypto_word_t *out_s, crypto_word_t *out_a,
                           const crypto_word_t s1, const crypto_word_t a1,
                           const crypto_word_t s2, const crypto_word_t a2) {
  const crypto_word_t t = a1 ^ a2;
  *out_s = (s1 ^ a2) & (t ^ s2);
  *out_a = t | (s1 ^ s2);
}

static void poly3_mul_const(struct poly3 *p, crypto_word_t ms,
                            crypto_word_t ma) {
  ms = lsb_to_all(ms);
  ma = lsb_to_all(ma);

  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    poly3_word_mul(&p->s.v[i], &p->a.v[i], p->s.v[i], p->a.v[i], ms, ma);
  }
}

static void poly3_fmsub(struct poly3 *out, const struct poly3 *in,
                        crypto_word_t ms, crypto_word_t ma) {
  crypto_word_t product_s, product_a;
  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    poly3_word_mul(&product_s, &product_a, in->s.v[i], in->a.v[i], ms, ma);
    poly3_word_sub(&out->s.v[i], &out->a.v[i], out->s.v[i], out->a.v[i],
                   product_s, product_a);
  }
}

static crypto_word_t final_bit_to_all(crypto_word_t v) {
  return lsb_to_all(v >> (BITS_IN_LAST_WORD - 1));
}

static void poly3_mod_phiN(struct poly3 *p) {

  const crypto_word_t factor_s = final_bit_to_all(p->s.v[WORDS_PER_POLY - 1]);
  const crypto_word_t factor_a = final_bit_to_all(p->a.v[WORDS_PER_POLY - 1]);

  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    poly3_word_sub(&p->s.v[i], &p->a.v[i], p->s.v[i], p->a.v[i], factor_s,
                   factor_a);
  }

  poly2_clear_top_bits(&p->s);
  poly2_clear_top_bits(&p->a);
}

static void poly3_cswap(struct poly3 *a, struct poly3 *b, crypto_word_t swap) {
  poly2_cswap(&a->s, &b->s, swap);
  poly2_cswap(&a->a, &b->a, swap);
}

static void poly3_lshift1(struct poly3 *p) {
  poly2_lshift1(&p->s);
  poly2_lshift1(&p->a);
}

static void poly3_rshift1(struct poly3 *p) {
  poly2_rshift1(&p->s);
  poly2_rshift1(&p->a);
}

struct poly3_span {
  crypto_word_t *s;
  crypto_word_t *a;
};

static void poly3_span_add(const struct poly3_span *out,
                           const struct poly3_span *a,
                           const struct poly3_span *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    poly3_word_add(&out->s[i], &out->a[i], a->s[i], a->a[i], b->s[i], b->a[i]);
  }
}

static void poly3_span_sub(const struct poly3_span *a,
                           const struct poly3_span *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    poly3_word_sub(&a->s[i], &a->a[i], a->s[i], a->a[i], b->s[i], b->a[i]);
  }
}

static void poly3_mul_aux(const struct poly3_span *out,
                          const struct poly3_span *scratch,
                          const struct poly3_span *a,
                          const struct poly3_span *b, size_t n) {
  if (n == 1) {
    crypto_word_t r_s_low = 0, r_s_high = 0, r_a_low = 0, r_a_high = 0;
    crypto_word_t b_s = b->s[0], b_a = b->a[0];
    const crypto_word_t a_s = a->s[0], a_a = a->a[0];

    for (size_t i = 0; i < BITS_PER_WORD; i++) {

      crypto_word_t m_s, m_a;
      poly3_word_mul(&m_s, &m_a, a_s, a_a, lsb_to_all(b_s), lsb_to_all(b_a));
      b_s >>= 1;
      b_a >>= 1;

      if (i == 0) {

        r_s_low = m_s;
        r_a_low = m_a;
        continue;
      }

      const crypto_word_t m_s_low = m_s << i;
      const crypto_word_t m_s_high = m_s >> (BITS_PER_WORD - i);
      const crypto_word_t m_a_low = m_a << i;
      const crypto_word_t m_a_high = m_a >> (BITS_PER_WORD - i);

      poly3_word_add(&r_s_low, &r_a_low, r_s_low, r_a_low, m_s_low, m_a_low);
      poly3_word_add(&r_s_high, &r_a_high, r_s_high, r_a_high, m_s_high,
                     m_a_high);
    }

    out->s[0] = r_s_low;
    out->s[1] = r_s_high;
    out->a[0] = r_a_low;
    out->a[1] = r_a_high;
    return;
  }

  const size_t low_len = n / 2;
  const size_t high_len = n - low_len;
  const struct poly3_span a_high = {&a->s[low_len], &a->a[low_len]};
  const struct poly3_span b_high = {&b->s[low_len], &b->a[low_len]};

  const struct poly3_span a_cross_sum = *out;
  const struct poly3_span b_cross_sum = {&out->s[high_len], &out->a[high_len]};
  poly3_span_add(&a_cross_sum, a, &a_high, low_len);
  poly3_span_add(&b_cross_sum, b, &b_high, low_len);
  if (high_len != low_len) {
    a_cross_sum.s[low_len] = a_high.s[low_len];
    a_cross_sum.a[low_len] = a_high.a[low_len];
    b_cross_sum.s[low_len] = b_high.s[low_len];
    b_cross_sum.a[low_len] = b_high.a[low_len];
  }

  const struct poly3_span child_scratch = {&scratch->s[2 * high_len],
                                           &scratch->a[2 * high_len]};
  const struct poly3_span out_mid = {&out->s[low_len], &out->a[low_len]};
  const struct poly3_span out_high = {&out->s[2 * low_len],
                                      &out->a[2 * low_len]};

  poly3_mul_aux(scratch, &child_scratch, &a_cross_sum, &b_cross_sum, high_len);

  poly3_mul_aux(&out_high, &child_scratch, &a_high, &b_high, high_len);

  poly3_mul_aux(out, &child_scratch, a, b, low_len);

  poly3_span_sub(scratch, out, low_len * 2);
  poly3_span_sub(scratch, &out_high, high_len * 2);

  poly3_span_add(&out_mid, &out_mid, scratch, high_len * 2);
}

void HRSS_poly3_mul(struct poly3 *out, const struct poly3 *x,
                    const struct poly3 *y) {
  crypto_word_t prod_s[WORDS_PER_POLY * 2];
  crypto_word_t prod_a[WORDS_PER_POLY * 2];
  crypto_word_t scratch_s[WORDS_PER_POLY * 2 + 2];
  crypto_word_t scratch_a[WORDS_PER_POLY * 2 + 2];
  const struct poly3_span prod_span = {prod_s, prod_a};
  const struct poly3_span scratch_span = {scratch_s, scratch_a};
  const struct poly3_span x_span = {(crypto_word_t *)x->s.v,
                                    (crypto_word_t *)x->a.v};
  const struct poly3_span y_span = {(crypto_word_t *)y->s.v,
                                    (crypto_word_t *)y->a.v};

  poly3_mul_aux(&prod_span, &scratch_span, &x_span, &y_span, WORDS_PER_POLY);

  for (size_t i = 0; i < WORDS_PER_POLY; i++) {
    crypto_word_t v_s = prod_s[WORDS_PER_POLY + i - 1] >> BITS_IN_LAST_WORD;
    v_s |= prod_s[WORDS_PER_POLY + i] << (BITS_PER_WORD - BITS_IN_LAST_WORD);
    crypto_word_t v_a = prod_a[WORDS_PER_POLY + i - 1] >> BITS_IN_LAST_WORD;
    v_a |= prod_a[WORDS_PER_POLY + i] << (BITS_PER_WORD - BITS_IN_LAST_WORD);

    poly3_word_add(&out->s.v[i], &out->a.v[i], prod_s[i], prod_a[i], v_s, v_a);
  }

  poly3_mod_phiN(out);
}

#if defined(HRSS_HAVE_VECTOR_UNIT) && !defined(OPENSSL_AARCH64)

static inline void poly3_vec_cswap(vec_t a_s[6], vec_t a_a[6], vec_t b_s[6],
                                   vec_t b_a[6], const vec_t swap) {
  for (int i = 0; i < 6; i++) {
    const vec_t sum_s = swap & (a_s[i] ^ b_s[i]);
    a_s[i] ^= sum_s;
    b_s[i] ^= sum_s;

    const vec_t sum_a = swap & (a_a[i] ^ b_a[i]);
    a_a[i] ^= sum_a;
    b_a[i] ^= sum_a;
  }
}

static inline void poly3_vec_fmsub(vec_t a_s[6], vec_t a_a[6], vec_t b_s[6],
                                   vec_t b_a[6], const vec_t ms,
                                   const vec_t ma) {
  for (int i = 0; i < 6; i++) {

    const vec_t s = b_s[i];
    const vec_t a = b_a[i];
    const vec_t product_a = a & ma;
    const vec_t product_s = (s ^ ms) & product_a;

    const vec_t out_s = a_s[i];
    const vec_t out_a = a_a[i];
    const vec_t t = out_a ^ product_a;
    a_s[i] = (out_s ^ product_a) & (t ^ product_s);
    a_a[i] = t | (out_s ^ product_s);
  }
}

static void poly3_invert_vec(struct poly3 *out, const struct poly3 *in) {

  const vec_t kZero = {0};
  const vec_t kOne = {1};
  static const uint8_t kBottomSixtyOne[sizeof(vec_t)] = {
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1f};

  vec_t v_s[6], v_a[6], r_s[6], r_a[6], f_s[6], f_a[6], g_s[6], g_a[6];

  memset(&v_s, 0, sizeof(v_s));
  memset(&v_a, 0, sizeof(v_a));

  memset(&r_s, 0, sizeof(r_s));
  memset(&r_a, 0, sizeof(r_a));
  r_a[0] = kOne;

  memset(f_s, 0, sizeof(f_s));
  memset(f_a, 0xff, 5 * sizeof(vec_t));
  memcpy(&f_a[5], kBottomSixtyOne, sizeof(kBottomSixtyOne));

  struct poly3 in_reversed;
  poly3_reverse_700(&in_reversed, in);
  g_s[5] = kZero;
  memcpy(&g_s, &in_reversed.s.v, WORDS_PER_POLY * sizeof(crypto_word_t));
  g_a[5] = kZero;
  memcpy(&g_a, &in_reversed.a.v, WORDS_PER_POLY * sizeof(crypto_word_t));

  int delta = 1;

  for (size_t i = 0; i < (2 * (N - 1)) - 1; i++) {
    poly3_vec_lshift1(v_s, v_a);

    const crypto_word_t delta_sign_bit = (delta >> (sizeof(delta) * 8 - 1)) & 1;
    const crypto_word_t delta_is_non_negative = delta_sign_bit - 1;
    const crypto_word_t delta_is_non_zero = ~constant_time_is_zero_w(delta);
    const vec_t g_has_constant_term = vec_broadcast_bit(g_a[0]);
    const vec_t mask_w = {
        static_cast<std::remove_reference<decltype(mask_w[0])>::type>(
            delta_is_non_negative & delta_is_non_zero)};
    const vec_t mask = vec_broadcast_bit(mask_w) & g_has_constant_term;

    const vec_t c_a = vec_broadcast_bit(f_a[0] & g_a[0]);
    const vec_t c_s = vec_broadcast_bit((f_s[0] ^ g_s[0]) & c_a);

    delta = constant_time_select_int(lsb_to_all(mask[0]), -delta, delta);
    delta++;

    poly3_vec_cswap(f_s, f_a, g_s, g_a, mask);
    poly3_vec_fmsub(g_s, g_a, f_s, f_a, c_s, c_a);
    poly3_vec_rshift1(g_s, g_a);

    poly3_vec_cswap(v_s, v_a, r_s, r_a, mask);
    poly3_vec_fmsub(r_s, r_a, v_s, v_a, c_s, c_a);
  }

  assert(delta == 0);
  memcpy(out->s.v, v_s, WORDS_PER_POLY * sizeof(crypto_word_t));
  memcpy(out->a.v, v_a, WORDS_PER_POLY * sizeof(crypto_word_t));
  poly3_mul_const(out, vec_get_word(f_s[0], 0), vec_get_word(f_a[0], 0));
  poly3_reverse_700(out, out);
}

#endif

void HRSS_poly3_invert(struct poly3 *out, const struct poly3 *in) {

#if defined(HRSS_HAVE_VECTOR_UNIT) && !defined(OPENSSL_AARCH64)
  if (vec_capable()) {
    poly3_invert_vec(out, in);
    return;
  }
#endif

  struct poly3 v, r, f, g;

  poly3_zero(&v);

  poly3_zero(&r);
  r.a.v[0] = 1;

  OPENSSL_memset(&f.s, 0, sizeof(struct poly2));
  OPENSSL_memset(&f.a, 0xff, sizeof(struct poly2));
  f.a.v[WORDS_PER_POLY - 1] >>= BITS_PER_WORD - BITS_IN_LAST_WORD;

  poly3_reverse_700(&g, in);
  int delta = 1;

  for (size_t i = 0; i < (2 * (N - 1)) - 1; i++) {
    poly3_lshift1(&v);

    const crypto_word_t delta_sign_bit = (delta >> (sizeof(delta) * 8 - 1)) & 1;
    const crypto_word_t delta_is_non_negative = delta_sign_bit - 1;
    const crypto_word_t delta_is_non_zero = ~constant_time_is_zero_w(delta);
    const crypto_word_t g_has_constant_term = lsb_to_all(g.a.v[0]);
    const crypto_word_t mask =
        g_has_constant_term & delta_is_non_negative & delta_is_non_zero;

    crypto_word_t c_s, c_a;
    poly3_word_mul(&c_s, &c_a, f.s.v[0], f.a.v[0], g.s.v[0], g.a.v[0]);
    c_s = lsb_to_all(c_s);
    c_a = lsb_to_all(c_a);

    delta = constant_time_select_int(mask, -delta, delta);
    delta++;

    poly3_cswap(&f, &g, mask);
    poly3_fmsub(&g, &f, c_s, c_a);
    poly3_rshift1(&g);

    poly3_cswap(&v, &r, mask);
    poly3_fmsub(&r, &v, c_s, c_a);
  }

  assert(delta == 0);
  poly3_mul_const(&v, f.s.v[0], f.a.v[0]);
  poly3_reverse_700(out, &v);
}

#define Q 8192

#define COEFFICIENTS_PER_VEC (sizeof(vec_t) / sizeof(uint16_t))
#define VECS_PER_POLY ((N + COEFFICIENTS_PER_VEC - 1) / COEFFICIENTS_PER_VEC)

namespace {

struct poly {
#if defined(HRSS_HAVE_VECTOR_UNIT)
  union {

    uint16_t v[N + 3];
    vec_t vectors[VECS_PER_POLY];
  };
#else

  alignas(16) uint16_t v[N + 3];
#endif
};
}

static void poly_normalize(struct poly *x) {
  OPENSSL_memset(&x->v[N], 0, 3 * sizeof(uint16_t));
}

static void poly_assert_normalized(const struct poly *x) {
  assert(x->v[N] == 0);
  assert(x->v[N + 1] == 0);
  assert(x->v[N + 2] == 0);
}

namespace {

struct POLY_MUL_SCRATCH {
  union {

    struct {
      uint16_t prod[2 * N];
      uint16_t scratch[1318];
    } novec;

#if defined(HRSS_HAVE_VECTOR_UNIT)

    struct {
      vec_t prod[VECS_PER_POLY * 2];
      vec_t scratch[172];
    } vec;
#endif

#if defined(POLY_RQ_MUL_ASM)

    uint8_t rq[POLY_MUL_RQ_SCRATCH_SPACE];
#endif
  } u;
};
}

#if defined(HRSS_HAVE_VECTOR_UNIT)

static void poly_mul_vec_aux(vec_t *out, vec_t *scratch, const vec_t *a,
                             const vec_t *b, const size_t n) {

  if (n == 2) {
    vec_t result[4];
    vec_t vec_a[3];
    static const vec_t kZero = {0};
    vec_a[0] = a[0];
    vec_a[1] = a[1];
    vec_a[2] = kZero;

    result[0] = vec_mul(vec_a[0], vec_get_word(b[0], 0));
    result[1] = vec_mul(vec_a[1], vec_get_word(b[0], 0));

    result[1] = vec_fma(result[1], vec_a[0], vec_get_word(b[1], 0));
    result[2] = vec_mul(vec_a[1], vec_get_word(b[1], 0));
    result[3] = kZero;

    vec3_rshift_word(vec_a);

#define BLOCK(x, y)                                                      \
  do {                                                                   \
    result[x + 0] =                                                      \
        vec_fma(result[x + 0], vec_a[0], vec_get_word(b[y / 8], y % 8)); \
    result[x + 1] =                                                      \
        vec_fma(result[x + 1], vec_a[1], vec_get_word(b[y / 8], y % 8)); \
    result[x + 2] =                                                      \
        vec_fma(result[x + 2], vec_a[2], vec_get_word(b[y / 8], y % 8)); \
  } while (0)

    BLOCK(0, 1);
    BLOCK(1, 9);

    vec3_rshift_word(vec_a);

    BLOCK(0, 2);
    BLOCK(1, 10);

    vec3_rshift_word(vec_a);

    BLOCK(0, 3);
    BLOCK(1, 11);

    vec3_rshift_word(vec_a);

    BLOCK(0, 4);
    BLOCK(1, 12);

    vec3_rshift_word(vec_a);

    BLOCK(0, 5);
    BLOCK(1, 13);

    vec3_rshift_word(vec_a);

    BLOCK(0, 6);
    BLOCK(1, 14);

    vec3_rshift_word(vec_a);

    BLOCK(0, 7);
    BLOCK(1, 15);

#undef BLOCK

    memcpy(out, result, sizeof(result));
    return;
  }

  if (n == 3) {
    vec_t result[6];
    vec_t vec_a[4];
    static const vec_t kZero = {0};
    vec_a[0] = a[0];
    vec_a[1] = a[1];
    vec_a[2] = a[2];
    vec_a[3] = kZero;

    result[0] = vec_mul(a[0], vec_get_word(b[0], 0));
    result[1] = vec_mul(a[1], vec_get_word(b[0], 0));
    result[2] = vec_mul(a[2], vec_get_word(b[0], 0));

#define BLOCK_PRE(x, y)                                                  \
  do {                                                                   \
    result[x + 0] =                                                      \
        vec_fma(result[x + 0], vec_a[0], vec_get_word(b[y / 8], y % 8)); \
    result[x + 1] =                                                      \
        vec_fma(result[x + 1], vec_a[1], vec_get_word(b[y / 8], y % 8)); \
    result[x + 2] = vec_mul(vec_a[2], vec_get_word(b[y / 8], y % 8));    \
  } while (0)

    BLOCK_PRE(1, 8);
    BLOCK_PRE(2, 16);

    result[5] = kZero;

    vec4_rshift_word(vec_a);

#define BLOCK(x, y)                                                      \
  do {                                                                   \
    result[x + 0] =                                                      \
        vec_fma(result[x + 0], vec_a[0], vec_get_word(b[y / 8], y % 8)); \
    result[x + 1] =                                                      \
        vec_fma(result[x + 1], vec_a[1], vec_get_word(b[y / 8], y % 8)); \
    result[x + 2] =                                                      \
        vec_fma(result[x + 2], vec_a[2], vec_get_word(b[y / 8], y % 8)); \
    result[x + 3] =                                                      \
        vec_fma(result[x + 3], vec_a[3], vec_get_word(b[y / 8], y % 8)); \
  } while (0)

    BLOCK(0, 1);
    BLOCK(1, 9);
    BLOCK(2, 17);

    vec4_rshift_word(vec_a);

    BLOCK(0, 2);
    BLOCK(1, 10);
    BLOCK(2, 18);

    vec4_rshift_word(vec_a);

    BLOCK(0, 3);
    BLOCK(1, 11);
    BLOCK(2, 19);

    vec4_rshift_word(vec_a);

    BLOCK(0, 4);
    BLOCK(1, 12);
    BLOCK(2, 20);

    vec4_rshift_word(vec_a);

    BLOCK(0, 5);
    BLOCK(1, 13);
    BLOCK(2, 21);

    vec4_rshift_word(vec_a);

    BLOCK(0, 6);
    BLOCK(1, 14);
    BLOCK(2, 22);

    vec4_rshift_word(vec_a);

    BLOCK(0, 7);
    BLOCK(1, 15);
    BLOCK(2, 23);

#undef BLOCK
#undef BLOCK_PRE

    memcpy(out, result, sizeof(result));

    return;
  }

  const size_t low_len = n / 2;
  const size_t high_len = n - low_len;
  const vec_t *a_high = &a[low_len];
  const vec_t *b_high = &b[low_len];

  for (size_t i = 0; i < low_len; i++) {
    out[i] = vec_add(a_high[i], a[i]);
    out[high_len + i] = vec_add(b_high[i], b[i]);
  }
  if (high_len != low_len) {
    out[low_len] = a_high[low_len];
    out[high_len + low_len] = b_high[low_len];
  }

  vec_t *const child_scratch = &scratch[2 * high_len];

  poly_mul_vec_aux(scratch, child_scratch, out, &out[high_len], high_len);

  poly_mul_vec_aux(&out[low_len * 2], child_scratch, a_high, b_high, high_len);

  poly_mul_vec_aux(out, child_scratch, a, b, low_len);

  for (size_t i = 0; i < low_len * 2; i++) {
    scratch[i] = vec_sub(scratch[i], vec_add(out[i], out[low_len * 2 + i]));
  }
  if (low_len != high_len) {
    scratch[low_len * 2] = vec_sub(scratch[low_len * 2], out[low_len * 4]);
    scratch[low_len * 2 + 1] =
        vec_sub(scratch[low_len * 2 + 1], out[low_len * 4 + 1]);
  }

  for (size_t i = 0; i < high_len * 2; i++) {
    out[low_len + i] = vec_add(out[low_len + i], scratch[i]);
  }
}

static void poly_mul_vec(struct POLY_MUL_SCRATCH *scratch, struct poly *out,
                         const struct poly *x, const struct poly *y) {
  static_assert(sizeof(out->v) == sizeof(vec_t) * VECS_PER_POLY,
                "struct poly is the wrong size");
  static_assert(alignof(struct poly) == alignof(vec_t),
                "struct poly has incorrect alignment");
  poly_assert_normalized(x);
  poly_assert_normalized(y);

  vec_t *const prod = scratch->u.vec.prod;
  vec_t *const aux_scratch = scratch->u.vec.scratch;
  poly_mul_vec_aux(prod, aux_scratch, x->vectors, y->vectors, VECS_PER_POLY);

  vec_t *out_vecs = (vec_t *)out->v;

  for (size_t i = 0; i < VECS_PER_POLY; i++) {
    const vec_t prev = prod[VECS_PER_POLY - 1 + i];
    const vec_t this_vec = prod[VECS_PER_POLY + i];
    out_vecs[i] = vec_add(prod[i], vec_merge_3_5(prev, this_vec));
  }

  OPENSSL_memset(&out->v[N], 0, 3 * sizeof(uint16_t));
}

#endif

static void poly_mul_novec_aux(uint16_t *out, uint16_t *scratch,
                               const uint16_t *a, const uint16_t *b, size_t n) {
  static const size_t kSchoolbookLimit = 64;
  if (n < kSchoolbookLimit) {
    OPENSSL_memset(out, 0, sizeof(uint16_t) * n * 2);
    for (size_t i = 0; i < n; i++) {
      for (size_t j = 0; j < n; j++) {
        out[i + j] += (unsigned)a[i] * b[j];
      }
    }

    return;
  }

  const size_t low_len = n / 2;
  const size_t high_len = n - low_len;
  const uint16_t *const a_high = &a[low_len];
  const uint16_t *const b_high = &b[low_len];

  for (size_t i = 0; i < low_len; i++) {
    out[i] = a_high[i] + a[i];
    out[high_len + i] = b_high[i] + b[i];
  }
  if (high_len != low_len) {
    out[low_len] = a_high[low_len];
    out[high_len + low_len] = b_high[low_len];
  }

  uint16_t *const child_scratch = &scratch[2 * high_len];
  poly_mul_novec_aux(scratch, child_scratch, out, &out[high_len], high_len);
  poly_mul_novec_aux(&out[low_len * 2], child_scratch, a_high, b_high,
                     high_len);
  poly_mul_novec_aux(out, child_scratch, a, b, low_len);

  for (size_t i = 0; i < low_len * 2; i++) {
    scratch[i] -= out[i] + out[low_len * 2 + i];
  }
  if (low_len != high_len) {
    scratch[low_len * 2] -= out[low_len * 4];
    assert(out[low_len * 4 + 1] == 0);
  }

  for (size_t i = 0; i < high_len * 2; i++) {
    out[low_len + i] += scratch[i];
  }
}

static void poly_mul_novec(struct POLY_MUL_SCRATCH *scratch, struct poly *out,
                           const struct poly *x, const struct poly *y) {
  uint16_t *const prod = scratch->u.novec.prod;
  uint16_t *const aux_scratch = scratch->u.novec.scratch;
  poly_mul_novec_aux(prod, aux_scratch, x->v, y->v, N);

  for (size_t i = 0; i < N; i++) {
    out->v[i] = prod[i] + prod[i + N];
  }
  OPENSSL_memset(&out->v[N], 0, 3 * sizeof(uint16_t));
}

static void poly_mul(struct POLY_MUL_SCRATCH *scratch, struct poly *r,
                     const struct poly *a, const struct poly *b) {
#if defined(POLY_RQ_MUL_ASM)
  if (CRYPTO_is_AVX2_capable()) {
    poly_Rq_mul(r->v, a->v, b->v, scratch->u.rq);
    poly_normalize(r);
  } else
#endif

#if defined(HRSS_HAVE_VECTOR_UNIT)
      if (vec_capable()) {
    poly_mul_vec(scratch, r, a, b);
  } else
#endif

  {
    poly_mul_novec(scratch, r, a, b);
  }

  poly_assert_normalized(r);
}

static void poly_mul_x_minus_1(struct poly *p) {

  const uint16_t orig_final_coefficient = p->v[N - 1];

  for (size_t i = N - 1; i > 0; i--) {
    p->v[i] = p->v[i - 1] - p->v[i];
  }
  p->v[0] = orig_final_coefficient - p->v[0];
}

static void poly_mod_phiN(struct poly *p) {
  const uint16_t coeff700 = p->v[N - 1];

  for (unsigned i = 0; i < N; i++) {
    p->v[i] -= coeff700;
  }
}

static void poly_clamp(struct poly *p) {
  for (unsigned i = 0; i < N; i++) {
    p->v[i] &= Q - 1;
  }
}

static void poly2_from_poly(struct poly2 *out, const struct poly *in) {
  crypto_word_t *words = out->v;
  unsigned shift = 0;
  crypto_word_t word = 0;

  for (unsigned i = 0; i < N; i++) {
    word >>= 1;
    word |= (crypto_word_t)(in->v[i] & 1) << (BITS_PER_WORD - 1);
    shift++;

    if (shift == BITS_PER_WORD) {
      *words = word;
      words++;
      word = 0;
      shift = 0;
    }
  }

  word >>= BITS_PER_WORD - shift;
  *words = word;
}

static uint16_t mod3(int16_t a) {
  const int16_t q = ((int32_t)a * 21845) >> 16;
  int16_t ret = a - 3 * q;

  return ret & ((ret & (ret >> 1)) - 1);
}

static void poly3_from_poly(struct poly3 *out, const struct poly *in) {
  crypto_word_t *words_s = out->s.v;
  crypto_word_t *words_a = out->a.v;
  crypto_word_t s = 0;
  crypto_word_t a = 0;
  unsigned shift = 0;

  for (unsigned i = 0; i < N; i++) {

    const uint16_t v = mod3((int16_t)(in->v[i] << 3) >> 3);
    s >>= 1;
    const crypto_word_t s_bit = (crypto_word_t)(v & 2) << (BITS_PER_WORD - 2);
    s |= s_bit;
    a >>= 1;
    a |= s_bit | (crypto_word_t)(v & 1) << (BITS_PER_WORD - 1);
    shift++;

    if (shift == BITS_PER_WORD) {
      *words_s = s;
      words_s++;
      *words_a = a;
      words_a++;
      s = a = 0;
      shift = 0;
    }
  }

  s >>= BITS_PER_WORD - shift;
  a >>= BITS_PER_WORD - shift;
  *words_s = s;
  *words_a = a;
}

static crypto_word_t poly3_from_poly_checked(struct poly3 *out,
                                             const struct poly *in) {
  crypto_word_t *words_s = out->s.v;
  crypto_word_t *words_a = out->a.v;
  crypto_word_t s = 0;
  crypto_word_t a = 0;
  unsigned shift = 0;
  crypto_word_t ok = CONSTTIME_TRUE_W;

  for (unsigned i = 0; i < N; i++) {
    const uint16_t v = in->v[i];

    uint16_t mod3 = v & 3;
    mod3 ^= mod3 >> 1;
    const uint16_t expected = (uint16_t)((~((mod3 >> 1) - 1)) | mod3) % Q;
    ok &= constant_time_eq_w(v, expected);

    s >>= 1;
    const crypto_word_t s_bit = (crypto_word_t)(mod3 & 2)
                                << (BITS_PER_WORD - 2);
    s |= s_bit;
    a >>= 1;
    a |= s_bit | (crypto_word_t)(mod3 & 1) << (BITS_PER_WORD - 1);
    shift++;

    if (shift == BITS_PER_WORD) {
      *words_s = s;
      words_s++;
      *words_a = a;
      words_a++;
      s = a = 0;
      shift = 0;
    }
  }

  s >>= BITS_PER_WORD - shift;
  a >>= BITS_PER_WORD - shift;
  *words_s = s;
  *words_a = a;

  return ok;
}

static void poly_from_poly2(struct poly *out, const struct poly2 *in) {
  const crypto_word_t *words = in->v;
  unsigned shift = 0;
  crypto_word_t word = *words;

  for (unsigned i = 0; i < N; i++) {
    out->v[i] = word & 1;
    word >>= 1;
    shift++;

    if (shift == BITS_PER_WORD) {
      words++;
      word = *words;
      shift = 0;
    }
  }

  poly_normalize(out);
}

static void poly_from_poly3(struct poly *out, const struct poly3 *in) {
  const crypto_word_t *words_s = in->s.v;
  const crypto_word_t *words_a = in->a.v;
  crypto_word_t word_s = ~(*words_s);
  crypto_word_t word_a = *words_a;
  unsigned shift = 0;

  for (unsigned i = 0; i < N; i++) {
    out->v[i] = (uint16_t)(word_s & 1) - 1;
    out->v[i] |= word_a & 1;
    word_s >>= 1;
    word_a >>= 1;
    shift++;

    if (shift == BITS_PER_WORD) {
      words_s++;
      words_a++;
      word_s = ~(*words_s);
      word_a = *words_a;
      shift = 0;
    }
  }

  poly_normalize(out);
}

static void poly_invert_mod2(struct poly *out, const struct poly *in) {

  struct poly2 v, r, f, g;

  poly2_zero(&v);

  poly2_zero(&r);
  r.v[0] = 1;

  OPENSSL_memset(&f, 0xff, sizeof(struct poly2));
  f.v[WORDS_PER_POLY - 1] >>= BITS_PER_WORD - BITS_IN_LAST_WORD;

  poly2_from_poly(&g, in);
  poly2_mod_phiN(&g);
  poly2_reverse_700(&g, &g);
  int delta = 1;

  for (size_t i = 0; i < (2 * (N - 1)) - 1; i++) {
    poly2_lshift1(&v);

    const crypto_word_t delta_sign_bit = (delta >> (sizeof(delta) * 8 - 1)) & 1;
    const crypto_word_t delta_is_non_negative = delta_sign_bit - 1;
    const crypto_word_t delta_is_non_zero = ~constant_time_is_zero_w(delta);
    const crypto_word_t g_has_constant_term = lsb_to_all(g.v[0]);
    const crypto_word_t mask =
        g_has_constant_term & delta_is_non_negative & delta_is_non_zero;

    const crypto_word_t c = lsb_to_all(f.v[0] & g.v[0]);

    delta = constant_time_select_int(mask, -delta, delta);
    delta++;

    poly2_cswap(&f, &g, mask);
    poly2_fmadd(&g, &f, c);
    poly2_rshift1(&g);

    poly2_cswap(&v, &r, mask);
    poly2_fmadd(&r, &v, c);
  }

  assert(delta == 0);
  assert(f.v[0] & 1);
  poly2_reverse_700(&v, &v);
  poly_from_poly2(out, &v);
  poly_assert_normalized(out);
}

static void poly_invert(struct POLY_MUL_SCRATCH *scratch, struct poly *out,
                        const struct poly *in) {

  struct poly a, *b, tmp;

  for (unsigned i = 0; i < N; i++) {
    a.v[i] = -in->v[i];
  }
  poly_normalize(&a);

  b = out;
  poly_invert_mod2(b, in);

  for (unsigned i = 0; i < 4; i++) {
    poly_mul(scratch, &tmp, &a, b);
    tmp.v[0] += 2;
    poly_mul(scratch, b, b, &tmp);
  }

  poly_assert_normalized(out);
}

#define POLY_BYTES 1138

static void poly_marshal(uint8_t out[POLY_BYTES], const struct poly *in) {
  const uint16_t *p = in->v;

  for (size_t i = 0; i < N / 8; i++) {
    out[0] = p[0];
    out[1] = (0x1f & (p[0] >> 8)) | ((p[1] & 0x07) << 5);
    out[2] = p[1] >> 3;
    out[3] = (3 & (p[1] >> 11)) | ((p[2] & 0x3f) << 2);
    out[4] = (0x7f & (p[2] >> 6)) | ((p[3] & 0x01) << 7);
    out[5] = p[3] >> 1;
    out[6] = (0xf & (p[3] >> 9)) | ((p[4] & 0x0f) << 4);
    out[7] = p[4] >> 4;
    out[8] = (1 & (p[4] >> 12)) | ((p[5] & 0x7f) << 1);
    out[9] = (0x3f & (p[5] >> 7)) | ((p[6] & 0x03) << 6);
    out[10] = p[6] >> 2;
    out[11] = (7 & (p[6] >> 10)) | ((p[7] & 0x1f) << 3);
    out[12] = p[7] >> 5;

    p += 8;
    out += 13;
  }

  out[0] = p[0];
  out[1] = (0x1f & (p[0] >> 8)) | ((p[1] & 0x07) << 5);
  out[2] = p[1] >> 3;
  out[3] = (3 & (p[1] >> 11)) | ((p[2] & 0x3f) << 2);
  out[4] = (0x7f & (p[2] >> 6)) | ((p[3] & 0x01) << 7);
  out[5] = p[3] >> 1;
  out[6] = 0xf & (p[3] >> 9);
}

static int poly_unmarshal(struct poly *out, const uint8_t in[POLY_BYTES]) {
  uint16_t *p = out->v;

  for (size_t i = 0; i < N / 8; i++) {
    p[0] = (uint16_t)(in[0]) | (uint16_t)(in[1] & 0x1f) << 8;
    p[1] = (uint16_t)(in[1] >> 5) | (uint16_t)(in[2]) << 3 |
           (uint16_t)(in[3] & 3) << 11;
    p[2] = (uint16_t)(in[3] >> 2) | (uint16_t)(in[4] & 0x7f) << 6;
    p[3] = (uint16_t)(in[4] >> 7) | (uint16_t)(in[5]) << 1 |
           (uint16_t)(in[6] & 0xf) << 9;
    p[4] = (uint16_t)(in[6] >> 4) | (uint16_t)(in[7]) << 4 |
           (uint16_t)(in[8] & 1) << 12;
    p[5] = (uint16_t)(in[8] >> 1) | (uint16_t)(in[9] & 0x3f) << 7;
    p[6] = (uint16_t)(in[9] >> 6) | (uint16_t)(in[10]) << 2 |
           (uint16_t)(in[11] & 7) << 10;
    p[7] = (uint16_t)(in[11] >> 3) | (uint16_t)(in[12]) << 5;

    p += 8;
    in += 13;
  }

  p[0] = (uint16_t)(in[0]) | (uint16_t)(in[1] & 0x1f) << 8;
  p[1] = (uint16_t)(in[1] >> 5) | (uint16_t)(in[2]) << 3 |
         (uint16_t)(in[3] & 3) << 11;
  p[2] = (uint16_t)(in[3] >> 2) | (uint16_t)(in[4] & 0x7f) << 6;
  p[3] = (uint16_t)(in[4] >> 7) | (uint16_t)(in[5]) << 1 |
         (uint16_t)(in[6] & 0xf) << 9;

  for (unsigned i = 0; i < N - 1; i++) {
    out->v[i] = (int16_t)(out->v[i] << 3) >> 3;
  }

  if ((in[6] & 0xf0) != 0) {
    return 0;
  }

  uint32_t sum = 0;
  for (size_t i = 0; i < N - 1; i++) {
    sum += out->v[i];
  }

  out->v[N - 1] = (uint16_t)(0u - sum);
  poly_normalize(out);

  return 1;
}

static uint16_t mod3_from_modQ(uint16_t v) {
  v &= 3;
  return v ^ (v >> 1);
}

static void poly_marshal_mod3(uint8_t out[HRSS_POLY3_BYTES],
                              const struct poly *in) {
  const uint16_t *coeffs = in->v;

  assert(coeffs[N - 1] == 0);

  for (size_t i = 0; i < HRSS_POLY3_BYTES; i++) {
    const uint16_t coeffs0 = mod3_from_modQ(coeffs[0]);
    const uint16_t coeffs1 = mod3_from_modQ(coeffs[1]);
    const uint16_t coeffs2 = mod3_from_modQ(coeffs[2]);
    const uint16_t coeffs3 = mod3_from_modQ(coeffs[3]);
    const uint16_t coeffs4 = mod3_from_modQ(coeffs[4]);
    out[i] = coeffs0 + coeffs1 * 3 + coeffs2 * 9 + coeffs3 * 27 + coeffs4 * 81;
    coeffs += 5;
  }
}

static void poly_short_sample(struct poly *out,
                              const uint8_t in[HRSS_SAMPLE_BYTES]) {
  static_assert(HRSS_SAMPLE_BYTES == N - 1, "HRSS_SAMPLE_BYTES incorrect");
  for (size_t i = 0; i < N - 1; i++) {
    uint16_t v = mod3(in[i]);

    v |= ((v >> 1) ^ 1) - 1;
    out->v[i] = v;
  }
  out->v[N - 1] = 0;
  poly_normalize(out);
}

static void poly_short_sample_plus(struct poly *out,
                                   const uint8_t in[HRSS_SAMPLE_BYTES]) {
  poly_short_sample(out, in);

  uint16_t sum = 0;
  for (unsigned i = 0; i < N - 2; i++) {
    sum += (unsigned)out->v[i] * out->v[i + 1];
  }

  sum = ((int16_t)sum) >> 15;
  const uint16_t scale = sum | (~sum & 1);
  for (unsigned i = 0; i < N; i += 2) {
    out->v[i] = (unsigned)out->v[i] * scale;
  }
  poly_assert_normalized(out);
}

static void poly_lift(struct poly *out, const struct poly *a) {

  out->v[0] = a->v[0] + a->v[2];
  out->v[1] = a->v[1];
  out->v[2] = -a->v[0] + a->v[2];

  uint16_t s0 = 0, s2 = 0;
  for (size_t i = 3; i < 699; i += 3) {
    s0 += -a->v[i] + a->v[i + 2];

    s2 += a->v[i + 1] - a->v[i + 2];
  }

  s0 -= a->v[699];

  s2 += a->v[700];

  out->v[0] += s0;
  out->v[1] -= (s0 + s2);
  out->v[2] += s2;

  for (size_t i = 3; i < N; i++) {
    out->v[i] = (out->v[i - 3] - (a->v[i - 2] + a->v[i - 1] + a->v[i]));
  }

  const crypto_word_t v = out->v[700];
  for (unsigned i = 0; i < N; i++) {
    const uint16_t vi_mod3 = mod3(out->v[i] - v);

    out->v[i] = (~((vi_mod3 >> 1) - 1)) | vi_mod3;
  }

  poly_mul_x_minus_1(out);
  poly_normalize(out);
}

namespace {

struct public_key {
  struct poly ph;
};

struct private_key {
  struct poly3 f, f_inverse;
  struct poly ph_inverse;
  uint8_t hmac_key[32];
};

}

static struct public_key *public_key_from_external(
    struct HRSS_public_key *ext) {
  static_assert(
      sizeof(struct HRSS_public_key) >= sizeof(struct public_key) + 15,
      "HRSS public key too small");

  return reinterpret_cast<public_key *>(align_pointer(ext->opaque, 16));
}

static struct private_key *private_key_from_external(
    struct HRSS_private_key *ext) {
  static_assert(
      sizeof(struct HRSS_private_key) >= sizeof(struct private_key) + 15,
      "HRSS private key too small");

  return reinterpret_cast<private_key *>(align_pointer(ext->opaque, 16));
}

static void *malloc_align32(void **out_ptr, size_t size) {
  void *ptr = OPENSSL_malloc(size + 31);
  if (!ptr) {
    *out_ptr = NULL;
    return NULL;
  }

  *out_ptr = ptr;
  return align_pointer(ptr, 32);
}

int HRSS_generate_key(
    struct HRSS_public_key *out_pub, struct HRSS_private_key *out_priv,
    const uint8_t in[HRSS_SAMPLE_BYTES + HRSS_SAMPLE_BYTES + 32]) {
  struct public_key *pub = public_key_from_external(out_pub);
  struct private_key *priv = private_key_from_external(out_priv);

  struct vars {
    struct POLY_MUL_SCRATCH scratch;
    struct poly f;
    struct poly pg_phi1;
    struct poly pfg_phi1;
    struct poly pfg_phi1_inverse;
  };

  void *malloc_ptr;
  struct vars *const vars = reinterpret_cast<struct vars *>(
      malloc_align32(&malloc_ptr, sizeof(struct vars)));
  if (!vars) {

    memset(out_pub, 0, sizeof(struct HRSS_public_key));
    RAND_bytes((uint8_t *)out_priv, sizeof(struct HRSS_private_key));
    return 0;
  }

#if !defined(NDEBUG)
  OPENSSL_memset(vars, 0xff, sizeof(struct vars));
#endif

  OPENSSL_memcpy(priv->hmac_key, in + 2 * HRSS_SAMPLE_BYTES,
                 sizeof(priv->hmac_key));

  poly_short_sample_plus(&vars->f, in);
  poly3_from_poly(&priv->f, &vars->f);
  HRSS_poly3_invert(&priv->f_inverse, &priv->f);

  poly_short_sample_plus(&vars->pg_phi1, in + HRSS_SAMPLE_BYTES);
  for (unsigned i = 0; i < N; i++) {
    vars->pg_phi1.v[i] *= 3;
  }
  poly_mul_x_minus_1(&vars->pg_phi1);

  poly_mul(&vars->scratch, &vars->pfg_phi1, &vars->f, &vars->pg_phi1);

  poly_invert(&vars->scratch, &vars->pfg_phi1_inverse, &vars->pfg_phi1);

  poly_mul(&vars->scratch, &pub->ph, &vars->pfg_phi1_inverse, &vars->pg_phi1);
  poly_mul(&vars->scratch, &pub->ph, &pub->ph, &vars->pg_phi1);
  poly_clamp(&pub->ph);

  poly_mul(&vars->scratch, &priv->ph_inverse, &vars->pfg_phi1_inverse,
           &vars->f);
  poly_mul(&vars->scratch, &priv->ph_inverse, &priv->ph_inverse, &vars->f);
  poly_clamp(&priv->ph_inverse);

  OPENSSL_free(malloc_ptr);
  return 1;
}

static const char kSharedKey[] = "shared key";

int HRSS_encap(uint8_t out_ciphertext[POLY_BYTES], uint8_t out_shared_key[32],
               const struct HRSS_public_key *in_pub,
               const uint8_t in[HRSS_SAMPLE_BYTES + HRSS_SAMPLE_BYTES]) {
  const struct public_key *pub =
      public_key_from_external((struct HRSS_public_key *)in_pub);

  struct vars {
    struct POLY_MUL_SCRATCH scratch;
    struct poly m, r, m_lifted;
    struct poly prh_plus_m;
    SHA256_CTX hash_ctx;
    uint8_t m_bytes[HRSS_POLY3_BYTES];
    uint8_t r_bytes[HRSS_POLY3_BYTES];
  };

  void *malloc_ptr;
  struct vars *const vars = reinterpret_cast<struct vars *>(
      malloc_align32(&malloc_ptr, sizeof(struct vars)));
  if (!vars) {

    memset(out_ciphertext, 0, POLY_BYTES);
    RAND_bytes(out_shared_key, 32);
    return 0;
  }

#if !defined(NDEBUG)
  OPENSSL_memset(vars, 0xff, sizeof(struct vars));
#endif

  poly_short_sample(&vars->m, in);
  poly_short_sample(&vars->r, in + HRSS_SAMPLE_BYTES);
  poly_lift(&vars->m_lifted, &vars->m);

  poly_mul(&vars->scratch, &vars->prh_plus_m, &vars->r, &pub->ph);
  for (unsigned i = 0; i < N; i++) {
    vars->prh_plus_m.v[i] += vars->m_lifted.v[i];
  }

  poly_marshal(out_ciphertext, &vars->prh_plus_m);

  poly_marshal_mod3(vars->m_bytes, &vars->m);
  poly_marshal_mod3(vars->r_bytes, &vars->r);

  SHA256_Init(&vars->hash_ctx);
  SHA256_Update(&vars->hash_ctx, kSharedKey, sizeof(kSharedKey));
  SHA256_Update(&vars->hash_ctx, vars->m_bytes, sizeof(vars->m_bytes));
  SHA256_Update(&vars->hash_ctx, vars->r_bytes, sizeof(vars->r_bytes));
  SHA256_Update(&vars->hash_ctx, out_ciphertext, POLY_BYTES);
  SHA256_Final(out_shared_key, &vars->hash_ctx);

  OPENSSL_free(malloc_ptr);
  return 1;
}

int HRSS_decap(uint8_t out_shared_key[HRSS_KEY_BYTES],
               const struct HRSS_private_key *in_priv,
               const uint8_t *ciphertext, size_t ciphertext_len) {
  const struct private_key *priv =
      private_key_from_external((struct HRSS_private_key *)in_priv);

#if defined(_MSC_VER)

#pragma warning(push)
#pragma warning(disable : 4324)
#endif
  struct vars {
    struct POLY_MUL_SCRATCH scratch;
    uint8_t masked_key[SHA256_CBLOCK];
    SHA256_CTX hash_ctx;
    struct poly c;
    struct poly f, cf;
    struct poly3 cf3, m3;
    struct poly m, m_lifted;
    struct poly r;
    struct poly3 r3;
    uint8_t expected_ciphertext[HRSS_CIPHERTEXT_BYTES];
    uint8_t m_bytes[HRSS_POLY3_BYTES];
    uint8_t r_bytes[HRSS_POLY3_BYTES];
    uint8_t shared_key[32];
  };
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

  void *malloc_ptr;
  struct vars *const vars = reinterpret_cast<struct vars *>(
      malloc_align32(&malloc_ptr, sizeof(struct vars)));
  if (!vars) {

    RAND_bytes(out_shared_key, HRSS_KEY_BYTES);
    return 0;
  }

#if !defined(NDEBUG)
  OPENSSL_memset(vars, 0xff, sizeof(struct vars));
#endif

  static_assert(sizeof(priv->hmac_key) <= sizeof(vars->masked_key),
                "HRSS HMAC key larger than SHA-256 block size");
  for (size_t i = 0; i < sizeof(priv->hmac_key); i++) {
    vars->masked_key[i] = priv->hmac_key[i] ^ 0x36;
  }
  OPENSSL_memset(vars->masked_key + sizeof(priv->hmac_key), 0x36,
                 sizeof(vars->masked_key) - sizeof(priv->hmac_key));

  SHA256_Init(&vars->hash_ctx);
  SHA256_Update(&vars->hash_ctx, vars->masked_key, sizeof(vars->masked_key));
  SHA256_Update(&vars->hash_ctx, ciphertext, ciphertext_len);
  uint8_t inner_digest[SHA256_DIGEST_LENGTH];
  SHA256_Final(inner_digest, &vars->hash_ctx);

  for (size_t i = 0; i < sizeof(priv->hmac_key); i++) {
    vars->masked_key[i] ^= (0x5c ^ 0x36);
  }
  OPENSSL_memset(vars->masked_key + sizeof(priv->hmac_key), 0x5c,
                 sizeof(vars->masked_key) - sizeof(priv->hmac_key));

  SHA256_Init(&vars->hash_ctx);
  SHA256_Update(&vars->hash_ctx, vars->masked_key, sizeof(vars->masked_key));
  SHA256_Update(&vars->hash_ctx, inner_digest, sizeof(inner_digest));
  static_assert(HRSS_KEY_BYTES == SHA256_DIGEST_LENGTH,
                "HRSS shared key length incorrect");
  SHA256_Final(out_shared_key, &vars->hash_ctx);

  crypto_word_t ok = 0;
  if (ciphertext_len != HRSS_CIPHERTEXT_BYTES ||
      !poly_unmarshal(&vars->c, ciphertext)) {
    goto out;
  }

  poly_from_poly3(&vars->f, &priv->f);
  poly_mul(&vars->scratch, &vars->cf, &vars->c, &vars->f);
  poly3_from_poly(&vars->cf3, &vars->cf);

  HRSS_poly3_mul(&vars->m3, &vars->cf3, &priv->f_inverse);

  poly_from_poly3(&vars->m, &vars->m3);
  poly_lift(&vars->m_lifted, &vars->m);

  for (unsigned i = 0; i < N; i++) {
    vars->r.v[i] = vars->c.v[i] - vars->m_lifted.v[i];
  }
  poly_normalize(&vars->r);
  poly_mul(&vars->scratch, &vars->r, &vars->r, &priv->ph_inverse);
  poly_mod_phiN(&vars->r);
  poly_clamp(&vars->r);

  ok = poly3_from_poly_checked(&vars->r3, &vars->r);

  static_assert(HRSS_CIPHERTEXT_BYTES == POLY_BYTES,
                "ciphertext is the wrong size");
  assert(ciphertext_len == sizeof(vars->expected_ciphertext));
  poly_marshal(vars->expected_ciphertext, &vars->c);

  poly_marshal_mod3(vars->m_bytes, &vars->m);
  poly_marshal_mod3(vars->r_bytes, &vars->r);

  ok &= constant_time_is_zero_w(
      CRYPTO_memcmp(ciphertext, vars->expected_ciphertext,
                    sizeof(vars->expected_ciphertext)));

  SHA256_Init(&vars->hash_ctx);
  SHA256_Update(&vars->hash_ctx, kSharedKey, sizeof(kSharedKey));
  SHA256_Update(&vars->hash_ctx, vars->m_bytes, sizeof(vars->m_bytes));
  SHA256_Update(&vars->hash_ctx, vars->r_bytes, sizeof(vars->r_bytes));
  SHA256_Update(&vars->hash_ctx, vars->expected_ciphertext,
                sizeof(vars->expected_ciphertext));
  SHA256_Final(vars->shared_key, &vars->hash_ctx);

  for (unsigned i = 0; i < sizeof(vars->shared_key); i++) {
    out_shared_key[i] =
        constant_time_select_8(ok, vars->shared_key[i], out_shared_key[i]);
  }

out:
  OPENSSL_free(malloc_ptr);
  return 1;
}

void HRSS_marshal_public_key(uint8_t out[HRSS_PUBLIC_KEY_BYTES],
                             const struct HRSS_public_key *in_pub) {
  const struct public_key *pub =
      public_key_from_external((struct HRSS_public_key *)in_pub);
  poly_marshal(out, &pub->ph);
}

int HRSS_parse_public_key(struct HRSS_public_key *out,
                          const uint8_t in[HRSS_PUBLIC_KEY_BYTES]) {
  struct public_key *pub = public_key_from_external(out);
  if (!poly_unmarshal(&pub->ph, in)) {
    return 0;
  }
  OPENSSL_memset(&pub->ph.v[N], 0, 3 * sizeof(uint16_t));
  return 1;
}
