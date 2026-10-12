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

#ifndef OPENSSL_HEADER_ERR_H
#define OPENSSL_HEADER_ERR_H

#include <stdio.h>

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT void ERR_load_BIO_strings(void);

OPENSSL_EXPORT void ERR_load_ERR_strings(void);

OPENSSL_EXPORT void ERR_load_crypto_strings(void);

OPENSSL_EXPORT void ERR_load_RAND_strings(void);

OPENSSL_EXPORT void ERR_free_strings(void);

OPENSSL_INLINE int ERR_GET_LIB(uint32_t packed_error) {
  return (int)((packed_error >> 24) & 0xff);
}

OPENSSL_INLINE int ERR_GET_REASON(uint32_t packed_error) {
  return (int)(packed_error & 0xfff);
}

OPENSSL_EXPORT uint32_t ERR_get_error(void);

OPENSSL_EXPORT uint32_t ERR_get_error_line(const char **file, int *line);

#define ERR_FLAG_STRING 1

#define ERR_FLAG_MALLOCED 2

OPENSSL_EXPORT uint32_t ERR_get_error_line_data(const char **file, int *line,
                                                const char **data, int *flags);

OPENSSL_EXPORT uint32_t ERR_peek_error(void);
OPENSSL_EXPORT uint32_t ERR_peek_error_line(const char **file, int *line);
OPENSSL_EXPORT uint32_t ERR_peek_error_line_data(const char **file, int *line,
                                                 const char **data, int *flags);

OPENSSL_EXPORT uint32_t ERR_peek_last_error(void);
OPENSSL_EXPORT uint32_t ERR_peek_last_error_line(const char **file, int *line);
OPENSSL_EXPORT uint32_t ERR_peek_last_error_line_data(const char **file,
                                                      int *line,
                                                      const char **data,
                                                      int *flags);

OPENSSL_EXPORT char *ERR_error_string_n(uint32_t packed_error, char *buf,
                                        size_t len);

OPENSSL_EXPORT const char *ERR_lib_error_string(uint32_t packed_error);

OPENSSL_EXPORT const char *ERR_reason_error_string(uint32_t packed_error);

OPENSSL_EXPORT const char *ERR_lib_symbol_name(uint32_t packed_error);

OPENSSL_EXPORT const char *ERR_reason_symbol_name(uint32_t packed_error);

typedef int (*ERR_print_errors_callback_t)(const char *str, size_t len,
                                           void *ctx);

OPENSSL_EXPORT void ERR_print_errors_cb(ERR_print_errors_callback_t callback,
                                        void *ctx);

OPENSSL_EXPORT void ERR_print_errors_fp(FILE *file);

OPENSSL_EXPORT void ERR_clear_error(void);

OPENSSL_EXPORT int ERR_set_mark(void);

OPENSSL_EXPORT int ERR_pop_to_mark(void);

OPENSSL_EXPORT int ERR_get_next_error_library(void);

enum {
  ERR_LIB_NONE = 1,
  ERR_LIB_SYS,
  ERR_LIB_BN,
  ERR_LIB_RSA,
  ERR_LIB_DH,
  ERR_LIB_EVP,
  ERR_LIB_BUF,
  ERR_LIB_OBJ,
  ERR_LIB_PEM,
  ERR_LIB_DSA,
  ERR_LIB_X509,
  ERR_LIB_ASN1,
  ERR_LIB_CONF,
  ERR_LIB_CRYPTO,
  ERR_LIB_EC,
  ERR_LIB_SSL,
  ERR_LIB_BIO,
  ERR_LIB_PKCS7,
  ERR_LIB_PKCS8,
  ERR_LIB_X509V3,
  ERR_LIB_RAND,
  ERR_LIB_ENGINE,
  ERR_LIB_OCSP,
  ERR_LIB_UI,
  ERR_LIB_COMP,
  ERR_LIB_ECDSA,
  ERR_LIB_ECDH,
  ERR_LIB_HMAC,
  ERR_LIB_DIGEST,
  ERR_LIB_CIPHER,
  ERR_LIB_HKDF,
  ERR_LIB_TRUST_TOKEN,
  ERR_LIB_USER,
  ERR_NUM_LIBS
};

#define ERR_R_SYS_LIB ERR_LIB_SYS
#define ERR_R_BN_LIB ERR_LIB_BN
#define ERR_R_RSA_LIB ERR_LIB_RSA
#define ERR_R_DH_LIB ERR_LIB_DH
#define ERR_R_EVP_LIB ERR_LIB_EVP
#define ERR_R_BUF_LIB ERR_LIB_BUF
#define ERR_R_OBJ_LIB ERR_LIB_OBJ
#define ERR_R_PEM_LIB ERR_LIB_PEM
#define ERR_R_DSA_LIB ERR_LIB_DSA
#define ERR_R_X509_LIB ERR_LIB_X509
#define ERR_R_ASN1_LIB ERR_LIB_ASN1
#define ERR_R_CONF_LIB ERR_LIB_CONF
#define ERR_R_CRYPTO_LIB ERR_LIB_CRYPTO
#define ERR_R_EC_LIB ERR_LIB_EC
#define ERR_R_SSL_LIB ERR_LIB_SSL
#define ERR_R_BIO_LIB ERR_LIB_BIO
#define ERR_R_PKCS7_LIB ERR_LIB_PKCS7
#define ERR_R_PKCS8_LIB ERR_LIB_PKCS8
#define ERR_R_X509V3_LIB ERR_LIB_X509V3
#define ERR_R_RAND_LIB ERR_LIB_RAND
#define ERR_R_DSO_LIB ERR_LIB_DSO
#define ERR_R_ENGINE_LIB ERR_LIB_ENGINE
#define ERR_R_OCSP_LIB ERR_LIB_OCSP
#define ERR_R_UI_LIB ERR_LIB_UI
#define ERR_R_COMP_LIB ERR_LIB_COMP
#define ERR_R_ECDSA_LIB ERR_LIB_ECDSA
#define ERR_R_ECDH_LIB ERR_LIB_ECDH
#define ERR_R_STORE_LIB ERR_LIB_STORE
#define ERR_R_FIPS_LIB ERR_LIB_FIPS
#define ERR_R_CMS_LIB ERR_LIB_CMS
#define ERR_R_TS_LIB ERR_LIB_TS
#define ERR_R_HMAC_LIB ERR_LIB_HMAC
#define ERR_R_JPAKE_LIB ERR_LIB_JPAKE
#define ERR_R_USER_LIB ERR_LIB_USER
#define ERR_R_DIGEST_LIB ERR_LIB_DIGEST
#define ERR_R_CIPHER_LIB ERR_LIB_CIPHER
#define ERR_R_HKDF_LIB ERR_LIB_HKDF
#define ERR_R_TRUST_TOKEN_LIB ERR_LIB_TRUST_TOKEN

#define ERR_R_FATAL 64
#define ERR_R_MALLOC_FAILURE (1 | ERR_R_FATAL)
#define ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED (2 | ERR_R_FATAL)
#define ERR_R_PASSED_NULL_PARAMETER (3 | ERR_R_FATAL)
#define ERR_R_INTERNAL_ERROR (4 | ERR_R_FATAL)
#define ERR_R_OVERFLOW (5 | ERR_R_FATAL)

OPENSSL_EXPORT void ERR_remove_state(unsigned long pid);

OPENSSL_EXPORT void ERR_remove_thread_state(const CRYPTO_THREADID *tid);

OPENSSL_EXPORT const char *ERR_func_error_string(uint32_t packed_error);

OPENSSL_EXPORT char *ERR_error_string(uint32_t packed_error, char *buf);
#define ERR_ERROR_STRING_BUF_LEN 120

OPENSSL_INLINE int ERR_GET_FUNC(uint32_t packed_error) {
  (void)packed_error;
  return 0;
}

#define ERR_TXT_STRING ERR_FLAG_STRING
#define ERR_TXT_MALLOCED ERR_FLAG_MALLOCED

OPENSSL_EXPORT void ERR_clear_system_error(void);

#define OPENSSL_PUT_ERROR(library, reason) \
  ERR_put_error(ERR_LIB_##library, 0, reason, __FILE__, __LINE__)

#define OPENSSL_PUT_SYSTEM_ERROR() \
  ERR_put_error(ERR_LIB_SYS, 0, 0, __FILE__, __LINE__);

OPENSSL_EXPORT void ERR_put_error(int library, int unused, int reason,
                                  const char *file, unsigned line);

OPENSSL_EXPORT void ERR_add_error_data(unsigned count, ...);

OPENSSL_EXPORT void ERR_add_error_dataf(const char *format, ...)
    OPENSSL_PRINTF_FORMAT_FUNC(1, 2);

OPENSSL_EXPORT void ERR_set_error_data(char *data, int flags);

#define ERR_NUM_ERRORS 16

#define ERR_PACK(lib, reason)                                              \
  (((((uint32_t)(lib)) & 0xff) << 24) | ((((uint32_t)(reason)) & 0xfff)))

#define OPENSSL_DECLARE_ERROR_REASON(lib, reason)

#if defined(__cplusplus)
}
#endif

#endif
