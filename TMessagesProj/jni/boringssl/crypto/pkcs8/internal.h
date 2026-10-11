// Copyright 1999-2016 The OpenSSL Project Authors. All Rights Reserved.
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

#ifndef OPENSSL_HEADER_CRYPTO_PKCS8_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_PKCS8_INTERNAL_H

#include <openssl/base.h>
#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct pkcs8_priv_key_info_st {
  ASN1_INTEGER *version;
  X509_ALGOR *pkeyalg;
  ASN1_OCTET_STRING *pkey;
  STACK_OF(X509_ATTRIBUTE) *attributes;
};

int pkcs8_pbe_decrypt(uint8_t **out, size_t *out_len, CBS *algorithm,
                      const char *pass, size_t pass_len, const uint8_t *in,
                      size_t in_len);

#define PKCS12_KEY_ID 1
#define PKCS12_IV_ID 2
#define PKCS12_MAC_ID 3

int pkcs12_key_gen(const char *pass, size_t pass_len, const uint8_t *salt,
                   size_t salt_len, uint8_t id, uint32_t iterations,
                   size_t out_len, uint8_t *out, const EVP_MD *md);

int pkcs12_pbe_encrypt_init(CBB *out, EVP_CIPHER_CTX *ctx, int alg_nid,
                            const EVP_CIPHER *alg_cipher, uint32_t iterations,
                            const char *pass, size_t pass_len,
                            const uint8_t *salt, size_t salt_len);

struct pbe_suite {
  int pbe_nid;
  uint8_t oid[10];
  uint8_t oid_len;
  const EVP_CIPHER *(*cipher_func)(void);
  const EVP_MD *(*md_func)(void);

  int (*decrypt_init)(const struct pbe_suite *suite, EVP_CIPHER_CTX *ctx,
                      const char *pass, size_t pass_len, CBS *param);
};

#define PKCS5_SALT_LEN 8

const EVP_CIPHER *pkcs5_pbe2_nid_to_cipher(int nid);

int PKCS5_pbe2_decrypt_init(const struct pbe_suite *suite, EVP_CIPHER_CTX *ctx,
                            const char *pass, size_t pass_len, CBS *param);

int PKCS5_pbe2_encrypt_init(CBB *out, EVP_CIPHER_CTX *ctx,
                            const EVP_CIPHER *cipher, uint32_t iterations,
                            const char *pass, size_t pass_len,
                            const uint8_t *salt, size_t salt_len);

int pkcs12_iterations_acceptable(uint64_t iterations);

#if defined(__cplusplus)
}
#endif

#endif
