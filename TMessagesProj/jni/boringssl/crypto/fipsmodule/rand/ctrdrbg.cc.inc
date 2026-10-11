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

#include <openssl/ctrdrbg.h>

#include <assert.h>

#include <openssl/mem.h>

#include "../aes/internal.h"
#include "../service_indicator/internal.h"
#include "internal.h"

static const uint64_t kMaxReseedCount = UINT64_C(1) << 48;

CTR_DRBG_STATE *CTR_DRBG_new(const uint8_t entropy[CTR_DRBG_ENTROPY_LEN],
                             const uint8_t *personalization,
                             size_t personalization_len) {
  CTR_DRBG_STATE *drbg = reinterpret_cast<CTR_DRBG_STATE *>(
      OPENSSL_malloc(sizeof(CTR_DRBG_STATE)));
  if (drbg == NULL ||
      !CTR_DRBG_init(drbg, entropy, personalization, personalization_len)) {
    CTR_DRBG_free(drbg);
    return NULL;
  }

  return drbg;
}

void CTR_DRBG_free(CTR_DRBG_STATE *state) { OPENSSL_free(state); }

int CTR_DRBG_init(CTR_DRBG_STATE *drbg,
                  const uint8_t entropy[CTR_DRBG_ENTROPY_LEN],
                  const uint8_t *personalization, size_t personalization_len) {

  if (personalization_len > CTR_DRBG_ENTROPY_LEN) {
    return 0;
  }

  uint8_t seed_material[CTR_DRBG_ENTROPY_LEN];
  OPENSSL_memcpy(seed_material, entropy, CTR_DRBG_ENTROPY_LEN);

  for (size_t i = 0; i < personalization_len; i++) {
    seed_material[i] ^= personalization[i];
  }

  static const uint8_t kInitMask[CTR_DRBG_ENTROPY_LEN] = {
      0x53, 0x0f, 0x8a, 0xfb, 0xc7, 0x45, 0x36, 0xb9, 0xa9, 0x63, 0xb4, 0xf1,
      0xc4, 0xcb, 0x73, 0x8b, 0xce, 0xa7, 0x40, 0x3d, 0x4d, 0x60, 0x6b, 0x6e,
      0x07, 0x4e, 0xc5, 0xd3, 0xba, 0xf3, 0x9d, 0x18, 0x72, 0x60, 0x03, 0xca,
      0x37, 0xa6, 0x2a, 0x74, 0xd1, 0xa2, 0xf5, 0x8e, 0x75, 0x06, 0x35, 0x8e,
  };

  for (size_t i = 0; i < sizeof(kInitMask); i++) {
    seed_material[i] ^= kInitMask[i];
  }

  drbg->ctr = aes_ctr_set_key(&drbg->ks, NULL, &drbg->block, seed_material, 32);
  OPENSSL_memcpy(drbg->counter, seed_material + 32, 16);
  drbg->reseed_counter = 1;

  return 1;
}

static_assert(CTR_DRBG_ENTROPY_LEN % AES_BLOCK_SIZE == 0,
              "not a multiple of AES block size");

static void ctr32_add(CTR_DRBG_STATE *drbg, uint32_t n) {
  uint32_t ctr = CRYPTO_load_u32_be(drbg->counter + 12);
  CRYPTO_store_u32_be(drbg->counter + 12, ctr + n);
}

static int ctr_drbg_update(CTR_DRBG_STATE *drbg, const uint8_t *data,
                           size_t data_len) {

  if (data_len > CTR_DRBG_ENTROPY_LEN) {
    return 0;
  }

  uint8_t temp[CTR_DRBG_ENTROPY_LEN];
  for (size_t i = 0; i < CTR_DRBG_ENTROPY_LEN; i += AES_BLOCK_SIZE) {
    ctr32_add(drbg, 1);
    drbg->block(drbg->counter, temp + i, &drbg->ks);
  }

  for (size_t i = 0; i < data_len; i++) {
    temp[i] ^= data[i];
  }

  drbg->ctr = aes_ctr_set_key(&drbg->ks, NULL, &drbg->block, temp, 32);
  OPENSSL_memcpy(drbg->counter, temp + 32, 16);

  return 1;
}

int CTR_DRBG_reseed(CTR_DRBG_STATE *drbg,
                    const uint8_t entropy[CTR_DRBG_ENTROPY_LEN],
                    const uint8_t *additional_data,
                    size_t additional_data_len) {

  uint8_t entropy_copy[CTR_DRBG_ENTROPY_LEN];

  if (additional_data_len > 0) {
    if (additional_data_len > CTR_DRBG_ENTROPY_LEN) {
      return 0;
    }

    OPENSSL_memcpy(entropy_copy, entropy, CTR_DRBG_ENTROPY_LEN);
    for (size_t i = 0; i < additional_data_len; i++) {
      entropy_copy[i] ^= additional_data[i];
    }

    entropy = entropy_copy;
  }

  if (!ctr_drbg_update(drbg, entropy, CTR_DRBG_ENTROPY_LEN)) {
    return 0;
  }

  drbg->reseed_counter = 1;

  return 1;
}

int CTR_DRBG_generate(CTR_DRBG_STATE *drbg, uint8_t *out, size_t out_len,
                      const uint8_t *additional_data,
                      size_t additional_data_len) {

  if (out_len > CTR_DRBG_MAX_GENERATE_LENGTH) {
    return 0;
  }

  if (drbg->reseed_counter > kMaxReseedCount) {
    return 0;
  }

  if (additional_data_len != 0 &&
      !ctr_drbg_update(drbg, additional_data, additional_data_len)) {
    return 0;
  }

  static const size_t kChunkSize = 8 * 1024;

  while (out_len >= AES_BLOCK_SIZE) {
    size_t todo = kChunkSize;
    if (todo > out_len) {
      todo = out_len;
    }

    todo &= ~(AES_BLOCK_SIZE - 1);
    const size_t num_blocks = todo / AES_BLOCK_SIZE;

    OPENSSL_memset(out, 0, todo);
    ctr32_add(drbg, 1);
    drbg->ctr(out, out, num_blocks, &drbg->ks, drbg->counter);
    ctr32_add(drbg, (uint32_t)(num_blocks - 1));

    out += todo;
    out_len -= todo;
  }

  if (out_len > 0) {
    uint8_t block[AES_BLOCK_SIZE];
    ctr32_add(drbg, 1);
    drbg->block(drbg->counter, block, &drbg->ks);

    OPENSSL_memcpy(out, block, out_len);
  }

  if (!ctr_drbg_update(drbg, additional_data, additional_data_len)) {
    return 0;
  }

  drbg->reseed_counter++;
  FIPS_service_indicator_update_state();
  return 1;
}

void CTR_DRBG_clear(CTR_DRBG_STATE *drbg) {
  OPENSSL_cleanse(drbg, sizeof(CTR_DRBG_STATE));
}
