// Copyright 2017 The CRC32C Authors. All rights reserved.

#include "gtest/gtest.h"

#include "./crc32c_extend_unittests.h"
#include "./crc32c_internal.h"

namespace crc32c {

struct PortableTestTraits {
  static uint32_t Extend(uint32_t crc, const uint8_t* data, size_t count) {
    return ExtendPortable(crc, data, count);
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(Portable, ExtendTest, PortableTestTraits);

}
