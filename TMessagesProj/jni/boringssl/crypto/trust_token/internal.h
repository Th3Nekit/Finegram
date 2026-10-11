// Copyright 2019 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_TRUST_TOKEN_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_TRUST_TOKEN_INTERNAL_H

#include <openssl/base.h>
#include <openssl/ec.h>
#include <openssl/ec_key.h>
#include <openssl/nid.h>

#include "../fipsmodule/ec/internal.h"

#include <openssl/trust_token.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define TRUST_TOKEN_NONCE_SIZE 64

typedef struct {

  EC_AFFINE pub0;
  EC_AFFINE pub1;
  EC_AFFINE pubs;
} TRUST_TOKEN_CLIENT_KEY;

typedef struct {
  EC_SCALAR x0;
  EC_SCALAR y0;
  EC_SCALAR x1;
  EC_SCALAR y1;
  EC_SCALAR xs;
  EC_SCALAR ys;
  EC_AFFINE pub0;
  EC_PRECOMP pub0_precomp;
  EC_AFFINE pub1;
  EC_PRECOMP pub1_precomp;
  EC_AFFINE pubs;
  EC_PRECOMP pubs_precomp;
} TRUST_TOKEN_ISSUER_KEY;

typedef struct pmb_pretoken_st {
  uint8_t salt[TRUST_TOKEN_NONCE_SIZE];
  uint8_t t[TRUST_TOKEN_NONCE_SIZE];
  EC_SCALAR r;
  EC_AFFINE Tp;
} TRUST_TOKEN_PRETOKEN;

OPENSSL_EXPORT void TRUST_TOKEN_PRETOKEN_free(TRUST_TOKEN_PRETOKEN *token);

DEFINE_STACK_OF(TRUST_TOKEN_PRETOKEN)

int pmbtoken_exp1_generate_key(CBB *out_private, CBB *out_public);
int pmbtoken_exp1_derive_key_from_secret(CBB *out_private, CBB *out_public,
                                         const uint8_t *secret,
                                         size_t secret_len);
int pmbtoken_exp1_client_key_from_bytes(TRUST_TOKEN_CLIENT_KEY *key,
                                        const uint8_t *in, size_t len);
int pmbtoken_exp1_issuer_key_from_bytes(TRUST_TOKEN_ISSUER_KEY *key,
                                        const uint8_t *in, size_t len);
STACK_OF(TRUST_TOKEN_PRETOKEN) *pmbtoken_exp1_blind(CBB *cbb, size_t count,
                                                    int include_message,
                                                    const uint8_t *msg,
                                                    size_t msg_len);
int pmbtoken_exp1_sign(const TRUST_TOKEN_ISSUER_KEY *key, CBB *cbb, CBS *cbs,
                       size_t num_requested, size_t num_to_issue,
                       uint8_t private_metadata);
STACK_OF(TRUST_TOKEN) *pmbtoken_exp1_unblind(
    const TRUST_TOKEN_CLIENT_KEY *key,
    const STACK_OF(TRUST_TOKEN_PRETOKEN) *pretokens, CBS *cbs, size_t count,
    uint32_t key_id);
int pmbtoken_exp1_read(const TRUST_TOKEN_ISSUER_KEY *key,
                       uint8_t out_nonce[TRUST_TOKEN_NONCE_SIZE],
                       uint8_t *out_private_metadata, const uint8_t *token,
                       size_t token_len, int include_message,
                       const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int pmbtoken_exp1_get_h_for_testing(uint8_t out[97]);

int pmbtoken_exp2_generate_key(CBB *out_private, CBB *out_public);
int pmbtoken_exp2_derive_key_from_secret(CBB *out_private, CBB *out_public,
                                         const uint8_t *secret,
                                         size_t secret_len);
int pmbtoken_exp2_client_key_from_bytes(TRUST_TOKEN_CLIENT_KEY *key,
                                        const uint8_t *in, size_t len);
int pmbtoken_exp2_issuer_key_from_bytes(TRUST_TOKEN_ISSUER_KEY *key,
                                        const uint8_t *in, size_t len);
STACK_OF(TRUST_TOKEN_PRETOKEN) *pmbtoken_exp2_blind(CBB *cbb, size_t count,
                                                    int include_message,
                                                    const uint8_t *msg,
                                                    size_t msg_len);
int pmbtoken_exp2_sign(const TRUST_TOKEN_ISSUER_KEY *key, CBB *cbb, CBS *cbs,
                       size_t num_requested, size_t num_to_issue,
                       uint8_t private_metadata);
STACK_OF(TRUST_TOKEN) *pmbtoken_exp2_unblind(
    const TRUST_TOKEN_CLIENT_KEY *key,
    const STACK_OF(TRUST_TOKEN_PRETOKEN) *pretokens, CBS *cbs, size_t count,
    uint32_t key_id);
int pmbtoken_exp2_read(const TRUST_TOKEN_ISSUER_KEY *key,
                       uint8_t out_nonce[TRUST_TOKEN_NONCE_SIZE],
                       uint8_t *out_private_metadata, const uint8_t *token,
                       size_t token_len, int include_message,
                       const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int pmbtoken_exp2_get_h_for_testing(uint8_t out[97]);

int pmbtoken_pst1_generate_key(CBB *out_private, CBB *out_public);
int pmbtoken_pst1_derive_key_from_secret(CBB *out_private, CBB *out_public,
                                         const uint8_t *secret,
                                         size_t secret_len);
int pmbtoken_pst1_client_key_from_bytes(TRUST_TOKEN_CLIENT_KEY *key,
                                        const uint8_t *in, size_t len);
int pmbtoken_pst1_issuer_key_from_bytes(TRUST_TOKEN_ISSUER_KEY *key,
                                        const uint8_t *in, size_t len);
STACK_OF(TRUST_TOKEN_PRETOKEN) *pmbtoken_pst1_blind(CBB *cbb, size_t count,
                                                    int include_message,
                                                    const uint8_t *msg,
                                                    size_t msg_len);
int pmbtoken_pst1_sign(const TRUST_TOKEN_ISSUER_KEY *key, CBB *cbb, CBS *cbs,
                       size_t num_requested, size_t num_to_issue,
                       uint8_t private_metadata);
STACK_OF(TRUST_TOKEN) *pmbtoken_pst1_unblind(
    const TRUST_TOKEN_CLIENT_KEY *key,
    const STACK_OF(TRUST_TOKEN_PRETOKEN) *pretokens, CBS *cbs, size_t count,
    uint32_t key_id);
int pmbtoken_pst1_read(const TRUST_TOKEN_ISSUER_KEY *key,
                       uint8_t out_nonce[TRUST_TOKEN_NONCE_SIZE],
                       uint8_t *out_private_metadata, const uint8_t *token,
                       size_t token_len, int include_message,
                       const uint8_t *msg, size_t msg_len);

OPENSSL_EXPORT int pmbtoken_pst1_get_h_for_testing(uint8_t out[97]);

int voprf_exp2_generate_key(CBB *out_private, CBB *out_public);
int voprf_exp2_derive_key_from_secret(CBB *out_private, CBB *out_public,
                                      const uint8_t *secret, size_t secret_len);
int voprf_exp2_client_key_from_bytes(TRUST_TOKEN_CLIENT_KEY *key,
                                     const uint8_t *in, size_t len);
int voprf_exp2_issuer_key_from_bytes(TRUST_TOKEN_ISSUER_KEY *key,
                                     const uint8_t *in, size_t len);
STACK_OF(TRUST_TOKEN_PRETOKEN) *voprf_exp2_blind(CBB *cbb, size_t count,
                                                 int include_message,
                                                 const uint8_t *msg,
                                                 size_t msg_len);
int voprf_exp2_sign(const TRUST_TOKEN_ISSUER_KEY *key, CBB *cbb, CBS *cbs,
                    size_t num_requested, size_t num_to_issue,
                    uint8_t private_metadata);
STACK_OF(TRUST_TOKEN) *voprf_exp2_unblind(
    const TRUST_TOKEN_CLIENT_KEY *key,
    const STACK_OF(TRUST_TOKEN_PRETOKEN) *pretokens, CBS *cbs, size_t count,
    uint32_t key_id);
int voprf_exp2_read(const TRUST_TOKEN_ISSUER_KEY *key,
                    uint8_t out_nonce[TRUST_TOKEN_NONCE_SIZE],
                    uint8_t *out_private_metadata, const uint8_t *token,
                    size_t token_len, int include_message, const uint8_t *msg,
                    size_t msg_len);

int voprf_pst1_generate_key(CBB *out_private, CBB *out_public);
int voprf_pst1_derive_key_from_secret(CBB *out_private, CBB *out_public,
                                      const uint8_t *secret, size_t secret_len);
int voprf_pst1_client_key_from_bytes(TRUST_TOKEN_CLIENT_KEY *key,
                                     const uint8_t *in, size_t len);
int voprf_pst1_issuer_key_from_bytes(TRUST_TOKEN_ISSUER_KEY *key,
                                     const uint8_t *in, size_t len);
STACK_OF(TRUST_TOKEN_PRETOKEN) *voprf_pst1_blind(CBB *cbb, size_t count,
                                                 int include_message,
                                                 const uint8_t *msg,
                                                 size_t msg_len);
int voprf_pst1_sign(const TRUST_TOKEN_ISSUER_KEY *key, CBB *cbb, CBS *cbs,
                    size_t num_requested, size_t num_to_issue,
                    uint8_t private_metadata);
OPENSSL_EXPORT int voprf_pst1_sign_with_proof_scalar_for_testing(
    const TRUST_TOKEN_ISSUER_KEY *key, CBB *cbb, CBS *cbs, size_t num_requested,
    size_t num_to_issue, uint8_t private_metadata,
    const uint8_t *proof_scalar_buf, size_t proof_scalar_len);
STACK_OF(TRUST_TOKEN) *voprf_pst1_unblind(
    const TRUST_TOKEN_CLIENT_KEY *key,
    const STACK_OF(TRUST_TOKEN_PRETOKEN) *pretokens, CBS *cbs, size_t count,
    uint32_t key_id);
int voprf_pst1_read(const TRUST_TOKEN_ISSUER_KEY *key,
                    uint8_t out_nonce[TRUST_TOKEN_NONCE_SIZE],
                    uint8_t *out_private_metadata, const uint8_t *token,
                    size_t token_len, int include_message, const uint8_t *msg,
                    size_t msg_len);

struct trust_token_method_st {

  int (*generate_key)(CBB *out_private, CBB *out_public);

  int (*derive_key_from_secret)(CBB *out_private, CBB *out_public,
                                const uint8_t *secret, size_t secret_len);

  int (*client_key_from_bytes)(TRUST_TOKEN_CLIENT_KEY *key, const uint8_t *in,
                               size_t len);

  int (*issuer_key_from_bytes)(TRUST_TOKEN_ISSUER_KEY *key, const uint8_t *in,
                               size_t len);

  STACK_OF(TRUST_TOKEN_PRETOKEN) *(*blind)(CBB *cbb, size_t count,
                                           int include_message,
                                           const uint8_t *msg, size_t msg_len);

  int (*sign)(const TRUST_TOKEN_ISSUER_KEY *key, CBB *cbb, CBS *cbs,
              size_t num_requested, size_t num_to_issue,
              uint8_t private_metadata);

  STACK_OF(TRUST_TOKEN) *(*unblind)(
      const TRUST_TOKEN_CLIENT_KEY *key,
      const STACK_OF(TRUST_TOKEN_PRETOKEN) *pretokens, CBS *cbs, size_t count,
      uint32_t key_id);

  int (*read)(const TRUST_TOKEN_ISSUER_KEY *key,
              uint8_t out_nonce[TRUST_TOKEN_NONCE_SIZE],
              uint8_t *out_private_metadata, const uint8_t *token,
              size_t token_len, int include_message, const uint8_t *msg,
              size_t msg_len);

  int has_private_metadata;

  size_t max_keys;

  int has_srr;
};

struct trust_token_client_key_st {
  uint32_t id;
  TRUST_TOKEN_CLIENT_KEY key;
};

struct trust_token_issuer_key_st {
  uint32_t id;
  TRUST_TOKEN_ISSUER_KEY key;
};

struct trust_token_client_st {
  const TRUST_TOKEN_METHOD *method;

  uint16_t max_batchsize;

  struct trust_token_client_key_st keys[6];

  size_t num_keys;

  STACK_OF(TRUST_TOKEN_PRETOKEN) *pretokens;

  EVP_PKEY *srr_key;
};

struct trust_token_issuer_st {
  const TRUST_TOKEN_METHOD *method;

  uint16_t max_batchsize;

  struct trust_token_issuer_key_st keys[6];

  size_t num_keys;

  EVP_PKEY *srr_key;

  uint8_t *metadata_key;
  size_t metadata_key_len;
};

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(TRUST_TOKEN_PRETOKEN, TRUST_TOKEN_PRETOKEN_free)

BSSL_NAMESPACE_END

}
#endif

#endif
