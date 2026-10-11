// Copyright 2015 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CMAC_H
#define OPENSSL_HEADER_CMAC_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int AES_CMAC(uint8_t out[16], const uint8_t *key, size_t key_len,
                            const uint8_t *in, size_t in_len);

OPENSSL_EXPORT CMAC_CTX *CMAC_CTX_new(void);

OPENSSL_EXPORT void CMAC_CTX_free(CMAC_CTX *ctx);

OPENSSL_EXPORT int CMAC_CTX_copy(CMAC_CTX *out, const CMAC_CTX *in);

OPENSSL_EXPORT int CMAC_Init(CMAC_CTX *ctx, const void *key, size_t key_len,
                             const EVP_CIPHER *cipher, ENGINE *engine);

OPENSSL_EXPORT int CMAC_Reset(CMAC_CTX *ctx);

OPENSSL_EXPORT int CMAC_Update(CMAC_CTX *ctx, const uint8_t *in, size_t in_len);

OPENSSL_EXPORT int CMAC_Final(CMAC_CTX *ctx, uint8_t *out, size_t *out_len);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(CMAC_CTX, CMAC_CTX_free)

BSSL_NAMESPACE_END

}

#endif

#endif
