// Copyright 2024 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_BCM_PUBLIC_H_
#define OPENSSL_HEADER_BCM_PUBLIC_H_

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define BCM_SHA_CBLOCK 64

struct sha_state_st {
#if defined(__cplusplus) || defined(OPENSSL_WINDOWS)
  uint32_t h[5];
#else

  union {
    uint32_t h[5];
    struct {
      uint32_t h0;
      uint32_t h1;
      uint32_t h2;
      uint32_t h3;
      uint32_t h4;
    };
  };
#endif
  uint32_t Nl, Nh;
  uint8_t data[BCM_SHA_CBLOCK];
  unsigned num;
};

#define BCM_SHA256_CBLOCK 64

struct sha256_state_st {
  uint32_t h[8];
  uint32_t Nl, Nh;
  uint8_t data[BCM_SHA256_CBLOCK];
  unsigned num, md_len;
};

#define BCM_SHA512_CBLOCK 128

struct sha512_state_st {
  uint64_t h[8];
  uint64_t Nl, Nh;
  uint8_t p[BCM_SHA512_CBLOCK];
  unsigned num, md_len;
};

#if defined(__cplusplus)
}
#endif

#endif
