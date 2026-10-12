// Copyright (c) 2006, Google Inc.
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

// contained_range_map_unittest.cc: Unit tests for ContainedRangeMap
//
// Author: Mark Mentovai

#include <stdio.h>

#include "processor/contained_range_map-inl.h"

#include "processor/logging.h"

#define ASSERT_TRUE(condition) \
  if (!(condition)) { \
    fprintf(stderr, "FAIL: %s @ %s:%d\n", #condition, __FILE__, __LINE__); \
    return false; \
  }

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

namespace {

using google_breakpad::ContainedRangeMap;

static bool RunTests() {
  ContainedRangeMap<unsigned int, int> crm;

  ASSERT_TRUE (crm.StoreRange(10, 10,  1));
  ASSERT_FALSE(crm.StoreRange(10, 10,  2));
  ASSERT_FALSE(crm.StoreRange(11, 10,  3));
  ASSERT_FALSE(crm.StoreRange( 9, 10,  4));
  ASSERT_TRUE (crm.StoreRange(11,  9,  5));
  ASSERT_TRUE (crm.StoreRange(12,  7,  6));
  ASSERT_TRUE (crm.StoreRange( 9, 12,  7));
  ASSERT_TRUE (crm.StoreRange( 9, 13,  8));
  ASSERT_TRUE (crm.StoreRange( 8, 14,  9));
  ASSERT_TRUE (crm.StoreRange(30,  3, 10));
  ASSERT_TRUE (crm.StoreRange(33,  3, 11));
  ASSERT_TRUE (crm.StoreRange(30,  6, 12));
  ASSERT_TRUE (crm.StoreRange(40,  8, 13));
  ASSERT_TRUE (crm.StoreRange(40,  4, 14));
  ASSERT_TRUE (crm.StoreRange(44,  4, 15));
  ASSERT_FALSE(crm.StoreRange(32, 10, 16));
  ASSERT_FALSE(crm.StoreRange(50,  0, 17));
  ASSERT_TRUE (crm.StoreRange(50, 10, 18));
  ASSERT_TRUE (crm.StoreRange(50,  1, 19));
  ASSERT_TRUE (crm.StoreRange(59,  1, 20));
  ASSERT_TRUE (crm.StoreRange(60,  1, 21));
  ASSERT_TRUE (crm.StoreRange(69,  1, 22));
  ASSERT_TRUE (crm.StoreRange(60, 10, 23));
  ASSERT_TRUE (crm.StoreRange(68,  1, 24));
  ASSERT_TRUE (crm.StoreRange(61,  1, 25));
  ASSERT_TRUE (crm.StoreRange(61,  8, 26));
  ASSERT_FALSE(crm.StoreRange(59,  9, 27));
  ASSERT_FALSE(crm.StoreRange(59, 10, 28));
  ASSERT_FALSE(crm.StoreRange(59, 11, 29));
  ASSERT_TRUE (crm.StoreRange(70, 10, 30));
  ASSERT_TRUE (crm.StoreRange(74,  2, 31));
  ASSERT_TRUE (crm.StoreRange(77,  2, 32));
  ASSERT_FALSE(crm.StoreRange(72,  6, 33));
  ASSERT_TRUE (crm.StoreRange(80,  3, 34));
  ASSERT_TRUE (crm.StoreRange(81,  1, 35));
  ASSERT_TRUE (crm.StoreRange(82,  1, 36));
  ASSERT_TRUE (crm.StoreRange(83,  3, 37));
  ASSERT_TRUE (crm.StoreRange(84,  1, 38));
  ASSERT_TRUE (crm.StoreRange(83,  1, 39));
  ASSERT_TRUE (crm.StoreRange(86,  5, 40));
  ASSERT_TRUE (crm.StoreRange(88,  1, 41));
  ASSERT_TRUE (crm.StoreRange(90,  1, 42));
  ASSERT_TRUE (crm.StoreRange(86,  1, 43));
  ASSERT_TRUE (crm.StoreRange(87,  1, 44));
  ASSERT_TRUE (crm.StoreRange(89,  1, 45));
  ASSERT_TRUE (crm.StoreRange(87,  4, 46));
  ASSERT_TRUE (crm.StoreRange(87,  3, 47));
  ASSERT_FALSE(crm.StoreRange(86,  2, 48));

  const int test_data[] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    9,
    7,
    1,
    5,
    6,
    6,
    6,
    6,
    6,
    6,
    6,
    5,
    7,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    10,
    10,
    10,
    11,
    11,
    11,
    0,
    0,
    0,
    0,
    14,
    14,
    14,
    14,
    15,
    15,
    15,
    15,
    0,
    0,
    19,
    18,
    18,
    18,
    18,
    18,
    18,
    18,
    18,
    20,
    21,
    25,
    26,
    26,
    26,
    26,
    26,
    26,
    24,
    22,
    30,
    30,
    30,
    30,
    31,
    31,
    30,
    32,
    32,
    30,
    34,
    35,
    36,
    39,
    38,
    37,
    43,
    44,
    41,
    45,
    42,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
  };
  unsigned int test_high = sizeof(test_data) / sizeof(int);

#ifdef GENERATE_TEST_DATA
  printf("  const int test_data[] = {\n");
#endif

  for (unsigned int address = 0; address < test_high; ++address) {
    int value;
    if (!crm.RetrieveRange(address, &value))
      value = 0;

#ifndef GENERATE_TEST_DATA

    if (value != test_data[address]) {
      fprintf(stderr, "FAIL: retrieve %d expected %d observed %d @ %s:%d\n",
              address, test_data[address], value, __FILE__, __LINE__);
      return false;
    }
#else
    printf("    %d%c%s  // %d\n", value,
                                  address == test_high - 1 ? ' ' : ',',
                                  value < 10 ? " " : "",
                                  address);
#endif
  }

#ifdef GENERATE_TEST_DATA
  printf("  };\n");
#endif

  return true;
}

}

int main(int argc, char **argv) {
  BPLOG_INIT(&argc, &argv);

  return RunTests() ? 0 : 1;
}
