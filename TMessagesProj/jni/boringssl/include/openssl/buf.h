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

#ifndef OPENSSL_HEADER_BUFFER_H
#define OPENSSL_HEADER_BUFFER_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct buf_mem_st {
  size_t length;
  char *data;
  size_t max;
};

OPENSSL_EXPORT BUF_MEM *BUF_MEM_new(void);

OPENSSL_EXPORT void BUF_MEM_free(BUF_MEM *buf);

OPENSSL_EXPORT int BUF_MEM_reserve(BUF_MEM *buf, size_t cap);

OPENSSL_EXPORT size_t BUF_MEM_grow(BUF_MEM *buf, size_t len);

OPENSSL_EXPORT size_t BUF_MEM_grow_clean(BUF_MEM *buf, size_t len);

OPENSSL_EXPORT int BUF_MEM_append(BUF_MEM *buf, const void *in, size_t len);

OPENSSL_EXPORT char *BUF_strdup(const char *str);

OPENSSL_EXPORT size_t BUF_strnlen(const char *str, size_t max_len);

OPENSSL_EXPORT char *BUF_strndup(const char *str, size_t size);

OPENSSL_EXPORT void *BUF_memdup(const void *data, size_t size);

OPENSSL_EXPORT size_t BUF_strlcpy(char *dst, const char *src, size_t dst_size);

OPENSSL_EXPORT size_t BUF_strlcat(char *dst, const char *src, size_t dst_size);

#if defined(__cplusplus)
}

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(BUF_MEM, BUF_MEM_free)

BSSL_NAMESPACE_END

}

#endif

#endif
