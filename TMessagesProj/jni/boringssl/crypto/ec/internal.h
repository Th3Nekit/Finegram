// Copyright 2020 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_EC_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_EC_INTERNAL_H

#include <openssl/ec.h>

#include "../fipsmodule/ec/internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int ec_hash_to_curve_p256_xmd_sha256_sswu(
    const EC_GROUP *group, EC_JACOBIAN *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int ec_hash_to_curve_p384_xmd_sha384_sswu(
    const EC_GROUP *group, EC_JACOBIAN *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int ec_hash_to_scalar_p384_xmd_sha384(
    const EC_GROUP *group, EC_SCALAR *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int ec_hash_to_curve_p384_xmd_sha512_sswu_draft07(
    const EC_GROUP *group, EC_JACOBIAN *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int ec_hash_to_scalar_p384_xmd_sha512_draft07(
    const EC_GROUP *group, EC_SCALAR *out, const uint8_t *dst, size_t dst_len,
    const uint8_t *msg, size_t msg_len);

#if defined(__cplusplus)
}
#endif

#endif
