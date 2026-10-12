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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_TLS_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_TLS_INTERNAL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int CRYPTO_tls1_prf(const EVP_MD *digest,
                                   uint8_t *out, size_t out_len,
                                   const uint8_t *secret, size_t secret_len,
                                   const char *label, size_t label_len,
                                   const uint8_t *seed1, size_t seed1_len,
                                   const uint8_t *seed2, size_t seed2_len);

OPENSSL_EXPORT int CRYPTO_tls13_hkdf_expand_label(
    uint8_t *out, size_t out_len, const EVP_MD *digest,
    const uint8_t *secret, size_t secret_len,
    const uint8_t *label, size_t label_len,
    const uint8_t *hash, size_t hash_len);

#if defined(__cplusplus)
}
#endif

#endif
