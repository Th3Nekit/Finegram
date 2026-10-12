/* Copyright 2017 The CRC32C Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file. See the AUTHORS file for names of contributors. */

#ifndef CRC32C_CRC32C_H_
#define CRC32C_CRC32C_H_

#if defined(__cplusplus)

#include <cstddef>
#include <cstdint>
#include <string>

#else

#include <stddef.h>
#include <stdint.h>

#endif

#if defined(__cplusplus)
extern "C" {
#endif

uint32_t crc32c_extend(uint32_t crc, const uint8_t* data, size_t count);

uint32_t crc32c_value(const uint8_t* data, size_t count);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus)

namespace crc32c {

uint32_t Extend(uint32_t crc, const uint8_t* data, size_t count);

inline uint32_t Crc32c(const uint8_t* data, size_t count) {
  return Extend(0, data, count);
}

inline uint32_t Crc32c(const char* data, size_t count) {
  return Extend(0, reinterpret_cast<const uint8_t*>(data), count);
}

inline uint32_t Crc32c(const std::string& string) {
  return Crc32c(reinterpret_cast<const uint8_t*>(string.data()),
                string.size());
}

}

#if __cplusplus > 201402L
#if __has_include(<string_view>)
#include <string_view>

namespace crc32c {

inline uint32_t Crc32c(const std::string_view& string_view) {
  return Crc32c(reinterpret_cast<const uint8_t*>(string_view.data()),
                string_view.size());
}

}

#endif
#endif

#endif

#endif
