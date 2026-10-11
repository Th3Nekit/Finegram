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

#ifndef OPENSSL_HEADER_EX_DATA_H
#define OPENSSL_HEADER_EX_DATA_H

#include <openssl/base.h>

#include <openssl/stack.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct crypto_ex_data_st CRYPTO_EX_DATA;

#if 0

OPENSSL_EXPORT int TYPE_get_ex_new_index(long argl, void *argp,
                                         CRYPTO_EX_unused *unused,
                                         CRYPTO_EX_dup *dup_unused,
                                         CRYPTO_EX_free *free_func);

OPENSSL_EXPORT int TYPE_set_ex_data(TYPE *t, int index, void *arg);

OPENSSL_EXPORT void *TYPE_get_ex_data(const TYPE *t, int index);

OPENSSL_EXPORT int TYPE_set_app_data(TYPE *t, void *arg);

OPENSSL_EXPORT void *TYPE_get_app_data(const TYPE *t);

#endif

typedef void CRYPTO_EX_free(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
                            int index, long argl, void *argp);

OPENSSL_EXPORT void CRYPTO_cleanup_all_ex_data(void);

typedef int CRYPTO_EX_dup(CRYPTO_EX_DATA *to, const CRYPTO_EX_DATA *from,
                          void **from_d, int index, long argl, void *argp);

typedef int CRYPTO_EX_unused;

struct crypto_ex_data_st {
  STACK_OF(void) *sk;
};

#if defined(__cplusplus)
}
#endif

#endif
