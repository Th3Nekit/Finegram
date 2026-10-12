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

#ifndef OPENSSL_HEADER_SHA_H
#define OPENSSL_HEADER_SHA_H

#include <openssl/base.h>
#include <openssl/bcm_public.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define SHA_CBLOCK 64

#define SHA_DIGEST_LENGTH 20

OPENSSL_EXPORT int SHA1_Init(SHA_CTX *sha);

OPENSSL_EXPORT int SHA1_Update(SHA_CTX *sha, const void *data, size_t len);

OPENSSL_EXPORT int SHA1_Final(uint8_t out[SHA_DIGEST_LENGTH], SHA_CTX *sha);

OPENSSL_EXPORT uint8_t *SHA1(const uint8_t *data, size_t len,
                             uint8_t out[SHA_DIGEST_LENGTH]);

OPENSSL_EXPORT void SHA1_Transform(SHA_CTX *sha,
                                   const uint8_t block[SHA_CBLOCK]);

OPENSSL_EXPORT void CRYPTO_fips_186_2_prf(
    uint8_t *out, size_t out_len, const uint8_t xkey[SHA_DIGEST_LENGTH]);

#define SHA224_CBLOCK 64

#define SHA224_DIGEST_LENGTH 28

OPENSSL_EXPORT int SHA224_Init(SHA256_CTX *sha);

OPENSSL_EXPORT int SHA224_Update(SHA256_CTX *sha, const void *data, size_t len);

OPENSSL_EXPORT int SHA224_Final(uint8_t out[SHA224_DIGEST_LENGTH],
                                SHA256_CTX *sha);

OPENSSL_EXPORT uint8_t *SHA224(const uint8_t *data, size_t len,
                               uint8_t out[SHA224_DIGEST_LENGTH]);

#define SHA256_CBLOCK 64

#define SHA256_DIGEST_LENGTH 32

OPENSSL_EXPORT int SHA256_Init(SHA256_CTX *sha);

OPENSSL_EXPORT int SHA256_Update(SHA256_CTX *sha, const void *data, size_t len);

OPENSSL_EXPORT int SHA256_Final(uint8_t out[SHA256_DIGEST_LENGTH],
                                SHA256_CTX *sha);

OPENSSL_EXPORT uint8_t *SHA256(const uint8_t *data, size_t len,
                               uint8_t out[SHA256_DIGEST_LENGTH]);

OPENSSL_EXPORT void SHA256_Transform(SHA256_CTX *sha,
                                     const uint8_t block[SHA256_CBLOCK]);

OPENSSL_EXPORT void SHA256_TransformBlocks(uint32_t state[8],
                                           const uint8_t *data,
                                           size_t num_blocks);

#define SHA384_CBLOCK 128

#define SHA384_DIGEST_LENGTH 48

OPENSSL_EXPORT int SHA384_Init(SHA512_CTX *sha);

OPENSSL_EXPORT int SHA384_Update(SHA512_CTX *sha, const void *data, size_t len);

OPENSSL_EXPORT int SHA384_Final(uint8_t out[SHA384_DIGEST_LENGTH],
                                SHA512_CTX *sha);

OPENSSL_EXPORT uint8_t *SHA384(const uint8_t *data, size_t len,
                               uint8_t out[SHA384_DIGEST_LENGTH]);

#define SHA512_CBLOCK 128

#define SHA512_DIGEST_LENGTH 64

OPENSSL_EXPORT int SHA512_Init(SHA512_CTX *sha);

OPENSSL_EXPORT int SHA512_Update(SHA512_CTX *sha, const void *data, size_t len);

OPENSSL_EXPORT int SHA512_Final(uint8_t out[SHA512_DIGEST_LENGTH],
                                SHA512_CTX *sha);

OPENSSL_EXPORT uint8_t *SHA512(const uint8_t *data, size_t len,
                               uint8_t out[SHA512_DIGEST_LENGTH]);

OPENSSL_EXPORT void SHA512_Transform(SHA512_CTX *sha,
                                     const uint8_t block[SHA512_CBLOCK]);

#define SHA512_256_DIGEST_LENGTH 32

OPENSSL_EXPORT int SHA512_256_Init(SHA512_CTX *sha);

OPENSSL_EXPORT int SHA512_256_Update(SHA512_CTX *sha, const void *data,
                                     size_t len);

OPENSSL_EXPORT int SHA512_256_Final(uint8_t out[SHA512_256_DIGEST_LENGTH],
                                    SHA512_CTX *sha);

OPENSSL_EXPORT uint8_t *SHA512_256(const uint8_t *data, size_t len,
                                   uint8_t out[SHA512_256_DIGEST_LENGTH]);

#if defined(__cplusplus)
}
#endif

#endif
