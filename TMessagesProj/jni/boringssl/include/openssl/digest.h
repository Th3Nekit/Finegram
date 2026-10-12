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

#ifndef OPENSSL_HEADER_DIGEST_H
#define OPENSSL_HEADER_DIGEST_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT const EVP_MD *EVP_md4(void);
OPENSSL_EXPORT const EVP_MD *EVP_md5(void);
OPENSSL_EXPORT const EVP_MD *EVP_sha1(void);
OPENSSL_EXPORT const EVP_MD *EVP_sha224(void);
OPENSSL_EXPORT const EVP_MD *EVP_sha256(void);
OPENSSL_EXPORT const EVP_MD *EVP_sha384(void);
OPENSSL_EXPORT const EVP_MD *EVP_sha512(void);
OPENSSL_EXPORT const EVP_MD *EVP_sha512_256(void);
OPENSSL_EXPORT const EVP_MD *EVP_blake2b256(void);

OPENSSL_EXPORT const EVP_MD *EVP_md5_sha1(void);

OPENSSL_EXPORT const EVP_MD *EVP_get_digestbynid(int nid);

OPENSSL_EXPORT const EVP_MD *EVP_get_digestbyobj(const ASN1_OBJECT *obj);

OPENSSL_EXPORT void EVP_MD_CTX_init(EVP_MD_CTX *ctx);

OPENSSL_EXPORT EVP_MD_CTX *EVP_MD_CTX_new(void);

OPENSSL_EXPORT int EVP_MD_CTX_cleanup(EVP_MD_CTX *ctx);

OPENSSL_EXPORT void EVP_MD_CTX_cleanse(EVP_MD_CTX *ctx);

OPENSSL_EXPORT void EVP_MD_CTX_free(EVP_MD_CTX *ctx);

OPENSSL_EXPORT int EVP_MD_CTX_copy_ex(EVP_MD_CTX *out, const EVP_MD_CTX *in);

OPENSSL_EXPORT void EVP_MD_CTX_move(EVP_MD_CTX *out, EVP_MD_CTX *in);

OPENSSL_EXPORT int EVP_MD_CTX_reset(EVP_MD_CTX *ctx);

OPENSSL_EXPORT int EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *type,
                                     ENGINE *engine);

OPENSSL_EXPORT int EVP_DigestInit(EVP_MD_CTX *ctx, const EVP_MD *type);

OPENSSL_EXPORT int EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *data,
                                    size_t len);

#define EVP_MAX_MD_SIZE 64  // SHA-512 is the longest so far.

#define EVP_MAX_MD_BLOCK_SIZE 128  // SHA-512 is the longest so far.

OPENSSL_EXPORT int EVP_DigestFinal_ex(EVP_MD_CTX *ctx, uint8_t *md_out,
                                      unsigned int *out_size);

OPENSSL_EXPORT int EVP_DigestFinal(EVP_MD_CTX *ctx, uint8_t *md_out,
                                   unsigned int *out_size);

OPENSSL_EXPORT int EVP_Digest(const void *data, size_t len, uint8_t *md_out,
                              unsigned int *md_out_size, const EVP_MD *type,
                              ENGINE *impl);

OPENSSL_EXPORT int EVP_MD_type(const EVP_MD *md);

OPENSSL_EXPORT uint32_t EVP_MD_flags(const EVP_MD *md);

OPENSSL_EXPORT size_t EVP_MD_size(const EVP_MD *md);

OPENSSL_EXPORT size_t EVP_MD_block_size(const EVP_MD *md);

#define EVP_MD_FLAG_PKEY_DIGEST 1

#define EVP_MD_FLAG_DIGALGID_ABSENT 2

#define EVP_MD_FLAG_XOF 4

OPENSSL_EXPORT const EVP_MD *EVP_MD_CTX_get0_md(const EVP_MD_CTX *ctx);

OPENSSL_EXPORT const EVP_MD *EVP_MD_CTX_md(const EVP_MD_CTX *ctx);

OPENSSL_EXPORT size_t EVP_MD_CTX_size(const EVP_MD_CTX *ctx);

OPENSSL_EXPORT size_t EVP_MD_CTX_block_size(const EVP_MD_CTX *ctx);

OPENSSL_EXPORT int EVP_MD_CTX_type(const EVP_MD_CTX *ctx);

OPENSSL_EXPORT const EVP_MD *EVP_parse_digest_algorithm(CBS *cbs);

OPENSSL_EXPORT int EVP_marshal_digest_algorithm(CBB *cbb, const EVP_MD *md);

OPENSSL_EXPORT int EVP_MD_CTX_copy(EVP_MD_CTX *out, const EVP_MD_CTX *in);

OPENSSL_EXPORT int EVP_add_digest(const EVP_MD *digest);

OPENSSL_EXPORT const EVP_MD *EVP_get_digestbyname(const char *);

OPENSSL_EXPORT const EVP_MD *EVP_dss1(void);

OPENSSL_EXPORT EVP_MD_CTX *EVP_MD_CTX_create(void);

OPENSSL_EXPORT void EVP_MD_CTX_destroy(EVP_MD_CTX *ctx);

OPENSSL_EXPORT int EVP_DigestFinalXOF(EVP_MD_CTX *ctx, uint8_t *out,
                                      size_t len);

OPENSSL_EXPORT uint32_t EVP_MD_meth_get_flags(const EVP_MD *md);

OPENSSL_EXPORT void EVP_MD_CTX_set_flags(EVP_MD_CTX *ctx, int flags);

#define EVP_MD_CTX_FLAG_NON_FIPS_ALLOW 0

OPENSSL_EXPORT int EVP_MD_nid(const EVP_MD *md);

struct evp_md_pctx_ops;

struct env_md_ctx_st {

  const EVP_MD *digest;

  void *md_data;

  EVP_PKEY_CTX *pctx;

  const struct evp_md_pctx_ops *pctx_ops;
}                 ;

#if defined(__cplusplus)
}

#if !defined(BORINGSSL_NO_CXX)
extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(EVP_MD_CTX, EVP_MD_CTX_free)

using ScopedEVP_MD_CTX =
    internal::StackAllocatedMovable<EVP_MD_CTX, int, EVP_MD_CTX_init,
                                    EVP_MD_CTX_cleanup, EVP_MD_CTX_move>;

BSSL_NAMESPACE_END

}
#endif

#endif

#define DIGEST_R_INPUT_NOT_INITIALIZED 100
#define DIGEST_R_DECODE_ERROR 101
#define DIGEST_R_UNKNOWN_HASH 102

#endif
