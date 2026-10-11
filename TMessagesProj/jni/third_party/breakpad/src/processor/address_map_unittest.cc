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

// address_map_unittest.cc: Unit tests for AddressMap.
//
// Author: Mark Mentovai

#include <limits.h>
#include <stdio.h>

#include "processor/address_map-inl.h"
#include "processor/linked_ptr.h"
#include "processor/logging.h"

#define ASSERT_TRUE(condition) \
  if (!(condition)) { \
    fprintf(stderr, "FAIL: %s @ %s:%d\n", #condition, __FILE__, __LINE__); \
    return false; \
  }

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(e1, e2) ASSERT_TRUE((e1) == (e2))

namespace {

using google_breakpad::AddressMap;
using google_breakpad::linked_ptr;

class CountedObject {
 public:
  explicit CountedObject(int id) : id_(id) { ++count_; }
  ~CountedObject() { --count_; }

  static int count() { return count_; }
  int id() const { return id_; }

 private:
  static int count_;
  int id_;
};

int CountedObject::count_;

typedef int AddressType;
typedef AddressMap< AddressType, linked_ptr<CountedObject> > TestMap;

static bool DoAddressMapTest() {
  ASSERT_EQ(CountedObject::count(), 0);

  TestMap test_map;
  linked_ptr<CountedObject> entry;
  AddressType address;

  ASSERT_FALSE(test_map.Retrieve(0, &entry, &address));
  ASSERT_FALSE(test_map.Retrieve(INT_MIN, &entry, &address));
  ASSERT_FALSE(test_map.Retrieve(INT_MAX, &entry, &address));

  ASSERT_EQ(CountedObject::count(), 0);
  ASSERT_TRUE(test_map.Store(1,
      linked_ptr<CountedObject>(new CountedObject(0))));
  ASSERT_TRUE(test_map.Retrieve(1, &entry, &address));
  ASSERT_EQ(CountedObject::count(), 1);
  test_map.Clear();
  ASSERT_EQ(CountedObject::count(), 1);

  ASSERT_FALSE(test_map.Retrieve(0, &entry, &address));
  ASSERT_FALSE(test_map.Retrieve(INT_MIN, &entry, &address));
  ASSERT_FALSE(test_map.Retrieve(INT_MAX, &entry, &address));

  ASSERT_TRUE(test_map.Store(10,
      linked_ptr<CountedObject>(new CountedObject(1))));
  ASSERT_FALSE(test_map.Retrieve(9, &entry, &address));
  ASSERT_TRUE(test_map.Retrieve(10, &entry, &address));
  ASSERT_EQ(CountedObject::count(), 1);
  ASSERT_EQ(entry->id(), 1);
  ASSERT_EQ(address, 10);
  ASSERT_TRUE(test_map.Retrieve(11, &entry, &address));
  ASSERT_TRUE(test_map.Retrieve(11, &entry, NULL));

  ASSERT_TRUE(test_map.Store(5,
      linked_ptr<CountedObject>(new CountedObject(2))));
  ASSERT_EQ(CountedObject::count(), 2);
  ASSERT_TRUE(test_map.Store(20,
      linked_ptr<CountedObject>(new CountedObject(3))));
  ASSERT_TRUE(test_map.Store(15,
      linked_ptr<CountedObject>(new CountedObject(4))));
  ASSERT_FALSE(test_map.Store(10,
      linked_ptr<CountedObject>(new CountedObject(5))));
  ASSERT_TRUE(test_map.Store(16,
      linked_ptr<CountedObject>(new CountedObject(6))));
  ASSERT_TRUE(test_map.Store(14,
      linked_ptr<CountedObject>(new CountedObject(7))));

  for (AddressType key = 0; key < 5; ++key) {
    if (test_map.Retrieve(key, &entry, &address)) {
      fprintf(stderr,
              "FAIL: retrieve %d expected false observed true @ %s:%d\n",
              key, __FILE__, __LINE__);
      return false;
    }
  }

  const int id_verify[] = { 0, 0, 0, 0, 0,
                            2, 2, 2, 2, 2,
                            1, 1, 1, 1, 7,
                            4, 6, 6, 6, 6,
                            3, 3, 3, 3, 3,
                            3, 3, 3, 3, 3 };
  const AddressType address_verify[] = {  0,  0,  0,  0,  0,
                                          5,  5,  5,  5,  5,
                                         10, 10, 10, 10, 14,
                                         15, 16, 16, 16, 16,
                                         20, 20, 20, 20, 20,
                                         20, 20, 20, 20, 20 };

  for (AddressType key = 5; key < 30; ++key) {
    if (!test_map.Retrieve(key, &entry, &address)) {
      fprintf(stderr,
              "FAIL: retrieve %d expected true observed false @ %s:%d\n",
              key, __FILE__, __LINE__);
      return false;
    }
    if (entry->id() != id_verify[key]) {
      fprintf(stderr,
              "FAIL: retrieve %d expected entry %d observed %d @ %s:%d\n",
              key, id_verify[key], entry->id(), __FILE__, __LINE__);
      return false;
    }
    if (address != address_verify[key]) {
      fprintf(stderr,
              "FAIL: retrieve %d expected address %d observed %d @ %s:%d\n",
              key, address_verify[key], address, __FILE__, __LINE__);
      return false;
    }
  }

  ASSERT_EQ(CountedObject::count(), 6);

  return true;
}

static bool RunTests() {
  if (!DoAddressMapTest())
    return false;

  ASSERT_EQ(CountedObject::count(), 0);

  return true;
}

}

int main(int argc, char **argv) {
  BPLOG_INIT(&argc, &argv);

  return RunTests() ? 0 : 1;
}
