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

#include <openssl/aes.h>

#include <assert.h>
#include <string.h>

#include "../../internal.h"
#include "internal.h"

#if defined(OPENSSL_SSE2)
#include <emmintrin.h>
#endif

#if defined(OPENSSL_SSE2)
typedef __m128i aes_word_t;

#define AES_NOHW_WORD_SIZE 16
#define AES_NOHW_BATCH_SIZE 8
#define AES_NOHW_ROW0_MASK \
  _mm_set_epi32(0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff)
#define AES_NOHW_ROW1_MASK \
  _mm_set_epi32(0x0000ff00, 0x0000ff00, 0x0000ff00, 0x0000ff00)
#define AES_NOHW_ROW2_MASK \
  _mm_set_epi32(0x00ff0000, 0x00ff0000, 0x00ff0000, 0x00ff0000)
#define AES_NOHW_ROW3_MASK \
  _mm_set_epi32(0xff000000, 0xff000000, 0xff000000, 0xff000000)
#define AES_NOHW_COL01_MASK \
  _mm_set_epi32(0x00000000, 0x00000000, 0xffffffff, 0xffffffff)
#define AES_NOHW_COL2_MASK \
  _mm_set_epi32(0x00000000, 0xffffffff, 0x00000000, 0x00000000)
#define AES_NOHW_COL3_MASK \
  _mm_set_epi32(0xffffffff, 0x00000000, 0x00000000, 0x00000000)

static inline aes_word_t aes_nohw_and(aes_word_t a, aes_word_t b) {
  return _mm_and_si128(a, b);
}

static inline aes_word_t aes_nohw_or(aes_word_t a, aes_word_t b) {
  return _mm_or_si128(a, b);
}

static inline aes_word_t aes_nohw_xor(aes_word_t a, aes_word_t b) {
  return _mm_xor_si128(a, b);
}

static inline aes_word_t aes_nohw_not(aes_word_t a) {
  return _mm_xor_si128(
      a, _mm_set_epi32(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff));
}

#define aes_nohw_shift_left(                 a,             i) \
  _mm_slli_si128((a), (i))
#define aes_nohw_shift_right(                 a,             i) \
  _mm_srli_si128((a), (i))
#else
#if defined(OPENSSL_64_BIT)
typedef uint64_t aes_word_t;
#define AES_NOHW_WORD_SIZE 8
#define AES_NOHW_BATCH_SIZE 4
#define AES_NOHW_ROW0_MASK UINT64_C(0x000f000f000f000f)
#define AES_NOHW_ROW1_MASK UINT64_C(0x00f000f000f000f0)
#define AES_NOHW_ROW2_MASK UINT64_C(0x0f000f000f000f00)
#define AES_NOHW_ROW3_MASK UINT64_C(0xf000f000f000f000)
#define AES_NOHW_COL01_MASK UINT64_C(0x00000000ffffffff)
#define AES_NOHW_COL2_MASK UINT64_C(0x0000ffff00000000)
#define AES_NOHW_COL3_MASK UINT64_C(0xffff000000000000)
#else
typedef uint32_t aes_word_t;
#define AES_NOHW_WORD_SIZE 4
#define AES_NOHW_BATCH_SIZE 2
#define AES_NOHW_ROW0_MASK 0x03030303
#define AES_NOHW_ROW1_MASK 0x0c0c0c0c
#define AES_NOHW_ROW2_MASK 0x30303030
#define AES_NOHW_ROW3_MASK 0xc0c0c0c0
#define AES_NOHW_COL01_MASK 0x0000ffff
#define AES_NOHW_COL2_MASK 0x00ff0000
#define AES_NOHW_COL3_MASK 0xff000000
#endif

static inline aes_word_t aes_nohw_and(aes_word_t a, aes_word_t b) {
  return a & b;
}

static inline aes_word_t aes_nohw_or(aes_word_t a, aes_word_t b) {
  return a | b;
}

static inline aes_word_t aes_nohw_xor(aes_word_t a, aes_word_t b) {
  return a ^ b;
}

static inline aes_word_t aes_nohw_not(aes_word_t a) { return ~a; }

static inline aes_word_t aes_nohw_shift_left(aes_word_t a, aes_word_t i) {
  return a << (i * AES_NOHW_BATCH_SIZE);
}

static inline aes_word_t aes_nohw_shift_right(aes_word_t a, aes_word_t i) {
  return a >> (i * AES_NOHW_BATCH_SIZE);
}
#endif

static_assert(AES_NOHW_BATCH_SIZE * 128 == 8 * 8 * sizeof(aes_word_t),
              "batch size does not match word size");
static_assert(AES_NOHW_WORD_SIZE == sizeof(aes_word_t),
              "AES_NOHW_WORD_SIZE is incorrect");

#define AES_NOHW_BLOCK_WORDS (16 / sizeof(aes_word_t))

typedef struct {
  aes_word_t w[8];
} AES_NOHW_BATCH;

typedef struct {

  AES_NOHW_BATCH keys[AES_MAXNR + 1];
} AES_NOHW_SCHEDULE;

static inline void aes_nohw_batch_set(AES_NOHW_BATCH *batch,
                                      const aes_word_t in[AES_NOHW_BLOCK_WORDS],
                                      size_t i) {

  assert(i < AES_NOHW_BATCH_SIZE);
#if defined(OPENSSL_SSE2)
  batch->w[i] = in[0];
#elif defined(OPENSSL_64_BIT)
  batch->w[i] = in[0];
  batch->w[i + 4] = in[1];
#else
  batch->w[i] = in[0];
  batch->w[i + 2] = in[1];
  batch->w[i + 4] = in[2];
  batch->w[i + 6] = in[3];
#endif
}

static inline void aes_nohw_batch_get(const AES_NOHW_BATCH *batch,
                                      aes_word_t out[AES_NOHW_BLOCK_WORDS],
                                      size_t i) {
  assert(i < AES_NOHW_BATCH_SIZE);
#if defined(OPENSSL_SSE2)
  out[0] = batch->w[i];
#elif defined(OPENSSL_64_BIT)
  out[0] = batch->w[i];
  out[1] = batch->w[i + 4];
#else
  out[0] = batch->w[i];
  out[1] = batch->w[i + 2];
  out[2] = batch->w[i + 4];
  out[3] = batch->w[i + 6];
#endif
}

#if !defined(OPENSSL_SSE2)

static inline aes_word_t aes_nohw_delta_swap(aes_word_t a, aes_word_t mask,
                                             aes_word_t shift) {

  aes_word_t b = (a ^ (a >> shift)) & mask;
  return a ^ b ^ (b << shift);
}

#if defined(OPENSSL_64_BIT)
static inline uint64_t aes_nohw_compact_word(uint64_t a) {

  a = aes_nohw_delta_swap(a, UINT64_C(0x00f000f000f000f0), 4);

  a = aes_nohw_delta_swap(a, UINT64_C(0x0000ff000000ff00), 8);

  a = aes_nohw_delta_swap(a, UINT64_C(0x00000000ffff0000), 16);
  return a;
}

static inline uint64_t aes_nohw_uncompact_word(uint64_t a) {

  a = aes_nohw_delta_swap(a, UINT64_C(0x00000000ffff0000), 16);
  a = aes_nohw_delta_swap(a, UINT64_C(0x0000ff000000ff00), 8);
  a = aes_nohw_delta_swap(a, UINT64_C(0x00f000f000f000f0), 4);
  return a;
}
#else
static inline uint32_t aes_nohw_compact_word(uint32_t a) {

  a = aes_nohw_delta_swap(a, 0x00cc00cc, 6);

  a = aes_nohw_delta_swap(a, 0x0000f0f0, 12);
  return a;
}

static inline uint32_t aes_nohw_uncompact_word(uint32_t a) {

  a = aes_nohw_delta_swap(a, 0x0000f0f0, 12);
  a = aes_nohw_delta_swap(a, 0x00cc00cc, 6);
  return a;
}

static inline uint32_t aes_nohw_word_from_bytes(uint8_t a0, uint8_t a1,
                                                uint8_t a2, uint8_t a3) {
  return (uint32_t)a0 | ((uint32_t)a1 << 8) | ((uint32_t)a2 << 16) |
         ((uint32_t)a3 << 24);
}
#endif
#endif

static inline void aes_nohw_compact_block(aes_word_t out[AES_NOHW_BLOCK_WORDS],
                                          const uint8_t in[16]) {
  memcpy(out, in, 16);
#if defined(OPENSSL_SSE2)

#elif defined(OPENSSL_64_BIT)
  uint64_t a0 = aes_nohw_compact_word(out[0]);
  uint64_t a1 = aes_nohw_compact_word(out[1]);
  out[0] = (a0 & UINT64_C(0x00000000ffffffff)) | (a1 << 32);
  out[1] = (a1 & UINT64_C(0xffffffff00000000)) | (a0 >> 32);
#else
  uint32_t a0 = aes_nohw_compact_word(out[0]);
  uint32_t a1 = aes_nohw_compact_word(out[1]);
  uint32_t a2 = aes_nohw_compact_word(out[2]);
  uint32_t a3 = aes_nohw_compact_word(out[3]);

  out[0] = aes_nohw_word_from_bytes(a0, a1, a2, a3);
  out[1] = aes_nohw_word_from_bytes(a0 >> 8, a1 >> 8, a2 >> 8, a3 >> 8);
  out[2] = aes_nohw_word_from_bytes(a0 >> 16, a1 >> 16, a2 >> 16, a3 >> 16);
  out[3] = aes_nohw_word_from_bytes(a0 >> 24, a1 >> 24, a2 >> 24, a3 >> 24);
#endif
}

static inline void aes_nohw_uncompact_block(
    uint8_t out[16], const aes_word_t in[AES_NOHW_BLOCK_WORDS]) {
#if defined(OPENSSL_SSE2)
  memcpy(out, in, 16);
#elif defined(OPENSSL_64_BIT)
  uint64_t a0 = in[0];
  uint64_t a1 = in[1];
  uint64_t b0 =
      aes_nohw_uncompact_word((a0 & UINT64_C(0x00000000ffffffff)) | (a1 << 32));
  uint64_t b1 =
      aes_nohw_uncompact_word((a1 & UINT64_C(0xffffffff00000000)) | (a0 >> 32));
  memcpy(out, &b0, 8);
  memcpy(out + 8, &b1, 8);
#else
  uint32_t a0 = in[0];
  uint32_t a1 = in[1];
  uint32_t a2 = in[2];
  uint32_t a3 = in[3];

  uint32_t b0 = aes_nohw_word_from_bytes(a0, a1, a2, a3);
  uint32_t b1 = aes_nohw_word_from_bytes(a0 >> 8, a1 >> 8, a2 >> 8, a3 >> 8);
  uint32_t b2 =
      aes_nohw_word_from_bytes(a0 >> 16, a1 >> 16, a2 >> 16, a3 >> 16);
  uint32_t b3 =
      aes_nohw_word_from_bytes(a0 >> 24, a1 >> 24, a2 >> 24, a3 >> 24);
  b0 = aes_nohw_uncompact_word(b0);
  b1 = aes_nohw_uncompact_word(b1);
  b2 = aes_nohw_uncompact_word(b2);
  b3 = aes_nohw_uncompact_word(b3);
  memcpy(out, &b0, 4);
  memcpy(out + 4, &b1, 4);
  memcpy(out + 8, &b2, 4);
  memcpy(out + 12, &b3, 4);
#endif
}

#if defined(OPENSSL_SSE2)

#define aes_nohw_swap_bits(              a,               b,              \
                                          mask,             shift)        \
  do {                                                                    \
    __m128i swap =                                                        \
        _mm_and_si128(_mm_xor_si128(_mm_srli_epi32(*(a), (shift)), *(b)), \
                      _mm_set_epi32((mask), (mask), (mask), (mask)));     \
    *(a) = _mm_xor_si128(*(a), _mm_slli_epi32(swap, (shift)));            \
    *(b) = _mm_xor_si128(*(b), swap);                                     \
                                                                          \
  } while (0)
#else
static inline void aes_nohw_swap_bits(aes_word_t *a, aes_word_t *b,
                                      uint32_t mask, aes_word_t shift) {
#if defined(OPENSSL_64_BIT)
  aes_word_t mask_w = (((uint64_t)mask) << 32) | mask;
#else
  aes_word_t mask_w = mask;
#endif

  aes_word_t swap = ((*a >> shift) ^ *b) & mask_w;
  *a ^= swap << shift;
  *b ^= swap;
}
#endif

static void aes_nohw_transpose(AES_NOHW_BATCH *batch) {

  aes_nohw_swap_bits(&batch->w[0], &batch->w[1], 0x55555555, 1);
  aes_nohw_swap_bits(&batch->w[2], &batch->w[3], 0x55555555, 1);
  aes_nohw_swap_bits(&batch->w[4], &batch->w[5], 0x55555555, 1);
  aes_nohw_swap_bits(&batch->w[6], &batch->w[7], 0x55555555, 1);

#if AES_NOHW_BATCH_SIZE >= 4

  aes_nohw_swap_bits(&batch->w[0], &batch->w[2], 0x33333333, 2);
  aes_nohw_swap_bits(&batch->w[1], &batch->w[3], 0x33333333, 2);
  aes_nohw_swap_bits(&batch->w[4], &batch->w[6], 0x33333333, 2);
  aes_nohw_swap_bits(&batch->w[5], &batch->w[7], 0x33333333, 2);
#endif

#if AES_NOHW_BATCH_SIZE >= 8

  aes_nohw_swap_bits(&batch->w[0], &batch->w[4], 0x0f0f0f0f, 4);
  aes_nohw_swap_bits(&batch->w[1], &batch->w[5], 0x0f0f0f0f, 4);
  aes_nohw_swap_bits(&batch->w[2], &batch->w[6], 0x0f0f0f0f, 4);
  aes_nohw_swap_bits(&batch->w[3], &batch->w[7], 0x0f0f0f0f, 4);
#endif
}

static void aes_nohw_to_batch(AES_NOHW_BATCH *out, const uint8_t *in,
                              size_t num_blocks) {

  memset(out, 0, sizeof(AES_NOHW_BATCH));
  assert(num_blocks <= AES_NOHW_BATCH_SIZE);
  for (size_t i = 0; i < num_blocks; i++) {
    aes_word_t block[AES_NOHW_BLOCK_WORDS];
    aes_nohw_compact_block(block, in + 16 * i);
    aes_nohw_batch_set(out, block, i);
  }

  aes_nohw_transpose(out);
}

static void aes_nohw_from_batch(uint8_t *out, size_t num_blocks,
                                const AES_NOHW_BATCH *batch) {
  AES_NOHW_BATCH copy = *batch;
  aes_nohw_transpose(&copy);

  assert(num_blocks <= AES_NOHW_BATCH_SIZE);
  for (size_t i = 0; i < num_blocks; i++) {
    aes_word_t block[AES_NOHW_BLOCK_WORDS];
    aes_nohw_batch_get(&copy, block, i);
    aes_nohw_uncompact_block(out + 16 * i, block);
  }
}

static void aes_nohw_add_round_key(AES_NOHW_BATCH *batch,
                                   const AES_NOHW_BATCH *key) {
  for (size_t i = 0; i < 8; i++) {
    batch->w[i] = aes_nohw_xor(batch->w[i], key->w[i]);
  }
}

static void aes_nohw_sub_bytes(AES_NOHW_BATCH *batch) {

  aes_word_t x0 = batch->w[7];
  aes_word_t x1 = batch->w[6];
  aes_word_t x2 = batch->w[5];
  aes_word_t x3 = batch->w[4];
  aes_word_t x4 = batch->w[3];
  aes_word_t x5 = batch->w[2];
  aes_word_t x6 = batch->w[1];
  aes_word_t x7 = batch->w[0];

  aes_word_t y14 = aes_nohw_xor(x3, x5);
  aes_word_t y13 = aes_nohw_xor(x0, x6);
  aes_word_t y9 = aes_nohw_xor(x0, x3);
  aes_word_t y8 = aes_nohw_xor(x0, x5);
  aes_word_t t0 = aes_nohw_xor(x1, x2);
  aes_word_t y1 = aes_nohw_xor(t0, x7);
  aes_word_t y4 = aes_nohw_xor(y1, x3);
  aes_word_t y12 = aes_nohw_xor(y13, y14);
  aes_word_t y2 = aes_nohw_xor(y1, x0);
  aes_word_t y5 = aes_nohw_xor(y1, x6);
  aes_word_t y3 = aes_nohw_xor(y5, y8);
  aes_word_t t1 = aes_nohw_xor(x4, y12);
  aes_word_t y15 = aes_nohw_xor(t1, x5);
  aes_word_t y20 = aes_nohw_xor(t1, x1);
  aes_word_t y6 = aes_nohw_xor(y15, x7);
  aes_word_t y10 = aes_nohw_xor(y15, t0);
  aes_word_t y11 = aes_nohw_xor(y20, y9);
  aes_word_t y7 = aes_nohw_xor(x7, y11);
  aes_word_t y17 = aes_nohw_xor(y10, y11);
  aes_word_t y19 = aes_nohw_xor(y10, y8);
  aes_word_t y16 = aes_nohw_xor(t0, y11);
  aes_word_t y21 = aes_nohw_xor(y13, y16);
  aes_word_t y18 = aes_nohw_xor(x0, y16);

  aes_word_t t2 = aes_nohw_and(y12, y15);
  aes_word_t t3 = aes_nohw_and(y3, y6);
  aes_word_t t4 = aes_nohw_xor(t3, t2);
  aes_word_t t5 = aes_nohw_and(y4, x7);
  aes_word_t t6 = aes_nohw_xor(t5, t2);
  aes_word_t t7 = aes_nohw_and(y13, y16);
  aes_word_t t8 = aes_nohw_and(y5, y1);
  aes_word_t t9 = aes_nohw_xor(t8, t7);
  aes_word_t t10 = aes_nohw_and(y2, y7);
  aes_word_t t11 = aes_nohw_xor(t10, t7);
  aes_word_t t12 = aes_nohw_and(y9, y11);
  aes_word_t t13 = aes_nohw_and(y14, y17);
  aes_word_t t14 = aes_nohw_xor(t13, t12);
  aes_word_t t15 = aes_nohw_and(y8, y10);
  aes_word_t t16 = aes_nohw_xor(t15, t12);
  aes_word_t t17 = aes_nohw_xor(t4, t14);
  aes_word_t t18 = aes_nohw_xor(t6, t16);
  aes_word_t t19 = aes_nohw_xor(t9, t14);
  aes_word_t t20 = aes_nohw_xor(t11, t16);
  aes_word_t t21 = aes_nohw_xor(t17, y20);
  aes_word_t t22 = aes_nohw_xor(t18, y19);
  aes_word_t t23 = aes_nohw_xor(t19, y21);
  aes_word_t t24 = aes_nohw_xor(t20, y18);
  aes_word_t t25 = aes_nohw_xor(t21, t22);
  aes_word_t t26 = aes_nohw_and(t21, t23);
  aes_word_t t27 = aes_nohw_xor(t24, t26);
  aes_word_t t28 = aes_nohw_and(t25, t27);
  aes_word_t t29 = aes_nohw_xor(t28, t22);
  aes_word_t t30 = aes_nohw_xor(t23, t24);
  aes_word_t t31 = aes_nohw_xor(t22, t26);
  aes_word_t t32 = aes_nohw_and(t31, t30);
  aes_word_t t33 = aes_nohw_xor(t32, t24);
  aes_word_t t34 = aes_nohw_xor(t23, t33);
  aes_word_t t35 = aes_nohw_xor(t27, t33);
  aes_word_t t36 = aes_nohw_and(t24, t35);
  aes_word_t t37 = aes_nohw_xor(t36, t34);
  aes_word_t t38 = aes_nohw_xor(t27, t36);
  aes_word_t t39 = aes_nohw_and(t29, t38);
  aes_word_t t40 = aes_nohw_xor(t25, t39);
  aes_word_t t41 = aes_nohw_xor(t40, t37);
  aes_word_t t42 = aes_nohw_xor(t29, t33);
  aes_word_t t43 = aes_nohw_xor(t29, t40);
  aes_word_t t44 = aes_nohw_xor(t33, t37);
  aes_word_t t45 = aes_nohw_xor(t42, t41);
  aes_word_t z0 = aes_nohw_and(t44, y15);
  aes_word_t z1 = aes_nohw_and(t37, y6);
  aes_word_t z2 = aes_nohw_and(t33, x7);
  aes_word_t z3 = aes_nohw_and(t43, y16);
  aes_word_t z4 = aes_nohw_and(t40, y1);
  aes_word_t z5 = aes_nohw_and(t29, y7);
  aes_word_t z6 = aes_nohw_and(t42, y11);
  aes_word_t z7 = aes_nohw_and(t45, y17);
  aes_word_t z8 = aes_nohw_and(t41, y10);
  aes_word_t z9 = aes_nohw_and(t44, y12);
  aes_word_t z10 = aes_nohw_and(t37, y3);
  aes_word_t z11 = aes_nohw_and(t33, y4);
  aes_word_t z12 = aes_nohw_and(t43, y13);
  aes_word_t z13 = aes_nohw_and(t40, y5);
  aes_word_t z14 = aes_nohw_and(t29, y2);
  aes_word_t z15 = aes_nohw_and(t42, y9);
  aes_word_t z16 = aes_nohw_and(t45, y14);
  aes_word_t z17 = aes_nohw_and(t41, y8);

  aes_word_t t46 = aes_nohw_xor(z15, z16);
  aes_word_t t47 = aes_nohw_xor(z10, z11);
  aes_word_t t48 = aes_nohw_xor(z5, z13);
  aes_word_t t49 = aes_nohw_xor(z9, z10);
  aes_word_t t50 = aes_nohw_xor(z2, z12);
  aes_word_t t51 = aes_nohw_xor(z2, z5);
  aes_word_t t52 = aes_nohw_xor(z7, z8);
  aes_word_t t53 = aes_nohw_xor(z0, z3);
  aes_word_t t54 = aes_nohw_xor(z6, z7);
  aes_word_t t55 = aes_nohw_xor(z16, z17);
  aes_word_t t56 = aes_nohw_xor(z12, t48);
  aes_word_t t57 = aes_nohw_xor(t50, t53);
  aes_word_t t58 = aes_nohw_xor(z4, t46);
  aes_word_t t59 = aes_nohw_xor(z3, t54);
  aes_word_t t60 = aes_nohw_xor(t46, t57);
  aes_word_t t61 = aes_nohw_xor(z14, t57);
  aes_word_t t62 = aes_nohw_xor(t52, t58);
  aes_word_t t63 = aes_nohw_xor(t49, t58);
  aes_word_t t64 = aes_nohw_xor(z4, t59);
  aes_word_t t65 = aes_nohw_xor(t61, t62);
  aes_word_t t66 = aes_nohw_xor(z1, t63);
  aes_word_t s0 = aes_nohw_xor(t59, t63);
  aes_word_t s6 = aes_nohw_xor(t56, aes_nohw_not(t62));
  aes_word_t s7 = aes_nohw_xor(t48, aes_nohw_not(t60));
  aes_word_t t67 = aes_nohw_xor(t64, t65);
  aes_word_t s3 = aes_nohw_xor(t53, t66);
  aes_word_t s4 = aes_nohw_xor(t51, t66);
  aes_word_t s5 = aes_nohw_xor(t47, t65);
  aes_word_t s1 = aes_nohw_xor(t64, aes_nohw_not(s3));
  aes_word_t s2 = aes_nohw_xor(t55, aes_nohw_not(t67));

  batch->w[0] = s7;
  batch->w[1] = s6;
  batch->w[2] = s5;
  batch->w[3] = s4;
  batch->w[4] = s3;
  batch->w[5] = s2;
  batch->w[6] = s1;
  batch->w[7] = s0;
}

static void aes_nohw_sub_bytes_inv_affine(AES_NOHW_BATCH *batch) {
  aes_word_t a0 = batch->w[0];
  aes_word_t a1 = batch->w[1];
  aes_word_t a2 = batch->w[2];
  aes_word_t a3 = batch->w[3];
  aes_word_t a4 = batch->w[4];
  aes_word_t a5 = batch->w[5];
  aes_word_t a6 = batch->w[6];
  aes_word_t a7 = batch->w[7];

  aes_word_t b0 = aes_nohw_xor(a2, aes_nohw_xor(a5, a7));
  aes_word_t b1 = aes_nohw_xor(a3, aes_nohw_xor(a6, a0));
  aes_word_t b2 = aes_nohw_xor(a4, aes_nohw_xor(a7, a1));
  aes_word_t b3 = aes_nohw_xor(a5, aes_nohw_xor(a0, a2));
  aes_word_t b4 = aes_nohw_xor(a6, aes_nohw_xor(a1, a3));
  aes_word_t b5 = aes_nohw_xor(a7, aes_nohw_xor(a2, a4));
  aes_word_t b6 = aes_nohw_xor(a0, aes_nohw_xor(a3, a5));
  aes_word_t b7 = aes_nohw_xor(a1, aes_nohw_xor(a4, a6));

  batch->w[0] = aes_nohw_not(b0);
  batch->w[1] = b1;
  batch->w[2] = aes_nohw_not(b2);
  batch->w[3] = b3;
  batch->w[4] = b4;
  batch->w[5] = b5;
  batch->w[6] = b6;
  batch->w[7] = b7;
}

static void aes_nohw_inv_sub_bytes(AES_NOHW_BATCH *batch) {

  aes_nohw_sub_bytes_inv_affine(batch);
  aes_nohw_sub_bytes(batch);
  aes_nohw_sub_bytes_inv_affine(batch);
}

#define aes_nohw_rotate_cols_right(                 v,             n) \
  (aes_nohw_or(aes_nohw_shift_right((v), (n)*4),                      \
               aes_nohw_shift_left((v), 16 - (n)*4)))

static void aes_nohw_shift_rows(AES_NOHW_BATCH *batch) {
  for (size_t i = 0; i < 8; i++) {
    aes_word_t row0 = aes_nohw_and(batch->w[i], AES_NOHW_ROW0_MASK);
    aes_word_t row1 = aes_nohw_and(batch->w[i], AES_NOHW_ROW1_MASK);
    aes_word_t row2 = aes_nohw_and(batch->w[i], AES_NOHW_ROW2_MASK);
    aes_word_t row3 = aes_nohw_and(batch->w[i], AES_NOHW_ROW3_MASK);
    row1 = aes_nohw_rotate_cols_right(row1, 1);
    row2 = aes_nohw_rotate_cols_right(row2, 2);
    row3 = aes_nohw_rotate_cols_right(row3, 3);
    batch->w[i] = aes_nohw_or(aes_nohw_or(row0, row1), aes_nohw_or(row2, row3));
  }
}

static void aes_nohw_inv_shift_rows(AES_NOHW_BATCH *batch) {
  for (size_t i = 0; i < 8; i++) {
    aes_word_t row0 = aes_nohw_and(batch->w[i], AES_NOHW_ROW0_MASK);
    aes_word_t row1 = aes_nohw_and(batch->w[i], AES_NOHW_ROW1_MASK);
    aes_word_t row2 = aes_nohw_and(batch->w[i], AES_NOHW_ROW2_MASK);
    aes_word_t row3 = aes_nohw_and(batch->w[i], AES_NOHW_ROW3_MASK);
    row1 = aes_nohw_rotate_cols_right(row1, 3);
    row2 = aes_nohw_rotate_cols_right(row2, 2);
    row3 = aes_nohw_rotate_cols_right(row3, 1);
    batch->w[i] = aes_nohw_or(aes_nohw_or(row0, row1), aes_nohw_or(row2, row3));
  }
}

static inline aes_word_t aes_nohw_rotate_rows_down(aes_word_t v) {
#if defined(OPENSSL_SSE2)
  return _mm_or_si128(_mm_srli_epi32(v, 8), _mm_slli_epi32(v, 24));
#elif defined(OPENSSL_64_BIT)
  return ((v >> 4) & UINT64_C(0x0fff0fff0fff0fff)) |
         ((v << 12) & UINT64_C(0xf000f000f000f000));
#else
  return ((v >> 2) & 0x3f3f3f3f) | ((v << 6) & 0xc0c0c0c0);
#endif
}

static inline aes_word_t aes_nohw_rotate_rows_twice(aes_word_t v) {
#if defined(OPENSSL_SSE2)
  return _mm_or_si128(_mm_srli_epi32(v, 16), _mm_slli_epi32(v, 16));
#elif defined(OPENSSL_64_BIT)
  return ((v >> 8) & UINT64_C(0x00ff00ff00ff00ff)) |
         ((v << 8) & UINT64_C(0xff00ff00ff00ff00));
#else
  return ((v >> 4) & 0x0f0f0f0f) | ((v << 4) & 0xf0f0f0f0);
#endif
}

static void aes_nohw_mix_columns(AES_NOHW_BATCH *batch) {

  aes_word_t a0 = batch->w[0];
  aes_word_t a1 = batch->w[1];
  aes_word_t a2 = batch->w[2];
  aes_word_t a3 = batch->w[3];
  aes_word_t a4 = batch->w[4];
  aes_word_t a5 = batch->w[5];
  aes_word_t a6 = batch->w[6];
  aes_word_t a7 = batch->w[7];

  aes_word_t r0 = aes_nohw_rotate_rows_down(a0);
  aes_word_t a0_r0 = aes_nohw_xor(a0, r0);
  aes_word_t r1 = aes_nohw_rotate_rows_down(a1);
  aes_word_t a1_r1 = aes_nohw_xor(a1, r1);
  aes_word_t r2 = aes_nohw_rotate_rows_down(a2);
  aes_word_t a2_r2 = aes_nohw_xor(a2, r2);
  aes_word_t r3 = aes_nohw_rotate_rows_down(a3);
  aes_word_t a3_r3 = aes_nohw_xor(a3, r3);
  aes_word_t r4 = aes_nohw_rotate_rows_down(a4);
  aes_word_t a4_r4 = aes_nohw_xor(a4, r4);
  aes_word_t r5 = aes_nohw_rotate_rows_down(a5);
  aes_word_t a5_r5 = aes_nohw_xor(a5, r5);
  aes_word_t r6 = aes_nohw_rotate_rows_down(a6);
  aes_word_t a6_r6 = aes_nohw_xor(a6, r6);
  aes_word_t r7 = aes_nohw_rotate_rows_down(a7);
  aes_word_t a7_r7 = aes_nohw_xor(a7, r7);

  batch->w[0] =
      aes_nohw_xor(aes_nohw_xor(a7_r7, r0), aes_nohw_rotate_rows_twice(a0_r0));
  batch->w[1] =
      aes_nohw_xor(aes_nohw_xor(a0_r0, a7_r7),
                   aes_nohw_xor(r1, aes_nohw_rotate_rows_twice(a1_r1)));
  batch->w[2] =
      aes_nohw_xor(aes_nohw_xor(a1_r1, r2), aes_nohw_rotate_rows_twice(a2_r2));
  batch->w[3] =
      aes_nohw_xor(aes_nohw_xor(a2_r2, a7_r7),
                   aes_nohw_xor(r3, aes_nohw_rotate_rows_twice(a3_r3)));
  batch->w[4] =
      aes_nohw_xor(aes_nohw_xor(a3_r3, a7_r7),
                   aes_nohw_xor(r4, aes_nohw_rotate_rows_twice(a4_r4)));
  batch->w[5] =
      aes_nohw_xor(aes_nohw_xor(a4_r4, r5), aes_nohw_rotate_rows_twice(a5_r5));
  batch->w[6] =
      aes_nohw_xor(aes_nohw_xor(a5_r5, r6), aes_nohw_rotate_rows_twice(a6_r6));
  batch->w[7] =
      aes_nohw_xor(aes_nohw_xor(a6_r6, r7), aes_nohw_rotate_rows_twice(a7_r7));
}

static void aes_nohw_inv_mix_columns(AES_NOHW_BATCH *batch) {
  aes_word_t a0 = batch->w[0];
  aes_word_t a1 = batch->w[1];
  aes_word_t a2 = batch->w[2];
  aes_word_t a3 = batch->w[3];
  aes_word_t a4 = batch->w[4];
  aes_word_t a5 = batch->w[5];
  aes_word_t a6 = batch->w[6];
  aes_word_t a7 = batch->w[7];

  aes_word_t a0_r0 = aes_nohw_xor(a0, aes_nohw_rotate_rows_twice(a0));
  aes_word_t a1_r1 = aes_nohw_xor(a1, aes_nohw_rotate_rows_twice(a1));
  aes_word_t a2_r2 = aes_nohw_xor(a2, aes_nohw_rotate_rows_twice(a2));
  aes_word_t a3_r3 = aes_nohw_xor(a3, aes_nohw_rotate_rows_twice(a3));
  aes_word_t a4_r4 = aes_nohw_xor(a4, aes_nohw_rotate_rows_twice(a4));
  aes_word_t a5_r5 = aes_nohw_xor(a5, aes_nohw_rotate_rows_twice(a5));
  aes_word_t a6_r6 = aes_nohw_xor(a6, aes_nohw_rotate_rows_twice(a6));
  aes_word_t a7_r7 = aes_nohw_xor(a7, aes_nohw_rotate_rows_twice(a7));

  batch->w[0] = aes_nohw_xor(a0, a6_r6);
  batch->w[1] = aes_nohw_xor(a1, aes_nohw_xor(a6_r6, a7_r7));
  batch->w[2] = aes_nohw_xor(a2, aes_nohw_xor(a0_r0, a7_r7));
  batch->w[3] = aes_nohw_xor(a3, aes_nohw_xor(a1_r1, a6_r6));
  batch->w[4] =
      aes_nohw_xor(aes_nohw_xor(a4, a2_r2), aes_nohw_xor(a6_r6, a7_r7));
  batch->w[5] = aes_nohw_xor(a5, aes_nohw_xor(a3_r3, a7_r7));
  batch->w[6] = aes_nohw_xor(a6, a4_r4);
  batch->w[7] = aes_nohw_xor(a7, a5_r5);

  aes_nohw_mix_columns(batch);
}

static void aes_nohw_encrypt_batch(const AES_NOHW_SCHEDULE *key,
                                   size_t num_rounds, AES_NOHW_BATCH *batch) {
  aes_nohw_add_round_key(batch, &key->keys[0]);
  for (size_t i = 1; i < num_rounds; i++) {
    aes_nohw_sub_bytes(batch);
    aes_nohw_shift_rows(batch);
    aes_nohw_mix_columns(batch);
    aes_nohw_add_round_key(batch, &key->keys[i]);
  }
  aes_nohw_sub_bytes(batch);
  aes_nohw_shift_rows(batch);
  aes_nohw_add_round_key(batch, &key->keys[num_rounds]);
}

static void aes_nohw_decrypt_batch(const AES_NOHW_SCHEDULE *key,
                                   size_t num_rounds, AES_NOHW_BATCH *batch) {
  aes_nohw_add_round_key(batch, &key->keys[num_rounds]);
  aes_nohw_inv_shift_rows(batch);
  aes_nohw_inv_sub_bytes(batch);
  for (size_t i = num_rounds - 1; i > 0; i--) {
    aes_nohw_add_round_key(batch, &key->keys[i]);
    aes_nohw_inv_mix_columns(batch);
    aes_nohw_inv_shift_rows(batch);
    aes_nohw_inv_sub_bytes(batch);
  }
  aes_nohw_add_round_key(batch, &key->keys[0]);
}

static void aes_nohw_expand_round_keys(AES_NOHW_SCHEDULE *out,
                                       const AES_KEY *key) {
  for (size_t i = 0; i <= key->rounds; i++) {

    for (size_t j = 0; j < AES_NOHW_BATCH_SIZE; j++) {
      aes_word_t tmp[AES_NOHW_BLOCK_WORDS];
      memcpy(tmp, key->rd_key + 4 * i, 16);
      aes_nohw_batch_set(&out->keys[i], tmp, j);
    }
    aes_nohw_transpose(&out->keys[i]);
  }
}

static const uint8_t aes_nohw_rcon[10] = {0x01, 0x02, 0x04, 0x08, 0x10,
                                          0x20, 0x40, 0x80, 0x1b, 0x36};

static inline aes_word_t aes_nohw_rcon_slice(uint8_t rcon, size_t i) {
  rcon = (rcon >> (i * AES_NOHW_BATCH_SIZE)) & ((1 << AES_NOHW_BATCH_SIZE) - 1);
#if defined(OPENSSL_SSE2)
  return _mm_set_epi32(0, 0, 0, rcon);
#else
  return ((aes_word_t)rcon);
#endif
}

static void aes_nohw_sub_block(aes_word_t out[AES_NOHW_BLOCK_WORDS],
                               const aes_word_t in[AES_NOHW_BLOCK_WORDS]) {
  AES_NOHW_BATCH batch;
  memset(&batch, 0, sizeof(batch));
  aes_nohw_batch_set(&batch, in, 0);
  aes_nohw_transpose(&batch);
  aes_nohw_sub_bytes(&batch);
  aes_nohw_transpose(&batch);
  aes_nohw_batch_get(&batch, out, 0);
}

static void aes_nohw_setup_key_128(AES_KEY *key, const uint8_t in[16]) {
  key->rounds = 10;

  aes_word_t block[AES_NOHW_BLOCK_WORDS];
  aes_nohw_compact_block(block, in);
  memcpy(key->rd_key, block, 16);

  for (size_t i = 1; i <= 10; i++) {
    aes_word_t sub[AES_NOHW_BLOCK_WORDS];
    aes_nohw_sub_block(sub, block);
    uint8_t rcon = aes_nohw_rcon[i - 1];
    for (size_t j = 0; j < AES_NOHW_BLOCK_WORDS; j++) {

      block[j] = aes_nohw_xor(block[j], aes_nohw_rcon_slice(rcon, j));
      block[j] = aes_nohw_xor(
          block[j],
          aes_nohw_shift_right(aes_nohw_rotate_rows_down(sub[j]), 12));

      aes_word_t v = block[j];
      block[j] = aes_nohw_xor(block[j], aes_nohw_shift_left(v, 4));
      block[j] = aes_nohw_xor(block[j], aes_nohw_shift_left(v, 8));
      block[j] = aes_nohw_xor(block[j], aes_nohw_shift_left(v, 12));
    }
    memcpy(key->rd_key + 4 * i, block, 16);
  }
}

static void aes_nohw_setup_key_192(AES_KEY *key, const uint8_t in[24]) {
  key->rounds = 12;

  aes_word_t storage1[AES_NOHW_BLOCK_WORDS], storage2[AES_NOHW_BLOCK_WORDS];
  aes_word_t *block1 = storage1, *block2 = storage2;

  aes_nohw_compact_block(block1, in);
  memcpy(key->rd_key, block1, 16);

  uint8_t half_block[16] = {0};
  memcpy(half_block, in + 16, 8);
  aes_nohw_compact_block(block2, half_block);

  for (size_t i = 0; i < 4; i++) {
    aes_word_t sub[AES_NOHW_BLOCK_WORDS];
    aes_nohw_sub_block(sub, block2);
    uint8_t rcon = aes_nohw_rcon[2 * i];
    for (size_t j = 0; j < AES_NOHW_BLOCK_WORDS; j++) {

      block2[j] = aes_nohw_or(
          block2[j],
          aes_nohw_shift_left(
              aes_nohw_xor(block1[j], aes_nohw_rcon_slice(rcon, j)), 8));

      block2[j] = aes_nohw_xor(
          block2[j], aes_nohw_and(aes_nohw_shift_left(
                                      aes_nohw_rotate_rows_down(sub[j]), 4),
                                  AES_NOHW_COL2_MASK));
      block2[j] = aes_nohw_xor(
          block2[j],
          aes_nohw_and(aes_nohw_shift_left(block2[j], 4), AES_NOHW_COL3_MASK));

      block1[j] = aes_nohw_shift_right(block1[j], 8);
      block1[j] = aes_nohw_or(block1[j], aes_nohw_shift_left(block2[j], 8));

      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_right(block2[j], 12));
      aes_word_t v = block1[j];
      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_left(v, 4));
      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_left(v, 8));
      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_left(v, 12));
    }

    memcpy(key->rd_key + 4 * (3 * i + 1), block2, 16);
    memcpy(key->rd_key + 4 * (3 * i + 2), block1, 16);

    aes_nohw_sub_block(sub, block1);
    rcon = aes_nohw_rcon[2 * i + 1];
    for (size_t j = 0; j < AES_NOHW_BLOCK_WORDS; j++) {

      block2[j] = aes_nohw_shift_right(block2[j], 8);
      block2[j] = aes_nohw_or(block2[j], aes_nohw_shift_left(block1[j], 8));

      block2[j] = aes_nohw_xor(block2[j], aes_nohw_rcon_slice(rcon, j));
      block2[j] = aes_nohw_xor(
          block2[j],
          aes_nohw_shift_right(aes_nohw_rotate_rows_down(sub[j]), 12));

      aes_word_t v = block2[j];
      block2[j] = aes_nohw_xor(block2[j], aes_nohw_shift_left(v, 4));
      block2[j] = aes_nohw_xor(block2[j], aes_nohw_shift_left(v, 8));
      block2[j] = aes_nohw_xor(block2[j], aes_nohw_shift_left(v, 12));

      block1[j] = aes_nohw_shift_right(block1[j], 8);

      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_right(block2[j], 12));
      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_left(block1[j], 4));
      block1[j] = aes_nohw_and(block1[j], AES_NOHW_COL01_MASK);
    }

    memcpy(key->rd_key + 4 * (3 * i + 3), block2, 16);

    aes_word_t *tmp = block1;
    block1 = block2;
    block2 = tmp;
  }
}

static void aes_nohw_setup_key_256(AES_KEY *key, const uint8_t in[32]) {
  key->rounds = 14;

  aes_word_t block1[AES_NOHW_BLOCK_WORDS], block2[AES_NOHW_BLOCK_WORDS];
  aes_nohw_compact_block(block1, in);
  memcpy(key->rd_key, block1, 16);

  aes_nohw_compact_block(block2, in + 16);
  memcpy(key->rd_key + 4, block2, 16);

  for (size_t i = 2; i <= 14; i += 2) {
    aes_word_t sub[AES_NOHW_BLOCK_WORDS];
    aes_nohw_sub_block(sub, block2);
    uint8_t rcon = aes_nohw_rcon[i / 2 - 1];
    for (size_t j = 0; j < AES_NOHW_BLOCK_WORDS; j++) {

      block1[j] = aes_nohw_xor(block1[j], aes_nohw_rcon_slice(rcon, j));
      block1[j] = aes_nohw_xor(
          block1[j],
          aes_nohw_shift_right(aes_nohw_rotate_rows_down(sub[j]), 12));

      aes_word_t v = block1[j];
      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_left(v, 4));
      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_left(v, 8));
      block1[j] = aes_nohw_xor(block1[j], aes_nohw_shift_left(v, 12));
    }
    memcpy(key->rd_key + 4 * i, block1, 16);

    if (i == 14) {
      break;
    }

    aes_nohw_sub_block(sub, block1);
    for (size_t j = 0; j < AES_NOHW_BLOCK_WORDS; j++) {

      block2[j] = aes_nohw_xor(block2[j], aes_nohw_shift_right(sub[j], 12));

      aes_word_t v = block2[j];
      block2[j] = aes_nohw_xor(block2[j], aes_nohw_shift_left(v, 4));
      block2[j] = aes_nohw_xor(block2[j], aes_nohw_shift_left(v, 8));
      block2[j] = aes_nohw_xor(block2[j], aes_nohw_shift_left(v, 12));
    }
    memcpy(key->rd_key + 4 * (i + 1), block2, 16);
  }
}

int aes_nohw_set_encrypt_key(const uint8_t *key, unsigned bits,
                             AES_KEY *aeskey) {
  switch (bits) {
    case 128:
      aes_nohw_setup_key_128(aeskey, key);
      return 0;
    case 192:
      aes_nohw_setup_key_192(aeskey, key);
      return 0;
    case 256:
      aes_nohw_setup_key_256(aeskey, key);
      return 0;
  }
  return 1;
}

int aes_nohw_set_decrypt_key(const uint8_t *key, unsigned bits,
                             AES_KEY *aeskey) {
  return aes_nohw_set_encrypt_key(key, bits, aeskey);
}

void aes_nohw_encrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key) {
  AES_NOHW_SCHEDULE sched;
  aes_nohw_expand_round_keys(&sched, key);
  AES_NOHW_BATCH batch;
  aes_nohw_to_batch(&batch, in,                1);
  aes_nohw_encrypt_batch(&sched, key->rounds, &batch);
  aes_nohw_from_batch(out,                1, &batch);
}

void aes_nohw_decrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key) {
  AES_NOHW_SCHEDULE sched;
  aes_nohw_expand_round_keys(&sched, key);
  AES_NOHW_BATCH batch;
  aes_nohw_to_batch(&batch, in,                1);
  aes_nohw_decrypt_batch(&sched, key->rounds, &batch);
  aes_nohw_from_batch(out,                1, &batch);
}

static inline void aes_nohw_xor_block(uint8_t out[16], const uint8_t a[16],
                                      const uint8_t b[16]) {
  for (size_t i = 0; i < 16; i += sizeof(aes_word_t)) {
    aes_word_t x, y;
    memcpy(&x, a + i, sizeof(aes_word_t));
    memcpy(&y, b + i, sizeof(aes_word_t));
    x = aes_nohw_xor(x, y);
    memcpy(out + i, &x, sizeof(aes_word_t));
  }
}

void aes_nohw_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out,
                                   size_t blocks, const AES_KEY *key,
                                   const uint8_t ivec[16]) {
  if (blocks == 0) {
    return;
  }

  AES_NOHW_SCHEDULE sched;
  aes_nohw_expand_round_keys(&sched, key);

  alignas(AES_NOHW_WORD_SIZE) uint8_t ivs[AES_NOHW_BATCH_SIZE * 16];
  alignas(AES_NOHW_WORD_SIZE) uint8_t enc_ivs[AES_NOHW_BATCH_SIZE * 16];
  for (size_t i = 0; i < AES_NOHW_BATCH_SIZE; i++) {
    memcpy(ivs + 16 * i, ivec, 16);
  }

  uint32_t ctr = CRYPTO_load_u32_be(ivs + 12);
  for (;;) {

    for (size_t i = 0; i < AES_NOHW_BATCH_SIZE; i++) {
      CRYPTO_store_u32_be(ivs + 16 * i + 12, ctr + (uint32_t)i);
    }

    size_t todo = blocks >= AES_NOHW_BATCH_SIZE ? AES_NOHW_BATCH_SIZE : blocks;
    AES_NOHW_BATCH batch;
    aes_nohw_to_batch(&batch, ivs, todo);
    aes_nohw_encrypt_batch(&sched, key->rounds, &batch);
    aes_nohw_from_batch(enc_ivs, todo, &batch);

    for (size_t i = 0; i < todo; i++) {
      aes_nohw_xor_block(out + 16 * i, in + 16 * i, enc_ivs + 16 * i);
    }

    blocks -= todo;
    if (blocks == 0) {
      break;
    }

    in += 16 * AES_NOHW_BATCH_SIZE;
    out += 16 * AES_NOHW_BATCH_SIZE;
    ctr += AES_NOHW_BATCH_SIZE;
  }
}

void aes_nohw_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                          const AES_KEY *key, uint8_t *ivec, const int enc) {
  assert(len % 16 == 0);
  size_t blocks = len / 16;
  if (blocks == 0) {
    return;
  }

  AES_NOHW_SCHEDULE sched;
  aes_nohw_expand_round_keys(&sched, key);
  alignas(AES_NOHW_WORD_SIZE) uint8_t iv[16];
  memcpy(iv, ivec, 16);

  if (enc) {

    while (blocks > 0) {
      aes_nohw_xor_block(iv, iv, in);

      AES_NOHW_BATCH batch;
      aes_nohw_to_batch(&batch, iv,                1);
      aes_nohw_encrypt_batch(&sched, key->rounds, &batch);
      aes_nohw_from_batch(out,                1, &batch);

      memcpy(iv, out, 16);

      in += 16;
      out += 16;
      blocks--;
    }
    memcpy(ivec, iv, 16);
    return;
  }

  for (;;) {
    size_t todo = blocks >= AES_NOHW_BATCH_SIZE ? AES_NOHW_BATCH_SIZE : blocks;

    alignas(AES_NOHW_WORD_SIZE) uint8_t copy[AES_NOHW_BATCH_SIZE * 16];
    memcpy(copy, in, todo * 16);

    AES_NOHW_BATCH batch;
    aes_nohw_to_batch(&batch, in, todo);
    aes_nohw_decrypt_batch(&sched, key->rounds, &batch);
    aes_nohw_from_batch(out, todo, &batch);

    aes_nohw_xor_block(out, out, iv);
    for (size_t i = 1; i < todo; i++) {
      aes_nohw_xor_block(out + 16 * i, out + 16 * i, copy + 16 * (i - 1));
    }

    memcpy(iv, copy + 16 * (todo - 1), 16);

    blocks -= todo;
    if (blocks == 0) {
      break;
    }

    in += 16 * AES_NOHW_BATCH_SIZE;
    out += 16 * AES_NOHW_BATCH_SIZE;
  }

  memcpy(ivec, iv, 16);
}
