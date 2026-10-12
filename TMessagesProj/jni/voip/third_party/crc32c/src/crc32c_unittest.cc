// Copyright 2017 The CRC32C Authors. All rights reserved.

#include "voip/third_party/crc32c/src/include/crc32c/crc32c.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "gtest/gtest.h"

#include "./crc32c_extend_unittests.h"

TEST(Crc32CTest, Crc32c) {

  uint8_t buf[32];

  std::memset(buf, 0, sizeof(buf));
  EXPECT_EQ(static_cast<uint32_t>(0x8a9136aa),
            crc32c::Crc32c(buf, sizeof(buf)));

  std::memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(static_cast<uint32_t>(0x62a8ab43),
            crc32c::Crc32c(buf, sizeof(buf)));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<uint8_t>(i);
  EXPECT_EQ(static_cast<uint32_t>(0x46dd794e),
            crc32c::Crc32c(buf, sizeof(buf)));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<uint8_t>(31 - i);
  EXPECT_EQ(static_cast<uint32_t>(0x113fdb5c),
            crc32c::Crc32c(buf, sizeof(buf)));

  uint8_t data[48] = {
      0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
      0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x18, 0x28, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_EQ(static_cast<uint32_t>(0xd9963a56),
            crc32c::Crc32c(data, sizeof(data)));
}

namespace crc32c {

struct ApiTestTraits {
  static uint32_t Extend(uint32_t crc, const uint8_t* data, size_t count) {
    return ::crc32c::Extend(crc, data, count);
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(Api, ExtendTest, ApiTestTraits);

}

TEST(CRC32CTest, Crc32cCharPointer) {
  char buf[32];

  std::memset(buf, 0, sizeof(buf));
  EXPECT_EQ(static_cast<uint32_t>(0x8a9136aa),
            crc32c::Crc32c(buf, sizeof(buf)));

  std::memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(static_cast<uint32_t>(0x62a8ab43),
            crc32c::Crc32c(buf, sizeof(buf)));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<char>(i);
  EXPECT_EQ(static_cast<uint32_t>(0x46dd794e),
            crc32c::Crc32c(buf, sizeof(buf)));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<char>(31 - i);
  EXPECT_EQ(static_cast<uint32_t>(0x113fdb5c),
            crc32c::Crc32c(buf, sizeof(buf)));
}

TEST(CRC32CTest, Crc32cStdString) {
  std::string buf;
  buf.resize(32);

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<char>(0x00);
  EXPECT_EQ(static_cast<uint32_t>(0x8a9136aa), crc32c::Crc32c(buf));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = '\xff';
  EXPECT_EQ(static_cast<uint32_t>(0x62a8ab43), crc32c::Crc32c(buf));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<char>(i);
  EXPECT_EQ(static_cast<uint32_t>(0x46dd794e), crc32c::Crc32c(buf));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<char>(31 - i);
  EXPECT_EQ(static_cast<uint32_t>(0x113fdb5c), crc32c::Crc32c(buf));
}

#if __cplusplus > 201402L
#if __has_include(<string_view>)

TEST(CRC32CTest, Crc32cStdStringView) {
  uint8_t buf[32];
  std::string_view view(reinterpret_cast<const char*>(buf), sizeof(buf));

  std::memset(buf, 0, sizeof(buf));
  EXPECT_EQ(static_cast<uint32_t>(0x8a9136aa), crc32c::Crc32c(view));

  std::memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(static_cast<uint32_t>(0x62a8ab43), crc32c::Crc32c(view));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<uint8_t>(i);
  EXPECT_EQ(static_cast<uint32_t>(0x46dd794e), crc32c::Crc32c(view));

  for (size_t i = 0; i < 32; ++i)
    buf[i] = static_cast<uint8_t>(31 - i);
  EXPECT_EQ(static_cast<uint32_t>(0x113fdb5c), crc32c::Crc32c(view));
}

#endif
#endif

#define TESTED_EXTEND Extend
#include "./crc32c_extend_unittests.h"
#undef TESTED_EXTEND
