// Copyright 2012-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#include <assert.h>
#include <string.h>

#include <openssl/digest.h>
#include <openssl/nid.h>
#include <openssl/sha.h>

#include "../internal.h"
#include "internal.h"
#include "../fipsmodule/cipher/internal.h"

int EVP_tls_cbc_remove_padding(crypto_word_t *out_padding_ok, size_t *out_len,
                               const uint8_t *in, size_t in_len,
                               size_t block_size, size_t mac_size) {
  const size_t overhead = 1                           + mac_size;

  if (overhead > in_len) {
    return 0;
  }

  size_t padding_length = in[in_len - 1];

  crypto_word_t good = constant_time_ge_w(in_len, overhead + padding_length);

  size_t to_check = 256;
  if (to_check > in_len) {
    to_check = in_len;
  }

  for (size_t i = 0; i < to_check; i++) {
    uint8_t mask = constant_time_ge_8(padding_length, i);
    uint8_t b = in[in_len - 1 - i];

    good &= ~(mask & (padding_length ^ b));
  }

  good = constant_time_eq_w(0xff, good & 0xff);

  padding_length = good & (padding_length + 1);
  *out_len = in_len - padding_length;
  *out_padding_ok = good;
  return 1;
}

void EVP_tls_cbc_copy_mac(uint8_t *out, size_t md_size, const uint8_t *in,
                          size_t in_len, size_t orig_len) {
  uint8_t rotated_mac1[EVP_MAX_MD_SIZE], rotated_mac2[EVP_MAX_MD_SIZE];
  uint8_t *rotated_mac = rotated_mac1;
  uint8_t *rotated_mac_tmp = rotated_mac2;

  size_t mac_end = in_len;
  size_t mac_start = mac_end - md_size;

  declassify_assert(orig_len >= in_len);
  declassify_assert(in_len >= md_size);
  assert(md_size <= EVP_MAX_MD_SIZE);
  assert(md_size > 0);

  size_t scan_start = 0;

  if (orig_len > md_size + 255 + 1) {
    scan_start = orig_len - (md_size + 255 + 1);
  }

  size_t rotate_offset = 0;
  uint8_t mac_started = 0;
  OPENSSL_memset(rotated_mac, 0, md_size);
  for (size_t i = scan_start, j = 0; i < orig_len; i++, j++) {
    if (j >= md_size) {
      j -= md_size;
    }
    crypto_word_t is_mac_start = constant_time_eq_w(i, mac_start);
    mac_started |= is_mac_start;
    uint8_t mac_ended = constant_time_ge_8(i, mac_end);
    rotated_mac[j] |= in[i] & mac_started & ~mac_ended;

    rotate_offset |= j & is_mac_start;
  }

  for (size_t offset = 1; offset < md_size; offset <<= 1, rotate_offset >>= 1) {

    const uint8_t skip_rotate = (rotate_offset & 1) - 1;
    for (size_t i = 0, j = offset; i < md_size; i++, j++) {
      if (j >= md_size) {
        j -= md_size;
      }
      rotated_mac_tmp[i] =
          constant_time_select_8(skip_rotate, rotated_mac[i], rotated_mac[j]);
    }

    uint8_t *tmp = rotated_mac;
    rotated_mac = rotated_mac_tmp;
    rotated_mac_tmp = tmp;
  }

  OPENSSL_memcpy(out, rotated_mac, md_size);
}

int EVP_sha1_final_with_secret_suffix(SHA_CTX *ctx,
                                      uint8_t out[SHA_DIGEST_LENGTH],
                                      const uint8_t *in, size_t len,
                                      size_t max_len) {

  size_t max_len_bits = max_len << 3;
  if (ctx->Nh != 0 ||
      (max_len_bits >> 3) != max_len ||
      ctx->Nl + max_len_bits < max_len_bits ||
      ctx->Nl + max_len_bits > UINT32_MAX) {
    return 0;
  }

  size_t num_blocks = (ctx->num + len + 1 + 8 + SHA_CBLOCK - 1) >> 6;
  size_t last_block = num_blocks - 1;
  size_t max_blocks = (ctx->num + max_len + 1 + 8 + SHA_CBLOCK - 1) >> 6;

  size_t total_bits = ctx->Nl + (len << 3);
  uint8_t length_bytes[4];
  length_bytes[0] = (uint8_t)(total_bits >> 24);
  length_bytes[1] = (uint8_t)(total_bits >> 16);
  length_bytes[2] = (uint8_t)(total_bits >> 8);
  length_bytes[3] = (uint8_t)total_bits;

  uint8_t block[SHA_CBLOCK] = {0};
  uint32_t result[5] = {0};

  size_t input_idx = 0;
  for (size_t i = 0; i < max_blocks; i++) {

    size_t block_start = 0;
    if (i == 0) {
      OPENSSL_memcpy(block, ctx->data, ctx->num);
      block_start = ctx->num;
    }
    if (input_idx < max_len) {
      size_t to_copy = SHA_CBLOCK - block_start;
      if (to_copy > max_len - input_idx) {
        to_copy = max_len - input_idx;
      }
      OPENSSL_memcpy(block + block_start, in + input_idx, to_copy);
    }

    for (size_t j = block_start; j < SHA_CBLOCK; j++) {

      size_t idx = input_idx + j - block_start;

      uint8_t is_in_bounds = constant_time_lt_8(idx, value_barrier_w(len));
      uint8_t is_padding_byte = constant_time_eq_8(idx, value_barrier_w(len));
      block[j] &= is_in_bounds;
      block[j] |= 0x80 & is_padding_byte;
    }

    input_idx += SHA_CBLOCK - block_start;

    crypto_word_t is_last_block = constant_time_eq_w(i, last_block);
    for (size_t j = 0; j < 4; j++) {
      block[SHA_CBLOCK - 4 + j] |= is_last_block & length_bytes[j];
    }

    SHA1_Transform(ctx, block);
    for (size_t j = 0; j < 5; j++) {
      result[j] |= is_last_block & ctx->h[j];
    }
  }

  for (size_t i = 0; i < 5; i++) {
    CRYPTO_store_u32_be(out + 4 * i, result[i]);
  }
  return 1;
}

int EVP_sha256_final_with_secret_suffix(SHA256_CTX *ctx,
                                        uint8_t out[SHA256_DIGEST_LENGTH],
                                        const uint8_t *in, size_t len,
                                        size_t max_len) {

  size_t max_len_bits = max_len << 3;
  if (ctx->Nh != 0 ||
      (max_len_bits >> 3) != max_len ||
      ctx->Nl + max_len_bits < max_len_bits ||
      ctx->Nl + max_len_bits > UINT32_MAX) {
    return 0;
  }

  size_t num_blocks = (ctx->num + len + 1 + 8 + SHA256_CBLOCK - 1) >> 6;
  size_t last_block = num_blocks - 1;
  size_t max_blocks = (ctx->num + max_len + 1 + 8 + SHA256_CBLOCK - 1) >> 6;

  size_t total_bits = ctx->Nl + (len << 3);
  uint8_t length_bytes[4];
  length_bytes[0] = (uint8_t)(total_bits >> 24);
  length_bytes[1] = (uint8_t)(total_bits >> 16);
  length_bytes[2] = (uint8_t)(total_bits >> 8);
  length_bytes[3] = (uint8_t)total_bits;

  uint8_t block[SHA256_CBLOCK] = {0};
  uint32_t result[8] = {0};

  size_t input_idx = 0;
  for (size_t i = 0; i < max_blocks; i++) {

    size_t block_start = 0;
    if (i == 0) {
      OPENSSL_memcpy(block, ctx->data, ctx->num);
      block_start = ctx->num;
    }
    if (input_idx < max_len) {
      size_t to_copy = SHA256_CBLOCK - block_start;
      if (to_copy > max_len - input_idx) {
        to_copy = max_len - input_idx;
      }
      OPENSSL_memcpy(block + block_start, in + input_idx, to_copy);
    }

    for (size_t j = block_start; j < SHA256_CBLOCK; j++) {

      size_t idx = input_idx + j - block_start;

      uint8_t is_in_bounds = constant_time_lt_8(idx, value_barrier_w(len));
      uint8_t is_padding_byte = constant_time_eq_8(idx, value_barrier_w(len));
      block[j] &= is_in_bounds;
      block[j] |= 0x80 & is_padding_byte;
    }

    input_idx += SHA256_CBLOCK - block_start;

    crypto_word_t is_last_block = constant_time_eq_w(i, last_block);
    for (size_t j = 0; j < 4; j++) {
      block[SHA256_CBLOCK - 4 + j] |= is_last_block & length_bytes[j];
    }

    SHA256_Transform(ctx, block);
    for (size_t j = 0; j < 8; j++) {
      result[j] |= is_last_block & ctx->h[j];
    }
  }

  for (size_t i = 0; i < 8; i++) {
    CRYPTO_store_u32_be(out + 4 * i, result[i]);
  }
  return 1;
}

int EVP_tls_cbc_record_digest_supported(const EVP_MD *md) {
  switch (EVP_MD_type(md)) {
    case NID_sha1:
    case NID_sha256:
      return 1;
    default:
      return 0;
  }
}

static int tls_cbc_digest_record_sha1(uint8_t *md_out, size_t *md_out_size,
                                      const uint8_t header[13],
                                      const uint8_t *data, size_t data_size,
                                      size_t data_plus_mac_plus_padding_size,
                                      const uint8_t *mac_secret,
                                      unsigned mac_secret_length) {
  if (mac_secret_length > SHA_CBLOCK) {

    assert(0);
    return 0;
  }

  uint8_t hmac_pad[SHA_CBLOCK];
  OPENSSL_memset(hmac_pad, 0, sizeof(hmac_pad));
  OPENSSL_memcpy(hmac_pad, mac_secret, mac_secret_length);
  for (size_t i = 0; i < SHA_CBLOCK; i++) {
    hmac_pad[i] ^= 0x36;
  }

  SHA_CTX ctx;
  SHA1_Init(&ctx);
  SHA1_Update(&ctx, hmac_pad, SHA_CBLOCK);
  SHA1_Update(&ctx, header, 13);

  size_t min_data_size = 0;
  if (data_plus_mac_plus_padding_size > SHA_DIGEST_LENGTH + 256) {
    min_data_size = data_plus_mac_plus_padding_size - SHA_DIGEST_LENGTH - 256;
  }

  SHA1_Update(&ctx, data, min_data_size);

  uint8_t mac_out[SHA_DIGEST_LENGTH];
  if (!EVP_sha1_final_with_secret_suffix(
          &ctx, mac_out, data + min_data_size, data_size - min_data_size,
          data_plus_mac_plus_padding_size - min_data_size)) {
    return 0;
  }

  SHA1_Init(&ctx);
  for (size_t i = 0; i < SHA_CBLOCK; i++) {
    hmac_pad[i] ^= 0x6a;
  }

  SHA1_Update(&ctx, hmac_pad, SHA_CBLOCK);
  SHA1_Update(&ctx, mac_out, SHA_DIGEST_LENGTH);
  SHA1_Final(md_out, &ctx);
  *md_out_size = SHA_DIGEST_LENGTH;
  return 1;
}

static int tls_cbc_digest_record_sha256(uint8_t *md_out, size_t *md_out_size,
                                        const uint8_t header[13],
                                        const uint8_t *data, size_t data_size,
                                        size_t data_plus_mac_plus_padding_size,
                                        const uint8_t *mac_secret,
                                        unsigned mac_secret_length) {
  if (mac_secret_length > SHA256_CBLOCK) {

    assert(0);
    return 0;
  }

  uint8_t hmac_pad[SHA256_CBLOCK];
  OPENSSL_memset(hmac_pad, 0, sizeof(hmac_pad));
  OPENSSL_memcpy(hmac_pad, mac_secret, mac_secret_length);
  for (size_t i = 0; i < SHA256_CBLOCK; i++) {
    hmac_pad[i] ^= 0x36;
  }

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, hmac_pad, SHA256_CBLOCK);
  SHA256_Update(&ctx, header, 13);

  size_t min_data_size = 0;
  if (data_plus_mac_plus_padding_size > SHA256_DIGEST_LENGTH + 256) {
    min_data_size =
        data_plus_mac_plus_padding_size - SHA256_DIGEST_LENGTH - 256;
  }

  SHA256_Update(&ctx, data, min_data_size);

  uint8_t mac_out[SHA256_DIGEST_LENGTH];
  if (!EVP_sha256_final_with_secret_suffix(
          &ctx, mac_out, data + min_data_size, data_size - min_data_size,
          data_plus_mac_plus_padding_size - min_data_size)) {
    return 0;
  }

  SHA256_Init(&ctx);
  for (size_t i = 0; i < SHA256_CBLOCK; i++) {
    hmac_pad[i] ^= 0x6a;
  }

  SHA256_Update(&ctx, hmac_pad, SHA256_CBLOCK);
  SHA256_Update(&ctx, mac_out, SHA256_DIGEST_LENGTH);
  SHA256_Final(md_out, &ctx);
  *md_out_size = SHA256_DIGEST_LENGTH;
  return 1;
}

int EVP_tls_cbc_digest_record(const EVP_MD *md, uint8_t *md_out,
                              size_t *md_out_size, const uint8_t header[13],
                              const uint8_t *data, size_t data_size,
                              size_t data_plus_mac_plus_padding_size,
                              const uint8_t *mac_secret,
                              unsigned mac_secret_length) {
  switch (EVP_MD_type(md)) {
    case NID_sha1:
      return tls_cbc_digest_record_sha1(
          md_out, md_out_size, header, data, data_size,
          data_plus_mac_plus_padding_size, mac_secret, mac_secret_length);

    case NID_sha256:
      return tls_cbc_digest_record_sha256(
          md_out, md_out_size, header, data, data_size,
          data_plus_mac_plus_padding_size, mac_secret, mac_secret_length);

    default:

      assert(0);
      *md_out_size = 0;
      return 0;
  }
}
