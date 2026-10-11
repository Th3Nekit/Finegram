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

#ifndef OPENSSL_HEADER_MEM_H
#define OPENSSL_HEADER_MEM_H

#include <openssl/base.h>

#include <stdlib.h>
#include <stdarg.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef _BORINGSSL_PROHIBIT_OPENSSL_MALLOC

OPENSSL_EXPORT void *OPENSSL_malloc(size_t size);

OPENSSL_EXPORT void *OPENSSL_zalloc(size_t size);

OPENSSL_EXPORT void *OPENSSL_calloc(size_t num, size_t size);

OPENSSL_EXPORT void *OPENSSL_realloc(void *ptr, size_t new_size);
#endif

OPENSSL_EXPORT void OPENSSL_free(void *ptr);

OPENSSL_EXPORT void OPENSSL_cleanse(void *ptr, size_t len);

OPENSSL_EXPORT int CRYPTO_memcmp(const void *a, const void *b, size_t len);

OPENSSL_EXPORT uint32_t OPENSSL_hash32(const void *ptr, size_t len);

OPENSSL_EXPORT uint32_t OPENSSL_strhash(const char *s);

OPENSSL_EXPORT char *OPENSSL_strdup(const char *s);

OPENSSL_EXPORT size_t OPENSSL_strnlen(const char *s, size_t len);

OPENSSL_EXPORT int OPENSSL_isalpha(int c);

OPENSSL_EXPORT int OPENSSL_isdigit(int c);

OPENSSL_EXPORT int OPENSSL_isxdigit(int c);

OPENSSL_EXPORT int OPENSSL_fromxdigit(uint8_t *out, int c);

OPENSSL_EXPORT int OPENSSL_isalnum(int c);

OPENSSL_EXPORT int OPENSSL_tolower(int c);

OPENSSL_EXPORT int OPENSSL_isspace(int c);

OPENSSL_EXPORT int OPENSSL_strcasecmp(const char *a, const char *b);

OPENSSL_EXPORT int OPENSSL_strncasecmp(const char *a, const char *b, size_t n);

#define DECIMAL_SIZE(type)	((sizeof(type)*8+2)/3+1)

OPENSSL_EXPORT int BIO_snprintf(char *buf, size_t n, const char *format, ...)
    OPENSSL_PRINTF_FORMAT_FUNC(3, 4);

OPENSSL_EXPORT int BIO_vsnprintf(char *buf, size_t n, const char *format,
                                 va_list args) OPENSSL_PRINTF_FORMAT_FUNC(3, 0);

OPENSSL_EXPORT int OPENSSL_vasprintf(char **str, const char *format,
                                     va_list args)
    OPENSSL_PRINTF_FORMAT_FUNC(2, 0);

OPENSSL_EXPORT int OPENSSL_asprintf(char **str, const char *format, ...)
    OPENSSL_PRINTF_FORMAT_FUNC(2, 3);

OPENSSL_EXPORT char *OPENSSL_strndup(const char *str, size_t size);

OPENSSL_EXPORT void *OPENSSL_memdup(const void *data, size_t size);

OPENSSL_EXPORT size_t OPENSSL_strlcpy(char *dst, const char *src,
                                      size_t dst_size);

OPENSSL_EXPORT size_t OPENSSL_strlcat(char *dst, const char *src,
                                      size_t dst_size);

OPENSSL_EXPORT void *CRYPTO_malloc(size_t size, const char *file, int line);

OPENSSL_EXPORT void *CRYPTO_realloc(void *ptr, size_t new_size,
                                    const char *file, int line);

OPENSSL_EXPORT void CRYPTO_free(void *ptr, const char *file, int line);

OPENSSL_EXPORT void OPENSSL_clear_free(void *ptr, size_t len);

OPENSSL_EXPORT int CRYPTO_secure_malloc_init(size_t size, size_t min_size);

OPENSSL_EXPORT int CRYPTO_secure_malloc_initialized(void);

OPENSSL_EXPORT size_t CRYPTO_secure_used(void);

OPENSSL_EXPORT void *OPENSSL_secure_malloc(size_t size);

OPENSSL_EXPORT void OPENSSL_secure_clear_free(void *ptr, size_t len);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(char, OPENSSL_free)
BORINGSSL_MAKE_DELETER(uint8_t, OPENSSL_free)

BSSL_NAMESPACE_END

}

#endif

#endif
