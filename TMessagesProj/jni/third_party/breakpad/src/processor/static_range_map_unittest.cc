// Copyright (c) 2010 Google Inc.
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

// static_range_map_unittest.cc: Unit tests for StaticRangeMap.
//
// Author: Siyang Xie (lambxsy@google.com)

#include "breakpad_googletest_includes.h"
#include "common/scoped_ptr.h"
#include "processor/range_map-inl.h"
#include "processor/static_range_map-inl.h"
#include "processor/simple_serializer-inl.h"
#include "processor/map_serializers-inl.h"
#include "processor/logging.h"

namespace {

typedef int AddressType;
typedef int EntryType;
typedef google_breakpad::StaticRangeMap< AddressType, EntryType > TestMap;
typedef google_breakpad::RangeMap< AddressType, EntryType > RMap;

struct RangeTest {

  AddressType address;

  AddressType size;

  EntryType id;

  bool expect_storable;
};

struct RangeTestSet {

  const RangeTest* range_tests;

  unsigned int range_test_count;
};

const RangeTest range_tests_0[] = {
  { INT_MIN,     16,      1,  true },
  { -2,          5,       2,  true },
  { INT_MAX - 9, 11,      3,  false },
  { INT_MAX - 9, 10,      4,  true },
  { 5,           0,       5,  false },
  { 5,           1,       6,  true },
  { -20,         15,      7,  true },

  { 10,          10,      10, true },
  { 9,           10,      11, false },
  { 9,           11,      12, false },
  { 9,           12,      13, false },
  { 10,          9,       14, false },
  { 10,          10,      15, false },
  { 10,          11,      16, false },
  { 11,          8,       17, false },
  { 11,          9,       18, false },
  { 11,          10,      19, false },
  { 9,           2,       20, false },
  { 10,          1,       21, false },
  { 19,          1,       22, false },
  { 19,          2,       23, false },

  { 9,           1,       24, true },
  { 20,          1,       25, true },

  { 6,           3,       26, true },
  { 7,           3,       27, false },
  { 7,           5,       28, false },
  { 4,           20,      29, false },

  { 30,          50,      30, true },
  { 90,          25,      31, true },
  { 35,          65,      32, false },
  { 120,         10000,   33, true },
  { 20000,       20000,   34, true },
  { 0x10001,     0x10001, 35, true },

  { 27,          -1,      36, false }
};

const RangeTest range_tests_1[] = {
  { INT_MIN, INT_MAX, 50, true },
  { -1,      2,       51, true },
  { 1,       INT_MAX, 52, true },
  { INT_MIN, INT_MAX, 53, false },
  { -1,      2,       54, false },
  { 1,       INT_MAX, 55, false },
  { -3,      6,       56, false },
};

const RangeTest range_tests_2[] = {
  { INT_MIN, 0, 100, false },
  { -1,      3, 101, true },
  { INT_MAX, 0, 102, false },
};

const RangeTest range_tests_3[] = {
  { INT_MIN + 1, 1, 110, true },
  { INT_MAX - 1, 1, 111, true },
  { INT_MIN,     0, 112, false },
  { INT_MAX,     0, 113, false }
};

const RangeTestSet range_test_sets[] = {
  { range_tests_0, sizeof(range_tests_0) / sizeof(RangeTest) },
  { range_tests_1, sizeof(range_tests_1) / sizeof(RangeTest) },
  { range_tests_2, sizeof(range_tests_2) / sizeof(RangeTest) },
  { range_tests_3, sizeof(range_tests_3) / sizeof(RangeTest) },
  { range_tests_0, sizeof(range_tests_0) / sizeof(RangeTest) }
};

}

namespace google_breakpad {
class TestStaticRangeMap : public ::testing::Test {
 protected:
  void SetUp() {
    kTestCasesCount_ = sizeof(range_test_sets) / sizeof(RangeTestSet);
  }

  void StoreTest(RMap* range_map, const RangeTest* range_test);

  void RetrieveTest(TestMap* range_map, const RangeTest* range_test);

  void RetrieveIndexTest(const TestMap* range_map, int set);

  void RunTestCase(int test_case);

  unsigned int kTestCasesCount_;
  RangeMapSerializer<AddressType, EntryType> serializer_;
};

void TestStaticRangeMap::StoreTest(RMap* range_map,
                                   const RangeTest* range_test) {
  bool stored = range_map->StoreRange(range_test->address,
                                      range_test->size,
                                      range_test->id);
  EXPECT_EQ(stored, range_test->expect_storable)
      << "StoreRange id " << range_test->id << "FAILED";
}

void TestStaticRangeMap::RetrieveTest(TestMap* range_map,
                                      const RangeTest* range_test) {
  for (unsigned int side = 0; side <= 1; ++side) {

    AddressType low_offset = -1;
    AddressType high_offset = 1;
    if (range_test->size == 1) {
      if (!side)
        high_offset = 0;
      else
        low_offset = 0;
    }

    for (AddressType offset = low_offset; offset <= high_offset; ++offset) {
      AddressType address =
          offset +
          (!side ? range_test->address :
                   range_test->address + range_test->size - 1);

      bool expected_result = false;
      if (range_test->expect_storable) {
        if (offset == 0)
          expected_result = true;
        else if (offset == -1)
          expected_result = side;
        else
          expected_result = !side;
      }

      const EntryType* id;
      AddressType retrieved_base;
      AddressType retrieved_size;
      bool retrieved = range_map->RetrieveRange(address, id,
                                                &retrieved_base,
                                                &retrieved_size);

      bool observed_result = retrieved && *id == range_test->id;
      EXPECT_EQ(observed_result, expected_result)
          << "RetrieveRange id " << range_test->id
          << ", side " << side << ", offset " << offset << " FAILED.";

      if (observed_result == true) {
        EXPECT_EQ(retrieved_base, range_test->address)
            << "RetrieveRange id " << range_test->id
            << ", side " << side << ", offset " << offset << " FAILED.";
        EXPECT_EQ(retrieved_size, range_test->size)
            << "RetrieveRange id " << range_test->id
            << ", side " << side << ", offset " << offset << " FAILED.";
      }

      bool expected_nearest = range_test->expect_storable;
      if (!side && offset < 0)
        expected_nearest = false;

      AddressType nearest_base;
      AddressType nearest_size;
      bool retrieved_nearest = range_map->RetrieveNearestRange(address,
                                                               id,
                                                               &nearest_base,
                                                               &nearest_size);

      if (side && offset > 0 && nearest_base == address) {
        expected_nearest = false;
      }

      bool observed_nearest = retrieved_nearest &&
                              *id == range_test->id;

      EXPECT_EQ(observed_nearest, expected_nearest)
          << "RetrieveRange id " << range_test->id
          << ", side " << side << ", offset " << offset << " FAILED.";

      if (expected_nearest ==true) {
        EXPECT_EQ(nearest_base, range_test->address)
            << "RetrieveRange id " << range_test->id
            << ", side " << side << ", offset " << offset << " FAILED.";
        EXPECT_EQ(nearest_size, range_test->size)
            << "RetrieveRange id " << range_test->id
            << ", side " << side << ", offset " << offset << " FAILED.";
      }
    }
  }
}

void TestStaticRangeMap::RetrieveIndexTest(const TestMap* range_map, int set) {
  AddressType last_base = 0;
  const EntryType* last_entry = 0;
  const EntryType* entry;
  int object_count = range_map->GetCount();
  for (int object_index = 0; object_index < object_count; ++object_index) {
    AddressType base;
    ASSERT_TRUE(range_map->RetrieveRangeAtIndex(object_index,
                                                entry,
                                                &base,
                                                NULL))
        << "FAILED: RetrieveRangeAtIndex set " << set
        << " index " << object_index;

    ASSERT_TRUE(entry) << "FAILED: RetrieveRangeAtIndex set " << set
                           << " index " << object_index;

    if (last_entry) {

      EXPECT_NE(*entry, *last_entry) << "FAILED: RetrieveRangeAtIndex set "
                                     << set << " index " << object_index;

      EXPECT_GT(base, last_base) << "FAILED: RetrieveRangeAtIndex set " << set
                                 << " index " << object_index;
    }
    last_entry = entry;
    last_base = base;
  }

  ASSERT_FALSE(range_map->RetrieveRangeAtIndex(
      object_count, entry, NULL, NULL)) << "FAILED: RetrieveRangeAtIndex set "
                                        << set << " index " << object_count
                                        << " (too large)";
}

void TestStaticRangeMap::RunTestCase(int test_case) {

  scoped_ptr<RMap> rmap(new RMap());

  const RangeTest* range_tests = range_test_sets[test_case].range_tests;
  unsigned int range_test_count = range_test_sets[test_case].range_test_count;

  int stored_count = 0;
  for (unsigned int range_test_index = 0;
       range_test_index < range_test_count;
       ++range_test_index) {
    const RangeTest* range_test = &range_tests[range_test_index];
    StoreTest(rmap.get(), range_test);

    if (range_test->expect_storable)
      ++stored_count;
  }

  scoped_array<char> memaddr(serializer_.Serialize(*rmap, NULL));
  scoped_ptr<TestMap> static_range_map(new TestMap(memaddr.get()));

  EXPECT_EQ(static_range_map->GetCount(), stored_count);

  for (unsigned int range_test_index = 0;
       range_test_index < range_test_count;
       ++range_test_index) {
    const RangeTest* range_test = &range_tests[range_test_index];
    RetrieveTest(static_range_map.get(), range_test);
  }

  RetrieveIndexTest(static_range_map.get(), test_case);
}

TEST_F(TestStaticRangeMap, TestCase0) {
  int test_case = 0;
  RunTestCase(test_case);
}

TEST_F(TestStaticRangeMap, TestCase1) {
  int test_case = 1;
  RunTestCase(test_case);
}

TEST_F(TestStaticRangeMap, TestCase2) {
  int test_case = 2;
  RunTestCase(test_case);
}

TEST_F(TestStaticRangeMap, TestCase3) {
  int test_case = 3;
  RunTestCase(test_case);
}

TEST_F(TestStaticRangeMap, RunTestCase0Again) {
  int test_case = 0;
  RunTestCase(test_case);
}

}

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
