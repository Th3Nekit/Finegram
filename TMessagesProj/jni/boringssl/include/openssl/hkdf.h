// Copyright 2014 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_HKDF_H
#define OPENSSL_HEADER_HKDF_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int HKDF(uint8_t *out_key, size_t out_len, const EVP_MD *digest,
                        const uint8_t *secret, size_t secret_len,
                        const uint8_t *salt, size_t salt_len,
                        const uint8_t *info, size_t info_len);

OPENSSL_EXPORT int HKDF_extract(uint8_t *out_key, size_t *out_len,
                                const EVP_MD *digest, const uint8_t *secret,
                                size_t secret_len, const uint8_t *salt,
                                size_t salt_len);

OPENSSL_EXPORT int HKDF_expand(uint8_t *out_key, size_t out_len,
                               const EVP_MD *digest, const uint8_t *prk,
                               size_t prk_len, const uint8_t *info,
                               size_t info_len);

#if defined(__cplusplus)
}
#endif

#define HKDF_R_OUTPUT_TOO_LARGE 100

#endif
