// Copyright 2014 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_RAND_H
#define OPENSSL_HEADER_RAND_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int RAND_bytes(uint8_t *buf, size_t len);

#if !defined(OPENSSL_WINDOWS)

OPENSSL_EXPORT void RAND_enable_fork_unsafe_buffering(int fd);

OPENSSL_EXPORT void RAND_disable_fork_unsafe_buffering(void);
#endif

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)

OPENSSL_EXPORT void RAND_reset_for_fuzzing(void);
#endif

OPENSSL_EXPORT void RAND_get_system_entropy_for_custom_prng(uint8_t *buf,
                                                            size_t len);

OPENSSL_EXPORT int RAND_pseudo_bytes(uint8_t *buf, size_t len);

OPENSSL_EXPORT void RAND_seed(const void *buf, int num);

OPENSSL_EXPORT int RAND_load_file(const char *path, long num);

OPENSSL_EXPORT const char *RAND_file_name(char *buf, size_t num);

OPENSSL_EXPORT void RAND_add(const void *buf, int num, double entropy);

OPENSSL_EXPORT int RAND_egd(const char *);

OPENSSL_EXPORT int RAND_poll(void);

OPENSSL_EXPORT int RAND_status(void);

OPENSSL_EXPORT void RAND_cleanup(void);

struct rand_meth_st {
  void (*seed) (const void *buf, int num);
  int (*bytes) (uint8_t *buf, size_t num);
  void (*cleanup) (void);
  void (*add) (const void *buf, int num, double entropy);
  int (*pseudorand) (uint8_t *buf, size_t num);
  int (*status) (void);
};

OPENSSL_EXPORT RAND_METHOD *RAND_SSLeay(void);

OPENSSL_EXPORT RAND_METHOD *RAND_OpenSSL(void);

OPENSSL_EXPORT const RAND_METHOD *RAND_get_rand_method(void);

OPENSSL_EXPORT int RAND_set_rand_method(const RAND_METHOD *);

#if defined(__cplusplus)
}
#endif

#endif
