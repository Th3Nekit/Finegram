// Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_AES_H
#define OPENSSL_HEADER_AES_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define AES_ENCRYPT 1
#define AES_DECRYPT 0

#define AES_MAXNR 14

#define AES_BLOCK_SIZE 16

struct aes_key_st {
  uint32_t rd_key[4 * (AES_MAXNR + 1)];
  unsigned rounds;
};
typedef struct aes_key_st AES_KEY;

OPENSSL_EXPORT int AES_set_encrypt_key(const uint8_t *key, unsigned bits,
                                       AES_KEY *aeskey);

OPENSSL_EXPORT int AES_set_decrypt_key(const uint8_t *key, unsigned bits,
                                       AES_KEY *aeskey);

OPENSSL_EXPORT void AES_encrypt(const uint8_t *in, uint8_t *out,
                                const AES_KEY *key);

OPENSSL_EXPORT void AES_decrypt(const uint8_t *in, uint8_t *out,
                                const AES_KEY *key);

OPENSSL_EXPORT void AES_ctr128_encrypt(const uint8_t *in, uint8_t *out,
                                       size_t len, const AES_KEY *key,
                                       uint8_t ivec[AES_BLOCK_SIZE],
                                       uint8_t ecount_buf[AES_BLOCK_SIZE],
                                       unsigned int *num);

OPENSSL_EXPORT void AES_ecb_encrypt(const uint8_t *in, uint8_t *out,
                                    const AES_KEY *key, const int enc);

OPENSSL_EXPORT void AES_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                                    const AES_KEY *key, uint8_t *ivec,
                                    const int enc);

OPENSSL_EXPORT void AES_ofb128_encrypt(const uint8_t *in, uint8_t *out,
                                       size_t len, const AES_KEY *key,
                                       uint8_t *ivec, int *num);

OPENSSL_EXPORT void AES_cfb128_encrypt(const uint8_t *in, uint8_t *out,
                                       size_t len, const AES_KEY *key,
                                       uint8_t *ivec, int *num, int enc);

OPENSSL_EXPORT int AES_wrap_key(const AES_KEY *key, const uint8_t *iv,
                                uint8_t *out, const uint8_t *in, size_t in_len);

OPENSSL_EXPORT int AES_unwrap_key(const AES_KEY *key, const uint8_t *iv,
                                  uint8_t *out, const uint8_t *in,
                                  size_t in_len);

OPENSSL_EXPORT int AES_wrap_key_padded(const AES_KEY *key, uint8_t *out,
                                       size_t *out_len, size_t max_out,
                                       const uint8_t *in, size_t in_len);

OPENSSL_EXPORT int AES_unwrap_key_padded(const AES_KEY *key, uint8_t *out,
                                         size_t *out_len, size_t max_out,
                                         const uint8_t *in, size_t in_len);

OPENSSL_EXPORT void AES_ige_encrypt(const unsigned char *in, unsigned char *out,
                     size_t length, const AES_KEY *key,
                     unsigned char *ivec, const int enc);
OPENSSL_EXPORT void AES_bi_ige_encrypt(const unsigned char *in, unsigned char *out,
                        size_t length, const AES_KEY *key,
                        const AES_KEY *key2, const unsigned char *ivec,
                        const int enc);

#if defined(__cplusplus)
}
#endif

#endif
