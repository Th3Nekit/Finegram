// Copyright 2017 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <limits.h>
#include <stdint.h>

#include <type_traits>

#include <gtest/gtest.h>

#include "test/test_util.h"

template <typename T>
static void CheckRepresentation(T value) {
  SCOPED_TRACE(value);

  using UnsignedT = typename std::make_unsigned<T>::type;
  UnsignedT value_u = static_cast<UnsignedT>(value);
  EXPECT_EQ(sizeof(UnsignedT), sizeof(T));

  uint8_t expected[sizeof(UnsignedT)];
  for (size_t i = 0; i < sizeof(UnsignedT); i++) {
    expected[i] = static_cast<uint8_t>(value_u);

    value_u = static_cast<UnsignedT>(value_u / 256);
  }
  EXPECT_EQ(0u, value_u);

  EXPECT_EQ(Bytes(expected),
            Bytes(reinterpret_cast<const uint8_t *>(&value), sizeof(value)));
}

TEST(CompilerTest, IntegerRepresentation) {
  static_assert(CHAR_BIT == 8, "BoringSSL only supports 8-bit chars");
  static_assert(UCHAR_MAX == 0xff, "BoringSSL only supports 8-bit chars");

  static_assert(
      std::is_same<unsigned char, uint8_t>::value,
      "BoringSSL requires uint8_t and unsigned char be the same type");
  uint8_t u8 = 0;
  unsigned char *ptr = &u8;
  (void)ptr;

  static_assert(sizeof(uint8_t) == 1u, "uint8_t has the wrong size");
  static_assert(sizeof(uint16_t) == 2u, "uint16_t has the wrong size");
  static_assert(sizeof(uint32_t) == 4u, "uint32_t has the wrong size");
  static_assert(sizeof(uint64_t) == 8u, "uint64_t has the wrong size");

  static_assert(sizeof(size_t) <= 8u, "size_t must not exceed uint64_t");

  static_assert(INT_MAX == INT32_MAX, "BoringSSL requires int be 32-bit");
  static_assert(UINT_MAX == UINT32_MAX,
                "BoringSSL requires unsigned be 32-bit");

  CheckRepresentation(static_cast<signed char>(127));
  CheckRepresentation(static_cast<signed char>(1));
  CheckRepresentation(static_cast<signed char>(0));
  CheckRepresentation(static_cast<signed char>(-1));
  CheckRepresentation(static_cast<signed char>(-42));
  CheckRepresentation(static_cast<signed char>(-128));

  CheckRepresentation(static_cast<int>(INT_MAX));
  CheckRepresentation(static_cast<int>(0x12345678));
  CheckRepresentation(static_cast<int>(1));
  CheckRepresentation(static_cast<int>(0));
  CheckRepresentation(static_cast<int>(-1));
  CheckRepresentation(static_cast<int>(-0x12345678));
  CheckRepresentation(static_cast<int>(INT_MIN));

  CheckRepresentation(static_cast<unsigned>(UINT_MAX));
  CheckRepresentation(static_cast<unsigned>(0x12345678));
  CheckRepresentation(static_cast<unsigned>(1));
  CheckRepresentation(static_cast<unsigned>(0));

  CheckRepresentation(static_cast<long>(LONG_MAX));
  CheckRepresentation(static_cast<long>(0x12345678));
  CheckRepresentation(static_cast<long>(1));
  CheckRepresentation(static_cast<long>(0));
  CheckRepresentation(static_cast<long>(-1));
  CheckRepresentation(static_cast<long>(-0x12345678));
  CheckRepresentation(static_cast<long>(LONG_MIN));

  CheckRepresentation(static_cast<unsigned long>(ULONG_MAX));
  CheckRepresentation(static_cast<unsigned long>(0x12345678));
  CheckRepresentation(static_cast<unsigned long>(1));
  CheckRepresentation(static_cast<unsigned long>(0));

  CheckRepresentation(static_cast<int16_t>(0x7fff));
  CheckRepresentation(static_cast<int16_t>(0x1234));
  CheckRepresentation(static_cast<int16_t>(1));
  CheckRepresentation(static_cast<int16_t>(0));
  CheckRepresentation(static_cast<int16_t>(-1));
  CheckRepresentation(static_cast<int16_t>(-0x7fff - 1));

  CheckRepresentation(static_cast<uint16_t>(0xffff));
  CheckRepresentation(static_cast<uint16_t>(0x1234));
  CheckRepresentation(static_cast<uint16_t>(1));
  CheckRepresentation(static_cast<uint16_t>(0));

  CheckRepresentation(static_cast<int32_t>(0x7fffffff));
  CheckRepresentation(static_cast<int32_t>(0x12345678));
  CheckRepresentation(static_cast<int32_t>(1));
  CheckRepresentation(static_cast<int32_t>(0));
  CheckRepresentation(static_cast<int32_t>(-1));
  CheckRepresentation(static_cast<int32_t>(-0x7fffffff - 1));

  CheckRepresentation(static_cast<uint32_t>(0xffffffff));
  CheckRepresentation(static_cast<uint32_t>(0x12345678));
  CheckRepresentation(static_cast<uint32_t>(1));
  CheckRepresentation(static_cast<uint32_t>(0));

  CheckRepresentation(static_cast<int64_t>(0x7fffffffffffffff));
  CheckRepresentation(static_cast<int64_t>(0x123456789abcdef0));
  CheckRepresentation(static_cast<int64_t>(1));
  CheckRepresentation(static_cast<int64_t>(0));
  CheckRepresentation(static_cast<int64_t>(-1));
  CheckRepresentation(static_cast<int64_t>(-0x7fffffffffffffff - 1));

  CheckRepresentation(static_cast<uint64_t>(0xffffffffffffffff));
  CheckRepresentation(static_cast<uint64_t>(0x12345678abcdef0));
  CheckRepresentation(static_cast<uint64_t>(1));
  CheckRepresentation(static_cast<uint64_t>(0));
}

TEST(CompilerTest, PointerRepresentation) {

  char chars[256];
  for (size_t i = 0; i < sizeof(chars); i++) {
    EXPECT_EQ(reinterpret_cast<uintptr_t>(chars) + i,
              reinterpret_cast<uintptr_t>(chars + i));
  }

  int ints[256];
  for (size_t i = 0; i < OPENSSL_ARRAY_SIZE(ints); i++) {
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ints) + i * sizeof(int),
              reinterpret_cast<uintptr_t>(ints + i));
  }

  int *null = nullptr;
  uint8_t bytes[sizeof(null)] = {0};
  EXPECT_EQ(Bytes(bytes),
            Bytes(reinterpret_cast<uint8_t *>(&null), sizeof(null)));
}
