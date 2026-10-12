// Copyright 2017 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_DELOCATE_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_DELOCATE_H

#include <openssl/base.h>

#include "../internal.h"

#if !defined(BORINGSSL_SHARED_LIBRARY) && defined(BORINGSSL_FIPS) && \
    !defined(OPENSSL_ASAN) && !defined(OPENSSL_MSAN)
#define DEFINE_BSS_GET(type, name, init_value)                                 \
                                                                               \
  extern "C" {                                                                 \
  extern type bcm_##name;                                                      \
  type bcm_##name = init_value;                                                \
  type *bcm_##name##_bss_get(void) __attribute__((const));                     \
  }                                                                            \
                                                                               \
                                                                                

                                                                               \
  static type *name##_bss_get(void) { return bcm_##name##_bss_get(); }

#define DEFINE_STATIC_ONCE(name) \
  DEFINE_BSS_GET(CRYPTO_once_t, name, CRYPTO_ONCE_INIT)

#define DEFINE_STATIC_MUTEX(name) \
  DEFINE_BSS_GET(CRYPTO_MUTEX, name, CRYPTO_MUTEX_INIT)

#define DEFINE_STATIC_EX_DATA_CLASS(name) \
  DEFINE_BSS_GET(CRYPTO_EX_DATA_CLASS, name, CRYPTO_EX_DATA_CLASS_INIT)
#else
#define DEFINE_BSS_GET(type, name, init_value) \
  static type name = init_value;               \
  static type *name##_bss_get(void) { return &name; }
#define DEFINE_STATIC_ONCE(name)                \
  static CRYPTO_once_t name = CRYPTO_ONCE_INIT; \
  static CRYPTO_once_t *name##_bss_get(void) { return &name; }
#define DEFINE_STATIC_MUTEX(name)               \
  static CRYPTO_MUTEX name = CRYPTO_MUTEX_INIT; \
  static CRYPTO_MUTEX *name##_bss_get(void) { return &name; }
#define DEFINE_STATIC_EX_DATA_CLASS(name)                       \
  static CRYPTO_EX_DATA_CLASS name = CRYPTO_EX_DATA_CLASS_INIT; \
  static CRYPTO_EX_DATA_CLASS *name##_bss_get(void) { return &name; }
#endif

#define DEFINE_DATA(type, name, accessor_decorations)                         \
  DEFINE_BSS_GET(type, name##_storage, {})                                    \
  DEFINE_STATIC_ONCE(name##_once)                                             \
  static void name##_do_init(type *out);                                      \
  static void name##_init(void) { name##_do_init(name##_storage_bss_get()); } \
  accessor_decorations type *name(void) {                                     \
    CRYPTO_once(name##_once_bss_get(), name##_init);                          \
                                                                               
                                                                              \
    return (const type *)name##_storage_bss_get();                            \
  }                                                                           \
  static void name##_do_init(type *out)

#define DEFINE_METHOD_FUNCTION(type, name) DEFINE_DATA(type, name, const)

#define DEFINE_LOCAL_DATA(type, name) DEFINE_DATA(type, name, static const)

#endif
