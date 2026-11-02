// Copyright 2017 The CRC32C Authors. All rights reserved.

#ifndef CRC32C_CRC32C_PREFETCH_H_
#define CRC32C_CRC32C_PREFETCH_H_

#include <cstddef>
#include <cstdint>

#include "voip/third_party/crc32c/src/include/crc32c/crc32c_config.h"

#if HAVE_MM_PREFETCH

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <xmmintrin.h>
#endif

#endif

namespace crc32c {

inline void RequestPrefetch(const uint8_t* address) {
#if HAVE_BUILTIN_PREFETCH

  __builtin_prefetch(reinterpret_cast<const char*>(address), 0                 ,
                     0                            );
#elif HAVE_MM_PREFETCH

  _mm_prefetch(reinterpret_cast<const char*>(address), _MM_HINT_NTA);
#else

  (void)address;
#endif
}

}

#endif
