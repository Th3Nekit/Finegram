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

#ifndef OPENSSL_HEADER_CRYPTO_LHASH_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_LHASH_INTERNAL_H

#include <openssl/lhash.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef int (*lhash_cmp_func)(const void *a, const void *b);
typedef int (*lhash_cmp_func_helper)(lhash_cmp_func func, const void *a,
                                     const void *b);

typedef uint32_t (*lhash_hash_func)(const void *a);
typedef uint32_t (*lhash_hash_func_helper)(lhash_hash_func func, const void *a);

typedef struct lhash_st _LHASH;

OPENSSL_EXPORT _LHASH *OPENSSL_lh_new(lhash_hash_func hash,
                                      lhash_cmp_func comp);

OPENSSL_EXPORT void OPENSSL_lh_free(_LHASH *lh);

OPENSSL_EXPORT size_t OPENSSL_lh_num_items(const _LHASH *lh);

OPENSSL_EXPORT void *OPENSSL_lh_retrieve(const _LHASH *lh, const void *data,
                                         lhash_hash_func_helper call_hash_func,
                                         lhash_cmp_func_helper call_cmp_func);

OPENSSL_EXPORT void *OPENSSL_lh_retrieve_key(const _LHASH *lh, const void *key,
                                             uint32_t key_hash,
                                             int (*cmp_key)(const void *key,
                                                            const void *value));

OPENSSL_EXPORT int OPENSSL_lh_insert(_LHASH *lh, void **old_data, void *data,
                                     lhash_hash_func_helper call_hash_func,
                                     lhash_cmp_func_helper call_cmp_func);

OPENSSL_EXPORT void *OPENSSL_lh_delete(_LHASH *lh, const void *data,
                                       lhash_hash_func_helper call_hash_func,
                                       lhash_cmp_func_helper call_cmp_func);

OPENSSL_EXPORT void OPENSSL_lh_doall_arg(_LHASH *lh,
                                         void (*func)(void *, void *),
                                         void *arg);

#define DEFINE_LHASH_OF(type)                                                  \
                                                                                

                                                                               \
  OPENSSL_MSVC_PRAGMA(warning(push))                                           \
  OPENSSL_MSVC_PRAGMA(warning(disable : 4191))                                 \
                                                                               \
  DECLARE_LHASH_OF(type)                                                       \
                                                                               \
  typedef int (*lhash_##type##_cmp_func)(const type *, const type *);          \
  typedef uint32_t (*lhash_##type##_hash_func)(const type *);                  \
                                                                               \
  inline int lh_##type##_call_cmp_func(lhash_cmp_func func, const void *a,     \
                                       const void *b) {                        \
    return ((lhash_##type##_cmp_func)func)((const type *)a, (const type *)b);  \
  }                                                                            \
                                                                               \
  inline uint32_t lh_##type##_call_hash_func(lhash_hash_func func,             \
                                             const void *a) {                  \
    return ((lhash_##type##_hash_func)func)((const type *)a);                  \
  }                                                                            \
                                                                               \
  inline LHASH_OF(type) *lh_##type##_new(lhash_##type##_hash_func hash,        \
                                         lhash_##type##_cmp_func comp) {       \
    return (LHASH_OF(type) *)OPENSSL_lh_new((lhash_hash_func)hash,             \
                                            (lhash_cmp_func)comp);             \
  }                                                                            \
                                                                               \
  inline void lh_##type##_free(LHASH_OF(type) *lh) {                           \
    OPENSSL_lh_free((_LHASH *)lh);                                             \
  }                                                                            \
                                                                               \
  inline size_t lh_##type##_num_items(const LHASH_OF(type) *lh) {              \
    return OPENSSL_lh_num_items((const _LHASH *)lh);                           \
  }                                                                            \
                                                                               \
  inline type *lh_##type##_retrieve(const LHASH_OF(type) *lh,                  \
                                    const type *data) {                        \
    return (type *)OPENSSL_lh_retrieve((const _LHASH *)lh, data,               \
                                       lh_##type##_call_hash_func,             \
                                       lh_##type##_call_cmp_func);             \
  }                                                                            \
                                                                               \
  typedef struct {                                                             \
    int (*cmp_key)(const void *key, const type *value);                        \
    const void *key;                                                           \
  } LHASH_CMP_KEY_##type;                                                      \
                                                                               \
  inline int lh_##type##_call_cmp_key(const void *key, const void *value) {    \
    const LHASH_CMP_KEY_##type *cb = (const LHASH_CMP_KEY_##type *)key;        \
    return cb->cmp_key(cb->key, (const type *)value);                          \
  }                                                                            \
                                                                               \
  inline type *lh_##type##_retrieve_key(                                       \
      const LHASH_OF(type) *lh, const void *key, uint32_t key_hash,            \
      int (*cmp_key)(const void *key, const type *value)) {                    \
    LHASH_CMP_KEY_##type cb = {cmp_key, key};                                  \
    return (type *)OPENSSL_lh_retrieve_key((const _LHASH *)lh, &cb, key_hash,  \
                                           lh_##type##_call_cmp_key);          \
  }                                                                            \
                                                                               \
  inline int lh_##type##_insert(LHASH_OF(type) *lh, type **old_data,           \
                                type *data) {                                  \
    void *old_data_void = NULL;                                                \
    int ret = OPENSSL_lh_insert((_LHASH *)lh, &old_data_void, data,            \
                                lh_##type##_call_hash_func,                    \
                                lh_##type##_call_cmp_func);                    \
    *old_data = (type *)old_data_void;                                         \
    return ret;                                                                \
  }                                                                            \
                                                                               \
  inline type *lh_##type##_delete(LHASH_OF(type) *lh, const type *data) {      \
    return (type *)OPENSSL_lh_delete((_LHASH *)lh, data,                       \
                                     lh_##type##_call_hash_func,               \
                                     lh_##type##_call_cmp_func);               \
  }                                                                            \
                                                                               \
  typedef struct {                                                             \
    void (*doall_arg)(type *, void *);                                         \
    void *arg;                                                                 \
  } LHASH_DOALL_##type;                                                        \
                                                                               \
  inline void lh_##type##_call_doall_arg(void *value, void *arg) {             \
    const LHASH_DOALL_##type *cb = (const LHASH_DOALL_##type *)arg;            \
    cb->doall_arg((type *)value, cb->arg);                                     \
  }                                                                            \
                                                                               \
  inline void lh_##type##_doall_arg(LHASH_OF(type) *lh,                        \
                                    void (*func)(type *, void *), void *arg) { \
    LHASH_DOALL_##type cb = {func, arg};                                       \
    OPENSSL_lh_doall_arg((_LHASH *)lh, lh_##type##_call_doall_arg, &cb);       \
  }                                                                            \
                                                                               \
  OPENSSL_MSVC_PRAGMA(warning(pop))

#if defined(__cplusplus)
}
#endif

#endif
