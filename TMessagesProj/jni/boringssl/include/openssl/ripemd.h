// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_RIPEMD_H
#define OPENSSL_HEADER_RIPEMD_H

#include <openssl/base.h>

#ifdef  __cplusplus
extern "C" {
#endif

# define RIPEMD160_CBLOCK        64
# define RIPEMD160_LBLOCK        (RIPEMD160_CBLOCK/4)
# define RIPEMD160_DIGEST_LENGTH 20

struct RIPEMD160state_st {
  uint32_t h[5];
  uint32_t Nl, Nh;
  uint8_t data[RIPEMD160_CBLOCK];
  unsigned num;
};

OPENSSL_EXPORT int RIPEMD160_Init(RIPEMD160_CTX *ctx);

OPENSSL_EXPORT int RIPEMD160_Update(RIPEMD160_CTX *ctx, const void *data,
                                   size_t len);

OPENSSL_EXPORT int RIPEMD160_Final(uint8_t out[RIPEMD160_DIGEST_LENGTH],
                                   RIPEMD160_CTX *ctx);

OPENSSL_EXPORT uint8_t *RIPEMD160(const uint8_t *data, size_t len,
                                  uint8_t out[RIPEMD160_DIGEST_LENGTH]);

OPENSSL_EXPORT void RIPEMD160_Transform(RIPEMD160_CTX *ctx,
                                        const uint8_t block[RIPEMD160_CBLOCK]);

#if defined(__cplusplus)
}
#endif

#endif
