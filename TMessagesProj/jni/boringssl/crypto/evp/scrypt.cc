// Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <openssl/evp.h>

#include <assert.h>

#include <openssl/err.h>
#include <openssl/mem.h>

#include "../internal.h"

typedef struct {
  uint32_t words[16];
} block_t;

static_assert(sizeof(block_t) == 64, "block_t has padding");

static void salsa208_word_specification(block_t *inout) {
  block_t x;
  OPENSSL_memcpy(&x, inout, sizeof(x));

  for (int i = 8; i > 0; i -= 2) {
    x.words[4] ^= CRYPTO_rotl_u32(x.words[0] + x.words[12], 7);
    x.words[8] ^= CRYPTO_rotl_u32(x.words[4] + x.words[0], 9);
    x.words[12] ^= CRYPTO_rotl_u32(x.words[8] + x.words[4], 13);
    x.words[0] ^= CRYPTO_rotl_u32(x.words[12] + x.words[8], 18);
    x.words[9] ^= CRYPTO_rotl_u32(x.words[5] + x.words[1], 7);
    x.words[13] ^= CRYPTO_rotl_u32(x.words[9] + x.words[5], 9);
    x.words[1] ^= CRYPTO_rotl_u32(x.words[13] + x.words[9], 13);
    x.words[5] ^= CRYPTO_rotl_u32(x.words[1] + x.words[13], 18);
    x.words[14] ^= CRYPTO_rotl_u32(x.words[10] + x.words[6], 7);
    x.words[2] ^= CRYPTO_rotl_u32(x.words[14] + x.words[10], 9);
    x.words[6] ^= CRYPTO_rotl_u32(x.words[2] + x.words[14], 13);
    x.words[10] ^= CRYPTO_rotl_u32(x.words[6] + x.words[2], 18);
    x.words[3] ^= CRYPTO_rotl_u32(x.words[15] + x.words[11], 7);
    x.words[7] ^= CRYPTO_rotl_u32(x.words[3] + x.words[15], 9);
    x.words[11] ^= CRYPTO_rotl_u32(x.words[7] + x.words[3], 13);
    x.words[15] ^= CRYPTO_rotl_u32(x.words[11] + x.words[7], 18);
    x.words[1] ^= CRYPTO_rotl_u32(x.words[0] + x.words[3], 7);
    x.words[2] ^= CRYPTO_rotl_u32(x.words[1] + x.words[0], 9);
    x.words[3] ^= CRYPTO_rotl_u32(x.words[2] + x.words[1], 13);
    x.words[0] ^= CRYPTO_rotl_u32(x.words[3] + x.words[2], 18);
    x.words[6] ^= CRYPTO_rotl_u32(x.words[5] + x.words[4], 7);
    x.words[7] ^= CRYPTO_rotl_u32(x.words[6] + x.words[5], 9);
    x.words[4] ^= CRYPTO_rotl_u32(x.words[7] + x.words[6], 13);
    x.words[5] ^= CRYPTO_rotl_u32(x.words[4] + x.words[7], 18);
    x.words[11] ^= CRYPTO_rotl_u32(x.words[10] + x.words[9], 7);
    x.words[8] ^= CRYPTO_rotl_u32(x.words[11] + x.words[10], 9);
    x.words[9] ^= CRYPTO_rotl_u32(x.words[8] + x.words[11], 13);
    x.words[10] ^= CRYPTO_rotl_u32(x.words[9] + x.words[8], 18);
    x.words[12] ^= CRYPTO_rotl_u32(x.words[15] + x.words[14], 7);
    x.words[13] ^= CRYPTO_rotl_u32(x.words[12] + x.words[15], 9);
    x.words[14] ^= CRYPTO_rotl_u32(x.words[13] + x.words[12], 13);
    x.words[15] ^= CRYPTO_rotl_u32(x.words[14] + x.words[13], 18);
  }

  for (int i = 0; i < 16; ++i) {
    inout->words[i] += x.words[i];
  }
}

static void xor_block(block_t *out, const block_t *a, const block_t *b) {
  for (size_t i = 0; i < 16; i++) {
    out->words[i] = a->words[i] ^ b->words[i];
  }
}

static void scryptBlockMix(block_t *out, const block_t *B, uint64_t r) {
  assert(out != B);

  block_t X;
  OPENSSL_memcpy(&X, &B[r * 2 - 1], sizeof(X));
  for (uint64_t i = 0; i < r * 2; i++) {
    xor_block(&X, &X, &B[i]);
    salsa208_word_specification(&X);

    OPENSSL_memcpy(&out[i / 2 + (i & 1) * r], &X, sizeof(X));
  }
}

static void scryptROMix(block_t *B, uint64_t r, uint64_t N, block_t *T,
                        block_t *V) {

  OPENSSL_memcpy(V, B, 2 * r * sizeof(block_t));
  for (uint64_t i = 1; i < N; i++) {
    scryptBlockMix(&V[2 * r * i                     ],
                   &V[2 * r * (i - 1)                       ], r);
  }
  scryptBlockMix(B, &V[2 * r * (N - 1)                       ], r);

  for (uint64_t i = 0; i < N; i++) {

    uint32_t j = B[2 * r - 1].words[0] & (N - 1);
    for (size_t k = 0; k < 2 * r; k++) {
      xor_block(&T[k], &B[k], &V[2 * r * j + k]);
    }
    scryptBlockMix(B, T, r);
  }
}

#define SCRYPT_PR_MAX ((1 << 30) - 1)

#define SCRYPT_MAX_MEM (1024 * 1024 * 65)

int EVP_PBE_scrypt(const char *password, size_t password_len,
                   const uint8_t *salt, size_t salt_len, uint64_t N, uint64_t r,
                   uint64_t p, size_t max_mem, uint8_t *out_key,
                   size_t key_len) {
  if (r == 0 || p == 0 || p > SCRYPT_PR_MAX / r ||

      N < 2 || (N & (N - 1)) ||

      N > UINT64_C(1) << 32 ||

      (16 * r <= 63 && N >= UINT64_C(1) << (16 * r))) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_INVALID_PARAMETERS);
    return 0;
  }

  if (max_mem == 0) {
    max_mem = SCRYPT_MAX_MEM;
  }

  size_t max_scrypt_blocks = max_mem / (2 * r * sizeof(block_t));
  if (max_scrypt_blocks < p + 1 || max_scrypt_blocks - p - 1 < N) {
    OPENSSL_PUT_ERROR(EVP, EVP_R_MEMORY_LIMIT_EXCEEDED);
    return 0;
  }

  static_assert(UINT64_MAX >= SIZE_MAX, "size_t exceeds uint64_t");
  size_t B_blocks = p * 2 * r;
  size_t B_bytes = B_blocks * sizeof(block_t);
  size_t T_blocks = 2 * r;
  size_t V_blocks = N * 2 * r;
  block_t *B = reinterpret_cast<block_t *>(
      OPENSSL_calloc(B_blocks + T_blocks + V_blocks, sizeof(block_t)));
  if (B == NULL) {
    return 0;
  }

  int ret = 0;
  block_t *T = B + B_blocks;
  block_t *V = T + T_blocks;

  if (!PKCS5_PBKDF2_HMAC(password, password_len, salt, salt_len, 1,
                         EVP_sha256(), B_bytes, (uint8_t *)B)) {
    goto err;
  }

  for (uint64_t i = 0; i < p; i++) {
    scryptROMix(B + 2 * r * i, r, N, T, V);
  }

  if (!PKCS5_PBKDF2_HMAC(password, password_len, (const uint8_t *)B, B_bytes, 1,
                         EVP_sha256(), key_len, out_key)) {
    goto err;
  }

  ret = 1;

err:
  OPENSSL_free(B);
  return ret;
}
