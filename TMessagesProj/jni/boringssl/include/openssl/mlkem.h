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

#ifndef OPENSSL_HEADER_MLKEM_H
#define OPENSSL_HEADER_MLKEM_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct MLKEM768_public_key {
  union {
    uint8_t bytes[512 * (3 + 9) + 32 + 32];
    uint16_t alignment;
  } opaque;
};

struct MLKEM768_private_key {
  union {
    uint8_t bytes[512 * (3 + 3 + 9) + 32 + 32 + 32];
    uint16_t alignment;
  } opaque;
};

#define MLKEM768_PUBLIC_KEY_BYTES 1184

#define MLKEM_SEED_BYTES 64

OPENSSL_EXPORT void MLKEM768_generate_key(
    uint8_t out_encoded_public_key[MLKEM768_PUBLIC_KEY_BYTES],
    uint8_t optional_out_seed[MLKEM_SEED_BYTES],
    struct MLKEM768_private_key *out_private_key);

OPENSSL_EXPORT int MLKEM768_private_key_from_seed(
    struct MLKEM768_private_key *out_private_key, const uint8_t *seed,
    size_t seed_len);

OPENSSL_EXPORT void MLKEM768_public_from_private(
    struct MLKEM768_public_key *out_public_key,
    const struct MLKEM768_private_key *private_key);

#define MLKEM768_CIPHERTEXT_BYTES 1088

#define MLKEM_SHARED_SECRET_BYTES 32

OPENSSL_EXPORT void MLKEM768_encap(
    uint8_t out_ciphertext[MLKEM768_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const struct MLKEM768_public_key *public_key);

OPENSSL_EXPORT int MLKEM768_decap(
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *ciphertext, size_t ciphertext_len,
    const struct MLKEM768_private_key *private_key);

OPENSSL_EXPORT int MLKEM768_marshal_public_key(
    CBB *out, const struct MLKEM768_public_key *public_key);

OPENSSL_EXPORT int MLKEM768_parse_public_key(
    struct MLKEM768_public_key *out_public_key, CBS *in);

struct MLKEM1024_public_key {
  union {
    uint8_t bytes[512 * (4 + 16) + 32 + 32];
    uint16_t alignment;
  } opaque;
};

struct MLKEM1024_private_key {
  union {
    uint8_t bytes[512 * (4 + 4 + 16) + 32 + 32 + 32];
    uint16_t alignment;
  } opaque;
};

#define MLKEM1024_PUBLIC_KEY_BYTES 1568

OPENSSL_EXPORT void MLKEM1024_generate_key(
    uint8_t out_encoded_public_key[MLKEM1024_PUBLIC_KEY_BYTES],
    uint8_t optional_out_seed[MLKEM_SEED_BYTES],
    struct MLKEM1024_private_key *out_private_key);

OPENSSL_EXPORT int MLKEM1024_private_key_from_seed(
    struct MLKEM1024_private_key *out_private_key, const uint8_t *seed,
    size_t seed_len);

OPENSSL_EXPORT void MLKEM1024_public_from_private(
    struct MLKEM1024_public_key *out_public_key,
    const struct MLKEM1024_private_key *private_key);

#define MLKEM1024_CIPHERTEXT_BYTES 1568

OPENSSL_EXPORT void MLKEM1024_encap(
    uint8_t out_ciphertext[MLKEM1024_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const struct MLKEM1024_public_key *public_key);

OPENSSL_EXPORT int MLKEM1024_decap(
    uint8_t out_shared_secret[MLKEM_SHARED_SECRET_BYTES],
    const uint8_t *ciphertext, size_t ciphertext_len,
    const struct MLKEM1024_private_key *private_key);

OPENSSL_EXPORT int MLKEM1024_marshal_public_key(
    CBB *out, const struct MLKEM1024_public_key *public_key);

OPENSSL_EXPORT int MLKEM1024_parse_public_key(
    struct MLKEM1024_public_key *out_public_key, CBS *in);

#if defined(__cplusplus)
}
#endif

#endif
