// Copyright 2013-2016 The OpenSSL Project Authors. All Rights Reserved.
// Copyright (c) 2012, Intel Corporation. All Rights Reserved.
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
//
// Originally written by Shay Gueron (1, 2), and Vlad Krasnov (1)
// (1) Intel Corporation, Israel Development Center, Haifa, Israel
// (2) University of Haifa, Israel

#ifndef OPENSSL_HEADER_CRYPTO_FIPSMODULE_BN_RSAZ_EXP_H
#define OPENSSL_HEADER_CRYPTO_FIPSMODULE_BN_RSAZ_EXP_H

#include <openssl/bn.h>

#include "internal.h"
#include "../../internal.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(OPENSSL_NO_ASM) && defined(OPENSSL_X86_64)
#define RSAZ_ENABLED

void RSAZ_1024_mod_exp_avx2(BN_ULONG result[16], const BN_ULONG base_norm[16],
                            const BN_ULONG exponent[16],
                            const BN_ULONG m_norm[16], const BN_ULONG RR[16],
                            BN_ULONG k0,
                            BN_ULONG storage_words[MOD_EXP_CTIME_STORAGE_LEN]);

inline int rsaz_avx2_capable(void) { return CRYPTO_is_AVX2_capable(); }

inline int rsaz_avx2_preferred(void) {
  if (CRYPTO_is_BMI1_capable() && CRYPTO_is_BMI2_capable() &&
      CRYPTO_is_ADX_capable()) {

    return 0;
  }
  return CRYPTO_is_AVX2_capable();
}

void rsaz_1024_norm2red_avx2(BN_ULONG red[40], const BN_ULONG norm[16]);

void rsaz_1024_mul_avx2(BN_ULONG ret[40], const BN_ULONG a[40],
                        const BN_ULONG b[40], const BN_ULONG n[40], BN_ULONG k);

void rsaz_1024_sqr_avx2(BN_ULONG ret[40], const BN_ULONG a[40],
                        const BN_ULONG n[40], BN_ULONG k, int count);

void rsaz_1024_scatter5_avx2(BN_ULONG tbl[32 * 18], const BN_ULONG val[40],
                             int i);

void rsaz_1024_gather5_avx2(BN_ULONG val[40], const BN_ULONG tbl[32 * 18],
                            int i);

void rsaz_1024_red2norm_avx2(BN_ULONG norm[16], const BN_ULONG red[40]);

#endif

#if defined(__cplusplus)
}
#endif

#endif
