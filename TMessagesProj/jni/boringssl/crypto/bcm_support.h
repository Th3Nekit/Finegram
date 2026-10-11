// Copyright 2024 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CRYPTO_BCM_SUPPORT_H
#define OPENSSL_HEADER_CRYPTO_BCM_SUPPORT_H

#include <openssl/base.h>

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

void CRYPTO_init_sysrand(void);

void CRYPTO_sysrand(uint8_t *buf, size_t len);

int CRYPTO_sysrand_if_available(uint8_t *buf, size_t len);

void CRYPTO_sysrand_for_seed(uint8_t *buf, size_t len);

void RAND_need_entropy(size_t bytes_needed);

OPENSSL_EXPORT uint64_t CRYPTO_get_fork_generation(void);

OPENSSL_EXPORT void CRYPTO_fork_detect_force_madv_wipeonfork_for_testing(
    int on);

FILE *CRYPTO_get_stderr(void);

#if defined(__cplusplus)
}
#endif

#endif
