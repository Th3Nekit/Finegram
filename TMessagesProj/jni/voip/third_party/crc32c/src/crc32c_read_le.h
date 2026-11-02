// Copyright 2017 The CRC32C Authors. All rights reserved.

#ifndef CRC32C_CRC32C_READ_LE_H_
#define CRC32C_CRC32C_READ_LE_H_

#include <cstdint>
#include <cstring>

#include "voip/third_party/crc32c/src/include/crc32c/crc32c_config.h"

namespace crc32c {

inline uint32_t ReadUint32LE(const uint8_t* buffer) {
#if BYTE_ORDER_BIG_ENDIAN
  return ((static_cast<uint32_t>(static_cast<uint8_t>(buffer[0]))) |
          (static_cast<uint32_t>(static_cast<uint8_t>(buffer[1])) << 8) |
          (static_cast<uint32_t>(static_cast<uint8_t>(buffer[2])) << 16) |
          (static_cast<uint32_t>(static_cast<uint8_t>(buffer[3])) << 24));
#else
  uint32_t result;

  std::memcpy(&result, buffer, sizeof(result));
  return result;
#endif
}

inline uint64_t ReadUint64LE(const uint8_t* buffer) {
#if BYTE_ORDER_BIG_ENDIAN
  return ((static_cast<uint64_t>(static_cast<uint8_t>(buffer[0]))) |
          (static_cast<uint64_t>(static_cast<uint8_t>(buffer[1])) << 8) |
          (static_cast<uint64_t>(static_cast<uint8_t>(buffer[2])) << 16) |
          (static_cast<uint64_t>(static_cast<uint8_t>(buffer[3])) << 24) |
          (static_cast<uint64_t>(static_cast<uint8_t>(buffer[4])) << 32) |
          (static_cast<uint64_t>(static_cast<uint8_t>(buffer[5])) << 40) |
          (static_cast<uint64_t>(static_cast<uint8_t>(buffer[6])) << 48) |
          (static_cast<uint64_t>(static_cast<uint8_t>(buffer[7])) << 56));
#else
  uint64_t result;

  std::memcpy(&result, buffer, sizeof(result));
  return result;
#endif
}

}

#endif
