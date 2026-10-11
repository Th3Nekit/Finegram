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

#ifndef OPENSSL_HEADER_CRYPTO_H
#define OPENSSL_HEADER_CRYPTO_H

#include <openssl/base.h>
#include <openssl/sha.h>

#include <openssl/mem.h>

#include <openssl/thread.h>

#if defined(__cplusplus)
extern "C" {
#endif

OPENSSL_EXPORT int CRYPTO_is_confidential_build(void);

OPENSSL_EXPORT int CRYPTO_has_asm(void);

OPENSSL_EXPORT int BORINGSSL_self_test(void);

OPENSSL_EXPORT int BORINGSSL_integrity_test(void);

OPENSSL_EXPORT void CRYPTO_pre_sandbox_init(void);

#if defined(OPENSSL_ARM) && defined(OPENSSL_LINUX) && \
    !defined(OPENSSL_STATIC_ARMCAP)

OPENSSL_EXPORT int CRYPTO_needs_hwcap2_workaround(void);
#endif

#if defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION)

OPENSSL_EXPORT void CRYPTO_set_fuzzer_mode(int enabled);
#endif

OPENSSL_EXPORT int FIPS_mode(void);

enum fips_counter_t {
  fips_counter_evp_aes_128_gcm = 0,
  fips_counter_evp_aes_256_gcm = 1,
  fips_counter_evp_aes_128_ctr = 2,
  fips_counter_evp_aes_256_ctr = 3,

  fips_counter_max = 3,
};

OPENSSL_EXPORT size_t FIPS_read_counter(enum fips_counter_t counter);

#define OPENSSL_VERSION_TEXT "OpenSSL 1.1.1 (compatible; BoringSSL)"

#define OPENSSL_VERSION 0
#define OPENSSL_CFLAGS 1
#define OPENSSL_BUILT_ON 2
#define OPENSSL_PLATFORM 3
#define OPENSSL_DIR 4

OPENSSL_EXPORT const char *OpenSSL_version(int which);

#define SSLEAY_VERSION OPENSSL_VERSION
#define SSLEAY_CFLAGS OPENSSL_CFLAGS
#define SSLEAY_BUILT_ON OPENSSL_BUILT_ON
#define SSLEAY_PLATFORM OPENSSL_PLATFORM
#define SSLEAY_DIR OPENSSL_DIR

OPENSSL_EXPORT const char *SSLeay_version(int which);

OPENSSL_EXPORT unsigned long SSLeay(void);

OPENSSL_EXPORT unsigned long OpenSSL_version_num(void);

OPENSSL_EXPORT int CRYPTO_malloc_init(void);

OPENSSL_EXPORT int OPENSSL_malloc_init(void);

OPENSSL_EXPORT void ENGINE_load_builtin_engines(void);

OPENSSL_EXPORT int ENGINE_register_all_complete(void);

OPENSSL_EXPORT void OPENSSL_load_builtin_modules(void);

#define OPENSSL_INIT_NO_LOAD_CRYPTO_STRINGS 0
#define OPENSSL_INIT_LOAD_CRYPTO_STRINGS 0
#define OPENSSL_INIT_ADD_ALL_CIPHERS 0
#define OPENSSL_INIT_ADD_ALL_DIGESTS 0
#define OPENSSL_INIT_NO_ADD_ALL_CIPHERS 0
#define OPENSSL_INIT_NO_ADD_ALL_DIGESTS 0
#define OPENSSL_INIT_LOAD_CONFIG 0
#define OPENSSL_INIT_NO_LOAD_CONFIG 0
#define OPENSSL_INIT_NO_ATEXIT 0
#define OPENSSL_INIT_ATFORK 0
#define OPENSSL_INIT_ENGINE_RDRAND 0
#define OPENSSL_INIT_ENGINE_DYNAMIC 0
#define OPENSSL_INIT_ENGINE_OPENSSL 0
#define OPENSSL_INIT_ENGINE_CRYPTODEV 0
#define OPENSSL_INIT_ENGINE_CAPI 0
#define OPENSSL_INIT_ENGINE_PADLOCK 0
#define OPENSSL_INIT_ENGINE_AFALG 0
#define OPENSSL_INIT_ENGINE_ALL_BUILTIN 0

OPENSSL_EXPORT int OPENSSL_init_crypto(uint64_t opts,
                                       const OPENSSL_INIT_SETTINGS *settings);

OPENSSL_EXPORT void OPENSSL_cleanup(void);

OPENSSL_EXPORT int FIPS_mode_set(int on);

OPENSSL_EXPORT const char *FIPS_module_name(void);

OPENSSL_EXPORT const uint8_t *FIPS_module_hash(void);

OPENSSL_EXPORT uint32_t FIPS_version(void);

OPENSSL_EXPORT int FIPS_query_algorithm_status(const char *algorithm);

#if defined(OPENSSL_ARM) && defined(OPENSSL_LINUX) && \
    !defined(OPENSSL_STATIC_ARMCAP)

OPENSSL_EXPORT int CRYPTO_has_broken_NEON(void);
#endif

OPENSSL_EXPORT void CRYPTO_library_init(void);

#if defined(__cplusplus)
}
#endif

#endif
