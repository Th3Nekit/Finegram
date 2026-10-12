// Copyright 2017 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_AES_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_AES_INTERNAL_H

#include <stdlib.h>

#include "../bcm_interface.h"
#include "../../internal.h"

extern "C" {

typedef void (*block128_f)(const uint8_t in[16], uint8_t out[16],
                           const AES_KEY *key);

typedef void (*ctr128_f)(const uint8_t *in, uint8_t *out, size_t blocks,
                         const AES_KEY *key, const uint8_t ivec[16]);

ctr128_f aes_ctr_set_key(AES_KEY *aes_key, int *out_is_hwaes,
                         block128_f *out_block, const uint8_t *key,
                         size_t key_bytes);

#if !defined(OPENSSL_NO_ASM)

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)
#define HWAES
#define HWAES_ECB

inline int hwaes_capable(void) { return CRYPTO_is_AESNI_capable(); }

#define VPAES
#define VPAES_CBC
inline int vpaes_capable(void) { return CRYPTO_is_SSSE3_capable(); }

#elif defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)
#define HWAES

inline int hwaes_capable(void) { return CRYPTO_is_ARMv8_AES_capable(); }

#if defined(OPENSSL_ARM)
#define BSAES
#define VPAES
inline int bsaes_capable(void) { return CRYPTO_is_NEON_capable(); }
inline int vpaes_capable(void) { return CRYPTO_is_NEON_capable(); }
#endif

#if defined(OPENSSL_AARCH64)
#define VPAES
#define VPAES_CBC
inline int vpaes_capable(void) { return CRYPTO_is_NEON_capable(); }
#endif

#endif

#endif

#if defined(HWAES)

int aes_hw_set_encrypt_key(const uint8_t *user_key, int bits, AES_KEY *key);
int aes_hw_set_decrypt_key(const uint8_t *user_key, int bits, AES_KEY *key);
void aes_hw_encrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key);
void aes_hw_decrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key);
void aes_hw_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t length,
                        const AES_KEY *key, uint8_t *ivec, int enc);
void aes_hw_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out, size_t len,
                                 const AES_KEY *key, const uint8_t ivec[16]);

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)

void aes_hw_encrypt_key_to_decrypt_key(AES_KEY *key);

inline int aes_hw_set_encrypt_key_alt_capable(void) {
  return hwaes_capable() && CRYPTO_is_SSSE3_capable();
}
inline int aes_hw_set_encrypt_key_alt_preferred(void) {
  return hwaes_capable() && CRYPTO_is_AVX_capable();
}
int aes_hw_set_encrypt_key_base(const uint8_t *user_key, int bits,
                                AES_KEY *key);
int aes_hw_set_encrypt_key_alt(const uint8_t *user_key, int bits, AES_KEY *key);
#endif

#else

inline int hwaes_capable(void) { return 0; }

inline int aes_hw_set_encrypt_key(const uint8_t *user_key, int bits,
                                  AES_KEY *key) {
  abort();
}

inline int aes_hw_set_decrypt_key(const uint8_t *user_key, int bits,
                                  AES_KEY *key) {
  abort();
}

inline void aes_hw_encrypt(const uint8_t *in, uint8_t *out,
                           const AES_KEY *key) {
  abort();
}

inline void aes_hw_decrypt(const uint8_t *in, uint8_t *out,
                           const AES_KEY *key) {
  abort();
}

inline void aes_hw_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t length,
                               const AES_KEY *key, uint8_t *ivec, int enc) {
  abort();
}

inline void aes_hw_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out,
                                        size_t len, const AES_KEY *key,
                                        const uint8_t ivec[16]) {
  abort();
}

#endif

#if defined(HWAES_ECB)
void aes_hw_ecb_encrypt(const uint8_t *in, uint8_t *out, size_t length,
                        const AES_KEY *key, int enc);
#endif

#if defined(BSAES)

void bsaes_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t length,
                       const AES_KEY *key, uint8_t ivec[16], int enc);
void bsaes_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out, size_t len,
                                const AES_KEY *key, const uint8_t ivec[16]);

void vpaes_encrypt_key_to_bsaes(AES_KEY *out_bsaes, const AES_KEY *vpaes);
void vpaes_decrypt_key_to_bsaes(AES_KEY *out_bsaes, const AES_KEY *vpaes);
void vpaes_ctr32_encrypt_blocks_with_bsaes(const uint8_t *in, uint8_t *out,
                                           size_t blocks, const AES_KEY *key,
                                           const uint8_t ivec[16]);
#else
inline int bsaes_capable(void) { return 0; }

inline void bsaes_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t length,
                              const AES_KEY *key, uint8_t ivec[16], int enc) {
  abort();
}

inline void bsaes_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out,
                                       size_t len, const AES_KEY *key,
                                       const uint8_t ivec[16]) {
  abort();
}

inline void vpaes_encrypt_key_to_bsaes(AES_KEY *out_bsaes,
                                       const AES_KEY *vpaes) {
  abort();
}

inline void vpaes_decrypt_key_to_bsaes(AES_KEY *out_bsaes,
                                       const AES_KEY *vpaes) {
  abort();
}
#endif

#if defined(VPAES)

int vpaes_set_encrypt_key(const uint8_t *userKey, int bits, AES_KEY *key);
int vpaes_set_decrypt_key(const uint8_t *userKey, int bits, AES_KEY *key);

void vpaes_encrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key);
void vpaes_decrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key);

#if defined(VPAES_CBC)
void vpaes_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t length,
                       const AES_KEY *key, uint8_t *ivec, int enc);
#endif
void vpaes_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out, size_t len,
                                const AES_KEY *key, const uint8_t ivec[16]);
#else
inline int vpaes_capable(void) { return 0; }

inline int vpaes_set_encrypt_key(const uint8_t *userKey, int bits,
                                 AES_KEY *key) {
  abort();
}
inline int vpaes_set_decrypt_key(const uint8_t *userKey, int bits,
                                 AES_KEY *key) {
  abort();
}
inline void vpaes_encrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key) {
  abort();
}
inline void vpaes_decrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key) {
  abort();
}
inline void vpaes_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t length,
                              const AES_KEY *key, uint8_t *ivec, int enc) {
  abort();
}
inline void vpaes_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out,
                                       size_t len, const AES_KEY *key,
                                       const uint8_t ivec[16]) {
  abort();
}
#endif

int aes_nohw_set_encrypt_key(const uint8_t *key, unsigned bits,
                             AES_KEY *aeskey);
int aes_nohw_set_decrypt_key(const uint8_t *key, unsigned bits,
                             AES_KEY *aeskey);
void aes_nohw_encrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key);
void aes_nohw_decrypt(const uint8_t *in, uint8_t *out, const AES_KEY *key);
void aes_nohw_ctr32_encrypt_blocks(const uint8_t *in, uint8_t *out,
                                   size_t blocks, const AES_KEY *key,
                                   const uint8_t ivec[16]);
void aes_nohw_cbc_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                          const AES_KEY *key, uint8_t *ivec, int enc);

inline void CRYPTO_xor16(uint8_t out[16], const uint8_t a[16],
                         const uint8_t b[16]) {

  static_assert(16 % sizeof(crypto_word_t) == 0,
                "block cannot be evenly divided into words");
  for (size_t i = 0; i < 16; i += sizeof(crypto_word_t)) {
    CRYPTO_store_word_le(
        out + i, CRYPTO_load_word_le(a + i) ^ CRYPTO_load_word_le(b + i));
  }
}

void CRYPTO_ctr128_encrypt_ctr32(const uint8_t *in, uint8_t *out, size_t len,
                                 const AES_KEY *key, uint8_t ivec[16],
                                 uint8_t ecount_buf[16], unsigned *num,
                                 ctr128_f ctr);

enum gcm_impl_t {
  gcm_separate = 0,
  gcm_x86_aesni,
  gcm_x86_vaes_avx2,
  gcm_x86_vaes_avx512,
  gcm_arm64_aes,
};

typedef struct { uint64_t hi,lo; } u128;

typedef void (*gmult_func)(uint8_t Xi[16], const u128 Htable[16]);

typedef void (*ghash_func)(uint8_t Xi[16], const u128 Htable[16],
                           const uint8_t *inp, size_t len);

typedef struct gcm128_key_st {
  u128 Htable[16];
  gmult_func gmult;
  ghash_func ghash;
  AES_KEY aes;

  ctr128_f ctr;
  block128_f block;
  enum gcm_impl_t impl;
} GCM128_KEY;

typedef struct {

  uint8_t Yi[16];
  uint8_t EKi[16];
  uint8_t EK0[16];
  struct {
    uint64_t aad;
    uint64_t msg;
  } len;
  uint8_t Xi[16];
  unsigned mres, ares;
} GCM128_CONTEXT;

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)

int crypto_gcm_clmul_enabled(void);
#endif

void CRYPTO_ghash_init(gmult_func *out_mult, ghash_func *out_hash,
                       u128 out_table[16], const uint8_t gcm_key[16]);

void CRYPTO_gcm128_init_aes_key(GCM128_KEY *gcm_key, const uint8_t *key,
                                size_t key_bytes);

void CRYPTO_gcm128_init_ctx(const GCM128_KEY *key, GCM128_CONTEXT *ctx,
                            const uint8_t *iv, size_t iv_len);

int CRYPTO_gcm128_aad(const GCM128_KEY *key, GCM128_CONTEXT *ctx,
                      const uint8_t *aad, size_t aad_len);

int CRYPTO_gcm128_encrypt(const GCM128_KEY *key, GCM128_CONTEXT *ctx,
                          const uint8_t *in, uint8_t *out, size_t len);

int CRYPTO_gcm128_decrypt(const GCM128_KEY *key, GCM128_CONTEXT *ctx,
                          const uint8_t *in, uint8_t *out, size_t len);

int CRYPTO_gcm128_finish(const GCM128_KEY *key, GCM128_CONTEXT *ctx,
                         const uint8_t *tag, size_t len);

void CRYPTO_gcm128_tag(const GCM128_KEY *key, GCM128_CONTEXT *ctx, uint8_t *tag,
                       size_t len);

void gcm_init_nohw(u128 Htable[16], const uint64_t H[2]);
void gcm_gmult_nohw(uint8_t Xi[16], const u128 Htable[16]);
void gcm_ghash_nohw(uint8_t Xi[16], const u128 Htable[16], const uint8_t *inp,
                    size_t len);

#if !defined(OPENSSL_NO_ASM)

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)
#define GCM_FUNCREF
void gcm_init_clmul(u128 Htable[16], const uint64_t Xi[2]);
void gcm_gmult_clmul(uint8_t Xi[16], const u128 Htable[16]);
void gcm_ghash_clmul(uint8_t Xi[16], const u128 Htable[16], const uint8_t *inp,
                     size_t len);

void gcm_init_ssse3(u128 Htable[16], const uint64_t Xi[2]);
void gcm_gmult_ssse3(uint8_t Xi[16], const u128 Htable[16]);
void gcm_ghash_ssse3(uint8_t Xi[16], const u128 Htable[16], const uint8_t *in,
                     size_t len);

#if defined(OPENSSL_X86_64)
#define GHASH_ASM_X86_64
void gcm_init_avx(u128 Htable[16], const uint64_t Xi[2]);
void gcm_gmult_avx(uint8_t Xi[16], const u128 Htable[16]);
void gcm_ghash_avx(uint8_t Xi[16], const u128 Htable[16], const uint8_t *in,
                   size_t len);

#define HW_GCM
size_t aesni_gcm_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                         const AES_KEY *key, uint8_t ivec[16],
                         const u128 Htable[16], uint8_t Xi[16]);
size_t aesni_gcm_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                         const AES_KEY *key, uint8_t ivec[16],
                         const u128 Htable[16], uint8_t Xi[16]);

void gcm_init_vpclmulqdq_avx2(u128 Htable[16], const uint64_t H[2]);
void gcm_gmult_vpclmulqdq_avx2(uint8_t Xi[16], const u128 Htable[16]);
void gcm_ghash_vpclmulqdq_avx2(uint8_t Xi[16], const u128 Htable[16],
                               const uint8_t *in, size_t len);
void aes_gcm_enc_update_vaes_avx2(const uint8_t *in, uint8_t *out, size_t len,
                                  const AES_KEY *key, const uint8_t ivec[16],
                                  const u128 Htable[16], uint8_t Xi[16]);
void aes_gcm_dec_update_vaes_avx2(const uint8_t *in, uint8_t *out, size_t len,
                                  const AES_KEY *key, const uint8_t ivec[16],
                                  const u128 Htable[16], uint8_t Xi[16]);

void gcm_init_vpclmulqdq_avx512(u128 Htable[16], const uint64_t H[2]);
void gcm_gmult_vpclmulqdq_avx512(uint8_t Xi[16], const u128 Htable[16]);
void gcm_ghash_vpclmulqdq_avx512(uint8_t Xi[16], const u128 Htable[16],
                                 const uint8_t *in, size_t len);
void aes_gcm_enc_update_vaes_avx512(const uint8_t *in, uint8_t *out, size_t len,
                                    const AES_KEY *key, const uint8_t ivec[16],
                                    const u128 Htable[16], uint8_t Xi[16]);
void aes_gcm_dec_update_vaes_avx512(const uint8_t *in, uint8_t *out, size_t len,
                                    const AES_KEY *key, const uint8_t ivec[16],
                                    const u128 Htable[16], uint8_t Xi[16]);

#endif

#if defined(OPENSSL_X86)
#define GHASH_ASM_X86
#endif

#elif defined(OPENSSL_ARM) || defined(OPENSSL_AARCH64)

#define GHASH_ASM_ARM
#define GCM_FUNCREF

inline int gcm_pmull_capable(void) { return CRYPTO_is_ARMv8_PMULL_capable(); }

void gcm_init_v8(u128 Htable[16], const uint64_t H[2]);
void gcm_gmult_v8(uint8_t Xi[16], const u128 Htable[16]);
void gcm_ghash_v8(uint8_t Xi[16], const u128 Htable[16], const uint8_t *inp,
                  size_t len);

inline int gcm_neon_capable(void) { return CRYPTO_is_NEON_capable(); }

void gcm_init_neon(u128 Htable[16], const uint64_t H[2]);
void gcm_gmult_neon(uint8_t Xi[16], const u128 Htable[16]);
void gcm_ghash_neon(uint8_t Xi[16], const u128 Htable[16], const uint8_t *inp,
                    size_t len);

#if defined(OPENSSL_AARCH64)
#define HW_GCM

void aes_gcm_enc_kernel(const uint8_t *in, uint64_t in_bits, void *out,
                        void *Xi, uint8_t *ivec, const AES_KEY *key,
                        const u128 Htable[16]);
void aes_gcm_dec_kernel(const uint8_t *in, uint64_t in_bits, void *out,
                        void *Xi, uint8_t *ivec, const AES_KEY *key,
                        const u128 Htable[16]);
#endif

#endif
#endif

typedef void (*cbc128_f)(const uint8_t *in, uint8_t *out, size_t len,
                         const AES_KEY *key, uint8_t ivec[16], int enc);

void CRYPTO_cbc128_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const AES_KEY *key, uint8_t ivec[16],
                           block128_f block);

void CRYPTO_cbc128_decrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const AES_KEY *key, uint8_t ivec[16],
                           block128_f block);

void CRYPTO_ofb128_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const AES_KEY *key, uint8_t ivec[16], unsigned *num,
                           block128_f block);

void CRYPTO_cfb128_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                           const AES_KEY *key, uint8_t ivec[16], unsigned *num,
                           int enc, block128_f block);

void CRYPTO_cfb128_8_encrypt(const uint8_t *in, uint8_t *out, size_t len,
                             const AES_KEY *key, uint8_t ivec[16],
                             unsigned *num, int enc, block128_f block);

void CRYPTO_cfb128_1_encrypt(const uint8_t *in, uint8_t *out, size_t bits,
                             const AES_KEY *key, uint8_t ivec[16],
                             unsigned *num, int enc, block128_f block);

size_t CRYPTO_cts128_encrypt_block(const uint8_t *in, uint8_t *out, size_t len,
                                   const AES_KEY *key, uint8_t ivec[16],
                                   block128_f block);

}

#endif
