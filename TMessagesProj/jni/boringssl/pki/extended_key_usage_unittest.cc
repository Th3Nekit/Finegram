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

#include <algorithm>

#include <gtest/gtest.h>
#include "extended_key_usage.h"
#include "input.h"

BSSL_NAMESPACE_BEGIN

namespace {

bool HasEKU(const std::vector<der::Input> &list, der::Input eku) {
  for (const auto &oid : list) {
    if (oid == eku) {
      return true;
    }
  }
  return false;
}

TEST(ExtendedKeyUsageTest, ParseEKUExtension) {

  const uint8_t raw_extension_value[] = {
      0x30, 0x14,
      0x06, 0x08,
      0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01,
      0x06, 0x08,
      0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02

  };

  der::Input extension_value(raw_extension_value);

  std::vector<der::Input> ekus;
  EXPECT_TRUE(ParseEKUExtension(extension_value, &ekus));

  EXPECT_EQ(2u, ekus.size());
  EXPECT_TRUE(HasEKU(ekus, der::Input(kServerAuth)));
  EXPECT_TRUE(HasEKU(ekus, der::Input(kClientAuth)));
}

TEST(ExtendedKeyUsageTest, RepeatedOid) {

  const uint8_t extension_bytes[] = {
      0x30, 0x14,
      0x06, 0x08,
      0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01,
      0x06, 0x08,
      0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
  };

  der::Input extension(extension_bytes);

  std::vector<der::Input> ekus;
  EXPECT_TRUE(ParseEKUExtension(extension, &ekus));
  EXPECT_EQ(2u, ekus.size());
  for (const auto &eku : ekus) {
    EXPECT_EQ(der::Input(kServerAuth), eku);
  }
}

TEST(ExtendedKeyUsageTest, ParseEKUExtensionGracefullyHandlesPrivateOids) {

  const uint8_t extension_bytes[] = {
    0x30, 0x13,
    0x06, 0x08,
    0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01,
    0x06, 0x07,
    0x2B, 0x06, 0x01, 0x04, 0x01, 0xD6, 0x79
  };

  der::Input extension(extension_bytes);

  std::vector<der::Input> ekus;
  EXPECT_TRUE(ParseEKUExtension(extension, &ekus));
  EXPECT_EQ(2u, ekus.size());
  EXPECT_TRUE(HasEKU(ekus, der::Input(kServerAuth)));

  const uint8_t google_oid[] = {0x2B, 0x06, 0x01, 0x04, 0x01, 0xD6, 0x79};
  der::Input google(google_oid);
  EXPECT_TRUE(HasEKU(ekus, google));
}

TEST(ExtendedKeyUsageTest, ExtraData) {

  const uint8_t extra_data[] = {
      0x30, 0x14,
      0x06, 0x08,
      0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01,
      0x06, 0x08,
      0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02,

      0x02, 0x01,
      0x01
  };

  std::vector<der::Input> ekus;
  EXPECT_FALSE(ParseEKUExtension(der::Input(extra_data), &ekus));
}

TEST(ExtendedKeyUsageTest, NotAnOid) {

  const uint8_t not_an_oid[] = {
      0x30, 0x0d,
      0x06, 0x08,
      0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01,
      0x02, 0x01,
      0x01

  };

  std::vector<der::Input> ekus;
  EXPECT_FALSE(ParseEKUExtension(der::Input(not_an_oid), &ekus));
}

TEST(ExtendedKeyUsageTest, NotASequence) {

  const uint8_t not_a_sequence[] = {
      0x06, 0x08,
      0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
  };

  std::vector<der::Input> ekus;
  EXPECT_FALSE(ParseEKUExtension(der::Input(not_a_sequence), &ekus));
}

TEST(ExtendedKeyUsageTest, EmptySequence) {
  const uint8_t empty_sequence[] = {0x30, 0x00};

  std::vector<der::Input> ekus;
  EXPECT_FALSE(ParseEKUExtension(der::Input(empty_sequence), &ekus));
}

TEST(ExtendedKeyUsageTest, EmptyExtension) {
  std::vector<der::Input> ekus;
  EXPECT_FALSE(ParseEKUExtension(der::Input(), &ekus));
}

}

BSSL_NAMESPACE_END
