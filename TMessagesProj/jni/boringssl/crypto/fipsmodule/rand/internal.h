// Copyright 2015 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_RAND_INTERNAL_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_RAND_INTERNAL_H

#include <openssl/aes.h>
#include <openssl/ctrdrbg.h>

#include "../../bcm_support.h"
#include "../aes/internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

int rand_fork_unsafe_buffering_enabled(void);

struct ctr_drbg_state_st {
  AES_KEY ks;
  block128_f block;
  ctr128_f ctr;
  uint8_t counter[16];
  uint64_t reseed_counter;
};

OPENSSL_EXPORT int CTR_DRBG_init(CTR_DRBG_STATE *drbg,
                                 const uint8_t entropy[CTR_DRBG_ENTROPY_LEN],
                                 const uint8_t *personalization,
                                 size_t personalization_len);

#if defined(OPENSSL_X86_64) && !defined(OPENSSL_NO_ASM)

inline int have_rdrand(void) { return CRYPTO_is_RDRAND_capable(); }

inline int have_fast_rdrand(void) {
  return CRYPTO_is_RDRAND_capable() && CRYPTO_is_intel_cpu();
}

int CRYPTO_rdrand(uint8_t out[8]);

int CRYPTO_rdrand_multiple8_buf(uint8_t *buf, size_t len);

#else

inline int have_rdrand(void) { return 0; }

inline int have_fast_rdrand(void) { return 0; }

#endif

#if defined(__cplusplus)
}
#endif

#endif
