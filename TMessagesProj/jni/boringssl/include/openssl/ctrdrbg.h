// Copyright 2022 The BoringSSL Authors
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

#ifndef OPENSSL_HEADER_CTRDRBG_H
#define OPENSSL_HEADER_CTRDRBG_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define CTR_DRBG_ENTROPY_LEN 48

#define CTR_DRBG_MAX_GENERATE_LENGTH 65536

OPENSSL_EXPORT CTR_DRBG_STATE *CTR_DRBG_new(
    const uint8_t entropy[CTR_DRBG_ENTROPY_LEN], const uint8_t *personalization,
    size_t personalization_len);

OPENSSL_EXPORT void CTR_DRBG_free(CTR_DRBG_STATE* state);

OPENSSL_EXPORT int CTR_DRBG_reseed(CTR_DRBG_STATE *drbg,
                                   const uint8_t entropy[CTR_DRBG_ENTROPY_LEN],
                                   const uint8_t *additional_data,
                                   size_t additional_data_len);

OPENSSL_EXPORT int CTR_DRBG_generate(CTR_DRBG_STATE *drbg, uint8_t *out,
                                     size_t out_len,
                                     const uint8_t *additional_data,
                                     size_t additional_data_len);

OPENSSL_EXPORT void CTR_DRBG_clear(CTR_DRBG_STATE *drbg);

#if defined(__cplusplus)
}

extern "C++" {
BSSL_NAMESPACE_BEGIN
BORINGSSL_MAKE_DELETER(CTR_DRBG_STATE, CTR_DRBG_free)
BSSL_NAMESPACE_END
}
#endif

#endif
