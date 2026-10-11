// Copyright 2020 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_HPKE_H
#define OPENSSL_HEADER_HPKE_H

#include <openssl/aead.h>
#include <openssl/base.h>
#include <openssl/curve25519.h>
#include <openssl/digest.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define EVP_HPKE_DHKEM_P256_HKDF_SHA256 0x0010
#define EVP_HPKE_DHKEM_X25519_HKDF_SHA256 0x0020

OPENSSL_EXPORT const EVP_HPKE_KEM *EVP_hpke_x25519_hkdf_sha256(void);
OPENSSL_EXPORT const EVP_HPKE_KEM *EVP_hpke_p256_hkdf_sha256(void);

OPENSSL_EXPORT uint16_t EVP_HPKE_KEM_id(const EVP_HPKE_KEM *kem);

#define EVP_HPKE_MAX_PUBLIC_KEY_LENGTH 65

OPENSSL_EXPORT size_t EVP_HPKE_KEM_public_key_len(const EVP_HPKE_KEM *kem);

#define EVP_HPKE_MAX_PRIVATE_KEY_LENGTH 32

OPENSSL_EXPORT size_t EVP_HPKE_KEM_private_key_len(const EVP_HPKE_KEM *kem);

#define EVP_HPKE_MAX_ENC_LENGTH 65

OPENSSL_EXPORT size_t EVP_HPKE_KEM_enc_len(const EVP_HPKE_KEM *kem);

#define EVP_HPKE_HKDF_SHA256 0x0001

OPENSSL_EXPORT const EVP_HPKE_KDF *EVP_hpke_hkdf_sha256(void);

OPENSSL_EXPORT uint16_t EVP_HPKE_KDF_id(const EVP_HPKE_KDF *kdf);

OPENSSL_EXPORT const EVP_MD *EVP_HPKE_KDF_hkdf_md(const EVP_HPKE_KDF *kdf);

#define EVP_HPKE_AES_128_GCM 0x0001
#define EVP_HPKE_AES_256_GCM 0x0002
#define EVP_HPKE_CHACHA20_POLY1305 0x0003

OPENSSL_EXPORT const EVP_HPKE_AEAD *EVP_hpke_aes_128_gcm(void);
OPENSSL_EXPORT const EVP_HPKE_AEAD *EVP_hpke_aes_256_gcm(void);
OPENSSL_EXPORT const EVP_HPKE_AEAD *EVP_hpke_chacha20_poly1305(void);

OPENSSL_EXPORT uint16_t EVP_HPKE_AEAD_id(const EVP_HPKE_AEAD *aead);

OPENSSL_EXPORT const EVP_AEAD *EVP_HPKE_AEAD_aead(const EVP_HPKE_AEAD *aead);

OPENSSL_EXPORT void EVP_HPKE_KEY_zero(EVP_HPKE_KEY *key);

OPENSSL_EXPORT void EVP_HPKE_KEY_cleanup(EVP_HPKE_KEY *key);

OPENSSL_EXPORT EVP_HPKE_KEY *EVP_HPKE_KEY_new(void);

OPENSSL_EXPORT void EVP_HPKE_KEY_free(EVP_HPKE_KEY *key);

OPENSSL_EXPORT int EVP_HPKE_KEY_copy(EVP_HPKE_KEY *dst,
                                     const EVP_HPKE_KEY *src);

OPENSSL_EXPORT void EVP_HPKE_KEY_move(EVP_HPKE_KEY *out, EVP_HPKE_KEY *in);

OPENSSL_EXPORT int EVP_HPKE_KEY_init(EVP_HPKE_KEY *key, const EVP_HPKE_KEM *kem,
                                     const uint8_t *priv_key,
                                     size_t priv_key_len);

OPENSSL_EXPORT int EVP_HPKE_KEY_generate(EVP_HPKE_KEY *key,
                                         const EVP_HPKE_KEM *kem);

OPENSSL_EXPORT const EVP_HPKE_KEM *EVP_HPKE_KEY_kem(const EVP_HPKE_KEY *key);

OPENSSL_EXPORT int EVP_HPKE_KEY_public_key(const EVP_HPKE_KEY *key,
                                           uint8_t *out, size_t *out_len,
                                           size_t max_out);

OPENSSL_EXPORT int EVP_HPKE_KEY_private_key(const EVP_HPKE_KEY *key,
                                            uint8_t *out, size_t *out_len,
                                            size_t max_out);

OPENSSL_EXPORT void EVP_HPKE_CTX_zero(EVP_HPKE_CTX *ctx);

OPENSSL_EXPORT void EVP_HPKE_CTX_cleanup(EVP_HPKE_CTX *ctx);

OPENSSL_EXPORT EVP_HPKE_CTX *EVP_HPKE_CTX_new(void);

OPENSSL_EXPORT void EVP_HPKE_CTX_free(EVP_HPKE_CTX *ctx);

OPENSSL_EXPORT int EVP_HPKE_CTX_setup_sender(
    EVP_HPKE_CTX *ctx, uint8_t *out_enc, size_t *out_enc_len, size_t max_enc,
    const EVP_HPKE_KEM *kem, const EVP_HPKE_KDF *kdf, const EVP_HPKE_AEAD *aead,
    const uint8_t *peer_public_key, size_t peer_public_key_len,
    const uint8_t *info, size_t info_len);

OPENSSL_EXPORT int EVP_HPKE_CTX_setup_sender_with_seed_for_testing(
    EVP_HPKE_CTX *ctx, uint8_t *out_enc, size_t *out_enc_len, size_t max_enc,
    const EVP_HPKE_KEM *kem, const EVP_HPKE_KDF *kdf, const EVP_HPKE_AEAD *aead,
    const uint8_t *peer_public_key, size_t peer_public_key_len,
    const uint8_t *info, size_t info_len, const uint8_t *seed, size_t seed_len);

OPENSSL_EXPORT int EVP_HPKE_CTX_setup_recipient(
    EVP_HPKE_CTX *ctx, const EVP_HPKE_KEY *key, const EVP_HPKE_KDF *kdf,
    const EVP_HPKE_AEAD *aead, const uint8_t *enc, size_t enc_len,
    const uint8_t *info, size_t info_len);

OPENSSL_EXPORT int EVP_HPKE_CTX_setup_auth_sender(
    EVP_HPKE_CTX *ctx, uint8_t *out_enc, size_t *out_enc_len, size_t max_enc,
    const EVP_HPKE_KEY *key, const EVP_HPKE_KDF *kdf, const EVP_HPKE_AEAD *aead,
    const uint8_t *peer_public_key, size_t peer_public_key_len,
    const uint8_t *info, size_t info_len);

OPENSSL_EXPORT int EVP_HPKE_CTX_setup_auth_sender_with_seed_for_testing(
    EVP_HPKE_CTX *ctx, uint8_t *out_enc, size_t *out_enc_len, size_t max_enc,
    const EVP_HPKE_KEY *key, const EVP_HPKE_KDF *kdf, const EVP_HPKE_AEAD *aead,
    const uint8_t *peer_public_key, size_t peer_public_key_len,
    const uint8_t *info, size_t info_len, const uint8_t *seed, size_t seed_len);

OPENSSL_EXPORT int EVP_HPKE_CTX_setup_auth_recipient(
    EVP_HPKE_CTX *ctx, const EVP_HPKE_KEY *key, const EVP_HPKE_KDF *kdf,
    const EVP_HPKE_AEAD *aead, const uint8_t *enc, size_t enc_len,
    const uint8_t *info, size_t info_len, const uint8_t *peer_public_key,
    size_t peer_public_key_len);

OPENSSL_EXPORT int EVP_HPKE_CTX_open(EVP_HPKE_CTX *ctx, uint8_t *out,
                                     size_t *out_len, size_t max_out_len,
                                     const uint8_t *in, size_t in_len,
                                     const uint8_t *ad, size_t ad_len);

OPENSSL_EXPORT int EVP_HPKE_CTX_seal(EVP_HPKE_CTX *ctx, uint8_t *out,
                                     size_t *out_len, size_t max_out_len,
                                     const uint8_t *in, size_t in_len,
                                     const uint8_t *ad, size_t ad_len);

OPENSSL_EXPORT int EVP_HPKE_CTX_export(const EVP_HPKE_CTX *ctx, uint8_t *out,
                                       size_t secret_len,
                                       const uint8_t *context,
                                       size_t context_len);

#define EVP_HPKE_MAX_OVERHEAD EVP_AEAD_MAX_OVERHEAD

OPENSSL_EXPORT size_t EVP_HPKE_CTX_max_overhead(const EVP_HPKE_CTX *ctx);

OPENSSL_EXPORT const EVP_HPKE_KEM *EVP_HPKE_CTX_kem(const EVP_HPKE_CTX *ctx);

OPENSSL_EXPORT const EVP_HPKE_AEAD *EVP_HPKE_CTX_aead(const EVP_HPKE_CTX *ctx);

OPENSSL_EXPORT const EVP_HPKE_KDF *EVP_HPKE_CTX_kdf(const EVP_HPKE_CTX *ctx);

struct evp_hpke_ctx_st {
  const EVP_HPKE_KEM *kem;
  const EVP_HPKE_AEAD *aead;
  const EVP_HPKE_KDF *kdf;
  EVP_AEAD_CTX aead_ctx;
  uint8_t base_nonce[EVP_AEAD_MAX_NONCE_LENGTH];
  uint8_t exporter_secret[EVP_MAX_MD_SIZE];
  uint64_t seq;
  int is_sender;
};

struct evp_hpke_key_st {
  const EVP_HPKE_KEM *kem;
  uint8_t private_key[EVP_HPKE_MAX_PRIVATE_KEY_LENGTH];
  uint8_t public_key[EVP_HPKE_MAX_PUBLIC_KEY_LENGTH];
};

#if defined(__cplusplus)
}
#endif

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

using ScopedEVP_HPKE_CTX =
    internal::StackAllocated<EVP_HPKE_CTX, void, EVP_HPKE_CTX_zero,
                             EVP_HPKE_CTX_cleanup>;
using ScopedEVP_HPKE_KEY =
    internal::StackAllocatedMovable<EVP_HPKE_KEY, void, EVP_HPKE_KEY_zero,
                                    EVP_HPKE_KEY_cleanup, EVP_HPKE_KEY_move>;

BORINGSSL_MAKE_DELETER(EVP_HPKE_CTX, EVP_HPKE_CTX_free)
BORINGSSL_MAKE_DELETER(EVP_HPKE_KEY, EVP_HPKE_KEY_free)

BSSL_NAMESPACE_END

}
#endif

#endif
