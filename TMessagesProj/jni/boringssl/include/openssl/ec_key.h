// Copyright 2000-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OPENSSL_HEADER_EC_KEY_H
#define OPENSSL_HEADER_EC_KEY_H

#include <openssl/base.h>

#include <openssl/ec.h>
#include <openssl/engine.h>
#include <openssl/ex_data.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT EC_KEY *EC_KEY_new(void);

OPENSSL_EXPORT EC_KEY *EC_KEY_new_method(const ENGINE *engine);

OPENSSL_EXPORT EC_KEY *EC_KEY_new_by_curve_name(int nid);

OPENSSL_EXPORT void EC_KEY_free(EC_KEY *key);

OPENSSL_EXPORT EC_KEY *EC_KEY_dup(const EC_KEY *src);

OPENSSL_EXPORT int EC_KEY_up_ref(EC_KEY *key);

OPENSSL_EXPORT int EC_KEY_is_opaque(const EC_KEY *key);

OPENSSL_EXPORT const EC_GROUP *EC_KEY_get0_group(const EC_KEY *key);

OPENSSL_EXPORT int EC_KEY_set_group(EC_KEY *key, const EC_GROUP *group);

OPENSSL_EXPORT const BIGNUM *EC_KEY_get0_private_key(const EC_KEY *key);

OPENSSL_EXPORT int EC_KEY_set_private_key(EC_KEY *key, const BIGNUM *priv);

OPENSSL_EXPORT const EC_POINT *EC_KEY_get0_public_key(const EC_KEY *key);

OPENSSL_EXPORT int EC_KEY_set_public_key(EC_KEY *key, const EC_POINT *pub);

#define EC_PKEY_NO_PARAMETERS 0x001
#define EC_PKEY_NO_PUBKEY 0x002

OPENSSL_EXPORT unsigned EC_KEY_get_enc_flags(const EC_KEY *key);

OPENSSL_EXPORT void EC_KEY_set_enc_flags(EC_KEY *key, unsigned flags);

OPENSSL_EXPORT point_conversion_form_t EC_KEY_get_conv_form(const EC_KEY *key);

OPENSSL_EXPORT void EC_KEY_set_conv_form(EC_KEY *key,
                                         point_conversion_form_t cform);

OPENSSL_EXPORT int EC_KEY_check_key(const EC_KEY *key);

OPENSSL_EXPORT int EC_KEY_check_fips(const EC_KEY *key);

OPENSSL_EXPORT int EC_KEY_set_public_key_affine_coordinates(EC_KEY *key,
                                                            const BIGNUM *x,
                                                            const BIGNUM *y);

OPENSSL_EXPORT int EC_KEY_oct2key(EC_KEY *key, const uint8_t *in, size_t len,
                                  BN_CTX *ctx);

OPENSSL_EXPORT size_t EC_KEY_key2buf(const EC_KEY *key,
                                     point_conversion_form_t form,
                                     uint8_t **out_buf, BN_CTX *ctx);

OPENSSL_EXPORT int EC_KEY_oct2priv(EC_KEY *key, const uint8_t *in, size_t len);

OPENSSL_EXPORT size_t EC_KEY_priv2oct(const EC_KEY *key, uint8_t *out,
                                      size_t max_out);

OPENSSL_EXPORT size_t EC_KEY_priv2buf(const EC_KEY *key, uint8_t **out_buf);

OPENSSL_EXPORT int EC_KEY_generate_key(EC_KEY *key);

OPENSSL_EXPORT int EC_KEY_generate_key_fips(EC_KEY *key);

OPENSSL_EXPORT EC_KEY *EC_KEY_derive_from_secret(const EC_GROUP *group,
                                                 const uint8_t *secret,
                                                 size_t secret_len);

OPENSSL_EXPORT EC_KEY *EC_KEY_parse_private_key(CBS *cbs,
                                                const EC_GROUP *group);

OPENSSL_EXPORT int EC_KEY_marshal_private_key(CBB *cbb, const EC_KEY *key,
                                              unsigned enc_flags);

OPENSSL_EXPORT EC_GROUP *EC_KEY_parse_curve_name(CBS *cbs);

OPENSSL_EXPORT int EC_KEY_marshal_curve_name(CBB *cbb, const EC_GROUP *group);

OPENSSL_EXPORT EC_GROUP *EC_KEY_parse_parameters(CBS *cbs);

OPENSSL_EXPORT int EC_KEY_get_ex_new_index(long argl, void *argp,
                                           CRYPTO_EX_unused *unused,
                                           CRYPTO_EX_dup *dup_unused,
                                           CRYPTO_EX_free *free_func);
OPENSSL_EXPORT int EC_KEY_set_ex_data(EC_KEY *r, int idx, void *arg);
OPENSSL_EXPORT void *EC_KEY_get_ex_data(const EC_KEY *r, int idx);

#define ECDSA_FLAG_OPAQUE 1

struct ecdsa_method_st {
  struct openssl_method_common_st common;

  void *app_data;

  int (*init)(EC_KEY *key);
  int (*finish)(EC_KEY *key);

  int (*sign)(const uint8_t *digest, size_t digest_len, uint8_t *sig,
              unsigned int *sig_len, EC_KEY *eckey);

  int flags;
};

OPENSSL_EXPORT void EC_KEY_set_asn1_flag(EC_KEY *key, int flag);

OPENSSL_EXPORT EC_KEY *d2i_ECPrivateKey(EC_KEY **out_key, const uint8_t **inp,
                                        long len);

OPENSSL_EXPORT int i2d_ECPrivateKey(const EC_KEY *key, uint8_t **outp);

OPENSSL_EXPORT EC_GROUP *d2i_ECPKParameters(EC_GROUP **out, const uint8_t **inp,
                                            long len);

OPENSSL_EXPORT int i2d_ECPKParameters(const EC_GROUP *group, uint8_t **outp);

OPENSSL_EXPORT EC_KEY *d2i_ECParameters(EC_KEY **out_key, const uint8_t **inp,
                                        long len);

OPENSSL_EXPORT int i2d_ECParameters(const EC_KEY *key, uint8_t **outp);

OPENSSL_EXPORT EC_KEY *o2i_ECPublicKey(EC_KEY **out_key, const uint8_t **inp,
                                       long len);

OPENSSL_EXPORT int i2o_ECPublicKey(const EC_KEY *key, unsigned char **outp);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(EC_KEY, EC_KEY_free)
BORINGSSL_MAKE_UP_REF(EC_KEY, EC_KEY_up_ref)

BSSL_NAMESPACE_END

}

#endif

#endif
