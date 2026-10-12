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

#ifndef OPENSSL_HEADER_MD5_H
#define OPENSSL_HEADER_MD5_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define MD5_CBLOCK 64

#define MD5_DIGEST_LENGTH 16

OPENSSL_EXPORT int MD5_Init(MD5_CTX *md5);

OPENSSL_EXPORT int MD5_Update(MD5_CTX *md5, const void *data, size_t len);

OPENSSL_EXPORT int MD5_Final(uint8_t out[MD5_DIGEST_LENGTH], MD5_CTX *md5);

OPENSSL_EXPORT uint8_t *MD5(const uint8_t *data, size_t len,
                            uint8_t out[MD5_DIGEST_LENGTH]);

OPENSSL_EXPORT void MD5_Transform(MD5_CTX *md5,
                                  const uint8_t block[MD5_CBLOCK]);

struct md5_state_st {
  uint32_t h[4];
  uint32_t Nl, Nh;
  uint8_t data[MD5_CBLOCK];
  unsigned num;
};

#if defined(__cplusplus)
}
#endif

#endif
