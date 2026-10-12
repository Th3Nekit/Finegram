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

#ifndef OPENSSL_HEADER_TRUST_TOKEN_H
#define OPENSSL_HEADER_TRUST_TOKEN_H

#include <openssl/base.h>
#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT const TRUST_TOKEN_METHOD *TRUST_TOKEN_experiment_v1(void);

OPENSSL_EXPORT const TRUST_TOKEN_METHOD *TRUST_TOKEN_experiment_v2_voprf(void);

OPENSSL_EXPORT const TRUST_TOKEN_METHOD *TRUST_TOKEN_experiment_v2_pmb(void);

OPENSSL_EXPORT const TRUST_TOKEN_METHOD *TRUST_TOKEN_pst_v1_voprf(void);

OPENSSL_EXPORT const TRUST_TOKEN_METHOD *TRUST_TOKEN_pst_v1_pmb(void);

struct trust_token_st {
  uint8_t *data;
  size_t len;
};

DEFINE_STACK_OF(TRUST_TOKEN)

OPENSSL_EXPORT TRUST_TOKEN *TRUST_TOKEN_new(const uint8_t *data, size_t len);

OPENSSL_EXPORT void TRUST_TOKEN_free(TRUST_TOKEN *token);

#define TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE 512
#define TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE 512

OPENSSL_EXPORT int TRUST_TOKEN_generate_key(
    const TRUST_TOKEN_METHOD *method, uint8_t *out_priv_key,
    size_t *out_priv_key_len, size_t max_priv_key_len, uint8_t *out_pub_key,
    size_t *out_pub_key_len, size_t max_pub_key_len, uint32_t id);

OPENSSL_EXPORT int TRUST_TOKEN_derive_key_from_secret(
    const TRUST_TOKEN_METHOD *method, uint8_t *out_priv_key,
    size_t *out_priv_key_len, size_t max_priv_key_len, uint8_t *out_pub_key,
    size_t *out_pub_key_len, size_t max_pub_key_len, uint32_t id,
    const uint8_t *secret, size_t secret_len);

OPENSSL_EXPORT TRUST_TOKEN_CLIENT *TRUST_TOKEN_CLIENT_new(
    const TRUST_TOKEN_METHOD *method, size_t max_batchsize);

OPENSSL_EXPORT void TRUST_TOKEN_CLIENT_free(TRUST_TOKEN_CLIENT *ctx);

OPENSSL_EXPORT int TRUST_TOKEN_CLIENT_add_key(TRUST_TOKEN_CLIENT *ctx,
                                              size_t *out_key_index,
                                              const uint8_t *key,
                                              size_t key_len);

OPENSSL_EXPORT int TRUST_TOKEN_CLIENT_set_srr_key(TRUST_TOKEN_CLIENT *ctx,
                                                  EVP_PKEY *key);

OPENSSL_EXPORT int TRUST_TOKEN_CLIENT_begin_issuance(TRUST_TOKEN_CLIENT *ctx,
                                                     uint8_t **out,
                                                     size_t *out_len,
                                                     size_t count);

OPENSSL_EXPORT int TRUST_TOKEN_CLIENT_begin_issuance_over_message(
    TRUST_TOKEN_CLIENT *ctx, uint8_t **out, size_t *out_len, size_t count,
    const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT STACK_OF(TRUST_TOKEN) *
    TRUST_TOKEN_CLIENT_finish_issuance(TRUST_TOKEN_CLIENT *ctx,
                                       size_t *out_key_index,
                                       const uint8_t *response,
                                       size_t response_len);

OPENSSL_EXPORT int TRUST_TOKEN_CLIENT_begin_redemption(
    TRUST_TOKEN_CLIENT *ctx, uint8_t **out, size_t *out_len,
    const TRUST_TOKEN *token, const uint8_t *data, size_t data_len,
    uint64_t time);

OPENSSL_EXPORT int TRUST_TOKEN_CLIENT_finish_redemption(
    TRUST_TOKEN_CLIENT *ctx, uint8_t **out_rr, size_t *out_rr_len,
    uint8_t **out_sig, size_t *out_sig_len, const uint8_t *response,
    size_t response_len);

OPENSSL_EXPORT TRUST_TOKEN_ISSUER *TRUST_TOKEN_ISSUER_new(
    const TRUST_TOKEN_METHOD *method, size_t max_batchsize);

OPENSSL_EXPORT void TRUST_TOKEN_ISSUER_free(TRUST_TOKEN_ISSUER *ctx);

OPENSSL_EXPORT int TRUST_TOKEN_ISSUER_add_key(TRUST_TOKEN_ISSUER *ctx,
                                              const uint8_t *key,
                                              size_t key_len);

OPENSSL_EXPORT int TRUST_TOKEN_ISSUER_set_srr_key(TRUST_TOKEN_ISSUER *ctx,
                                                  EVP_PKEY *key);

OPENSSL_EXPORT int TRUST_TOKEN_ISSUER_set_metadata_key(TRUST_TOKEN_ISSUER *ctx,
                                                       const uint8_t *key,
                                                       size_t len);

OPENSSL_EXPORT int TRUST_TOKEN_ISSUER_issue(
    const TRUST_TOKEN_ISSUER *ctx, uint8_t **out, size_t *out_len,
    size_t *out_tokens_issued, const uint8_t *request, size_t request_len,
    uint32_t public_metadata, uint8_t private_metadata, size_t max_issuance);

OPENSSL_EXPORT int TRUST_TOKEN_ISSUER_redeem(
    const TRUST_TOKEN_ISSUER *ctx, uint32_t *out_public, uint8_t *out_private,
    TRUST_TOKEN **out_token, uint8_t **out_client_data,
    size_t *out_client_data_len, const uint8_t *request, size_t request_len);

#define TRUST_TOKEN_ISSUER_redeem_raw TRUST_TOKEN_ISSUER_redeem

OPENSSL_EXPORT int TRUST_TOKEN_ISSUER_redeem_over_message(
    const TRUST_TOKEN_ISSUER *ctx, uint32_t *out_public, uint8_t *out_private,
    TRUST_TOKEN **out_token, uint8_t **out_client_data,
    size_t *out_client_data_len, const uint8_t *request, size_t request_len,
    const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int TRUST_TOKEN_decode_private_metadata(
    const TRUST_TOKEN_METHOD *method, uint8_t *out_value, const uint8_t *key,
    size_t key_len, const uint8_t *nonce, size_t nonce_len,
    uint8_t encrypted_bit);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(TRUST_TOKEN, TRUST_TOKEN_free)
BORINGSSL_MAKE_DELETER(TRUST_TOKEN_CLIENT, TRUST_TOKEN_CLIENT_free)
BORINGSSL_MAKE_DELETER(TRUST_TOKEN_ISSUER, TRUST_TOKEN_ISSUER_free)

BSSL_NAMESPACE_END

}
#endif

#define TRUST_TOKEN_R_KEYGEN_FAILURE 100
#define TRUST_TOKEN_R_BUFFER_TOO_SMALL 101
#define TRUST_TOKEN_R_OVER_BATCHSIZE 102
#define TRUST_TOKEN_R_DECODE_ERROR 103
#define TRUST_TOKEN_R_SRR_SIGNATURE_ERROR 104
#define TRUST_TOKEN_R_DECODE_FAILURE 105
#define TRUST_TOKEN_R_INVALID_METADATA 106
#define TRUST_TOKEN_R_TOO_MANY_KEYS 107
#define TRUST_TOKEN_R_NO_KEYS_CONFIGURED 108
#define TRUST_TOKEN_R_INVALID_KEY_ID 109
#define TRUST_TOKEN_R_INVALID_TOKEN 110
#define TRUST_TOKEN_R_BAD_VALIDITY_CHECK 111
#define TRUST_TOKEN_R_NO_SRR_KEY_CONFIGURED 112
#define TRUST_TOKEN_R_INVALID_METADATA_KEY 113
#define TRUST_TOKEN_R_INVALID_PROOF 114

#endif
