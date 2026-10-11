// Copyright 2009 Google Inc. All Rights Reserved.
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

// This sample shows how to use Google Test listener API to implement
// a primitive leak checker.

#include <stdio.h>
#include <stdlib.h>

#include "gtest/gtest.h"
using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::UnitTest;

namespace {

class Water {
 public:

  void* operator new(size_t allocation_size) {
    allocated_++;
    return malloc(allocation_size);
  }

  void operator delete(void* block, size_t                      ) {
    allocated_--;
    free(block);
  }

  static int allocated() { return allocated_; }

 private:
  static int allocated_;
};

int Water::allocated_ = 0;

class LeakChecker : public EmptyTestEventListener {
 private:

  void OnTestStart(const TestInfo&                ) override {
    initially_allocated_ = Water::allocated();
  }

  void OnTestEnd(const TestInfo&                ) override {
    int difference = Water::allocated() - initially_allocated_;

    EXPECT_LE(difference, 0) << "Leaked " << difference << " unit(s) of Water!";
  }

  int initially_allocated_;
};

TEST(ListenersTest, DoesNotLeak) {
  Water* water = new Water;
  delete water;
}

TEST(ListenersTest, LeaksWater) {
  Water* water = new Water;
  EXPECT_TRUE(water != nullptr);
}
}

int main(int argc, char** argv) {
  InitGoogleTest(&argc, argv);

  bool check_for_leaks = false;
  if (argc > 1 && strcmp(argv[1], "--check_for_leaks") == 0)
    check_for_leaks = true;
  else
    printf("%s\n",
           "Run this program with --check_for_leaks to enable "
           "custom leak checking in the tests.");

  if (check_for_leaks) {
    TestEventListeners& listeners = UnitTest::GetInstance()->listeners();

    listeners.Append(new LeakChecker);
  }
  return RUN_ALL_TESTS();
}
