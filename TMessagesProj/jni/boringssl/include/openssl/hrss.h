// Copyright 2018 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_HRSS_H
#define OPENSSL_HEADER_HRSS_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct HRSS_private_key {
  uint8_t opaque[1808];
};

struct HRSS_public_key {
  uint8_t opaque[1424];
};

#define HRSS_SAMPLE_BYTES (701 - 1)

#define HRSS_GENERATE_KEY_BYTES (HRSS_SAMPLE_BYTES + HRSS_SAMPLE_BYTES + 32)

#define HRSS_ENCAP_BYTES (HRSS_SAMPLE_BYTES + HRSS_SAMPLE_BYTES)

#define HRSS_PUBLIC_KEY_BYTES 1138

#define HRSS_CIPHERTEXT_BYTES 1138

#define HRSS_KEY_BYTES 32

#define HRSS_POLY3_BYTES 140
#define HRSS_PRIVATE_KEY_BYTES \
  (HRSS_POLY3_BYTES * 2 + HRSS_PUBLIC_KEY_BYTES + 2 + 32)

OPENSSL_EXPORT int HRSS_generate_key(
    struct HRSS_public_key *out_pub, struct HRSS_private_key *out_priv,
    const uint8_t input[HRSS_GENERATE_KEY_BYTES]);

OPENSSL_EXPORT int HRSS_encap(uint8_t out_ciphertext[HRSS_CIPHERTEXT_BYTES],
                              uint8_t out_shared_key[HRSS_KEY_BYTES],
                              const struct HRSS_public_key *in_pub,
                              const uint8_t in[HRSS_ENCAP_BYTES]);

OPENSSL_EXPORT int HRSS_decap(uint8_t out_shared_key[HRSS_KEY_BYTES],
                              const struct HRSS_private_key *in_priv,
                              const uint8_t *ciphertext, size_t ciphertext_len);

OPENSSL_EXPORT void HRSS_marshal_public_key(
    uint8_t out[HRSS_PUBLIC_KEY_BYTES], const struct HRSS_public_key *in_pub);

OPENSSL_EXPORT int HRSS_parse_public_key(
    struct HRSS_public_key *out, const uint8_t in[HRSS_PUBLIC_KEY_BYTES]);

#if defined(__cplusplus)
}
#endif

#endif
