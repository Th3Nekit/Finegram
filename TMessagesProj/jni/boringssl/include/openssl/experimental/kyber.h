// Copyright 2023 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_KYBER_H
#define OPENSSL_HEADER_KYBER_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(OPENSSL_UNSTABLE_EXPERIMENTAL_KYBER)

struct KYBER_public_key {
  union {
    uint8_t bytes[512 * (3 + 9) + 32 + 32];
    uint16_t alignment;
  } opaque;
};

struct KYBER_private_key {
  union {
    uint8_t bytes[512 * (3 + 3 + 9) + 32 + 32 + 32];
    uint16_t alignment;
  } opaque;
};

#define KYBER_PUBLIC_KEY_BYTES 1184

#define KYBER_SHARED_SECRET_BYTES 32

OPENSSL_EXPORT void KYBER_generate_key(
    uint8_t out_encoded_public_key[KYBER_PUBLIC_KEY_BYTES],
    struct KYBER_private_key *out_private_key);

OPENSSL_EXPORT void KYBER_public_from_private(
    struct KYBER_public_key *out_public_key,
    const struct KYBER_private_key *private_key);

#define KYBER_CIPHERTEXT_BYTES 1088

OPENSSL_EXPORT void KYBER_encap(
    uint8_t out_ciphertext[KYBER_CIPHERTEXT_BYTES],
    uint8_t out_shared_secret[KYBER_SHARED_SECRET_BYTES],
    const struct KYBER_public_key *public_key);

OPENSSL_EXPORT void KYBER_decap(
    uint8_t out_shared_secret[KYBER_SHARED_SECRET_BYTES],
    const uint8_t ciphertext[KYBER_CIPHERTEXT_BYTES],
    const struct KYBER_private_key *private_key);

OPENSSL_EXPORT int KYBER_marshal_public_key(
    CBB *out, const struct KYBER_public_key *public_key);

OPENSSL_EXPORT int KYBER_parse_public_key(
    struct KYBER_public_key *out_public_key, CBS *in);

OPENSSL_EXPORT int KYBER_marshal_private_key(
    CBB *out, const struct KYBER_private_key *private_key);

#define KYBER_PRIVATE_KEY_BYTES 2400

OPENSSL_EXPORT int KYBER_parse_private_key(
    struct KYBER_private_key *out_private_key, CBS *in);

#endif

#if defined(__cplusplus)
}
#endif

#endif
