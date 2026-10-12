// Copyright 2016 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_POOL_H
#define OPENSSL_HEADER_POOL_H

#include <openssl/base.h>

#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif

DEFINE_STACK_OF(CRYPTO_BUFFER)

OPENSSL_EXPORT CRYPTO_BUFFER_POOL* CRYPTO_BUFFER_POOL_new(void);

OPENSSL_EXPORT void CRYPTO_BUFFER_POOL_free(CRYPTO_BUFFER_POOL *pool);

OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_new(const uint8_t *data, size_t len,
                                                CRYPTO_BUFFER_POOL *pool);

OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_alloc(uint8_t **out_data,
                                                  size_t len);

OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_new_from_CBS(
    const CBS *cbs, CRYPTO_BUFFER_POOL *pool);

OPENSSL_EXPORT CRYPTO_BUFFER *CRYPTO_BUFFER_new_from_static_data_unsafe(
    const uint8_t *data, size_t len, CRYPTO_BUFFER_POOL *pool);

OPENSSL_EXPORT void CRYPTO_BUFFER_free(CRYPTO_BUFFER *buf);

OPENSSL_EXPORT int CRYPTO_BUFFER_up_ref(CRYPTO_BUFFER *buf);

OPENSSL_EXPORT const uint8_t *CRYPTO_BUFFER_data(const CRYPTO_BUFFER *buf);

OPENSSL_EXPORT size_t CRYPTO_BUFFER_len(const CRYPTO_BUFFER *buf);

OPENSSL_EXPORT void CRYPTO_BUFFER_init_CBS(const CRYPTO_BUFFER *buf, CBS *out);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(CRYPTO_BUFFER_POOL, CRYPTO_BUFFER_POOL_free)
BORINGSSL_MAKE_DELETER(CRYPTO_BUFFER, CRYPTO_BUFFER_free)
BORINGSSL_MAKE_UP_REF(CRYPTO_BUFFER, CRYPTO_BUFFER_up_ref)

BSSL_NAMESPACE_END

}

#endif

#endif
