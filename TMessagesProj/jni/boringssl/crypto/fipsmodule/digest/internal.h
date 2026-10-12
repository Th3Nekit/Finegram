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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_DIGEST_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_DIGEST_INTERNAL_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct env_md_st {

  int type;

  unsigned md_size;

  uint32_t flags;

  void (*init)(EVP_MD_CTX *ctx);

  void (*update)(EVP_MD_CTX *ctx, const void *data, size_t count);

  void (*final)(EVP_MD_CTX *ctx, uint8_t *out);

  unsigned block_size;

  unsigned ctx_size;
};

struct evp_md_pctx_ops {

  void (*free) (EVP_PKEY_CTX *pctx);

  EVP_PKEY_CTX* (*dup) (EVP_PKEY_CTX *pctx);
};

#if defined(__cplusplus)
}
#endif

#endif
