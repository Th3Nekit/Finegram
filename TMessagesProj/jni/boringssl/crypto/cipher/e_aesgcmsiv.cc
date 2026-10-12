// Copyright 2017 The BoringSSL Authors
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

#include <openssl/aead.h>

#include <assert.h>

#include <openssl/cipher.h>
#include <openssl/crypto.h>
#include <openssl/err.h>

#include "../fipsmodule/aes/internal.h"
#include "../fipsmodule/cipher/internal.h"
#include "../internal.h"

#define EVP_AEAD_AES_GCM_SIV_NONCE_LEN 12
#define EVP_AEAD_AES_GCM_SIV_TAG_LEN 16

namespace {

#if defined(OPENSSL_X86_64) && !defined(OPENSSL_NO_ASM) && \
    !defined(OPENSSL_WINDOWS)
#define AES_GCM_SIV_ASM

struct aead_aes_gcm_siv_asm_ctx {
  alignas(16) uint8_t key[16 * 15];
  int is_128_bit;
};

static_assert(sizeof(((EVP_AEAD_CTX *)NULL)->state) + 8 >=
                  sizeof(struct aead_aes_gcm_siv_asm_ctx),
              "AEAD state is too small");
static_assert(alignof(union evp_aead_ctx_st_state) >= 8,
              "AEAD state has insufficient alignment");

extern "C" {

extern void aes128gcmsiv_aes_ks(const uint8_t key[16],
                                uint8_t out_expanded_key[16 * 15]);

extern void aes256gcmsiv_aes_ks(const uint8_t key[32],
                                uint8_t out_expanded_key[16 * 15]);

extern void aesgcmsiv_polyval_horner(const uint8_t in_out_poly[16],
                                     const uint8_t key[16], const uint8_t *in,
                                     size_t in_blocks);

extern void aesgcmsiv_htable_init(uint8_t out_htable[16 * 8],
                                  const uint8_t auth_key[16]);

extern void aesgcmsiv_htable6_init(uint8_t out_htable[16 * 6],
                                   const uint8_t auth_key[16]);

extern void aesgcmsiv_htable_polyval(const uint8_t htable[16 * 8],
                                     const uint8_t *in, size_t in_len,
                                     uint8_t in_out_poly[16]);

extern void aes128gcmsiv_dec(const uint8_t *in, uint8_t *out,
                             uint8_t in_out_calculated_tag_and_scratch[16 * 8],
                             const uint8_t htable[16 * 6],
                             const struct aead_aes_gcm_siv_asm_ctx *key,
                             size_t in_len);

extern void aes256gcmsiv_dec(const uint8_t *in, uint8_t *out,
                             uint8_t in_out_calculated_tag_and_scratch[16 * 8],
                             const uint8_t htable[16 * 6],
                             const struct aead_aes_gcm_siv_asm_ctx *key,
                             size_t in_len);

extern void aes128gcmsiv_kdf(const uint8_t nonce[16],
                             uint64_t out_key_material[8],
                             const uint8_t *key_schedule);

extern void aes256gcmsiv_kdf(const uint8_t nonce[16],
                             uint64_t out_key_material[12],
                             const uint8_t *key_schedule);

extern void aes128gcmsiv_aes_ks_enc_x1(const uint8_t in[16], uint8_t out[16],
                                       uint8_t out_expanded_key[16 * 15],
                                       const uint64_t key[2]);

extern void aes256gcmsiv_aes_ks_enc_x1(const uint8_t in[16], uint8_t out[16],
                                       uint8_t out_expanded_key[16 * 15],
                                       const uint64_t key[4]);

extern void aes128gcmsiv_ecb_enc_block(
    const uint8_t in[16], uint8_t out[16],
    const struct aead_aes_gcm_siv_asm_ctx *expanded_key);

extern void aes256gcmsiv_ecb_enc_block(
    const uint8_t in[16], uint8_t out[16],
    const struct aead_aes_gcm_siv_asm_ctx *expanded_key);

extern void aes128gcmsiv_enc_msg_x4(const uint8_t *in, uint8_t *out,
                                    const uint8_t *tag,
                                    const struct aead_aes_gcm_siv_asm_ctx *key,
                                    size_t in_len);

extern void aes256gcmsiv_enc_msg_x4(const uint8_t *in, uint8_t *out,
                                    const uint8_t *tag,
                                    const struct aead_aes_gcm_siv_asm_ctx *key,
                                    size_t in_len);

extern void aes128gcmsiv_enc_msg_x8(const uint8_t *in, uint8_t *out,
                                    const uint8_t *tag,
                                    const struct aead_aes_gcm_siv_asm_ctx *key,
                                    size_t in_len);

extern void aes256gcmsiv_enc_msg_x8(const uint8_t *in, uint8_t *out,
                                    const uint8_t *tag,
                                    const struct aead_aes_gcm_siv_asm_ctx *key,
                                    size_t in_len);
}

struct aead_aes_gcm_siv_asm_ctx *asm_ctx_from_ctx(const EVP_AEAD_CTX *ctx) {

  const uintptr_t offset = ((uintptr_t)&ctx->state) & 8;
  return (struct aead_aes_gcm_siv_asm_ctx *)(&ctx->state.opaque[offset]);
}

int aead_aes_gcm_siv_asm_init(EVP_AEAD_CTX *ctx, const uint8_t *key,
                              size_t key_len, size_t tag_len) {
  const size_t key_bits = key_len * 8;

  if (key_bits != 128 && key_bits != 256) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_KEY_LENGTH);
    return 0;
  }

  if (tag_len == EVP_AEAD_DEFAULT_TAG_LENGTH) {
    tag_len = EVP_AEAD_AES_GCM_SIV_TAG_LEN;
  }

  if (tag_len != EVP_AEAD_AES_GCM_SIV_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TAG_TOO_LARGE);
    return 0;
  }

  struct aead_aes_gcm_siv_asm_ctx *gcm_siv_ctx = asm_ctx_from_ctx(ctx);
  assert((((uintptr_t)gcm_siv_ctx) & 15) == 0);

  if (key_bits == 128) {
    aes128gcmsiv_aes_ks(key, &gcm_siv_ctx->key[0]);
    gcm_siv_ctx->is_128_bit = 1;
  } else {
    aes256gcmsiv_aes_ks(key, &gcm_siv_ctx->key[0]);
    gcm_siv_ctx->is_128_bit = 0;
  }

  ctx->tag_len = tag_len;

  return 1;
}

void aead_aes_gcm_siv_asm_cleanup(EVP_AEAD_CTX *ctx) {}

void gcm_siv_asm_polyval(uint8_t out_tag[16], const uint8_t *in, size_t in_len,
                         const uint8_t *ad, size_t ad_len,
                         const uint8_t auth_key[16], const uint8_t nonce[12]) {
  OPENSSL_memset(out_tag, 0, 16);
  const size_t ad_blocks = ad_len / 16;
  const size_t in_blocks = in_len / 16;
  int htable_init = 0;
  alignas(16) uint8_t htable[16 * 8];

  if (ad_blocks > 8 || in_blocks > 8) {
    htable_init = 1;
    aesgcmsiv_htable_init(htable, auth_key);
  }

  if (htable_init) {
    aesgcmsiv_htable_polyval(htable, ad, ad_len & ~15, out_tag);
  } else {
    aesgcmsiv_polyval_horner(out_tag, auth_key, ad, ad_blocks);
  }

  uint8_t scratch[16];
  if (ad_len & 15) {
    OPENSSL_memset(scratch, 0, sizeof(scratch));
    OPENSSL_memcpy(scratch, &ad[ad_len & ~15], ad_len & 15);
    aesgcmsiv_polyval_horner(out_tag, auth_key, scratch, 1);
  }

  if (htable_init) {
    aesgcmsiv_htable_polyval(htable, in, in_len & ~15, out_tag);
  } else {
    aesgcmsiv_polyval_horner(out_tag, auth_key, in, in_blocks);
  }

  if (in_len & 15) {
    OPENSSL_memset(scratch, 0, sizeof(scratch));
    OPENSSL_memcpy(scratch, &in[in_len & ~15], in_len & 15);
    aesgcmsiv_polyval_horner(out_tag, auth_key, scratch, 1);
  }

  uint8_t length_block[16];
  CRYPTO_store_u64_le(length_block, ad_len * 8);
  CRYPTO_store_u64_le(length_block + 8, in_len * 8);
  aesgcmsiv_polyval_horner(out_tag, auth_key, length_block, 1);

  for (size_t i = 0; i < 12; i++) {
    out_tag[i] ^= nonce[i];
  }

  out_tag[15] &= 0x7f;
}

void aead_aes_gcm_siv_asm_crypt_last_block(
    int is_128_bit, uint8_t *out, const uint8_t *in, size_t in_len,
    const uint8_t tag[16],
    const struct aead_aes_gcm_siv_asm_ctx *enc_key_expanded) {
  alignas(16) uint8_t counter[16];
  OPENSSL_memcpy(&counter, tag, sizeof(counter));
  counter[15] |= 0x80;
  CRYPTO_store_u32_le(counter, CRYPTO_load_u32_le(counter) + in_len / 16);

  if (is_128_bit) {
    aes128gcmsiv_ecb_enc_block(counter, counter, enc_key_expanded);
  } else {
    aes256gcmsiv_ecb_enc_block(counter, counter, enc_key_expanded);
  }

  const size_t last_bytes_offset = in_len & ~15;
  const size_t last_bytes_len = in_len & 15;
  uint8_t *last_bytes_out = &out[last_bytes_offset];
  const uint8_t *last_bytes_in = &in[last_bytes_offset];
  for (size_t i = 0; i < last_bytes_len; i++) {
    last_bytes_out[i] = last_bytes_in[i] ^ counter[i];
  }
}

void aead_aes_gcm_siv_kdf(int is_128_bit,
                          const struct aead_aes_gcm_siv_asm_ctx *gcm_siv_ctx,
                          uint64_t out_record_auth_key[2],
                          uint64_t out_record_enc_key[4],
                          const uint8_t nonce[12]) {
  alignas(16) uint8_t padded_nonce[16];
  OPENSSL_memcpy(padded_nonce, nonce, 12);

  alignas(16) uint64_t key_material[12];
  if (is_128_bit) {
    aes128gcmsiv_kdf(padded_nonce, key_material, &gcm_siv_ctx->key[0]);
    out_record_enc_key[0] = key_material[4];
    out_record_enc_key[1] = key_material[6];
  } else {
    aes256gcmsiv_kdf(padded_nonce, key_material, &gcm_siv_ctx->key[0]);
    out_record_enc_key[0] = key_material[4];
    out_record_enc_key[1] = key_material[6];
    out_record_enc_key[2] = key_material[8];
    out_record_enc_key[3] = key_material[10];
  }

  out_record_auth_key[0] = key_material[0];
  out_record_auth_key[1] = key_material[2];
}

int aead_aes_gcm_siv_asm_seal_scatter(
    const EVP_AEAD_CTX *ctx, uint8_t *out, uint8_t *out_tag,
    size_t *out_tag_len, size_t max_out_tag_len, const uint8_t *nonce,
    size_t nonce_len, const uint8_t *in, size_t in_len, const uint8_t *extra_in,
    size_t extra_in_len, const uint8_t *ad, size_t ad_len) {
  const struct aead_aes_gcm_siv_asm_ctx *gcm_siv_ctx = asm_ctx_from_ctx(ctx);
  const uint64_t in_len_64 = in_len;
  const uint64_t ad_len_64 = ad_len;

  if (in_len_64 > (UINT64_C(1) << 36) || ad_len_64 >= (UINT64_C(1) << 61)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (max_out_tag_len < EVP_AEAD_AES_GCM_SIV_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }

  if (nonce_len != EVP_AEAD_AES_GCM_SIV_NONCE_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  alignas(16) uint64_t record_auth_key[2];
  alignas(16) uint64_t record_enc_key[4];
  aead_aes_gcm_siv_kdf(gcm_siv_ctx->is_128_bit, gcm_siv_ctx, record_auth_key,
                       record_enc_key, nonce);

  alignas(16) uint8_t tag[16] = {0};
  gcm_siv_asm_polyval(tag, in, in_len, ad, ad_len,
                      (const uint8_t *)record_auth_key, nonce);

  struct aead_aes_gcm_siv_asm_ctx enc_key_expanded;

  if (gcm_siv_ctx->is_128_bit) {
    aes128gcmsiv_aes_ks_enc_x1(tag, tag, &enc_key_expanded.key[0],
                               record_enc_key);

    if (in_len < 128) {
      aes128gcmsiv_enc_msg_x4(in, out, tag, &enc_key_expanded, in_len & ~15);
    } else {
      aes128gcmsiv_enc_msg_x8(in, out, tag, &enc_key_expanded, in_len & ~15);
    }
  } else {
    aes256gcmsiv_aes_ks_enc_x1(tag, tag, &enc_key_expanded.key[0],
                               record_enc_key);

    if (in_len < 128) {
      aes256gcmsiv_enc_msg_x4(in, out, tag, &enc_key_expanded, in_len & ~15);
    } else {
      aes256gcmsiv_enc_msg_x8(in, out, tag, &enc_key_expanded, in_len & ~15);
    }
  }

  if (in_len & 15) {
    aead_aes_gcm_siv_asm_crypt_last_block(gcm_siv_ctx->is_128_bit, out, in,
                                          in_len, tag, &enc_key_expanded);
  }

  OPENSSL_memcpy(out_tag, tag, sizeof(tag));
  *out_tag_len = EVP_AEAD_AES_GCM_SIV_TAG_LEN;

  return 1;
}

int aead_aes_gcm_siv_asm_open_gather(const EVP_AEAD_CTX *ctx, uint8_t *out,
                                     const uint8_t *nonce, size_t nonce_len,
                                     const uint8_t *in, size_t in_len,
                                     const uint8_t *in_tag, size_t in_tag_len,
                                     const uint8_t *ad, size_t ad_len) {
  const uint64_t ad_len_64 = ad_len;
  if (ad_len_64 >= (UINT64_C(1) << 61)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  const uint64_t in_len_64 = in_len;
  if (in_len_64 > UINT64_C(1) << 36 ||
      in_tag_len != EVP_AEAD_AES_GCM_SIV_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  if (nonce_len != EVP_AEAD_AES_GCM_SIV_NONCE_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  const struct aead_aes_gcm_siv_asm_ctx *gcm_siv_ctx = asm_ctx_from_ctx(ctx);

  alignas(16) uint64_t record_auth_key[2];
  alignas(16) uint64_t record_enc_key[4];
  aead_aes_gcm_siv_kdf(gcm_siv_ctx->is_128_bit, gcm_siv_ctx, record_auth_key,
                       record_enc_key, nonce);

  struct aead_aes_gcm_siv_asm_ctx expanded_key;
  if (gcm_siv_ctx->is_128_bit) {
    aes128gcmsiv_aes_ks((const uint8_t *)record_enc_key, &expanded_key.key[0]);
  } else {
    aes256gcmsiv_aes_ks((const uint8_t *)record_enc_key, &expanded_key.key[0]);
  }

  alignas(16) uint8_t calculated_tag[16 * 8] = {0};

  OPENSSL_memset(calculated_tag, 0, EVP_AEAD_AES_GCM_SIV_TAG_LEN);
  const size_t ad_blocks = ad_len / 16;
  aesgcmsiv_polyval_horner(calculated_tag, (const uint8_t *)record_auth_key, ad,
                           ad_blocks);

  uint8_t scratch[16];
  if (ad_len & 15) {
    OPENSSL_memset(scratch, 0, sizeof(scratch));
    OPENSSL_memcpy(scratch, &ad[ad_len & ~15], ad_len & 15);
    aesgcmsiv_polyval_horner(calculated_tag, (const uint8_t *)record_auth_key,
                             scratch, 1);
  }

  alignas(16) uint8_t htable[16 * 6];
  aesgcmsiv_htable6_init(htable, (const uint8_t *)record_auth_key);

  memcpy(calculated_tag + 16, in_tag, EVP_AEAD_AES_GCM_SIV_TAG_LEN);
  if (gcm_siv_ctx->is_128_bit) {
    aes128gcmsiv_dec(in, out, calculated_tag, htable, &expanded_key, in_len);
  } else {
    aes256gcmsiv_dec(in, out, calculated_tag, htable, &expanded_key, in_len);
  }

  if (in_len & 15) {
    aead_aes_gcm_siv_asm_crypt_last_block(gcm_siv_ctx->is_128_bit, out, in,
                                          in_len, in_tag, &expanded_key);
    OPENSSL_memset(scratch, 0, sizeof(scratch));
    OPENSSL_memcpy(scratch, out + (in_len & ~15), in_len & 15);
    aesgcmsiv_polyval_horner(calculated_tag, (const uint8_t *)record_auth_key,
                             scratch, 1);
  }

  uint8_t length_block[16];
  CRYPTO_store_u64_le(length_block, ad_len * 8);
  CRYPTO_store_u64_le(length_block + 8, in_len * 8);
  aesgcmsiv_polyval_horner(calculated_tag, (const uint8_t *)record_auth_key,
                           length_block, 1);

  for (size_t i = 0; i < 12; i++) {
    calculated_tag[i] ^= nonce[i];
  }

  calculated_tag[15] &= 0x7f;

  if (gcm_siv_ctx->is_128_bit) {
    aes128gcmsiv_ecb_enc_block(calculated_tag, calculated_tag, &expanded_key);
  } else {
    aes256gcmsiv_ecb_enc_block(calculated_tag, calculated_tag, &expanded_key);
  }

  if (CRYPTO_memcmp(calculated_tag, in_tag, EVP_AEAD_AES_GCM_SIV_TAG_LEN) !=
      0) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  return 1;
}

const EVP_AEAD aead_aes_128_gcm_siv_asm = {
    16,
    EVP_AEAD_AES_GCM_SIV_NONCE_LEN,
    EVP_AEAD_AES_GCM_SIV_TAG_LEN,
    EVP_AEAD_AES_GCM_SIV_TAG_LEN,
    0,

    aead_aes_gcm_siv_asm_init,
    NULL                          ,
    aead_aes_gcm_siv_asm_cleanup,
    NULL           ,
    aead_aes_gcm_siv_asm_seal_scatter,
    aead_aes_gcm_siv_asm_open_gather,
    NULL             ,
    NULL              ,
};

const EVP_AEAD aead_aes_256_gcm_siv_asm = {
    32,
    EVP_AEAD_AES_GCM_SIV_NONCE_LEN,
    EVP_AEAD_AES_GCM_SIV_TAG_LEN,
    EVP_AEAD_AES_GCM_SIV_TAG_LEN,
    0,

    aead_aes_gcm_siv_asm_init,
    NULL                          ,
    aead_aes_gcm_siv_asm_cleanup,
    NULL           ,
    aead_aes_gcm_siv_asm_seal_scatter,
    aead_aes_gcm_siv_asm_open_gather,
    NULL             ,
    NULL              ,
};

#endif

struct aead_aes_gcm_siv_ctx {
  union {
    double align;
    AES_KEY ks;
  } ks;
  block128_f kgk_block;
  unsigned is_256 : 1;
};

static_assert(sizeof(((EVP_AEAD_CTX *)NULL)->state) >=
                  sizeof(struct aead_aes_gcm_siv_ctx),
              "AEAD state is too small");
static_assert(alignof(union evp_aead_ctx_st_state) >=
                  alignof(struct aead_aes_gcm_siv_ctx),
              "AEAD state has insufficient alignment");

int aead_aes_gcm_siv_init(EVP_AEAD_CTX *ctx, const uint8_t *key, size_t key_len,
                          size_t tag_len) {
  const size_t key_bits = key_len * 8;

  if (key_bits != 128 && key_bits != 256) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_KEY_LENGTH);
    return 0;
  }

  if (tag_len == EVP_AEAD_DEFAULT_TAG_LENGTH) {
    tag_len = EVP_AEAD_AES_GCM_SIV_TAG_LEN;
  }
  if (tag_len != EVP_AEAD_AES_GCM_SIV_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TAG_TOO_LARGE);
    return 0;
  }

  struct aead_aes_gcm_siv_ctx *gcm_siv_ctx =
      (struct aead_aes_gcm_siv_ctx *)&ctx->state;
  OPENSSL_memset(gcm_siv_ctx, 0, sizeof(struct aead_aes_gcm_siv_ctx));

  aes_ctr_set_key(&gcm_siv_ctx->ks.ks, NULL, &gcm_siv_ctx->kgk_block, key,
                  key_len);
  gcm_siv_ctx->is_256 = (key_len == 32);
  ctx->tag_len = tag_len;

  return 1;
}

void aead_aes_gcm_siv_cleanup(EVP_AEAD_CTX *ctx) {}

void gcm_siv_crypt(uint8_t *out, const uint8_t *in, size_t in_len,
                   const uint8_t initial_counter[AES_BLOCK_SIZE],
                   block128_f enc_block, const AES_KEY *key) {
  uint8_t counter[16];

  OPENSSL_memcpy(counter, initial_counter, AES_BLOCK_SIZE);
  counter[15] |= 0x80;

  for (size_t done = 0; done < in_len;) {
    uint8_t keystream[AES_BLOCK_SIZE];
    enc_block(counter, keystream, key);
    CRYPTO_store_u32_le(counter, CRYPTO_load_u32_le(counter) + 1);

    size_t todo = AES_BLOCK_SIZE;
    if (in_len - done < todo) {
      todo = in_len - done;
    }

    for (size_t i = 0; i < todo; i++) {
      out[done + i] = keystream[i] ^ in[done + i];
    }

    done += todo;
  }
}

struct polyval_ctx {
  uint8_t S[16];
  u128 Htable[16];
  gmult_func gmult;
  ghash_func ghash;
};

void byte_reverse(uint8_t b[16]) {
  uint64_t hi = CRYPTO_load_u64_le(b);
  uint64_t lo = CRYPTO_load_u64_le(b + 8);
  CRYPTO_store_u64_le(b, CRYPTO_bswap8(lo));
  CRYPTO_store_u64_le(b + 8, CRYPTO_bswap8(hi));
}

void reverse_and_mulX_ghash(uint8_t b[16]) {
  uint64_t hi = CRYPTO_load_u64_le(b);
  uint64_t lo = CRYPTO_load_u64_le(b + 8);
  const crypto_word_t carry = constant_time_eq_w(hi & 1, 1);
  hi >>= 1;
  hi |= lo << 63;
  lo >>= 1;
  lo ^= ((uint64_t)constant_time_select_w(carry, 0xe1, 0)) << 56;

  CRYPTO_store_u64_le(b, CRYPTO_bswap8(lo));
  CRYPTO_store_u64_le(b + 8, CRYPTO_bswap8(hi));
}

void crypto_polyval_init(struct polyval_ctx *ctx, const uint8_t key[16]) {
  alignas(8) uint8_t H[16];
  OPENSSL_memcpy(H, key, 16);
  reverse_and_mulX_ghash(H);

  CRYPTO_ghash_init(&ctx->gmult, &ctx->ghash, ctx->Htable, H);
  OPENSSL_memset(&ctx->S, 0, sizeof(ctx->S));
}

void crypto_polyval_update_blocks(struct polyval_ctx *ctx, const uint8_t *in,
                                  size_t in_len) {
  assert((in_len & 15) == 0);
  alignas(8) uint8_t buf[32 * 16];

  while (in_len > 0) {
    size_t todo = in_len;
    if (todo > sizeof(buf)) {
      todo = sizeof(buf);
    }
    OPENSSL_memcpy(buf, in, todo);
    in += todo;
    in_len -= todo;

    size_t blocks = todo / 16;
    for (size_t i = 0; i < blocks; i++) {
      byte_reverse(buf + 16 * i);
    }

    ctx->ghash(ctx->S, ctx->Htable, buf, todo);
  }
}

void crypto_polyval_finish(const struct polyval_ctx *ctx, uint8_t out[16]) {
  OPENSSL_memcpy(out, &ctx->S, 16);
  byte_reverse(out);
}

void gcm_siv_polyval(uint8_t out_tag[16], const uint8_t *in, size_t in_len,
                     const uint8_t *ad, size_t ad_len,
                     const uint8_t auth_key[16],
                     const uint8_t nonce[EVP_AEAD_AES_GCM_SIV_NONCE_LEN]) {
  struct polyval_ctx polyval_ctx;
  crypto_polyval_init(&polyval_ctx, auth_key);

  crypto_polyval_update_blocks(&polyval_ctx, ad, ad_len & ~15);

  uint8_t scratch[16];
  if (ad_len & 15) {
    OPENSSL_memset(scratch, 0, sizeof(scratch));
    OPENSSL_memcpy(scratch, &ad[ad_len & ~15], ad_len & 15);
    crypto_polyval_update_blocks(&polyval_ctx, scratch, sizeof(scratch));
  }

  crypto_polyval_update_blocks(&polyval_ctx, in, in_len & ~15);
  if (in_len & 15) {
    OPENSSL_memset(scratch, 0, sizeof(scratch));
    OPENSSL_memcpy(scratch, &in[in_len & ~15], in_len & 15);
    crypto_polyval_update_blocks(&polyval_ctx, scratch, sizeof(scratch));
  }

  uint8_t length_block[16];
  CRYPTO_store_u64_le(length_block, ((uint64_t)ad_len) * 8);
  CRYPTO_store_u64_le(length_block + 8, ((uint64_t)in_len) * 8);
  crypto_polyval_update_blocks(&polyval_ctx, length_block,
                               sizeof(length_block));

  crypto_polyval_finish(&polyval_ctx, out_tag);
  for (size_t i = 0; i < EVP_AEAD_AES_GCM_SIV_NONCE_LEN; i++) {
    out_tag[i] ^= nonce[i];
  }
  out_tag[15] &= 0x7f;
}

struct gcm_siv_record_keys {
  uint8_t auth_key[16];
  union {
    double align;
    AES_KEY ks;
  } enc_key;
  block128_f enc_block;
};

void gcm_siv_keys(const struct aead_aes_gcm_siv_ctx *gcm_siv_ctx,
                  struct gcm_siv_record_keys *out_keys,
                  const uint8_t nonce[EVP_AEAD_AES_GCM_SIV_NONCE_LEN]) {
  const AES_KEY *const key = &gcm_siv_ctx->ks.ks;
  uint8_t key_material[(128                   + 256                  ) / 8];
  const size_t blocks_needed = gcm_siv_ctx->is_256 ? 6 : 4;

  uint8_t counter[AES_BLOCK_SIZE];
  OPENSSL_memset(counter, 0, AES_BLOCK_SIZE - EVP_AEAD_AES_GCM_SIV_NONCE_LEN);
  OPENSSL_memcpy(counter + AES_BLOCK_SIZE - EVP_AEAD_AES_GCM_SIV_NONCE_LEN,
                 nonce, EVP_AEAD_AES_GCM_SIV_NONCE_LEN);
  for (size_t i = 0; i < blocks_needed; i++) {
    counter[0] = i;

    uint8_t ciphertext[AES_BLOCK_SIZE];
    gcm_siv_ctx->kgk_block(counter, ciphertext, key);
    OPENSSL_memcpy(&key_material[i * 8], ciphertext, 8);
  }

  OPENSSL_memcpy(out_keys->auth_key, key_material, 16);

  aes_ctr_set_key(&out_keys->enc_key.ks, NULL, &out_keys->enc_block,
                  key_material + 16, gcm_siv_ctx->is_256 ? 32 : 16);
}

int aead_aes_gcm_siv_seal_scatter(const EVP_AEAD_CTX *ctx, uint8_t *out,
                                  uint8_t *out_tag, size_t *out_tag_len,
                                  size_t max_out_tag_len, const uint8_t *nonce,
                                  size_t nonce_len, const uint8_t *in,
                                  size_t in_len, const uint8_t *extra_in,
                                  size_t extra_in_len, const uint8_t *ad,
                                  size_t ad_len) {
  const struct aead_aes_gcm_siv_ctx *gcm_siv_ctx =
      (struct aead_aes_gcm_siv_ctx *)&ctx->state;
  const uint64_t in_len_64 = in_len;
  const uint64_t ad_len_64 = ad_len;

  if (in_len + EVP_AEAD_AES_GCM_SIV_TAG_LEN < in_len ||
      in_len_64 > (UINT64_C(1) << 36) || ad_len_64 >= (UINT64_C(1) << 61)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  if (max_out_tag_len < EVP_AEAD_AES_GCM_SIV_TAG_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BUFFER_TOO_SMALL);
    return 0;
  }

  if (nonce_len != EVP_AEAD_AES_GCM_SIV_NONCE_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  struct gcm_siv_record_keys keys;
  gcm_siv_keys(gcm_siv_ctx, &keys, nonce);

  uint8_t tag[16];
  gcm_siv_polyval(tag, in, in_len, ad, ad_len, keys.auth_key, nonce);
  keys.enc_block(tag, tag, &keys.enc_key.ks);

  gcm_siv_crypt(out, in, in_len, tag, keys.enc_block, &keys.enc_key.ks);

  OPENSSL_memcpy(out_tag, tag, EVP_AEAD_AES_GCM_SIV_TAG_LEN);
  *out_tag_len = EVP_AEAD_AES_GCM_SIV_TAG_LEN;

  return 1;
}

int aead_aes_gcm_siv_open_gather(const EVP_AEAD_CTX *ctx, uint8_t *out,
                                 const uint8_t *nonce, size_t nonce_len,
                                 const uint8_t *in, size_t in_len,
                                 const uint8_t *in_tag, size_t in_tag_len,
                                 const uint8_t *ad, size_t ad_len) {
  const uint64_t ad_len_64 = ad_len;
  if (ad_len_64 >= (UINT64_C(1) << 61)) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_TOO_LARGE);
    return 0;
  }

  const uint64_t in_len_64 = in_len;
  if (in_tag_len != EVP_AEAD_AES_GCM_SIV_TAG_LEN ||
      in_len_64 > (UINT64_C(1) << 36) + AES_BLOCK_SIZE) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  if (nonce_len != EVP_AEAD_AES_GCM_SIV_NONCE_LEN) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_UNSUPPORTED_NONCE_SIZE);
    return 0;
  }

  const struct aead_aes_gcm_siv_ctx *gcm_siv_ctx =
      (struct aead_aes_gcm_siv_ctx *)&ctx->state;

  struct gcm_siv_record_keys keys;
  gcm_siv_keys(gcm_siv_ctx, &keys, nonce);

  gcm_siv_crypt(out, in, in_len, in_tag, keys.enc_block, &keys.enc_key.ks);

  uint8_t expected_tag[EVP_AEAD_AES_GCM_SIV_TAG_LEN];
  gcm_siv_polyval(expected_tag, out, in_len, ad, ad_len, keys.auth_key, nonce);
  keys.enc_block(expected_tag, expected_tag, &keys.enc_key.ks);

  if (CRYPTO_memcmp(expected_tag, in_tag, sizeof(expected_tag)) != 0) {
    OPENSSL_PUT_ERROR(CIPHER, CIPHER_R_BAD_DECRYPT);
    return 0;
  }

  return 1;
}

const EVP_AEAD aead_aes_128_gcm_siv = {
    16,
    EVP_AEAD_AES_GCM_SIV_NONCE_LEN,
    EVP_AEAD_AES_GCM_SIV_TAG_LEN,
    EVP_AEAD_AES_GCM_SIV_TAG_LEN,
    0,

    aead_aes_gcm_siv_init,
    NULL                          ,
    aead_aes_gcm_siv_cleanup,
    NULL           ,
    aead_aes_gcm_siv_seal_scatter,
    aead_aes_gcm_siv_open_gather,
    NULL             ,
    NULL              ,
};

const EVP_AEAD aead_aes_256_gcm_siv = {
    32,
    EVP_AEAD_AES_GCM_SIV_NONCE_LEN,
    EVP_AEAD_AES_GCM_SIV_TAG_LEN,
    EVP_AEAD_AES_GCM_SIV_TAG_LEN,
    0,

    aead_aes_gcm_siv_init,
    NULL                          ,
    aead_aes_gcm_siv_cleanup,
    NULL           ,
    aead_aes_gcm_siv_seal_scatter,
    aead_aes_gcm_siv_open_gather,
    NULL             ,
    NULL              ,
};
}

#if defined(AES_GCM_SIV_ASM)

const EVP_AEAD *EVP_aead_aes_128_gcm_siv(void) {
  if (CRYPTO_is_AVX_capable() && CRYPTO_is_AESNI_capable()) {
    return &aead_aes_128_gcm_siv_asm;
  }
  return &aead_aes_128_gcm_siv;
}

const EVP_AEAD *EVP_aead_aes_256_gcm_siv(void) {
  if (CRYPTO_is_AVX_capable() && CRYPTO_is_AESNI_capable()) {
    return &aead_aes_256_gcm_siv_asm;
  }
  return &aead_aes_256_gcm_siv;
}

#else

const EVP_AEAD *EVP_aead_aes_128_gcm_siv(void) { return &aead_aes_128_gcm_siv; }

const EVP_AEAD *EVP_aead_aes_256_gcm_siv(void) { return &aead_aes_256_gcm_siv; }

#endif
