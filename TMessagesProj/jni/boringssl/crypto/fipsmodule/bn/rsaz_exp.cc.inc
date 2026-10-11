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

#include "rsaz_exp.h"

#if defined(RSAZ_ENABLED)

#include <openssl/mem.h>

#include <assert.h>

#include "internal.h"
#include "../../internal.h"

alignas(64) static const BN_ULONG rsaz_one[40] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

alignas(64) static const BN_ULONG rsaz_two80[40] = {
    0, 0, 1 << 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0,       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void RSAZ_1024_mod_exp_avx2(BN_ULONG result_norm[16],
                            const BN_ULONG base_norm[16],
                            const BN_ULONG exponent[16],
                            const BN_ULONG m_norm[16], const BN_ULONG RR[16],
                            BN_ULONG k0,
                            BN_ULONG storage[MOD_EXP_CTIME_STORAGE_LEN]) {
  static_assert(MOD_EXP_CTIME_ALIGN % 64 == 0,
                "MOD_EXP_CTIME_ALIGN is too small");
  assert((uintptr_t)storage % 64 == 0);

  BN_ULONG *a_inv, *m, *result, *table_s = storage + 40 * 3, *R2 = table_s;

  if (((((uintptr_t)storage & 4095) + 320) >> 12) != 0) {
    result = storage;
    a_inv = storage + 40;
    m = storage + 40 * 2;
  } else {
    m = storage;
    result = storage + 40;
    a_inv = storage + 40 * 2;
  }

  rsaz_1024_norm2red_avx2(m, m_norm);
  rsaz_1024_norm2red_avx2(a_inv, base_norm);
  rsaz_1024_norm2red_avx2(R2, RR);

  rsaz_1024_mul_avx2(R2, R2, R2, m, k0);

  rsaz_1024_mul_avx2(R2, R2, rsaz_two80, m, k0);

  rsaz_1024_mul_avx2(result, R2, rsaz_one, m, k0);
  rsaz_1024_mul_avx2(a_inv, a_inv, R2, m, k0);
  rsaz_1024_scatter5_avx2(table_s, result, 0);
  rsaz_1024_scatter5_avx2(table_s, a_inv, 1);

  rsaz_1024_sqr_avx2(result, a_inv, m, k0, 1);
  rsaz_1024_scatter5_avx2(table_s, result, 2);

  rsaz_1024_sqr_avx2(result, result, m, k0, 1);
  rsaz_1024_scatter5_avx2(table_s, result, 4);

  rsaz_1024_sqr_avx2(result, result, m, k0, 1);
  rsaz_1024_scatter5_avx2(table_s, result, 8);

  rsaz_1024_sqr_avx2(result, result, m, k0, 1);
  rsaz_1024_scatter5_avx2(table_s, result, 16);
  for (int i = 3; i < 32; i += 2) {

    rsaz_1024_gather5_avx2(result, table_s, i - 1);
    rsaz_1024_mul_avx2(result, result, a_inv, m, k0);
    rsaz_1024_scatter5_avx2(table_s, result, i);
    for (int j = 2 * i; j < 32; j *= 2) {

      rsaz_1024_sqr_avx2(result, result, m, k0, 1);
      rsaz_1024_scatter5_avx2(table_s, result, j);
    }
  }

  const uint8_t *p_str = (const uint8_t *)exponent;
  int wvalue = p_str[127] >> 3;
  rsaz_1024_gather5_avx2(result, table_s, wvalue);

  int index = 1014;
  while (index > -1) {
    rsaz_1024_sqr_avx2(result, result, m, k0, 5);

    uint16_t wvalue_16;
    memcpy(&wvalue_16, &p_str[index / 8], sizeof(wvalue_16));
    wvalue = wvalue_16;
    wvalue = (wvalue >> (index % 8)) & 31;
    index -= 5;

    rsaz_1024_gather5_avx2(a_inv, table_s, wvalue);
    rsaz_1024_mul_avx2(result, result, a_inv, m, k0);
  }

  rsaz_1024_sqr_avx2(result, result, m, k0, 4);

  wvalue = p_str[0] & 15;

  rsaz_1024_gather5_avx2(a_inv, table_s, wvalue);
  rsaz_1024_mul_avx2(result, result, a_inv, m, k0);

  rsaz_1024_mul_avx2(result, result, rsaz_one, m, k0);

  rsaz_1024_red2norm_avx2(result_norm, result);
  BN_ULONG scratch[16];
  bn_reduce_once_in_place(result_norm,           0, m_norm, scratch, 16);

  OPENSSL_cleanse(storage, MOD_EXP_CTIME_STORAGE_LEN * sizeof(BN_ULONG));
}

#endif
