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

#include <openssl/base.h>

#if !defined(OPENSSL_NO_ASM) && \
    (defined(OPENSSL_X86) || defined(OPENSSL_X86_64))

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <immintrin.h>
#include <intrin.h>
#endif

#include "internal.h"

static void OPENSSL_cpuid(uint32_t *out_eax, uint32_t *out_ebx,
                          uint32_t *out_ecx, uint32_t *out_edx, uint32_t leaf) {
#if defined(_MSC_VER)
  int tmp[4];
  __cpuid(tmp, (int)leaf);
  *out_eax = (uint32_t)tmp[0];
  *out_ebx = (uint32_t)tmp[1];
  *out_ecx = (uint32_t)tmp[2];
  *out_edx = (uint32_t)tmp[3];
#elif defined(__pic__) && defined(OPENSSL_32_BIT)

  __asm__ volatile(
      "xor %%ecx, %%ecx\n"
      "mov %%ebx, %%edi\n"
      "cpuid\n"
      "xchg %%edi, %%ebx\n"
      : "=a"(*out_eax), "=D"(*out_ebx), "=c"(*out_ecx), "=d"(*out_edx)
      : "a"(leaf));
#else
  __asm__ volatile(
      "xor %%ecx, %%ecx\n"
      "cpuid\n"
      : "=a"(*out_eax), "=b"(*out_ebx), "=c"(*out_ecx), "=d"(*out_edx)
      : "a"(leaf));
#endif
}

static uint64_t OPENSSL_xgetbv(uint32_t xcr) {
#if defined(_MSC_VER)
  return (uint64_t)_xgetbv(xcr);
#else
  uint32_t eax, edx;
  __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
  return (((uint64_t)edx) << 32) | eax;
#endif
}

static bool os_supports_avx512(uint64_t xcr0) {
#if defined(__APPLE__)

  return false;
#else
  return (xcr0 & 0xe6) == 0xe6;
#endif
}

static void handle_cpu_env(uint32_t *out, const char *in) {
  const int invert_op = in[0] == '~';
  const int or_op = in[0] == '|';
  const int skip_first_byte = invert_op || or_op;
  const int hex = in[skip_first_byte] == '0' && in[skip_first_byte + 1] == 'x';

  int sscanf_result;
  uint64_t v;
  if (hex) {
    sscanf_result = sscanf(in + invert_op + 2, "%" PRIx64, &v);
  } else {
    sscanf_result = sscanf(in + invert_op, "%" PRIu64, &v);
  }

  if (!sscanf_result) {
    return;
  }

  if (invert_op) {
    out[0] &= ~v;
    out[1] &= ~(v >> 32);
  } else if (or_op) {
    out[0] |= v;
    out[1] |= (v >> 32);
  } else {
    out[0] = v;
    out[1] = v >> 32;
  }
}

void OPENSSL_cpuid_setup(void) {

  uint32_t eax, ebx, ecx, edx;
  OPENSSL_cpuid(&eax, &ebx, &ecx, &edx, 0);

  uint32_t num_ids = eax;

  int is_intel = ebx == 0x756e6547            &&
                 edx == 0x49656e69            &&
                 ecx == 0x6c65746e           ;
  int is_amd = ebx == 0x68747541            &&
               edx == 0x69746e65            &&
               ecx == 0x444d4163           ;

  uint32_t extended_features[2] = {0};
  if (num_ids >= 7) {
    OPENSSL_cpuid(&eax, &ebx, &ecx, &edx, 7);
    extended_features[0] = ebx;
    extended_features[1] = ecx;
  }

  OPENSSL_cpuid(&eax, &ebx, &ecx, &edx, 1);

  const uint32_t base_family = (eax >> 8) & 15;
  const uint32_t base_model = (eax >> 4) & 15;

  uint32_t family = base_family;
  uint32_t model = base_model;
  if (base_family == 15) {
    const uint32_t ext_family = (eax >> 20) & 255;
    family += ext_family;
  }
  if (base_family == 6 || base_family == 15) {
    const uint32_t ext_model = (eax >> 16) & 15;
    model |= ext_model << 4;
  }

  if (is_amd) {
    if (family < 0x17 || (family == 0x17 && 0x70 <= model && model <= 0x7f)) {

      ecx &= ~(1u << 30);
    }
  }

  if (is_intel) {
    edx |= (1u << 30);
  } else {
    edx &= ~(1u << 30);
  }

  uint64_t xcr0 = 0;
  if (ecx & (1u << 27)) {

    xcr0 = OPENSSL_xgetbv(0);
  }

  if ((xcr0 & 6) != 6) {

    ecx &= ~(1u << 28);
    ecx &= ~(1u << 12);
    ecx &= ~(1u << 11);
    extended_features[0] &= ~(1u << 5);
    extended_features[1] &= ~(1u << 9);
    extended_features[1] &= ~(1u << 10);
  }

  if (!os_supports_avx512(xcr0)) {

    extended_features[0] &= ~(1u << 16);
    extended_features[0] &= ~(1u << 17);
    extended_features[0] &= ~(1u << 21);
    extended_features[0] &= ~(1u << 26);
    extended_features[0] &= ~(1u << 27);
    extended_features[0] &= ~(1u << 28);
    extended_features[0] &= ~(1u << 30);
    extended_features[0] &= ~(1u << 31);
    extended_features[1] &= ~(1u << 1);
    extended_features[1] &= ~(1u << 6);
    extended_features[1] &= ~(1u << 11);
    extended_features[1] &= ~(1u << 12);
    extended_features[1] &= ~(1u << 14);
  }

  if (is_intel && family == 6 &&
      (model == 85 ||
       model == 106 ||
       model == 108 ||
       model == 125 ||
       model == 126 ||
       model == 140 ||
       model == 141)) {
    extended_features[0] |= 1u << 14;
  } else {
    extended_features[0] &= ~(1u << 14);
  }

  OPENSSL_ia32cap_P[0] = edx;
  OPENSSL_ia32cap_P[1] = ecx;
  OPENSSL_ia32cap_P[2] = extended_features[0];
  OPENSSL_ia32cap_P[3] = extended_features[1];

  const char *env1, *env2;
  env1 = getenv("OPENSSL_ia32cap");
  if (env1 == NULL) {
    return;
  }

  handle_cpu_env(&OPENSSL_ia32cap_P[0], env1);
  env2 = strchr(env1, ':');
  if (env2 != NULL) {
    handle_cpu_env(&OPENSSL_ia32cap_P[2], env2 + 1);
  }
}

#endif
