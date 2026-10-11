// Copyright 2017 The CRC32C Authors. All rights reserved.

#ifndef CRC32C_CRC32C_ROUND_UP_H_
#define CRC32C_CRC32C_ROUND_UP_H_

#include <cstddef>
#include <cstdint>

namespace crc32c {

template <int N>
constexpr inline uintptr_t RoundUp(uintptr_t pointer) {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");
  return (pointer + (N - 1)) & ~(N - 1);
}

template <int N>
constexpr inline const uint8_t* RoundUp(const uint8_t* pointer) {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");
  return reinterpret_cast<uint8_t*>(
      RoundUp<N>(reinterpret_cast<uintptr_t>(pointer)));
}

}

#endif
