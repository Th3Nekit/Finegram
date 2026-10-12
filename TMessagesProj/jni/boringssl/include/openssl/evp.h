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

#ifndef OPENSSL_HEADER_EVP_H
#define OPENSSL_HEADER_EVP_H

#include <openssl/base.h>

#include <openssl/evp_errors.h>
#include <openssl/thread.h>

#include <openssl/aead.h>
#include <openssl/base64.h>
#include <openssl/cipher.h>
#include <openssl/digest.h>
#include <openssl/nid.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT EVP_PKEY *EVP_PKEY_new(void);

OPENSSL_EXPORT void EVP_PKEY_free(EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_up_ref(EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_is_opaque(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_cmp(const EVP_PKEY *a, const EVP_PKEY *b);

OPENSSL_EXPORT int EVP_PKEY_copy_parameters(EVP_PKEY *to, const EVP_PKEY *from);

OPENSSL_EXPORT int EVP_PKEY_missing_parameters(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_size(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_bits(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_id(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_set1_RSA(EVP_PKEY *pkey, RSA *key);
OPENSSL_EXPORT int EVP_PKEY_assign_RSA(EVP_PKEY *pkey, RSA *key);
OPENSSL_EXPORT RSA *EVP_PKEY_get0_RSA(const EVP_PKEY *pkey);
OPENSSL_EXPORT RSA *EVP_PKEY_get1_RSA(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_set1_DSA(EVP_PKEY *pkey, DSA *key);
OPENSSL_EXPORT int EVP_PKEY_assign_DSA(EVP_PKEY *pkey, DSA *key);
OPENSSL_EXPORT DSA *EVP_PKEY_get0_DSA(const EVP_PKEY *pkey);
OPENSSL_EXPORT DSA *EVP_PKEY_get1_DSA(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_set1_EC_KEY(EVP_PKEY *pkey, EC_KEY *key);
OPENSSL_EXPORT int EVP_PKEY_assign_EC_KEY(EVP_PKEY *pkey, EC_KEY *key);
OPENSSL_EXPORT EC_KEY *EVP_PKEY_get0_EC_KEY(const EVP_PKEY *pkey);
OPENSSL_EXPORT EC_KEY *EVP_PKEY_get1_EC_KEY(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_set1_DH(EVP_PKEY *pkey, DH *key);
OPENSSL_EXPORT int EVP_PKEY_assign_DH(EVP_PKEY *pkey, DH *key);
OPENSSL_EXPORT DH *EVP_PKEY_get0_DH(const EVP_PKEY *pkey);
OPENSSL_EXPORT DH *EVP_PKEY_get1_DH(const EVP_PKEY *pkey);

#define EVP_PKEY_NONE NID_undef
#define EVP_PKEY_RSA NID_rsaEncryption
#define EVP_PKEY_RSA_PSS NID_rsassaPss
#define EVP_PKEY_DSA NID_dsa
#define EVP_PKEY_EC NID_X9_62_id_ecPublicKey
#define EVP_PKEY_ED25519 NID_ED25519
#define EVP_PKEY_X25519 NID_X25519
#define EVP_PKEY_HKDF NID_hkdf
#define EVP_PKEY_DH NID_dhKeyAgreement

OPENSSL_EXPORT int EVP_PKEY_set_type(EVP_PKEY *pkey, int type);

OPENSSL_EXPORT int EVP_PKEY_cmp_parameters(const EVP_PKEY *a,
                                           const EVP_PKEY *b);

OPENSSL_EXPORT EVP_PKEY *EVP_parse_public_key(CBS *cbs);

OPENSSL_EXPORT int EVP_marshal_public_key(CBB *cbb, const EVP_PKEY *key);

OPENSSL_EXPORT EVP_PKEY *EVP_parse_private_key(CBS *cbs);

OPENSSL_EXPORT int EVP_marshal_private_key(CBB *cbb, const EVP_PKEY *key);

OPENSSL_EXPORT EVP_PKEY *EVP_PKEY_new_raw_private_key(int type, ENGINE *unused,
                                                      const uint8_t *in,
                                                      size_t len);

OPENSSL_EXPORT EVP_PKEY *EVP_PKEY_new_raw_public_key(int type, ENGINE *unused,
                                                     const uint8_t *in,
                                                     size_t len);

OPENSSL_EXPORT int EVP_PKEY_get_raw_private_key(const EVP_PKEY *pkey,
                                                uint8_t *out, size_t *out_len);

OPENSSL_EXPORT int EVP_PKEY_get_raw_public_key(const EVP_PKEY *pkey,
                                               uint8_t *out, size_t *out_len);

OPENSSL_EXPORT int EVP_DigestSignInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
                                      const EVP_MD *type, ENGINE *e,
                                      EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_DigestSignUpdate(EVP_MD_CTX *ctx, const void *data,
                                        size_t len);

OPENSSL_EXPORT int EVP_DigestSignFinal(EVP_MD_CTX *ctx, uint8_t *out_sig,
                                       size_t *out_sig_len);

OPENSSL_EXPORT int EVP_DigestSign(EVP_MD_CTX *ctx, uint8_t *out_sig,
                                  size_t *out_sig_len, const uint8_t *data,
                                  size_t data_len);

OPENSSL_EXPORT int EVP_DigestVerifyInit(EVP_MD_CTX *ctx, EVP_PKEY_CTX **pctx,
                                        const EVP_MD *type, ENGINE *e,
                                        EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_DigestVerifyUpdate(EVP_MD_CTX *ctx, const void *data,
                                          size_t len);

OPENSSL_EXPORT int EVP_DigestVerifyFinal(EVP_MD_CTX *ctx, const uint8_t *sig,
                                         size_t sig_len);

OPENSSL_EXPORT int EVP_DigestVerify(EVP_MD_CTX *ctx, const uint8_t *sig,
                                    size_t sig_len, const uint8_t *data,
                                    size_t len);

OPENSSL_EXPORT int EVP_SignInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type,
                                   ENGINE *impl);

OPENSSL_EXPORT int EVP_SignInit(EVP_MD_CTX *ctx, const EVP_MD *type);

OPENSSL_EXPORT int EVP_SignUpdate(EVP_MD_CTX *ctx, const void *data,
                                  size_t len);

OPENSSL_EXPORT int EVP_SignFinal(const EVP_MD_CTX *ctx, uint8_t *sig,
                                 unsigned int *out_sig_len, EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_VerifyInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type,
                                     ENGINE *impl);

OPENSSL_EXPORT int EVP_VerifyInit(EVP_MD_CTX *ctx, const EVP_MD *type);

OPENSSL_EXPORT int EVP_VerifyUpdate(EVP_MD_CTX *ctx, const void *data,
                                    size_t len);

OPENSSL_EXPORT int EVP_VerifyFinal(EVP_MD_CTX *ctx, const uint8_t *sig,
                                   size_t sig_len, EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_print_public(BIO *out, const EVP_PKEY *pkey,
                                         int indent, ASN1_PCTX *pctx);

OPENSSL_EXPORT int EVP_PKEY_print_private(BIO *out, const EVP_PKEY *pkey,
                                          int indent, ASN1_PCTX *pctx);

OPENSSL_EXPORT int EVP_PKEY_print_params(BIO *out, const EVP_PKEY *pkey,
                                         int indent, ASN1_PCTX *pctx);

OPENSSL_EXPORT int PKCS5_PBKDF2_HMAC(const char *password, size_t password_len,
                                     const uint8_t *salt, size_t salt_len,
                                     uint32_t iterations, const EVP_MD *digest,
                                     size_t key_len, uint8_t *out_key);

OPENSSL_EXPORT int PKCS5_PBKDF2_HMAC_SHA1(const char *password,
                                          size_t password_len,
                                          const uint8_t *salt, size_t salt_len,
                                          uint32_t iterations, size_t key_len,
                                          uint8_t *out_key);

OPENSSL_EXPORT int EVP_PBE_scrypt(const char *password, size_t password_len,
                                  const uint8_t *salt, size_t salt_len,
                                  uint64_t N, uint64_t r, uint64_t p,
                                  size_t max_mem, uint8_t *out_key,
                                  size_t key_len);

OPENSSL_EXPORT EVP_PKEY_CTX *EVP_PKEY_CTX_new(EVP_PKEY *pkey, ENGINE *e);

OPENSSL_EXPORT EVP_PKEY_CTX *EVP_PKEY_CTX_new_id(int id, ENGINE *e);

OPENSSL_EXPORT void EVP_PKEY_CTX_free(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT EVP_PKEY_CTX *EVP_PKEY_CTX_dup(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT EVP_PKEY *EVP_PKEY_CTX_get0_pkey(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_sign_init(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_sign(EVP_PKEY_CTX *ctx, uint8_t *sig,
                                 size_t *sig_len, const uint8_t *digest,
                                 size_t digest_len);

OPENSSL_EXPORT int EVP_PKEY_verify_init(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_verify(EVP_PKEY_CTX *ctx, const uint8_t *sig,
                                   size_t sig_len, const uint8_t *digest,
                                   size_t digest_len);

OPENSSL_EXPORT int EVP_PKEY_encrypt_init(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_encrypt(EVP_PKEY_CTX *ctx, uint8_t *out,
                                    size_t *out_len, const uint8_t *in,
                                    size_t in_len);

OPENSSL_EXPORT int EVP_PKEY_decrypt_init(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_decrypt(EVP_PKEY_CTX *ctx, uint8_t *out,
                                    size_t *out_len, const uint8_t *in,
                                    size_t in_len);

OPENSSL_EXPORT int EVP_PKEY_verify_recover_init(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_verify_recover(EVP_PKEY_CTX *ctx, uint8_t *out,
                                           size_t *out_len, const uint8_t *sig,
                                           size_t siglen);

OPENSSL_EXPORT int EVP_PKEY_derive_init(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_derive_set_peer(EVP_PKEY_CTX *ctx, EVP_PKEY *peer);

OPENSSL_EXPORT int EVP_PKEY_derive(EVP_PKEY_CTX *ctx, uint8_t *key,
                                   size_t *out_key_len);

OPENSSL_EXPORT int EVP_PKEY_keygen_init(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_keygen(EVP_PKEY_CTX *ctx, EVP_PKEY **out_pkey);

OPENSSL_EXPORT int EVP_PKEY_paramgen_init(EVP_PKEY_CTX *ctx);

OPENSSL_EXPORT int EVP_PKEY_paramgen(EVP_PKEY_CTX *ctx, EVP_PKEY **out_pkey);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_signature_md(EVP_PKEY_CTX *ctx,
                                                 const EVP_MD *md);

OPENSSL_EXPORT int EVP_PKEY_CTX_get_signature_md(EVP_PKEY_CTX *ctx,
                                                 const EVP_MD **out_md);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_padding(EVP_PKEY_CTX *ctx, int padding);

OPENSSL_EXPORT int EVP_PKEY_CTX_get_rsa_padding(EVP_PKEY_CTX *ctx,
                                                int *out_padding);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_pss_saltlen(EVP_PKEY_CTX *ctx,
                                                    int salt_len);

OPENSSL_EXPORT int EVP_PKEY_CTX_get_rsa_pss_saltlen(EVP_PKEY_CTX *ctx,
                                                    int *out_salt_len);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_keygen_bits(EVP_PKEY_CTX *ctx,
                                                    int bits);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_keygen_pubexp(EVP_PKEY_CTX *ctx,
                                                      BIGNUM *e);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_oaep_md(EVP_PKEY_CTX *ctx,
                                                const EVP_MD *md);

OPENSSL_EXPORT int EVP_PKEY_CTX_get_rsa_oaep_md(EVP_PKEY_CTX *ctx,
                                                const EVP_MD **out_md);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_mgf1_md(EVP_PKEY_CTX *ctx,
                                                const EVP_MD *md);

OPENSSL_EXPORT int EVP_PKEY_CTX_get_rsa_mgf1_md(EVP_PKEY_CTX *ctx,
                                                const EVP_MD **out_md);

OPENSSL_EXPORT int EVP_PKEY_CTX_set0_rsa_oaep_label(EVP_PKEY_CTX *ctx,
                                                    uint8_t *label,
                                                    size_t label_len);

OPENSSL_EXPORT int EVP_PKEY_CTX_get0_rsa_oaep_label(EVP_PKEY_CTX *ctx,
                                                    const uint8_t **out_label);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_ec_paramgen_curve_nid(EVP_PKEY_CTX *ctx,
                                                          int nid);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_dh_pad(EVP_PKEY_CTX *ctx, int pad);

#define EVP_PKEY_RSA2 NID_rsa

#define EVP_PKEY_X448 NID_X448

#define EVP_PKEY_ED448 NID_ED448

OPENSSL_EXPORT void *EVP_PKEY_get0(const EVP_PKEY *pkey);

OPENSSL_EXPORT void OpenSSL_add_all_algorithms(void);

OPENSSL_EXPORT void OPENSSL_add_all_algorithms_conf(void);

OPENSSL_EXPORT void OpenSSL_add_all_ciphers(void);

OPENSSL_EXPORT void OpenSSL_add_all_digests(void);

OPENSSL_EXPORT void EVP_cleanup(void);

OPENSSL_EXPORT void EVP_CIPHER_do_all_sorted(
    void (*callback)(const EVP_CIPHER *cipher, const char *name,
                     const char *unused, void *arg),
    void *arg);

OPENSSL_EXPORT void EVP_MD_do_all_sorted(void (*callback)(const EVP_MD *cipher,
                                                          const char *name,
                                                          const char *unused,
                                                          void *arg),
                                         void *arg);

OPENSSL_EXPORT void EVP_MD_do_all(void (*callback)(const EVP_MD *cipher,
                                                   const char *name,
                                                   const char *unused,
                                                   void *arg),
                                  void *arg);

OPENSSL_EXPORT int i2d_PrivateKey(const EVP_PKEY *key, uint8_t **outp);

OPENSSL_EXPORT int i2d_PublicKey(const EVP_PKEY *key, uint8_t **outp);

OPENSSL_EXPORT EVP_PKEY *d2i_PrivateKey(int type, EVP_PKEY **out,
                                        const uint8_t **inp, long len);

OPENSSL_EXPORT EVP_PKEY *d2i_AutoPrivateKey(EVP_PKEY **out, const uint8_t **inp,
                                            long len);

OPENSSL_EXPORT EVP_PKEY *d2i_PublicKey(int type, EVP_PKEY **out,
                                       const uint8_t **inp, long len);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_ec_param_enc(EVP_PKEY_CTX *ctx,
                                                 int encoding);

OPENSSL_EXPORT int EVP_PKEY_set1_tls_encodedpoint(EVP_PKEY *pkey,
                                                  const uint8_t *in,
                                                  size_t len);

OPENSSL_EXPORT size_t EVP_PKEY_get1_tls_encodedpoint(const EVP_PKEY *pkey,
                                                     uint8_t **out_ptr);

OPENSSL_EXPORT int EVP_PKEY_base_id(const EVP_PKEY *pkey);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_pss_keygen_md(EVP_PKEY_CTX *ctx,
                                                      const EVP_MD *md);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_pss_keygen_saltlen(EVP_PKEY_CTX *ctx,
                                                           int salt_len);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_rsa_pss_keygen_mgf1_md(EVP_PKEY_CTX *ctx,
                                                           const EVP_MD *md);

OPENSSL_EXPORT int i2d_PUBKEY(const EVP_PKEY *pkey, uint8_t **outp);

OPENSSL_EXPORT EVP_PKEY *d2i_PUBKEY(EVP_PKEY **out, const uint8_t **inp,
                                    long len);

OPENSSL_EXPORT int i2d_RSA_PUBKEY(const RSA *rsa, uint8_t **outp);

OPENSSL_EXPORT RSA *d2i_RSA_PUBKEY(RSA **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_DSA_PUBKEY(const DSA *dsa, uint8_t **outp);

OPENSSL_EXPORT DSA *d2i_DSA_PUBKEY(DSA **out, const uint8_t **inp, long len);

OPENSSL_EXPORT int i2d_EC_PUBKEY(const EC_KEY *ec_key, uint8_t **outp);

OPENSSL_EXPORT EC_KEY *d2i_EC_PUBKEY(EC_KEY **out, const uint8_t **inp,
                                     long len);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_dsa_paramgen_bits(EVP_PKEY_CTX *ctx,
                                                      int nbits);

OPENSSL_EXPORT int EVP_PKEY_CTX_set_dsa_paramgen_q_bits(EVP_PKEY_CTX *ctx,
                                                        int qbits);

OPENSSL_EXPORT int EVP_PKEY_assign(EVP_PKEY *pkey, int type, void *key);

OPENSSL_EXPORT int EVP_PKEY_type(int nid);

#if !defined(BORINGSSL_PREFIX)
#define EVP_PKEY_CTX_set_rsa_oaep_md EVP_PKEY_CTX_set_rsa_oaep_md
#define EVP_PKEY_CTX_set0_rsa_oaep_label EVP_PKEY_CTX_set0_rsa_oaep_label
#endif

#define EVPerr(function, reason) \
  ERR_put_error(ERR_LIB_EVP, 0, reason, __FILE__, __LINE__)

#if defined(__cplusplus)
}

extern "C++" {
BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(EVP_PKEY, EVP_PKEY_free)
BORINGSSL_MAKE_UP_REF(EVP_PKEY, EVP_PKEY_up_ref)
BORINGSSL_MAKE_DELETER(EVP_PKEY_CTX, EVP_PKEY_CTX_free)

BSSL_NAMESPACE_END

}

#endif

#endif
