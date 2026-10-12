// Copyright 2017 The CRC32C Authors. All rights reserved.

#ifndef CRC32C_CRC32C_INTERNAL_H_
#define CRC32C_CRC32C_INTERNAL_H_

#include <cstddef>
#include <cstdint>

namespace crc32c {

uint32_t ExtendPortable(uint32_t crc, const uint8_t* data, size_t count);

static constexpr const uint32_t kCRC32Xor = static_cast<uint32_t>(0xffffffffU);

}

#endif
