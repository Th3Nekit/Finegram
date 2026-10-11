// Copyright (c) 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// map_serializers_unittest.cc: Unit tests for std::map serializer and
// std::map wrapper serializers.
//
// Author: Siyang Xie (lambxsy@google.com)

#include <climits>
#include <map>
#include <string>
#include <utility>
#include <iostream>
#include <sstream>

#include "breakpad_googletest_includes.h"
#include "map_serializers-inl.h"

#include "processor/address_map-inl.h"
#include "processor/range_map-inl.h"
#include "processor/contained_range_map-inl.h"

typedef int32_t AddrType;
typedef int32_t EntryType;

class TestStdMapSerializer : public ::testing::Test {
 protected:
  void SetUp() {
    serialized_size_ = 0;
    serialized_data_ = NULL;
  }

  void TearDown() {
    delete [] serialized_data_;
  }

  std::map<AddrType, EntryType> std_map_;
  google_breakpad::StdMapSerializer<AddrType, EntryType> serializer_;
  uint32_t serialized_size_;
  char *serialized_data_;
};

TEST_F(TestStdMapSerializer, EmptyMapTestCase) {
  const int32_t correct_data[] = { 0 };
  uint32_t correct_size = sizeof(correct_data);

  serialized_data_ = serializer_.Serialize(std_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

TEST_F(TestStdMapSerializer, MapWithTwoElementsTestCase) {
  const int32_t correct_data[] = {

      2,

      20, 24,

      1, 3,

      2, 6
  };
  uint32_t correct_size = sizeof(correct_data);

  std_map_.insert(std::make_pair(1, 2));
  std_map_.insert(std::make_pair(3, 6));

  serialized_data_ = serializer_.Serialize(std_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

TEST_F(TestStdMapSerializer, MapWithFiveElementsTestCase) {
  const int32_t correct_data[] = {

      5,

      44, 48, 52, 56, 60,

      1, 2, 3, 4, 5,

      11, 12, 13, 14, 15
  };
  uint32_t correct_size = sizeof(correct_data);

  for (int i = 1; i < 6; ++i)
    std_map_.insert(std::make_pair(i, 10 + i));

  serialized_data_ = serializer_.Serialize(std_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

class TestAddressMapSerializer : public ::testing::Test {
 protected:
  void SetUp() {
    serialized_size_ = 0;
    serialized_data_ = 0;
  }

  void TearDown() {
    delete [] serialized_data_;
  }

  google_breakpad::AddressMap<AddrType, EntryType> address_map_;
  google_breakpad::AddressMapSerializer<AddrType, EntryType> serializer_;
  uint32_t serialized_size_;
  char *serialized_data_;
};

TEST_F(TestAddressMapSerializer, EmptyMapTestCase) {
  const int32_t correct_data[] = { 0 };
  uint32_t correct_size = sizeof(correct_data);

  serialized_data_ = serializer_.Serialize(address_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

TEST_F(TestAddressMapSerializer, MapWithTwoElementsTestCase) {
  const int32_t correct_data[] = {

      2,

      20, 24,

      1, 3,

      2, 6
  };
  uint32_t correct_size = sizeof(correct_data);

  address_map_.Store(1, 2);
  address_map_.Store(3, 6);

  serialized_data_ = serializer_.Serialize(address_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

TEST_F(TestAddressMapSerializer, MapWithFourElementsTestCase) {
  const int32_t correct_data[] = {

      4,

      36, 40, 44, 48,

      -6, -4, 8, 123,

      2, 3, 5, 8
  };
  uint32_t correct_size = sizeof(correct_data);

  address_map_.Store(-6, 2);
  address_map_.Store(-4, 3);
  address_map_.Store(8, 5);
  address_map_.Store(123, 8);

  serialized_data_ = serializer_.Serialize(address_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

class TestRangeMapSerializer : public ::testing::Test {
 protected:
  void SetUp() {
    serialized_size_ = 0;
    serialized_data_ = 0;
  }

  void TearDown() {
    delete [] serialized_data_;
  }

  google_breakpad::RangeMap<AddrType, EntryType> range_map_;
  google_breakpad::RangeMapSerializer<AddrType, EntryType> serializer_;
  uint32_t serialized_size_;
  char *serialized_data_;
};

TEST_F(TestRangeMapSerializer, EmptyMapTestCase) {
  const int32_t correct_data[] = { 0 };
  uint32_t correct_size = sizeof(correct_data);

  serialized_data_ = serializer_.Serialize(range_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

TEST_F(TestRangeMapSerializer, MapWithOneRangeTestCase) {
  const int32_t correct_data[] = {

      1,

      12,

      10,

      1, 6
  };
  uint32_t correct_size = sizeof(correct_data);

  range_map_.StoreRange(1, 10, 6);

  serialized_data_ = serializer_.Serialize(range_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

TEST_F(TestRangeMapSerializer, MapWithThreeRangesTestCase) {
  const int32_t correct_data[] = {

      3,

      28,    36,    44,

      5,     9,     20,

      2, 1,  6, 2,  10, 3
  };
  uint32_t correct_size = sizeof(correct_data);

  ASSERT_TRUE(range_map_.StoreRange(2, 4, 1));
  ASSERT_TRUE(range_map_.StoreRange(6, 4, 2));
  ASSERT_TRUE(range_map_.StoreRange(10, 11, 3));

  serialized_data_ = serializer_.Serialize(range_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

class TestContainedRangeMapSerializer : public ::testing::Test {
 protected:
  void SetUp() {
    serialized_size_ = 0;
    serialized_data_ = 0;
  }

  void TearDown() {
    delete [] serialized_data_;
  }

  google_breakpad::ContainedRangeMap<AddrType, EntryType> crm_map_;
  google_breakpad::ContainedRangeMapSerializer<AddrType, EntryType> serializer_;
  uint32_t serialized_size_;
  char *serialized_data_;
};

TEST_F(TestContainedRangeMapSerializer, EmptyMapTestCase) {
  const int32_t correct_data[] = {
      0,
      4,
      0,
      0
  };
  uint32_t correct_size = sizeof(correct_data);

  serialized_data_ = serializer_.Serialize(&crm_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

TEST_F(TestContainedRangeMapSerializer, MapWithOneRangeTestCase) {
  const int32_t correct_data[] = {
      0,
      4,
      0,

      1,
      12,
      9,

      3,
      4,
      -1,
      0
  };
  uint32_t correct_size = sizeof(correct_data);

  crm_map_.StoreRange(3, 7, -1);

  serialized_data_ = serializer_.Serialize(&crm_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

TEST_F(TestContainedRangeMapSerializer, MapWithTwoLevelsTestCase) {

  const int32_t correct_data[] = {

      0, 4, 0,

      2, 20, 84, 8, 20,

      2, 4, -1,

      2, 20, 36, 4, 7,

        3, 4, -1, 0,

        6, 4, -1, 0,

      10, 4, -1,

      1, 12, 20,

        16, 4, -1, 0
  };
  uint32_t correct_size = sizeof(correct_data);

  ASSERT_TRUE(crm_map_.StoreRange(2, 7, -1));

  ASSERT_TRUE(crm_map_.StoreRange(10, 11, -1));

  ASSERT_TRUE(crm_map_.StoreRange(3, 2, -1));

  ASSERT_TRUE(crm_map_.StoreRange(6, 2, -1));

  ASSERT_TRUE(crm_map_.StoreRange(16, 5, -1));

  serialized_data_ = serializer_.Serialize(&crm_map_, &serialized_size_);

  EXPECT_EQ(correct_size, serialized_size_);
  EXPECT_EQ(memcmp(correct_data, serialized_data_, correct_size), 0);
}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
