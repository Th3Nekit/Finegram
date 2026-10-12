// Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OPENSSL_HEADER_ECDH_H
#define OPENSSL_HEADER_ECDH_H

#include <openssl/base.h>

#include <openssl/ec_key.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int ECDH_compute_key(
    void *out, size_t outlen, const EC_POINT *pub_key, const EC_KEY *priv_key,
    void *(*kdf)(const void *in, size_t inlen, void *out, size_t *outlen));

OPENSSL_EXPORT int ECDH_compute_key_fips(uint8_t *out, size_t out_len,
                                         const EC_POINT *pub_key,
                                         const EC_KEY *priv_key);

#if defined(__cplusplus)
}
#endif

#define ECDH_R_KDF_FAILED 100
#define ECDH_R_NO_PRIVATE_VALUE 101
#define ECDH_R_POINT_ARITHMETIC_FAILURE 102
#define ECDH_R_UNKNOWN_DIGEST_LENGTH 103

#endif
