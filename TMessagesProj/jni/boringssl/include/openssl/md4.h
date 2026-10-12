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

#ifndef OPENSSL_HEADER_MD4_H
#define OPENSSL_HEADER_MD4_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define MD4_CBLOCK 64

#define MD4_DIGEST_LENGTH 16

OPENSSL_EXPORT int MD4_Init(MD4_CTX *md4);

OPENSSL_EXPORT int MD4_Update(MD4_CTX *md4, const void *data, size_t len);

OPENSSL_EXPORT int MD4_Final(uint8_t out[MD4_DIGEST_LENGTH], MD4_CTX *md4);

OPENSSL_EXPORT uint8_t *MD4(const uint8_t *data, size_t len,
                            uint8_t out[MD4_DIGEST_LENGTH]);

OPENSSL_EXPORT void MD4_Transform(MD4_CTX *md4,
                                  const uint8_t block[MD4_CBLOCK]);

struct md4_state_st {
  uint32_t h[4];
  uint32_t Nl, Nh;
  uint8_t data[MD4_CBLOCK];
  unsigned num;
};

#if defined(__cplusplus)
}
#endif

#endif
