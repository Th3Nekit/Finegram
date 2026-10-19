// Copyright 2017 The CRC32C Authors. All rights reserved.

#ifndef CRC32C_CRC32C_SSE42_H_
#define CRC32C_CRC32C_SSE42_H_

#include <cstddef>
#include <cstdint>

#include "voip/third_party/crc32c/src/include/crc32c/crc32c_config.h"

#if HAVE_SSE42 && (defined(_M_X64) || defined(__x86_64__))

namespace crc32c {

uint32_t ExtendSse42(uint32_t crc, const uint8_t* data, size_t count);

}

#endif

#endif
