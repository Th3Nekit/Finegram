// Copyright 2008, Google Inc.
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

// Tests the --gtest_repeat=number flag.

#include <stdlib.h>

#include <iostream>

#include "gtest/gtest.h"
#include "src/gtest-internal-inl.h"

namespace {

#define GTEST_CHECK_INT_EQ_(expected, actual)                      \
  do {                                                             \
    const int expected_val = (expected);                           \
    const int actual_val = (actual);                               \
    if (::testing::internal::IsTrue(expected_val != actual_val)) { \
      ::std::cout << "Value of: " #actual "\n"                     \
                  << "  Actual: " << actual_val << "\n"            \
                  << "Expected: " #expected "\n"                   \
                  << "Which is: " << expected_val << "\n";         \
      ::testing::internal::posix::Abort();                         \
    }                                                              \
  } while (::testing::internal::AlwaysFalse())

int g_environment_set_up_count = 0;
int g_environment_tear_down_count = 0;

class MyEnvironment : public testing::Environment {
 public:
  void SetUp() override { g_environment_set_up_count++; }
  void TearDown() override { g_environment_tear_down_count++; }
};

int g_should_fail_count = 0;

TEST(FooTest, ShouldFail) {
  g_should_fail_count++;
  EXPECT_EQ(0, 1) << "Expected failure.";
}

int g_should_pass_count = 0;

TEST(FooTest, ShouldPass) { g_should_pass_count++; }

int g_death_test_count = 0;

TEST(BarDeathTest, ThreadSafeAndFast) {
  g_death_test_count++;

  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH_IF_SUPPORTED(::testing::internal::posix::Abort(), "");

  GTEST_FLAG_SET(death_test_style, "fast");
  EXPECT_DEATH_IF_SUPPORTED(::testing::internal::posix::Abort(), "");
}

int g_param_test_count = 0;

const int kNumberOfParamTests = 10;

class MyParamTest : public testing::TestWithParam<int> {};

TEST_P(MyParamTest, ShouldPass) {
  GTEST_CHECK_INT_EQ_(g_param_test_count % kNumberOfParamTests, GetParam());
  g_param_test_count++;
}
INSTANTIATE_TEST_SUITE_P(MyParamSequence, MyParamTest,
                         testing::Range(0, kNumberOfParamTests));

void ResetCounts() {
  g_environment_set_up_count = 0;
  g_environment_tear_down_count = 0;
  g_should_fail_count = 0;
  g_should_pass_count = 0;
  g_death_test_count = 0;
  g_param_test_count = 0;
  testing::AddGlobalTestEnvironment(new MyEnvironment);
}

void CheckCounts(int expected) {
  GTEST_CHECK_INT_EQ_(expected, g_environment_set_up_count);
  GTEST_CHECK_INT_EQ_(expected, g_environment_tear_down_count);
  GTEST_CHECK_INT_EQ_(expected, g_should_fail_count);
  GTEST_CHECK_INT_EQ_(expected, g_should_pass_count);
  GTEST_CHECK_INT_EQ_(expected, g_death_test_count);
  GTEST_CHECK_INT_EQ_(expected * kNumberOfParamTests, g_param_test_count);
}

void TestRepeatUnspecified() {
  ResetCounts();
  GTEST_CHECK_INT_EQ_(1, RUN_ALL_TESTS());
  CheckCounts(1);
}

void TestRepeat(int repeat) {
  GTEST_FLAG_SET(repeat, repeat);
  GTEST_FLAG_SET(recreate_environments_when_repeating, true);

  ResetCounts();
  GTEST_CHECK_INT_EQ_(repeat > 0 ? 1 : 0, RUN_ALL_TESTS());
  CheckCounts(repeat);
}

void TestRepeatWithEmptyFilter(int repeat) {
  GTEST_FLAG_SET(repeat, repeat);
  GTEST_FLAG_SET(recreate_environments_when_repeating, true);
  GTEST_FLAG_SET(filter, "None");

  ResetCounts();
  GTEST_CHECK_INT_EQ_(0, RUN_ALL_TESTS());
  CheckCounts(0);
}

void TestRepeatWithFilterForSuccessfulTests(int repeat) {
  GTEST_FLAG_SET(repeat, repeat);
  GTEST_FLAG_SET(recreate_environments_when_repeating, true);
  GTEST_FLAG_SET(filter, "*-*ShouldFail");

  ResetCounts();
  GTEST_CHECK_INT_EQ_(0, RUN_ALL_TESTS());
  GTEST_CHECK_INT_EQ_(repeat, g_environment_set_up_count);
  GTEST_CHECK_INT_EQ_(repeat, g_environment_tear_down_count);
  GTEST_CHECK_INT_EQ_(0, g_should_fail_count);
  GTEST_CHECK_INT_EQ_(repeat, g_should_pass_count);
  GTEST_CHECK_INT_EQ_(repeat, g_death_test_count);
  GTEST_CHECK_INT_EQ_(repeat * kNumberOfParamTests, g_param_test_count);
}

void TestRepeatWithFilterForFailedTests(int repeat) {
  GTEST_FLAG_SET(repeat, repeat);
  GTEST_FLAG_SET(recreate_environments_when_repeating, true);
  GTEST_FLAG_SET(filter, "*ShouldFail");

  ResetCounts();
  GTEST_CHECK_INT_EQ_(1, RUN_ALL_TESTS());
  GTEST_CHECK_INT_EQ_(repeat, g_environment_set_up_count);
  GTEST_CHECK_INT_EQ_(repeat, g_environment_tear_down_count);
  GTEST_CHECK_INT_EQ_(repeat, g_should_fail_count);
  GTEST_CHECK_INT_EQ_(0, g_should_pass_count);
  GTEST_CHECK_INT_EQ_(0, g_death_test_count);
  GTEST_CHECK_INT_EQ_(0, g_param_test_count);
}

}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  TestRepeatUnspecified();
  TestRepeat(0);
  TestRepeat(1);
  TestRepeat(5);

  TestRepeatWithEmptyFilter(2);
  TestRepeatWithEmptyFilter(3);

  TestRepeatWithFilterForSuccessfulTests(3);

  TestRepeatWithFilterForFailedTests(4);

  printf("PASS\n");
  return 0;
}
