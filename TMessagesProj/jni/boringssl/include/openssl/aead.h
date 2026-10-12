// Copyright 2014 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_AEAD_H
#define OPENSSL_HEADER_AEAD_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_gcm(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_192_gcm(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_gcm(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_chacha20_poly1305(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_xchacha20_poly1305(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_ctr_hmac_sha256(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_ctr_hmac_sha256(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_gcm_siv(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_gcm_siv(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_gcm_randnonce(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_gcm_randnonce(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_ccm_bluetooth(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_ccm_bluetooth_8(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_ccm_matter(void);

OPENSSL_EXPORT int EVP_has_aes_hardware(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_eax(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_eax(void);

OPENSSL_EXPORT size_t EVP_AEAD_key_length(const EVP_AEAD *aead);

OPENSSL_EXPORT size_t EVP_AEAD_nonce_length(const EVP_AEAD *aead);

OPENSSL_EXPORT size_t EVP_AEAD_max_overhead(const EVP_AEAD *aead);

OPENSSL_EXPORT size_t EVP_AEAD_max_tag_len(const EVP_AEAD *aead);

union evp_aead_ctx_st_state {
  uint8_t opaque[564];
  uint64_t alignment;
};

struct evp_aead_ctx_st {
  const EVP_AEAD *aead;
  union evp_aead_ctx_st_state state;

  uint8_t tag_len;
};

#define EVP_AEAD_MAX_KEY_LENGTH 80

#define EVP_AEAD_MAX_NONCE_LENGTH 24

#define EVP_AEAD_MAX_OVERHEAD 64

#define EVP_AEAD_DEFAULT_TAG_LENGTH 0

OPENSSL_EXPORT void EVP_AEAD_CTX_zero(EVP_AEAD_CTX *ctx);

OPENSSL_EXPORT EVP_AEAD_CTX *EVP_AEAD_CTX_new(const EVP_AEAD *aead,
                                              const uint8_t *key,
                                              size_t key_len, size_t tag_len);

OPENSSL_EXPORT void EVP_AEAD_CTX_free(EVP_AEAD_CTX *ctx);

OPENSSL_EXPORT int EVP_AEAD_CTX_init(EVP_AEAD_CTX *ctx, const EVP_AEAD *aead,
                                     const uint8_t *key, size_t key_len,
                                     size_t tag_len, ENGINE *impl);

OPENSSL_EXPORT void EVP_AEAD_CTX_cleanup(EVP_AEAD_CTX *ctx);

OPENSSL_EXPORT int EVP_AEAD_CTX_seal(const EVP_AEAD_CTX *ctx, uint8_t *out,
                                     size_t *out_len, size_t max_out_len,
                                     const uint8_t *nonce, size_t nonce_len,
                                     const uint8_t *in, size_t in_len,
                                     const uint8_t *ad, size_t ad_len);

OPENSSL_EXPORT int EVP_AEAD_CTX_open(const EVP_AEAD_CTX *ctx, uint8_t *out,
                                     size_t *out_len, size_t max_out_len,
                                     const uint8_t *nonce, size_t nonce_len,
                                     const uint8_t *in, size_t in_len,
                                     const uint8_t *ad, size_t ad_len);

OPENSSL_EXPORT int EVP_AEAD_CTX_seal_scatter(
    const EVP_AEAD_CTX *ctx, uint8_t *out, uint8_t *out_tag,
    size_t *out_tag_len, size_t max_out_tag_len, const uint8_t *nonce,
    size_t nonce_len, const uint8_t *in, size_t in_len, const uint8_t *extra_in,
    size_t extra_in_len, const uint8_t *ad, size_t ad_len);

OPENSSL_EXPORT int EVP_AEAD_CTX_open_gather(
    const EVP_AEAD_CTX *ctx, uint8_t *out, const uint8_t *nonce,
    size_t nonce_len, const uint8_t *in, size_t in_len, const uint8_t *in_tag,
    size_t in_tag_len, const uint8_t *ad, size_t ad_len);

OPENSSL_EXPORT const EVP_AEAD *EVP_AEAD_CTX_aead(const EVP_AEAD_CTX *ctx);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_cbc_sha1_tls(void);
OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_cbc_sha1_tls_implicit_iv(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_cbc_sha256_tls(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_cbc_sha1_tls(void);
OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_cbc_sha1_tls_implicit_iv(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_des_ede3_cbc_sha1_tls(void);
OPENSSL_EXPORT const EVP_AEAD *EVP_aead_des_ede3_cbc_sha1_tls_implicit_iv(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_gcm_tls12(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_gcm_tls12(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_128_gcm_tls13(void);

OPENSSL_EXPORT const EVP_AEAD *EVP_aead_aes_256_gcm_tls13(void);

enum evp_aead_direction_t {
  evp_aead_open,
  evp_aead_seal,
};

OPENSSL_EXPORT int EVP_AEAD_CTX_init_with_direction(
    EVP_AEAD_CTX *ctx, const EVP_AEAD *aead, const uint8_t *key, size_t key_len,
    size_t tag_len, enum evp_aead_direction_t dir);

OPENSSL_EXPORT int EVP_AEAD_CTX_get_iv(const EVP_AEAD_CTX *ctx,
                                       const uint8_t **out_iv, size_t *out_len);

OPENSSL_EXPORT int EVP_AEAD_CTX_tag_len(const EVP_AEAD_CTX *ctx,
                                        size_t *out_tag_len,
                                        const size_t in_len,
                                        const size_t extra_in_len);

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

using ScopedEVP_AEAD_CTX =
    internal::StackAllocated<EVP_AEAD_CTX, void, EVP_AEAD_CTX_zero,
                             EVP_AEAD_CTX_cleanup>;

BORINGSSL_MAKE_DELETER(EVP_AEAD_CTX, EVP_AEAD_CTX_free)

BSSL_NAMESPACE_END

}
#endif

#endif

#endif
