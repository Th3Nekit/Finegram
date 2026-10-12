// Copyright 2015 The Chromium Authors
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

#include "parse_values.h"

#include <stdint.h>

#include <gtest/gtest.h>

BSSL_NAMESPACE_BEGIN
namespace der::test {

namespace {

template <size_t N>
Input FromStringLiteral(const char (&data)[N]) {

  return Input(reinterpret_cast<const uint8_t *>(data), N - 1);
}

}

TEST(ParseValuesTest, ParseBool) {
  uint8_t buf[] = {0xFF, 0x00};
  Input value(buf, 1);
  bool out;
  EXPECT_TRUE(ParseBool(value, &out));
  EXPECT_TRUE(out);

  buf[0] = 0;
  EXPECT_TRUE(ParseBool(value, &out));
  EXPECT_FALSE(out);

  buf[0] = 1;
  EXPECT_FALSE(ParseBool(value, &out));
  EXPECT_TRUE(ParseBoolRelaxed(value, &out));
  EXPECT_TRUE(out);

  buf[0] = 0xFF;
  value = Input(buf, 2);
  EXPECT_FALSE(ParseBool(value, &out));
  value = Input(buf, 0);
  EXPECT_FALSE(ParseBool(value, &out));
}

TEST(ParseValuesTest, ParseTimes) {
  GeneralizedTime out;

  EXPECT_TRUE(ParseUTCTime(FromStringLiteral("140218161200Z"), &out));

  EXPECT_FALSE(ParseUTCTime(FromStringLiteral("140218161200X"), &out));

  EXPECT_FALSE(ParseUTCTime(FromStringLiteral("-40218161200Z"), &out));

  EXPECT_TRUE(ParseUTCTime(FromStringLiteral("090218161200Z"), &out));

  EXPECT_FALSE(ParseUTCTime(FromStringLiteral("140218161200"), &out));
  EXPECT_FALSE(ParseUTCTime(FromStringLiteral("140218161200Z0"), &out));

  EXPECT_FALSE(ParseUTCTime(FromStringLiteral("1402181612Z"), &out));

  EXPECT_TRUE(ParseGeneralizedTime(FromStringLiteral("00000101000000Z"), &out));
  EXPECT_EQ(0, out.year);
  EXPECT_TRUE(ParseGeneralizedTime(FromStringLiteral("99991231235960Z"), &out));
  EXPECT_EQ(9999, out.year);

  EXPECT_TRUE(ParseGeneralizedTime(FromStringLiteral("20140218161260Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20140218161261Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20140218166000Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20140218240000Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20140200161200Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20140018161200Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20141318161200Z"), &out));

  EXPECT_TRUE(ParseGeneralizedTime(FromStringLiteral("20140131000000Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20140931000000Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20140229000000Z"), &out));

  EXPECT_TRUE(ParseGeneralizedTime(FromStringLiteral("20160229000000Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("21000229000000Z"), &out));

  EXPECT_TRUE(ParseGeneralizedTime(FromStringLiteral("20000229000000Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20001231010203Z\0"), &out));

  EXPECT_FALSE(ParseGeneralizedTime(FromStringLiteral("200\0"
                                                      "1231010203Z"),
                                    &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("0x201231000000Z"), &out));

  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20001231000000X"), &out));

  EXPECT_FALSE(ParseGeneralizedTime(FromStringLiteral("20140218161200"), &out));
  EXPECT_FALSE(
      ParseGeneralizedTime(FromStringLiteral("20140218161200Z0"), &out));
}

TEST(ParseValuesTest, TimesCompare) {
  GeneralizedTime time1;
  GeneralizedTime time2;
  GeneralizedTime time3;

  ASSERT_TRUE(
      ParseGeneralizedTime(FromStringLiteral("20140218161200Z"), &time1));

  ASSERT_TRUE(ParseUTCTime(FromStringLiteral("150218161200Z"), &time2));
  ASSERT_TRUE(
      ParseGeneralizedTime(FromStringLiteral("20160218161200Z"), &time3));
  EXPECT_TRUE(time1 < time2);
  EXPECT_TRUE(time2 < time3);

  EXPECT_TRUE(time2 > time1);
  EXPECT_TRUE(time2 >= time1);
  EXPECT_TRUE(time2 <= time3);
  EXPECT_TRUE(time1 <= time1);
  EXPECT_TRUE(time1 >= time1);
}

TEST(ParseValuesTest, UTCTimeRange) {
  GeneralizedTime time;
  ASSERT_TRUE(
      ParseGeneralizedTime(FromStringLiteral("20140218161200Z"), &time));
  EXPECT_TRUE(time.InUTCTimeRange());

  time.year = 1950;
  EXPECT_TRUE(time.InUTCTimeRange());

  time.year = 1949;
  EXPECT_FALSE(time.InUTCTimeRange());

  time.year = 2049;
  EXPECT_TRUE(time.InUTCTimeRange());

  time.year = 2050;
  EXPECT_FALSE(time.InUTCTimeRange());
}

struct Uint64TestData {
  bool should_pass;
  const uint8_t input[9];
  size_t length;
  uint64_t expected_value = 0;
};

const Uint64TestData kUint64TestData[] = {
    {true, {0x00}, 1, 0},

    {false, {0x00, 0x00}, 2},
    {true, {0x01}, 1, 1},
    {false, {0xFF}, 1},
    {true, {0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 8, INT64_MAX},
    {true,
     {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
     9,
     UINT64_MAX},

    {false, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 8},
    {false, {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 8},
    {false, {0x00, 0x01}, 2},
    {false, {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09}, 9},
    {false, {0}, 0},
};

TEST(ParseValuesTest, ParseUint64) {
  for (size_t i = 0; i < std::size(kUint64TestData); i++) {
    const Uint64TestData &test_case = kUint64TestData[i];
    SCOPED_TRACE(i);

    uint64_t result;
    EXPECT_EQ(test_case.should_pass,
              ParseUint64(Input(test_case.input, test_case.length), &result));
    if (test_case.should_pass) {
      EXPECT_EQ(test_case.expected_value, result);
    }
  }
}

struct Uint8TestData {
  bool should_pass;
  const uint8_t input[9];
  size_t length;
  uint8_t expected_value = 0;
};

const Uint8TestData kUint8TestData[] = {
    {true, {0x00}, 1, 0},

    {false, {0x00, 0x00}, 2},
    {true, {0x01}, 1, 1},
    {false, {0x01, 0xFF}, 2},
    {false, {0x03, 0x83}, 2},
    {true, {0x7F}, 1, 0x7F},
    {true, {0x00, 0xFF}, 2, 0xFF},

    {false, {0xFF}, 1},
    {false, {0x80}, 1},
    {false, {0x00, 0x01}, 2},
    {false, {0}, 0},
};

TEST(ParseValuesTest, ParseUint8) {
  for (size_t i = 0; i < std::size(kUint8TestData); i++) {
    const Uint8TestData &test_case = kUint8TestData[i];
    SCOPED_TRACE(i);

    uint8_t result;
    EXPECT_EQ(test_case.should_pass,
              ParseUint8(Input(test_case.input, test_case.length), &result));
    if (test_case.should_pass) {
      EXPECT_EQ(test_case.expected_value, result);
    }
  }
}

struct IsValidIntegerTestData {
  bool should_pass;
  const uint8_t input[2];
  size_t length;
  bool negative = false;
};

const IsValidIntegerTestData kIsValidIntegerTestData[] = {

    {false, {0x00}, 0},

    {true, {0x00}, 1, false},

    {false, {0x00, 0x00}, 2},

    {true, {0x80}, 1, true},
    {true, {0xFF}, 1, true},

    {false, {0xFF, 0x80}, 2},

    {true, {0x00, 0x80}, 2, false},

    {true, {0xFF, 0x7F}, 2, true},
};

TEST(ParseValuesTest, IsValidInteger) {
  for (size_t i = 0; i < std::size(kIsValidIntegerTestData); i++) {
    const auto &test_case = kIsValidIntegerTestData[i];
    SCOPED_TRACE(i);

    bool negative;
    EXPECT_EQ(
        test_case.should_pass,
        IsValidInteger(Input(test_case.input, test_case.length), &negative));
    if (test_case.should_pass) {
      EXPECT_EQ(test_case.negative, negative);
    }
  }
}

TEST(ParseValuesTest, ParseBitStringEmptyNoUnusedBits) {
  const uint8_t kData[] = {0x00};

  std::optional<BitString> bit_string = ParseBitString(Input(kData));
  ASSERT_TRUE(bit_string.has_value());

  EXPECT_EQ(0u, bit_string->unused_bits());
  EXPECT_EQ(0u, bit_string->bytes().size());

  EXPECT_FALSE(bit_string->AssertsBit(0));
  EXPECT_FALSE(bit_string->AssertsBit(1));
  EXPECT_FALSE(bit_string->AssertsBit(3));
}

TEST(ParseValuesTest, ParseBitStringEmptyOneUnusedBit) {
  const uint8_t kData[] = {0x01};

  std::optional<BitString> bit_string = ParseBitString(Input(kData));
  EXPECT_FALSE(bit_string.has_value());
}

TEST(ParseValuesTest, ParseBitStringNonEmptyTooManyUnusedBits) {
  const uint8_t kData[] = {0x08, 0x00};

  std::optional<BitString> bit_string = ParseBitString(Input(kData));
  EXPECT_FALSE(bit_string.has_value());
}

TEST(ParseValuesTest, ParseBitStringSevenOneBits) {
  const uint8_t kData[] = {0x01, 0xFE};

  std::optional<BitString> bit_string = ParseBitString(Input(kData));
  ASSERT_TRUE(bit_string.has_value());

  EXPECT_EQ(1u, bit_string->unused_bits());
  EXPECT_EQ(1u, bit_string->bytes().size());
  EXPECT_EQ(0xFE, bit_string->bytes()[0]);

  EXPECT_TRUE(bit_string->AssertsBit(0));
  EXPECT_TRUE(bit_string->AssertsBit(1));
  EXPECT_TRUE(bit_string->AssertsBit(2));
  EXPECT_TRUE(bit_string->AssertsBit(3));
  EXPECT_TRUE(bit_string->AssertsBit(4));
  EXPECT_TRUE(bit_string->AssertsBit(5));
  EXPECT_TRUE(bit_string->AssertsBit(6));
  EXPECT_FALSE(bit_string->AssertsBit(7));
  EXPECT_FALSE(bit_string->AssertsBit(8));
}

TEST(ParseValuesTest, ParseBitStringSevenOneBitsUnusedBitIsOne) {
  const uint8_t kData[] = {0x01, 0xFF};

  std::optional<BitString> bit_string = ParseBitString(Input(kData));
  EXPECT_FALSE(bit_string.has_value());
}

TEST(ParseValuesTest, ParseIA5String) {
  const uint8_t valid_der[] = {0x46, 0x6f, 0x6f, 0x20, 0x62,
                               0x61, 0x72, 0x01, 0x7f};
  std::string s;
  EXPECT_TRUE(ParseIA5String(der::Input(valid_der), &s));
  EXPECT_EQ("Foo bar\x01\x7f", s);

  const uint8_t invalid_der[] = {0x46, 0x6f, 0x80, 0x20, 0x62, 0x61, 0x72};
  EXPECT_FALSE(ParseIA5String(der::Input(invalid_der), &s));
}

TEST(ParseValuesTest, ParseVisibleString) {
  const uint8_t valid_der[] = {0x46, 0x6f, 0x6f, 0x20, 0x62, 0x61, 0x72, 0x7e};
  std::string s;
  EXPECT_TRUE(ParseVisibleString(der::Input(valid_der), &s));
  EXPECT_EQ("Foo bar\x7e", s);

  const uint8_t invalid_der[] = {0x46, 0x6f, 0x7f, 0x20, 0x62, 0x61, 0x72};
  EXPECT_FALSE(ParseVisibleString(der::Input(invalid_der), &s));

  const uint8_t invalid_der2[] = {0x46, 0x6f, 0x1f, 0x20, 0x62, 0x61, 0x72};
  EXPECT_FALSE(ParseVisibleString(der::Input(invalid_der2), &s));
}

TEST(ParseValuesTest, ParsePrintableString) {
  const uint8_t valid_der[] = {0x46, 0x6f, 0x6f, 0x20, 0x62, 0x61, 0x72};
  std::string s;
  EXPECT_TRUE(ParsePrintableString(der::Input(valid_der), &s));
  EXPECT_EQ("Foo bar", s);

  const uint8_t invalid_der[] = {0x46, 0x6f, 0x5f, 0x20, 0x62, 0x61, 0x72};
  EXPECT_FALSE(ParsePrintableString(der::Input(invalid_der), &s));
}

TEST(ParseValuesTest, ParseTeletexStringAsLatin1) {
  const uint8_t valid_der[] = {0x46, 0x6f, 0xd6, 0x20, 0x62, 0x61, 0x72};
  std::string s;
  EXPECT_TRUE(ParseTeletexStringAsLatin1(der::Input(valid_der), &s));
  EXPECT_EQ("FoÖ bar", s);
}

TEST(ParseValuesTest, ParseBmpString) {
  const uint8_t valid_der[] = {0x00, 0x66, 0x00, 0x6f, 0x00, 0x6f,
                               0x00, 0x62, 0x00, 0x61, 0x00, 0x72};
  std::string s;
  EXPECT_TRUE(ParseBmpString(der::Input(valid_der), &s));
  EXPECT_EQ("foobar", s);

  const uint8_t valid_nonascii_der[] = {0x27, 0x28, 0x26, 0xa1, 0x2b, 0x50};
  EXPECT_TRUE(ParseBmpString(der::Input(valid_nonascii_der), &s));
  EXPECT_EQ("✨⚡⭐", s);

  const uint8_t invalid_odd_der[] = {0x00, 0x66, 0x00, 0x6f, 0x00};
  EXPECT_FALSE(ParseBmpString(der::Input(invalid_odd_der), &s));

  const uint8_t invalid_bmp_valid_utf16_with_surrogate[] = {0xd8, 0x34, 0xdd,
                                                            0x1e};
  EXPECT_FALSE(
      ParseBmpString(der::Input(invalid_bmp_valid_utf16_with_surrogate), &s));
}

TEST(ParseValuesTest, ParseUniversalString) {
  const uint8_t valid_der[] = {0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x6f,
                               0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x62,
                               0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x72};
  std::string s;
  EXPECT_TRUE(ParseUniversalString(der::Input(valid_der), &s));
  EXPECT_EQ("foobar", s);

  const uint8_t valid_non_ascii_der[] = {0x0,  0x1,  0xf4, 0xe,  0x0,  0x0, 0x0,
                                         0x20, 0x0,  0x1,  0xd1, 0x1e, 0x0, 0x0,
                                         0x26, 0x69, 0x0,  0x0,  0x26, 0x6b};
  EXPECT_TRUE(ParseUniversalString(der::Input(valid_non_ascii_der), &s));
  EXPECT_EQ("🐎 𝄞♩♫", s);

  const uint8_t invalid_non_4_multiple_der[] = {0x00, 0x00, 0x00,
                                                0x66, 0x00, 0x00};
  EXPECT_FALSE(
      ParseUniversalString(der::Input(invalid_non_4_multiple_der), &s));
}

}
BSSL_NAMESPACE_END
