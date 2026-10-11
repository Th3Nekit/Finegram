// Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_DIGEST_MD32_COMMON_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_DIGEST_MD32_COMMON_H

#include <openssl/base.h>

#include <assert.h>

#include "../../internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*crypto_md32_block_func)(uint32_t *state, const uint8_t *data,
                                       size_t num_blocks);

static inline void crypto_md32_update(crypto_md32_block_func block_func,
                                      uint32_t *h, uint8_t *data,
                                      size_t block_size, unsigned *num,
                                      uint32_t *Nh, uint32_t *Nl,
                                      const uint8_t *in, size_t len) {
  if (len == 0) {
    return;
  }

  uint32_t l = *Nl + (((uint32_t)len) << 3);
  if (l < *Nl) {

    (*Nh)++;
  }
  *Nh += (uint32_t)(len >> 29);
  *Nl = l;

  size_t n = *num;
  if (n != 0) {
    if (len >= block_size || len + n >= block_size) {
      OPENSSL_memcpy(data + n, in, block_size - n);
      block_func(h, data, 1);
      n = block_size - n;
      in += n;
      len -= n;
      *num = 0;

      OPENSSL_memset(data, 0, block_size);
    } else {
      OPENSSL_memcpy(data + n, in, len);
      *num += (unsigned)len;
      return;
    }
  }

  n = len / block_size;
  if (n > 0) {
    block_func(h, in, n);
    n *= block_size;
    in += n;
    len -= n;
  }

  if (len != 0) {
    *num = (unsigned)len;
    OPENSSL_memcpy(data, in, len);
  }
}

static inline void crypto_md32_final(crypto_md32_block_func block_func,
                                     uint32_t *h, uint8_t *data,
                                     size_t block_size, unsigned *num,
                                     uint32_t Nh, uint32_t Nl,
                                     int is_big_endian) {

  size_t n = *num;
  assert(n < block_size);
  data[n] = 0x80;
  n++;

  if (n > block_size - 8) {
    OPENSSL_memset(data + n, 0, block_size - n);
    n = 0;
    block_func(h, data, 1);
  }
  OPENSSL_memset(data + n, 0, block_size - 8 - n);

  if (is_big_endian) {
    CRYPTO_store_u32_be(data + block_size - 8, Nh);
    CRYPTO_store_u32_be(data + block_size - 4, Nl);
  } else {
    CRYPTO_store_u32_le(data + block_size - 8, Nl);
    CRYPTO_store_u32_le(data + block_size - 4, Nh);
  }
  block_func(h, data, 1);
  *num = 0;
  OPENSSL_memset(data, 0, block_size);
}

#if defined(__cplusplus)
}
#endif

#endif
