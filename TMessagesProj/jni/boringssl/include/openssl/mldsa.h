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

#ifndef OPENSSL_HEADER_MLDSA_H_
#define OPENSSL_HEADER_MLDSA_H_

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define MLDSA_SEED_BYTES 32

struct MLDSA65_private_key {
  union {
    uint8_t bytes[32 + 32 + 64 + 256 * 4 * (5 + 6 + 6)];
    uint32_t alignment;
  } opaque;
};

struct MLDSA65_public_key {
  union {
    uint8_t bytes[32 + 64 + 256 * 4 * 6];
    uint32_t alignment;
  } opaque;
};

#define MLDSA65_PRIVATE_KEY_BYTES 4032

#define MLDSA65_PUBLIC_KEY_BYTES 1952

#define MLDSA65_SIGNATURE_BYTES 3309

OPENSSL_EXPORT int MLDSA65_generate_key(
    uint8_t out_encoded_public_key[MLDSA65_PUBLIC_KEY_BYTES],
    uint8_t out_seed[MLDSA_SEED_BYTES],
    struct MLDSA65_private_key *out_private_key);

OPENSSL_EXPORT int MLDSA65_private_key_from_seed(
    struct MLDSA65_private_key *out_private_key, const uint8_t *seed,
    size_t seed_len);

OPENSSL_EXPORT int MLDSA65_public_from_private(
    struct MLDSA65_public_key *out_public_key,
    const struct MLDSA65_private_key *private_key);

OPENSSL_EXPORT int MLDSA65_sign(
    uint8_t out_encoded_signature[MLDSA65_SIGNATURE_BYTES],
    const struct MLDSA65_private_key *private_key, const uint8_t *msg,
    size_t msg_len, const uint8_t *context, size_t context_len);

OPENSSL_EXPORT int MLDSA65_verify(const struct MLDSA65_public_key *public_key,
                                  const uint8_t *signature,
                                  size_t signature_len, const uint8_t *msg,
                                  size_t msg_len, const uint8_t *context,
                                  size_t context_len);

OPENSSL_EXPORT int MLDSA65_marshal_public_key(
    CBB *out, const struct MLDSA65_public_key *public_key);

OPENSSL_EXPORT int MLDSA65_parse_public_key(
    struct MLDSA65_public_key *public_key, CBS *in);

#if defined(__cplusplus)
}
#endif

#endif
