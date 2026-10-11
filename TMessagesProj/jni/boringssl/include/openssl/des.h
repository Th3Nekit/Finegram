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

#ifndef OPENSSL_HEADER_DES_H
#define OPENSSL_HEADER_DES_H

#include <openssl/base.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct DES_cblock_st {
  uint8_t bytes[8];
} DES_cblock;

typedef struct DES_ks {
  uint32_t subkeys[16][2];
} DES_key_schedule;

#define DES_KEY_SZ (sizeof(DES_cblock))
#define DES_SCHEDULE_SZ (sizeof(DES_key_schedule))

#define DES_ENCRYPT 1
#define DES_DECRYPT 0

#define DES_CBC_MODE 0
#define DES_PCBC_MODE 1

OPENSSL_EXPORT void DES_set_key(const DES_cblock *key,
                                DES_key_schedule *schedule);

OPENSSL_EXPORT void DES_set_odd_parity(DES_cblock *key);

OPENSSL_EXPORT void DES_ecb_encrypt(const DES_cblock *in, DES_cblock *out,
                                    const DES_key_schedule *schedule,
                                    int is_encrypt);

OPENSSL_EXPORT void DES_ncbc_encrypt(const uint8_t *in, uint8_t *out,
                                     size_t len,
                                     const DES_key_schedule *schedule,
                                     DES_cblock *ivec, int enc);

OPENSSL_EXPORT void DES_ecb3_encrypt(const DES_cblock *input,
                                     DES_cblock *output,
                                     const DES_key_schedule *ks1,
                                     const DES_key_schedule *ks2,
                                     const DES_key_schedule *ks3,
                                     int enc);

OPENSSL_EXPORT void DES_ede3_cbc_encrypt(const uint8_t *in, uint8_t *out,
                                         size_t len,
                                         const DES_key_schedule *ks1,
                                         const DES_key_schedule *ks2,
                                         const DES_key_schedule *ks3,
                                         DES_cblock *ivec, int enc);

OPENSSL_EXPORT void DES_ede2_cbc_encrypt(const uint8_t *in, uint8_t *out,
                                         size_t len,
                                         const DES_key_schedule *ks1,
                                         const DES_key_schedule *ks2,
                                         DES_cblock *ivec, int enc);

OPENSSL_EXPORT void DES_set_key_unchecked(const DES_cblock *key,
                                          DES_key_schedule *schedule);

OPENSSL_EXPORT void DES_ede3_cfb64_encrypt(const uint8_t *in, uint8_t *out,
                                           long length, DES_key_schedule *ks1,
                                           DES_key_schedule *ks2,
                                           DES_key_schedule *ks3,
                                           DES_cblock *ivec, int *num, int enc);

OPENSSL_EXPORT void DES_ede3_cfb_encrypt(const uint8_t *in, uint8_t *out,
                                         int numbits, long length,
                                         DES_key_schedule *ks1,
                                         DES_key_schedule *ks2,
                                         DES_key_schedule *ks3,
                                         DES_cblock *ivec, int enc);

#if defined(__cplusplus)
}
#endif

#endif
