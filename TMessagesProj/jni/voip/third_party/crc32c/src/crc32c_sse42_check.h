// Copyright 2017 The CRC32C Authors. All rights reserved.

#ifndef CRC32C_CRC32C_SSE42_CHECK_H_
#define CRC32C_CRC32C_SSE42_CHECK_H_

#include <cstddef>
#include <cstdint>

#include "voip/third_party/crc32c/src/include/crc32c/crc32c_config.h"

#if HAVE_SSE42 && (defined(_M_X64) || defined(__x86_64__))

#if defined(_MSC_VER)
#include <intrin.h>

namespace crc32c {

inline bool CanUseSse42() {
  int cpu_info[4];
  __cpuid(cpu_info, 1);
  return (cpu_info[2] & (1 << 20)) != 0;
}

}

#else
#include <cpuid.h>

namespace crc32c {

inline bool CanUseSse42() {
  unsigned int eax, ebx, ecx, edx;
  return __get_cpuid(1, &eax, &ebx, &ecx, &edx) && ((ecx & (1 << 20)) != 0);
}

}

#endif

#endif

#endif
