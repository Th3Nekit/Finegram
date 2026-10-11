// Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_HMAC_H
#define OPENSSL_HEADER_HMAC_H

#include <openssl/base.h>

#include <openssl/digest.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT uint8_t *HMAC(const EVP_MD *evp_md, const void *key,
                             size_t key_len, const uint8_t *data,
                             size_t data_len, uint8_t *out,
                             unsigned int *out_len);

OPENSSL_EXPORT void HMAC_CTX_init(HMAC_CTX *ctx);

OPENSSL_EXPORT HMAC_CTX *HMAC_CTX_new(void);

OPENSSL_EXPORT void HMAC_CTX_cleanup(HMAC_CTX *ctx);

OPENSSL_EXPORT void HMAC_CTX_cleanse(HMAC_CTX *ctx);

OPENSSL_EXPORT void HMAC_CTX_free(HMAC_CTX *ctx);

OPENSSL_EXPORT int HMAC_Init_ex(HMAC_CTX *ctx, const void *key, size_t key_len,
                                const EVP_MD *md, ENGINE *impl);

OPENSSL_EXPORT int HMAC_Update(HMAC_CTX *ctx, const uint8_t *data,
                               size_t data_len);

OPENSSL_EXPORT int HMAC_Final(HMAC_CTX *ctx, uint8_t *out,
                              unsigned int *out_len);

OPENSSL_EXPORT size_t HMAC_size(const HMAC_CTX *ctx);

OPENSSL_EXPORT const EVP_MD *HMAC_CTX_get_md(const HMAC_CTX *ctx);

OPENSSL_EXPORT int HMAC_CTX_copy_ex(HMAC_CTX *dest, const HMAC_CTX *src);

OPENSSL_EXPORT void HMAC_CTX_reset(HMAC_CTX *ctx);

OPENSSL_EXPORT int HMAC_Init(HMAC_CTX *ctx, const void *key, int key_len,
                             const EVP_MD *md);

OPENSSL_EXPORT int HMAC_CTX_copy(HMAC_CTX *dest, const HMAC_CTX *src);

struct hmac_ctx_st {
  const EVP_MD *md;
  EVP_MD_CTX md_ctx;
  EVP_MD_CTX i_ctx;
  EVP_MD_CTX o_ctx;
}               ;

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(HMAC_CTX, HMAC_CTX_free)

using ScopedHMAC_CTX =
    internal::StackAllocated<HMAC_CTX, void, HMAC_CTX_init, HMAC_CTX_cleanup>;

BSSL_NAMESPACE_END

}
#endif

#endif

#endif
