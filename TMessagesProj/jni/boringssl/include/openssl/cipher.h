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

#ifndef OPENSSL_HEADER_CIPHER_H
#define OPENSSL_HEADER_CIPHER_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT const EVP_CIPHER *EVP_rc4(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_des_cbc(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_des_ecb(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_des_ede(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_des_ede3(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_des_ede_cbc(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_des_ede3_cbc(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_128_ecb(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_128_cbc(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_128_ctr(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_128_ofb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_256_ecb(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_256_cbc(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_256_ctr(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_256_ofb(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_256_xts(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_enc_null(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_rc2_cbc(void);

const EVP_CIPHER *EVP_rc2_40_cbc(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_get_cipherbynid(int nid);

OPENSSL_EXPORT void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void);

OPENSSL_EXPORT int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT int EVP_CIPHER_CTX_copy(EVP_CIPHER_CTX *out,
                                       const EVP_CIPHER_CTX *in);

OPENSSL_EXPORT int EVP_CIPHER_CTX_reset(EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT int EVP_CipherInit_ex(EVP_CIPHER_CTX *ctx,
                                     const EVP_CIPHER *cipher, ENGINE *engine,
                                     const uint8_t *key, const uint8_t *iv,
                                     int enc);

OPENSSL_EXPORT int EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx,
                                      const EVP_CIPHER *cipher, ENGINE *impl,
                                      const uint8_t *key, const uint8_t *iv);

OPENSSL_EXPORT int EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx,
                                      const EVP_CIPHER *cipher, ENGINE *impl,
                                      const uint8_t *key, const uint8_t *iv);

OPENSSL_EXPORT int EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                     int *out_len, const uint8_t *in,
                                     int in_len);

OPENSSL_EXPORT int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                       int *out_len);

OPENSSL_EXPORT int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                     int *out_len, const uint8_t *in,
                                     int in_len);

OPENSSL_EXPORT int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                       int *out_len);

OPENSSL_EXPORT int EVP_CipherUpdate(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                    int *out_len, const uint8_t *in,
                                    int in_len);

OPENSSL_EXPORT int EVP_CipherFinal_ex(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                      int *out_len);

OPENSSL_EXPORT const EVP_CIPHER *EVP_CIPHER_CTX_cipher(
    const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT int EVP_CIPHER_CTX_nid(const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT int EVP_CIPHER_CTX_encrypting(const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT unsigned EVP_CIPHER_CTX_block_size(const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT unsigned EVP_CIPHER_CTX_key_length(const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT unsigned EVP_CIPHER_CTX_iv_length(const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT void *EVP_CIPHER_CTX_get_app_data(const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT void EVP_CIPHER_CTX_set_app_data(EVP_CIPHER_CTX *ctx,
                                                void *data);

OPENSSL_EXPORT uint32_t EVP_CIPHER_CTX_flags(const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT uint32_t EVP_CIPHER_CTX_mode(const EVP_CIPHER_CTX *ctx);

OPENSSL_EXPORT int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int command,
                                       int arg, void *ptr);

OPENSSL_EXPORT int EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *ctx, int pad);

OPENSSL_EXPORT int EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *ctx,
                                                 unsigned key_len);

OPENSSL_EXPORT int EVP_CIPHER_nid(const EVP_CIPHER *cipher);

OPENSSL_EXPORT unsigned EVP_CIPHER_block_size(const EVP_CIPHER *cipher);

OPENSSL_EXPORT unsigned EVP_CIPHER_key_length(const EVP_CIPHER *cipher);

OPENSSL_EXPORT unsigned EVP_CIPHER_iv_length(const EVP_CIPHER *cipher);

OPENSSL_EXPORT uint32_t EVP_CIPHER_flags(const EVP_CIPHER *cipher);

OPENSSL_EXPORT uint32_t EVP_CIPHER_mode(const EVP_CIPHER *cipher);

OPENSSL_EXPORT int EVP_BytesToKey(const EVP_CIPHER *type, const EVP_MD *md,
                                  const uint8_t salt[8], const uint8_t *data,
                                  size_t data_len, unsigned count, uint8_t *key,
                                  uint8_t *iv);

#define EVP_CIPH_STREAM_CIPHER 0x0
#define EVP_CIPH_ECB_MODE 0x1
#define EVP_CIPH_CBC_MODE 0x2
#define EVP_CIPH_CFB_MODE 0x3
#define EVP_CIPH_OFB_MODE 0x4
#define EVP_CIPH_CTR_MODE 0x5
#define EVP_CIPH_GCM_MODE 0x6
#define EVP_CIPH_XTS_MODE 0x7

#define EVP_CIPH_CCM_MODE 0x8
#define EVP_CIPH_OCB_MODE 0x9
#define EVP_CIPH_WRAP_MODE 0xa

#define EVP_CIPH_VARIABLE_LENGTH 0x40

#define EVP_CIPH_ALWAYS_CALL_INIT 0x80

#define EVP_CIPH_CUSTOM_IV 0x100

#define EVP_CIPH_CTRL_INIT 0x200

#define EVP_CIPH_FLAG_CUSTOM_CIPHER 0x400

#define EVP_CIPH_FLAG_AEAD_CIPHER 0x800

#define EVP_CIPH_CUSTOM_COPY 0x1000

#define EVP_CIPH_FLAG_NON_FIPS_ALLOW 0

OPENSSL_EXPORT int EVP_CipherInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                                  const uint8_t *key, const uint8_t *iv,
                                  int enc);

OPENSSL_EXPORT int EVP_EncryptInit(EVP_CIPHER_CTX *ctx,
                                   const EVP_CIPHER *cipher, const uint8_t *key,
                                   const uint8_t *iv);

OPENSSL_EXPORT int EVP_DecryptInit(EVP_CIPHER_CTX *ctx,
                                   const EVP_CIPHER *cipher, const uint8_t *key,
                                   const uint8_t *iv);

OPENSSL_EXPORT int EVP_CipherFinal(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                   int *out_len);

OPENSSL_EXPORT int EVP_EncryptFinal(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                    int *out_len);

OPENSSL_EXPORT int EVP_DecryptFinal(EVP_CIPHER_CTX *ctx, uint8_t *out,
                                    int *out_len);

OPENSSL_EXPORT int EVP_Cipher(EVP_CIPHER_CTX *ctx, uint8_t *out,
                              const uint8_t *in, size_t in_len);

OPENSSL_EXPORT int EVP_add_cipher_alias(const char *a, const char *b);

OPENSSL_EXPORT const EVP_CIPHER *EVP_get_cipherbyname(const char *name);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_128_gcm(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_256_gcm(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_192_ecb(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_192_cbc(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_192_ctr(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_192_gcm(void);
OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_192_ofb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_des_ede3_ecb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_128_cfb128(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_128_cfb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_192_cfb128(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_192_cfb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_256_cfb128(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_aes_256_cfb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_bf_ecb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_bf_cbc(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_bf_cfb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_cast5_ecb(void);

OPENSSL_EXPORT const EVP_CIPHER *EVP_cast5_cbc(void);

#define EVP_CIPHER_CTX_FLAG_WRAP_ALLOW 0

OPENSSL_EXPORT void EVP_CIPHER_CTX_set_flags(const EVP_CIPHER_CTX *ctx,
                                             uint32_t flags);

#define EVP_CIPH_NO_PADDING 0x800

#define EVP_CTRL_INIT 0x0
#define EVP_CTRL_SET_KEY_LENGTH 0x1
#define EVP_CTRL_GET_RC2_KEY_BITS 0x2
#define EVP_CTRL_SET_RC2_KEY_BITS 0x3
#define EVP_CTRL_GET_RC5_ROUNDS 0x4
#define EVP_CTRL_SET_RC5_ROUNDS 0x5
#define EVP_CTRL_RAND_KEY 0x6
#define EVP_CTRL_PBE_PRF_NID 0x7
#define EVP_CTRL_COPY 0x8
#define EVP_CTRL_AEAD_SET_IVLEN 0x9
#define EVP_CTRL_AEAD_GET_TAG 0x10
#define EVP_CTRL_AEAD_SET_TAG 0x11
#define EVP_CTRL_AEAD_SET_IV_FIXED 0x12
#define EVP_CTRL_GCM_IV_GEN 0x13
#define EVP_CTRL_AEAD_SET_MAC_KEY 0x17

#define EVP_CTRL_GCM_SET_IV_INV 0x18
#define EVP_CTRL_GET_IVLEN 0x19

#define EVP_GCM_TLS_FIXED_IV_LEN 4
#define EVP_GCM_TLS_EXPLICIT_IV_LEN 8
#define EVP_GCM_TLS_TAG_LEN 16

#define EVP_CTRL_GCM_SET_IVLEN EVP_CTRL_AEAD_SET_IVLEN
#define EVP_CTRL_GCM_GET_TAG EVP_CTRL_AEAD_GET_TAG
#define EVP_CTRL_GCM_SET_TAG EVP_CTRL_AEAD_SET_TAG
#define EVP_CTRL_GCM_SET_IV_FIXED EVP_CTRL_AEAD_SET_IV_FIXED

#define EVP_MAX_KEY_LENGTH 64
#define EVP_MAX_IV_LENGTH 16
#define EVP_MAX_BLOCK_LENGTH 32

struct evp_cipher_ctx_st {

  const EVP_CIPHER *cipher;

  void *app_data;

  void *cipher_data;

  unsigned key_len;

  int encrypt;

  uint32_t flags;

  uint8_t oiv[EVP_MAX_IV_LENGTH];

  uint8_t iv[EVP_MAX_IV_LENGTH];

  uint8_t buf[EVP_MAX_BLOCK_LENGTH];

  int buf_len;

  unsigned num;

  int final_used;

  uint8_t final[EVP_MAX_BLOCK_LENGTH];

  int poisoned;
}                     ;

typedef struct evp_cipher_info_st {
  const EVP_CIPHER *cipher;
  unsigned char iv[EVP_MAX_IV_LENGTH];
} EVP_CIPHER_INFO;

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(EVP_CIPHER_CTX, EVP_CIPHER_CTX_free)

using ScopedEVP_CIPHER_CTX =
    internal::StackAllocated<EVP_CIPHER_CTX, int, EVP_CIPHER_CTX_init,
                             EVP_CIPHER_CTX_cleanup>;

BSSL_NAMESPACE_END

}
#endif

#endif

#define CIPHER_R_AES_KEY_SETUP_FAILED 100
#define CIPHER_R_BAD_DECRYPT 101
#define CIPHER_R_BAD_KEY_LENGTH 102
#define CIPHER_R_BUFFER_TOO_SMALL 103
#define CIPHER_R_CTRL_NOT_IMPLEMENTED 104
#define CIPHER_R_CTRL_OPERATION_NOT_IMPLEMENTED 105
#define CIPHER_R_DATA_NOT_MULTIPLE_OF_BLOCK_LENGTH 106
#define CIPHER_R_INITIALIZATION_ERROR 107
#define CIPHER_R_INPUT_NOT_INITIALIZED 108
#define CIPHER_R_INVALID_AD_SIZE 109
#define CIPHER_R_INVALID_KEY_LENGTH 110
#define CIPHER_R_INVALID_NONCE_SIZE 111
#define CIPHER_R_INVALID_OPERATION 112
#define CIPHER_R_IV_TOO_LARGE 113
#define CIPHER_R_NO_CIPHER_SET 114
#define CIPHER_R_OUTPUT_ALIASES_INPUT 115
#define CIPHER_R_TAG_TOO_LARGE 116
#define CIPHER_R_TOO_LARGE 117
#define CIPHER_R_UNSUPPORTED_AD_SIZE 118
#define CIPHER_R_UNSUPPORTED_INPUT_SIZE 119
#define CIPHER_R_UNSUPPORTED_KEY_SIZE 120
#define CIPHER_R_UNSUPPORTED_NONCE_SIZE 121
#define CIPHER_R_UNSUPPORTED_TAG_SIZE 122
#define CIPHER_R_WRONG_FINAL_BLOCK_LENGTH 123
#define CIPHER_R_NO_DIRECTION_SET 124
#define CIPHER_R_INVALID_NONCE 125

#endif
