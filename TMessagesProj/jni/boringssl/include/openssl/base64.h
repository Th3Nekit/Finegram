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

#ifndef OPENSSL_HEADER_BASE64_H
#define OPENSSL_HEADER_BASE64_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT size_t EVP_EncodeBlock(uint8_t *dst, const uint8_t *src,
                                      size_t src_len);

OPENSSL_EXPORT int EVP_EncodedLength(size_t *out_len, size_t len);

OPENSSL_EXPORT int EVP_DecodedLength(size_t *out_len, size_t len);

OPENSSL_EXPORT int EVP_DecodeBase64(uint8_t *out, size_t *out_len,
                                    size_t max_out, const uint8_t *in,
                                    size_t in_len);

OPENSSL_EXPORT EVP_ENCODE_CTX *EVP_ENCODE_CTX_new(void);

OPENSSL_EXPORT void EVP_ENCODE_CTX_free(EVP_ENCODE_CTX *ctx);

OPENSSL_EXPORT void EVP_EncodeInit(EVP_ENCODE_CTX *ctx);

OPENSSL_EXPORT void EVP_EncodeUpdate(EVP_ENCODE_CTX *ctx, uint8_t *out,
                                     int *out_len, const uint8_t *in,
                                     size_t in_len);

OPENSSL_EXPORT void EVP_EncodeFinal(EVP_ENCODE_CTX *ctx, uint8_t *out,
                                    int *out_len);

OPENSSL_EXPORT void EVP_DecodeInit(EVP_ENCODE_CTX *ctx);

OPENSSL_EXPORT int EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx, uint8_t *out,
                                    int *out_len, const uint8_t *in,
                                    size_t in_len);

OPENSSL_EXPORT int EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, uint8_t *out,
                                   int *out_len);

OPENSSL_EXPORT int EVP_DecodeBlock(uint8_t *dst, const uint8_t *src,
                                   size_t src_len);

struct evp_encode_ctx_st {

  unsigned data_used;
  uint8_t data[48];

  char eof_seen;

  char error_encountered;
};

#if defined(__cplusplus)
}
#endif

#endif
