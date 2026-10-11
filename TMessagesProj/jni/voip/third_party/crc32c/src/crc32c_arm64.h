// Copyright 2017 The CRC32C Authors. All rights reserved.

#ifndef CRC32C_CRC32C_ARM_H_
#define CRC32C_CRC32C_ARM_H_

#include <cstddef>
#include <cstdint>

#include "voip/third_party/crc32c/src/include/crc32c/crc32c_config.h"

#if HAVE_ARM64_CRC32C

namespace crc32c {

uint32_t ExtendArm64(uint32_t crc, const uint8_t* data, size_t count);

}

#endif

#endif
