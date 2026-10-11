// Copyright 2005, Google Inc.
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
//
// The purpose of this file is to generate Google Test output under
// various conditions.  The output will then be verified by
// googletest-output-test.py to ensure that Google Test generates the
// desired messages.  Therefore, most tests in this file are MEANT TO
// FAIL.

#include <stdlib.h>

#include <algorithm>
#include <string>

#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"
#include "src/gtest-internal-inl.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4127                                         )

#ifdef GTEST_IS_THREADSAFE
using testing::ScopedFakeTestPartResultReporter;
using testing::TestPartResultArray;

using testing::internal::Notification;
using testing::internal::ThreadWithParam;
#endif

namespace posix = ::testing::internal::posix;

void TestEq1(int x) { ASSERT_EQ(1, x); }

void TryTestSubroutine() {

  TestEq1(2);

  if (testing::Test::HasFatalFailure()) return;

  FAIL() << "This should never be reached.";
}

TEST(PassingTest, PassingTest1) {}

TEST(PassingTest, PassingTest2) {}

class FailingParamTest : public testing::TestWithParam<int> {};

TEST_P(FailingParamTest, Fails) { EXPECT_EQ(1, GetParam()); }

INSTANTIATE_TEST_SUITE_P(PrintingFailingParams, FailingParamTest,
                         testing::Values(2));

class EmptyBasenameParamInst : public testing::TestWithParam<int> {};

TEST_P(EmptyBasenameParamInst, Passes) { EXPECT_EQ(1, GetParam()); }

INSTANTIATE_TEST_SUITE_P(, EmptyBasenameParamInst, testing::Values(1));

static const char kGoldenString[] = "\"Line\0 1\"\nLine 2";

TEST(NonfatalFailureTest, EscapesStringOperands) {
  std::string actual = "actual \"string\"";
  EXPECT_EQ(kGoldenString, actual);

  const char* golden = kGoldenString;
  EXPECT_EQ(golden, actual);
}

TEST(NonfatalFailureTest, DiffForLongStrings) {
  std::string golden_str(kGoldenString, sizeof(kGoldenString) - 1);
  EXPECT_EQ(golden_str, "Line 2");
}

TEST(FatalFailureTest, FatalFailureInSubroutine) {
  printf("(expecting a failure that x should be 1)\n");

  TryTestSubroutine();
}

TEST(FatalFailureTest, FatalFailureInNestedSubroutine) {
  printf("(expecting a failure that x should be 1)\n");

  TryTestSubroutine();

  if (HasFatalFailure()) return;

  FAIL() << "This should never be reached.";
}

TEST(FatalFailureTest, NonfatalFailureInSubroutine) {
  printf("(expecting a failure on false)\n");
  EXPECT_TRUE(false);
  ASSERT_FALSE(HasFatalFailure());
}

TEST(LoggingTest, InterleavingLoggingAndAssertions) {
  static const int a[4] = {3, 9, 2, 6};

  printf("(expecting 2 failures on (3) >= (a[i]))\n");
  for (int i = 0; i < static_cast<int>(sizeof(a) / sizeof(*a)); i++) {
    printf("i == %d\n", i);
    EXPECT_GE(3, a[i]);
  }
}

void SubWithoutTrace(int n) {
  EXPECT_EQ(1, n);
  ASSERT_EQ(2, n);
}

void SubWithTrace(int n) {
  SCOPED_TRACE(testing::Message() << "n = " << n);

  SubWithoutTrace(n);
}

TEST(SCOPED_TRACETest, AcceptedValues) {
  SCOPED_TRACE("literal string");
  SCOPED_TRACE(std::string("std::string"));
  SCOPED_TRACE(1337);
  const char* null_value = nullptr;
  SCOPED_TRACE(null_value);

  ADD_FAILURE() << "Just checking that all these values work fine.";
}

TEST(SCOPED_TRACETest, ObeysScopes) {
  printf("(expected to fail)\n");

  ADD_FAILURE() << "This failure is expected, and shouldn't have a trace.";

  {
    SCOPED_TRACE("Expected trace");

    ADD_FAILURE() << "This failure is expected, and should have a trace.";
  }

  ADD_FAILURE() << "This failure is expected, and shouldn't have a trace.";
}

TEST(SCOPED_TRACETest, WorksInLoop) {
  printf("(expected to fail)\n");

  for (int i = 1; i <= 2; i++) {
    SCOPED_TRACE(testing::Message() << "i = " << i);

    SubWithoutTrace(i);
  }
}

TEST(SCOPED_TRACETest, WorksInSubroutine) {
  printf("(expected to fail)\n");

  SubWithTrace(1);
  SubWithTrace(2);
}

TEST(SCOPED_TRACETest, CanBeNested) {
  printf("(expected to fail)\n");

  SCOPED_TRACE("");

  SubWithTrace(2);
}

TEST(SCOPED_TRACETest, CanBeRepeated) {
  printf("(expected to fail)\n");

  SCOPED_TRACE("A");
  ADD_FAILURE()
      << "This failure is expected, and should contain trace point A.";

  SCOPED_TRACE("B");
  ADD_FAILURE()
      << "This failure is expected, and should contain trace point A and B.";

  {
    SCOPED_TRACE("C");
    ADD_FAILURE() << "This failure is expected, and should "
                  << "contain trace point A, B, and C.";
  }

  SCOPED_TRACE("D");
  ADD_FAILURE() << "This failure is expected, and should "
                << "contain trace point A, B, and D.";
}

#ifdef GTEST_IS_THREADSAFE

struct CheckPoints {
  Notification n1;
  Notification n2;
  Notification n3;
};

static void ThreadWithScopedTrace(CheckPoints* check_points) {
  {
    SCOPED_TRACE("Trace B");
    ADD_FAILURE() << "Expected failure #1 (in thread B, only trace B alive).";
    check_points->n1.Notify();
    check_points->n2.WaitForNotification();

    ADD_FAILURE()
        << "Expected failure #3 (in thread B, trace A & B both alive).";
  }
  ADD_FAILURE() << "Expected failure #4 (in thread B, only trace A alive).";
  check_points->n3.Notify();
}

TEST(SCOPED_TRACETest, WorksConcurrently) {
  printf("(expecting 6 failures)\n");

  CheckPoints check_points;
  ThreadWithParam<CheckPoints*> thread(&ThreadWithScopedTrace, &check_points,
                                       nullptr);
  check_points.n1.WaitForNotification();

  {
    SCOPED_TRACE("Trace A");
    ADD_FAILURE()
        << "Expected failure #2 (in thread A, trace A & B both alive).";
    check_points.n2.Notify();
    check_points.n3.WaitForNotification();

    ADD_FAILURE() << "Expected failure #5 (in thread A, only trace A alive).";
  }
  ADD_FAILURE() << "Expected failure #6 (in thread A, no trace alive).";
  thread.Join();
}
#endif

TEST(ScopedTraceTest, WithExplicitFileAndLine) {
  testing::ScopedTrace trace("explicit_file.cc", 123, "expected trace message");
  ADD_FAILURE() << "Check that the trace is attached to a particular location.";
}

TEST(DisabledTestsWarningTest,
     DISABLED_AlsoRunDisabledTestsFlagSuppressesWarning) {

}

void AdHocTest() {
  printf("The non-test part of the code is expected to have 2 failures.\n\n");
  EXPECT_TRUE(false);
  EXPECT_EQ(2, 3);
}

int RunAllTests() {
  AdHocTest();
  return RUN_ALL_TESTS();
}

class NonFatalFailureInFixtureConstructorTest : public testing::Test {
 protected:
  NonFatalFailureInFixtureConstructorTest() {
    printf("(expecting 5 failures)\n");
    ADD_FAILURE() << "Expected failure #1, in the test fixture c'tor.";
  }

  ~NonFatalFailureInFixtureConstructorTest() override {
    ADD_FAILURE() << "Expected failure #5, in the test fixture d'tor.";
  }

  void SetUp() override { ADD_FAILURE() << "Expected failure #2, in SetUp()."; }

  void TearDown() override {
    ADD_FAILURE() << "Expected failure #4, in TearDown.";
  }
};

TEST_F(NonFatalFailureInFixtureConstructorTest, FailureInConstructor) {
  ADD_FAILURE() << "Expected failure #3, in the test body.";
}

class FatalFailureInFixtureConstructorTest : public testing::Test {
 protected:
  FatalFailureInFixtureConstructorTest() {
    printf("(expecting 2 failures)\n");
    Init();
  }

  ~FatalFailureInFixtureConstructorTest() override {
    ADD_FAILURE() << "Expected failure #2, in the test fixture d'tor.";
  }

  void SetUp() override {
    ADD_FAILURE() << "UNEXPECTED failure in SetUp().  "
                  << "We should never get here, as the test fixture c'tor "
                  << "had a fatal failure.";
  }

  void TearDown() override {
    ADD_FAILURE() << "UNEXPECTED failure in TearDown().  "
                  << "We should never get here, as the test fixture c'tor "
                  << "had a fatal failure.";
  }

 private:
  void Init() { FAIL() << "Expected failure #1, in the test fixture c'tor."; }
};

TEST_F(FatalFailureInFixtureConstructorTest, FailureInConstructor) {
  ADD_FAILURE() << "UNEXPECTED failure in the test body.  "
                << "We should never get here, as the test fixture c'tor "
                << "had a fatal failure.";
}

class NonFatalFailureInSetUpTest : public testing::Test {
 protected:
  ~NonFatalFailureInSetUpTest() override { Deinit(); }

  void SetUp() override {
    printf("(expecting 4 failures)\n");
    ADD_FAILURE() << "Expected failure #1, in SetUp().";
  }

  void TearDown() override { FAIL() << "Expected failure #3, in TearDown()."; }

 private:
  void Deinit() { FAIL() << "Expected failure #4, in the test fixture d'tor."; }
};

TEST_F(NonFatalFailureInSetUpTest, FailureInSetUp) {
  FAIL() << "Expected failure #2, in the test function.";
}

class FatalFailureInSetUpTest : public testing::Test {
 protected:
  ~FatalFailureInSetUpTest() override { Deinit(); }

  void SetUp() override {
    printf("(expecting 3 failures)\n");
    FAIL() << "Expected failure #1, in SetUp().";
  }

  void TearDown() override { FAIL() << "Expected failure #2, in TearDown()."; }

 private:
  void Deinit() { FAIL() << "Expected failure #3, in the test fixture d'tor."; }
};

TEST_F(FatalFailureInSetUpTest, FailureInSetUp) {
  FAIL() << "UNEXPECTED failure in the test function.  "
         << "We should never get here, as SetUp() failed.";
}

TEST(AddFailureAtTest, MessageContainsSpecifiedFileAndLineNumber) {
  ADD_FAILURE_AT("foo.cc", 42) << "Expected nonfatal failure in foo.cc";
}

TEST(GtestFailAtTest, MessageContainsSpecifiedFileAndLineNumber) {
  GTEST_FAIL_AT("foo.cc", 42) << "Expected fatal failure in foo.cc";
}

namespace foo {

class MixedUpTestSuiteTest : public testing::Test {};

TEST_F(MixedUpTestSuiteTest, FirstTestFromNamespaceFoo) {}
TEST_F(MixedUpTestSuiteTest, SecondTestFromNamespaceFoo) {}

class MixedUpTestSuiteWithSameTestNameTest : public testing::Test {};

TEST_F(MixedUpTestSuiteWithSameTestNameTest,
       TheSecondTestWithThisNameShouldFail) {}

}

namespace bar {

class MixedUpTestSuiteTest : public testing::Test {};

TEST_F(MixedUpTestSuiteTest, ThisShouldFail) {}
TEST_F(MixedUpTestSuiteTest, ThisShouldFailToo) {}

class MixedUpTestSuiteWithSameTestNameTest : public testing::Test {};

TEST_F(MixedUpTestSuiteWithSameTestNameTest,
       TheSecondTestWithThisNameShouldFail) {}

}

class TEST_F_before_TEST_in_same_test_case : public testing::Test {};

TEST_F(TEST_F_before_TEST_in_same_test_case, DefinedUsingTEST_F) {}

TEST(TEST_F_before_TEST_in_same_test_case, DefinedUsingTESTAndShouldFail) {}

class TEST_before_TEST_F_in_same_test_case : public testing::Test {};

TEST(TEST_before_TEST_F_in_same_test_case, DefinedUsingTEST) {}

TEST_F(TEST_before_TEST_F_in_same_test_case, DefinedUsingTEST_FAndShouldFail) {}

int global_integer = 0;

TEST(ExpectNonfatalFailureTest, CanReferenceGlobalVariables) {
  global_integer = 0;
  EXPECT_NONFATAL_FAILURE(
      { EXPECT_EQ(1, global_integer) << "Expected non-fatal failure."; },
      "Expected non-fatal failure.");
}

TEST(ExpectNonfatalFailureTest, CanReferenceLocalVariables) {
  int m = 0;
  static int n;
  n = 1;
  EXPECT_NONFATAL_FAILURE({ EXPECT_EQ(m, n) << "Expected non-fatal failure."; },
                          "Expected non-fatal failure.");
}

TEST(ExpectNonfatalFailureTest, SucceedsWhenThereIsOneNonfatalFailure) {
  EXPECT_NONFATAL_FAILURE({ ADD_FAILURE() << "Expected non-fatal failure."; },
                          "Expected non-fatal failure.");
}

TEST(ExpectNonfatalFailureTest, FailsWhenThereIsNoNonfatalFailure) {
  printf("(expecting a failure)\n");
  EXPECT_NONFATAL_FAILURE({}, "");
}

TEST(ExpectNonfatalFailureTest, FailsWhenThereAreTwoNonfatalFailures) {
  printf("(expecting a failure)\n");
  EXPECT_NONFATAL_FAILURE(
      {
        ADD_FAILURE() << "Expected non-fatal failure 1.";
        ADD_FAILURE() << "Expected non-fatal failure 2.";
      },
      "");
}

TEST(ExpectNonfatalFailureTest, FailsWhenThereIsOneFatalFailure) {
  printf("(expecting a failure)\n");
  EXPECT_NONFATAL_FAILURE({ FAIL() << "Expected fatal failure."; }, "");
}

TEST(ExpectNonfatalFailureTest, FailsWhenStatementReturns) {
  printf("(expecting a failure)\n");
  EXPECT_NONFATAL_FAILURE({ return; }, "");
}

#if GTEST_HAS_EXCEPTIONS

TEST(ExpectNonfatalFailureTest, FailsWhenStatementThrows) {
  printf("(expecting a failure)\n");
  try {
    EXPECT_NONFATAL_FAILURE({ throw 0; }, "");
  } catch (int) {
  }
}

#endif

TEST(ExpectFatalFailureTest, CanReferenceGlobalVariables) {
  global_integer = 0;
  EXPECT_FATAL_FAILURE(
      { ASSERT_EQ(1, global_integer) << "Expected fatal failure."; },
      "Expected fatal failure.");
}

TEST(ExpectFatalFailureTest, CanReferenceLocalStaticVariables) {
  static int n;
  n = 1;
  EXPECT_FATAL_FAILURE({ ASSERT_EQ(0, n) << "Expected fatal failure."; },
                       "Expected fatal failure.");
}

TEST(ExpectFatalFailureTest, SucceedsWhenThereIsOneFatalFailure) {
  EXPECT_FATAL_FAILURE({ FAIL() << "Expected fatal failure."; },
                       "Expected fatal failure.");
}

TEST(ExpectFatalFailureTest, FailsWhenThereIsNoFatalFailure) {
  printf("(expecting a failure)\n");
  EXPECT_FATAL_FAILURE({}, "");
}

void FatalFailure() { FAIL() << "Expected fatal failure."; }

TEST(ExpectFatalFailureTest, FailsWhenThereAreTwoFatalFailures) {
  printf("(expecting a failure)\n");
  EXPECT_FATAL_FAILURE(
      {
        FatalFailure();
        FatalFailure();
      },
      "");
}

TEST(ExpectFatalFailureTest, FailsWhenThereIsOneNonfatalFailure) {
  printf("(expecting a failure)\n");
  EXPECT_FATAL_FAILURE({ ADD_FAILURE() << "Expected non-fatal failure."; }, "");
}

TEST(ExpectFatalFailureTest, FailsWhenStatementReturns) {
  printf("(expecting a failure)\n");
  EXPECT_FATAL_FAILURE({ return; }, "");
}

#if GTEST_HAS_EXCEPTIONS

TEST(ExpectFatalFailureTest, FailsWhenStatementThrows) {
  printf("(expecting a failure)\n");
  try {
    EXPECT_FATAL_FAILURE({ throw 0; }, "");
  } catch (int) {
  }
}

#endif

std::string ParamNameFunc(const testing::TestParamInfo<std::string>& info) {
  return info.param;
}

class ParamTest : public testing::TestWithParam<std::string> {};

TEST_P(ParamTest, Success) { EXPECT_EQ("a", GetParam()); }

TEST_P(ParamTest, Failure) { EXPECT_EQ("b", GetParam()) << "Expected failure"; }

INSTANTIATE_TEST_SUITE_P(PrintingStrings, ParamTest,
                         testing::Values(std::string("a")), ParamNameFunc);

using NoTests = ParamTest;
INSTANTIATE_TEST_SUITE_P(ThisIsOdd, NoTests, ::testing::Values("Hello"));

class DetectNotInstantiatedTest : public testing::TestWithParam<int> {};
TEST_P(DetectNotInstantiatedTest, Used) {}

template <typename T>
class TypedTest : public testing::Test {};

TYPED_TEST_SUITE(TypedTest, testing::Types<int>);

TYPED_TEST(TypedTest, Success) { EXPECT_EQ(0, TypeParam()); }

TYPED_TEST(TypedTest, Failure) {
  EXPECT_EQ(1, TypeParam()) << "Expected failure";
}

typedef testing::Types<char, int> TypesForTestWithNames;

template <typename T>
class TypedTestWithNames : public testing::Test {};

class TypedTestNames {
 public:
  template <typename T>
  static std::string GetName(int i) {
    if (std::is_same<T, char>::value)
      return std::string("char") + ::testing::PrintToString(i);
    if (std::is_same<T, int>::value)
      return std::string("int") + ::testing::PrintToString(i);
  }
};

TYPED_TEST_SUITE(TypedTestWithNames, TypesForTestWithNames, TypedTestNames);

TYPED_TEST(TypedTestWithNames, Success) {}

TYPED_TEST(TypedTestWithNames, Failure) { FAIL(); }

template <typename T>
class TypedTestP : public testing::Test {};

TYPED_TEST_SUITE_P(TypedTestP);

TYPED_TEST_P(TypedTestP, Success) { EXPECT_EQ(0U, TypeParam()); }

TYPED_TEST_P(TypedTestP, Failure) {
  EXPECT_EQ(1U, TypeParam()) << "Expected failure";
}

REGISTER_TYPED_TEST_SUITE_P(TypedTestP, Success, Failure);

typedef testing::Types<unsigned char, unsigned int> UnsignedTypes;
INSTANTIATE_TYPED_TEST_SUITE_P(Unsigned, TypedTestP, UnsignedTypes);

class TypedTestPNames {
 public:
  template <typename T>
  static std::string GetName(int i) {
    if (std::is_same<T, unsigned char>::value) {
      return std::string("unsignedChar") + ::testing::PrintToString(i);
    }
    if (std::is_same<T, unsigned int>::value) {
      return std::string("unsignedInt") + ::testing::PrintToString(i);
    }
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(UnsignedCustomName, TypedTestP, UnsignedTypes,
                               TypedTestPNames);

template <typename T>
class DetectNotInstantiatedTypesTest : public testing::Test {};
TYPED_TEST_SUITE_P(DetectNotInstantiatedTypesTest);
TYPED_TEST_P(DetectNotInstantiatedTypesTest, Used) {
  TypeParam instantiate;
  (void)instantiate;
}
REGISTER_TYPED_TEST_SUITE_P(DetectNotInstantiatedTypesTest, Used);

#ifdef GTEST_HAS_DEATH_TEST

TEST(ADeathTest, ShouldRunFirst) {}

template <typename T>
class ATypedDeathTest : public testing::Test {};

typedef testing::Types<int, double> NumericTypes;
TYPED_TEST_SUITE(ATypedDeathTest, NumericTypes);

TYPED_TEST(ATypedDeathTest, ShouldRunFirst) {}

template <typename T>
class ATypeParamDeathTest : public testing::Test {};

TYPED_TEST_SUITE_P(ATypeParamDeathTest);

TYPED_TEST_P(ATypeParamDeathTest, ShouldRunFirst) {}

REGISTER_TYPED_TEST_SUITE_P(ATypeParamDeathTest, ShouldRunFirst);

INSTANTIATE_TYPED_TEST_SUITE_P(My, ATypeParamDeathTest, NumericTypes);

#endif

class ExpectFailureTest : public testing::Test {
 public:
  enum FailureMode { FATAL_FAILURE, NONFATAL_FAILURE };
  static void AddFailure(FailureMode failure) {
    if (failure == FATAL_FAILURE) {
      FAIL() << "Expected fatal failure.";
    } else {
      ADD_FAILURE() << "Expected non-fatal failure.";
    }
  }
};

TEST_F(ExpectFailureTest, ExpectFatalFailure) {

  printf("(expecting 1 failure)\n");
  EXPECT_FATAL_FAILURE(SUCCEED(), "Expected fatal failure.");

  printf("(expecting 1 failure)\n");
  EXPECT_FATAL_FAILURE(AddFailure(NONFATAL_FAILURE),
                       "Expected non-fatal "
                       "failure.");

  printf("(expecting 1 failure)\n");
  EXPECT_FATAL_FAILURE(AddFailure(FATAL_FAILURE),
                       "Some other fatal failure "
                       "expected.");
}

TEST_F(ExpectFailureTest, ExpectNonFatalFailure) {

  printf("(expecting 1 failure)\n");
  EXPECT_NONFATAL_FAILURE(SUCCEED(), "Expected non-fatal failure.");

  printf("(expecting 1 failure)\n");
  EXPECT_NONFATAL_FAILURE(AddFailure(FATAL_FAILURE), "Expected fatal failure.");

  printf("(expecting 1 failure)\n");
  EXPECT_NONFATAL_FAILURE(AddFailure(NONFATAL_FAILURE),
                          "Some other non-fatal "
                          "failure.");
}

#ifdef GTEST_IS_THREADSAFE

class ExpectFailureWithThreadsTest : public ExpectFailureTest {
 protected:
  static void AddFailureInOtherThread(FailureMode failure) {
    ThreadWithParam<FailureMode> thread(&AddFailure, failure, nullptr);
    thread.Join();
  }
};

TEST_F(ExpectFailureWithThreadsTest, ExpectFatalFailure) {

  printf("(expecting 2 failures)\n");
  EXPECT_FATAL_FAILURE(AddFailureInOtherThread(FATAL_FAILURE),
                       "Expected fatal failure.");
}

TEST_F(ExpectFailureWithThreadsTest, ExpectNonFatalFailure) {

  printf("(expecting 2 failures)\n");
  EXPECT_NONFATAL_FAILURE(AddFailureInOtherThread(NONFATAL_FAILURE),
                          "Expected non-fatal failure.");
}

typedef ExpectFailureWithThreadsTest ScopedFakeTestPartResultReporterTest;

TEST_F(ScopedFakeTestPartResultReporterTest, InterceptOnlyCurrentThread) {
  printf("(expecting 2 failures)\n");
  TestPartResultArray results;
  {
    ScopedFakeTestPartResultReporter reporter(
        ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD,
        &results);
    AddFailureInOtherThread(FATAL_FAILURE);
    AddFailureInOtherThread(NONFATAL_FAILURE);
  }

  EXPECT_EQ(0, results.size()) << "This shouldn't fail.";
}

#endif

TEST_F(ExpectFailureTest, ExpectFatalFailureOnAllThreads) {

  printf("(expecting 1 failure)\n");
  EXPECT_FATAL_FAILURE_ON_ALL_THREADS(SUCCEED(), "Expected fatal failure.");

  printf("(expecting 1 failure)\n");
  EXPECT_FATAL_FAILURE_ON_ALL_THREADS(AddFailure(NONFATAL_FAILURE),
                                      "Expected non-fatal failure.");

  printf("(expecting 1 failure)\n");
  EXPECT_FATAL_FAILURE_ON_ALL_THREADS(AddFailure(FATAL_FAILURE),
                                      "Some other fatal failure expected.");
}

TEST_F(ExpectFailureTest, ExpectNonFatalFailureOnAllThreads) {

  printf("(expecting 1 failure)\n");
  EXPECT_NONFATAL_FAILURE_ON_ALL_THREADS(SUCCEED(),
                                         "Expected non-fatal "
                                         "failure.");

  printf("(expecting 1 failure)\n");
  EXPECT_NONFATAL_FAILURE_ON_ALL_THREADS(AddFailure(FATAL_FAILURE),
                                         "Expected fatal failure.");

  printf("(expecting 1 failure)\n");
  EXPECT_NONFATAL_FAILURE_ON_ALL_THREADS(AddFailure(NONFATAL_FAILURE),
                                         "Some other non-fatal failure.");
}

class DynamicFixture : public testing::Test {
 protected:
  DynamicFixture() { printf("DynamicFixture()\n"); }
  ~DynamicFixture() override { printf("~DynamicFixture()\n"); }
  void SetUp() override { printf("DynamicFixture::SetUp\n"); }
  void TearDown() override { printf("DynamicFixture::TearDown\n"); }

  static void SetUpTestSuite() { printf("DynamicFixture::SetUpTestSuite\n"); }
  static void TearDownTestSuite() {
    printf("DynamicFixture::TearDownTestSuite\n");
  }
};

template <bool Pass>
class DynamicTest : public DynamicFixture {
 public:
  void TestBody() override { EXPECT_TRUE(Pass); }
};

auto dynamic_test = (

    testing::RegisterTest(
        "DynamicFixture", "DynamicTestPass", nullptr, nullptr, __FILE__,
        __LINE__, []() -> DynamicFixture* { return new DynamicTest<true>; }),
    testing::RegisterTest(
        "DynamicFixture", "DynamicTestFail", nullptr, nullptr, __FILE__,
        __LINE__, []() -> DynamicFixture* { return new DynamicTest<false>; }),

    testing::RegisterTest(
        "DynamicFixtureAnotherName", "DynamicTestPass", nullptr, nullptr,
        __FILE__, __LINE__,
        []() -> DynamicFixture* { return new DynamicTest<true>; }),

    testing::RegisterTest(
        "BadDynamicFixture1", "FixtureBase", nullptr, nullptr, __FILE__,
        __LINE__, []() -> DynamicFixture* { return new DynamicTest<true>; }),
    testing::RegisterTest(
        "BadDynamicFixture1", "TestBase", nullptr, nullptr, __FILE__, __LINE__,
        []() -> testing::Test* { return new DynamicTest<true>; }),

    testing::RegisterTest(
        "BadDynamicFixture2", "FixtureBase", nullptr, nullptr, __FILE__,
        __LINE__, []() -> DynamicFixture* { return new DynamicTest<true>; }),
    testing::RegisterTest("BadDynamicFixture2", "Derived", nullptr, nullptr,
                          __FILE__, __LINE__,
                          []() { return new DynamicTest<true>; }));

class FooEnvironment : public testing::Environment {
 public:
  void SetUp() override { printf("%s", "FooEnvironment::SetUp() called.\n"); }

  void TearDown() override {
    printf("%s", "FooEnvironment::TearDown() called.\n");
    FAIL() << "Expected fatal failure.";
  }
};

class BarEnvironment : public testing::Environment {
 public:
  void SetUp() override { printf("%s", "BarEnvironment::SetUp() called.\n"); }

  void TearDown() override {
    printf("%s", "BarEnvironment::TearDown() called.\n");
    ADD_FAILURE() << "Expected non-fatal failure.";
  }
};

class TestSuiteThatFailsToSetUp : public testing::Test {
 public:
  static void SetUpTestSuite() { EXPECT_TRUE(false); }
};
TEST_F(TestSuiteThatFailsToSetUp, ShouldNotRun) { std::abort(); }

class TestSuiteThatSkipsInSetUp : public testing::Test {
 public:
  static void SetUpTestSuite() { GTEST_SKIP() << "Skip entire test suite"; }
};
TEST_F(TestSuiteThatSkipsInSetUp, ShouldNotRun) { std::abort(); }

int main(int argc, char** argv) {
  GTEST_FLAG_SET(print_time, false);

  testing::InitGoogleTest(&argc, argv);
  bool internal_skip_environment_and_ad_hoc_tests =
      std::count(argv, argv + argc,
                 std::string("internal_skip_environment_and_ad_hoc_tests")) > 0;

#ifdef GTEST_HAS_DEATH_TEST
  if (!GTEST_FLAG_GET(internal_run_death_test).empty()) {

#if defined(GTEST_OS_WINDOWS)
    posix::FReopen("nul:", "w", stdout);
#else
    posix::FReopen("/dev/null", "w", stdout);
#endif
    return RUN_ALL_TESTS();
  }
#endif

  if (internal_skip_environment_and_ad_hoc_tests) return RUN_ALL_TESTS();

  testing::AddGlobalTestEnvironment(new FooEnvironment);
  testing::AddGlobalTestEnvironment(new BarEnvironment);
  GTEST_DISABLE_MSC_WARNINGS_POP_()
  return RunAllTests();
}
