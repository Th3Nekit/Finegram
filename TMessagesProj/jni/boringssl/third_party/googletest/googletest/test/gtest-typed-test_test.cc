// Copyright 2008 Google Inc.
// All Rights Reserved.
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

#include "test/gtest-typed-test_test.h"

#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4127                                         )

using testing::Test;

template <typename T>
class CommonTest : public Test {

 public:
  static void SetUpTestSuite() { shared_ = new T(5); }

  static void TearDownTestSuite() {
    delete shared_;
    shared_ = nullptr;
  }

 protected:

  typedef std::vector<T> Vector;
  typedef std::set<int> IntSet;

  CommonTest() : value_(1) {}

  ~CommonTest() override { EXPECT_EQ(3, value_); }

  void SetUp() override {
    EXPECT_EQ(1, value_);
    value_++;
  }

  void TearDown() override {
    EXPECT_EQ(2, value_);
    value_++;
  }

  T value_;
  static T* shared_;
};

template <typename T>
T* CommonTest<T>::shared_ = nullptr;

using testing::Types;

typedef Types<char, int> TwoTypes;
TYPED_TEST_SUITE(CommonTest, TwoTypes);

TYPED_TEST(CommonTest, ValuesAreCorrect) {

  EXPECT_EQ(5, *TestFixture::shared_);

  typename TestFixture::Vector empty;
  EXPECT_EQ(0U, empty.size());

  typename TestFixture::IntSet empty2;
  EXPECT_EQ(0U, empty2.size());

  EXPECT_EQ(2, this->value_);
}

TYPED_TEST(CommonTest, ValuesAreStillCorrect) {

  ASSERT_TRUE(this->shared_ != nullptr);
  EXPECT_EQ(5, *this->shared_);

  EXPECT_EQ(static_cast<TypeParam>(2), this->value_);
}

template <typename T>
class TypedTest1 : public Test {};

TYPED_TEST_SUITE(TypedTest1, int);
TYPED_TEST(TypedTest1, A) {}

template <typename T>
class TypedTest2 : public Test {};

TYPED_TEST_SUITE(TypedTest2, Types<int>);

TYPED_TEST(TypedTest2, A) {}

namespace library1 {

template <typename T>
class NumericTest : public Test {};

typedef Types<int, long> NumericTypes;
TYPED_TEST_SUITE(NumericTest, NumericTypes);

TYPED_TEST(NumericTest, DefaultIsZero) { EXPECT_EQ(0, TypeParam()); }

}

template <typename T>
class TypedTestWithNames : public Test {};

class TypedTestNames {
 public:
  template <typename T>
  static std::string GetName(int i) {
    if (std::is_same<T, char>::value) {
      return std::string("char") + ::testing::PrintToString(i);
    }
    if (std::is_same<T, int>::value) {
      return std::string("int") + ::testing::PrintToString(i);
    }
  }
};

TYPED_TEST_SUITE(TypedTestWithNames, TwoTypes, TypedTestNames);

TYPED_TEST(TypedTestWithNames, TestSuiteName) {
  if (std::is_same<TypeParam, char>::value) {
    EXPECT_STREQ(::testing::UnitTest::GetInstance()
                     ->current_test_info()
                     ->test_suite_name(),
                 "TypedTestWithNames/char0");
  }
  if (std::is_same<TypeParam, int>::value) {
    EXPECT_STREQ(::testing::UnitTest::GetInstance()
                     ->current_test_info()
                     ->test_suite_name(),
                 "TypedTestWithNames/int1");
  }
}

using testing::Types;
using testing::internal::TypedTestSuitePState;

class TypedTestSuitePStateTest : public Test {
 protected:
  void SetUp() override {
    state_.AddTestName("foo.cc", 0, "FooTest", "A");
    state_.AddTestName("foo.cc", 0, "FooTest", "B");
    state_.AddTestName("foo.cc", 0, "FooTest", "C");
  }

  TypedTestSuitePState state_;
};

TEST_F(TypedTestSuitePStateTest, SucceedsForMatchingList) {
  const char* tests = "A, B, C";
  EXPECT_EQ(tests,
            state_.VerifyRegisteredTestNames("Suite", "foo.cc", 1, tests));
}

TEST_F(TypedTestSuitePStateTest, IgnoresOrderAndSpaces) {
  const char* tests = "A,C,   B";
  EXPECT_EQ(tests,
            state_.VerifyRegisteredTestNames("Suite", "foo.cc", 1, tests));
}

using TypedTestSuitePStateDeathTest = TypedTestSuitePStateTest;

TEST_F(TypedTestSuitePStateDeathTest, DetectsDuplicates) {
  EXPECT_DEATH_IF_SUPPORTED(
      state_.VerifyRegisteredTestNames("Suite", "foo.cc", 1, "A, B, A, C"),
      "foo\\.cc.1.?: Test A is listed more than once\\.");
}

TEST_F(TypedTestSuitePStateDeathTest, DetectsExtraTest) {
  EXPECT_DEATH_IF_SUPPORTED(
      state_.VerifyRegisteredTestNames("Suite", "foo.cc", 1, "A, B, C, D"),
      "foo\\.cc.1.?: No test named D can be found in this test suite\\.");
}

TEST_F(TypedTestSuitePStateDeathTest, DetectsMissedTest) {
  EXPECT_DEATH_IF_SUPPORTED(
      state_.VerifyRegisteredTestNames("Suite", "foo.cc", 1, "A, C"),
      "foo\\.cc.1.?: You forgot to list test B\\.");
}

TEST_F(TypedTestSuitePStateDeathTest, DetectsTestAfterRegistration) {
  state_.VerifyRegisteredTestNames("Suite", "foo.cc", 1, "A, B, C");
  EXPECT_DEATH_IF_SUPPORTED(
      state_.AddTestName("foo.cc", 2, "FooTest", "D"),
      "foo\\.cc.2.?: Test D must be defined before REGISTER_TYPED_TEST_SUITE_P"
      "\\(FooTest, \\.\\.\\.\\)\\.");
}

template <typename T>
class DerivedTest : public CommonTest<T> {};

TYPED_TEST_SUITE_P(DerivedTest);

TYPED_TEST_P(DerivedTest, ValuesAreCorrect) {

  EXPECT_EQ(5, *TestFixture::shared_);

  EXPECT_EQ(2, this->value_);
}

TYPED_TEST_P(DerivedTest, ValuesAreStillCorrect) {

  ASSERT_TRUE(this->shared_ != nullptr);
  EXPECT_EQ(5, *this->shared_);
  EXPECT_EQ(2, this->value_);
}

REGISTER_TYPED_TEST_SUITE_P(DerivedTest, ValuesAreCorrect,
                            ValuesAreStillCorrect);

typedef Types<short, long> MyTwoTypes;
INSTANTIATE_TYPED_TEST_SUITE_P(My, DerivedTest, MyTwoTypes);

template <typename T>
class TypeParametrizedTestWithNames : public Test {};

TYPED_TEST_SUITE_P(TypeParametrizedTestWithNames);

TYPED_TEST_P(TypeParametrizedTestWithNames, TestSuiteName) {
  if (std::is_same<TypeParam, char>::value) {
    EXPECT_STREQ(::testing::UnitTest::GetInstance()
                     ->current_test_info()
                     ->test_suite_name(),
                 "CustomName/TypeParametrizedTestWithNames/parChar0");
  }
  if (std::is_same<TypeParam, int>::value) {
    EXPECT_STREQ(::testing::UnitTest::GetInstance()
                     ->current_test_info()
                     ->test_suite_name(),
                 "CustomName/TypeParametrizedTestWithNames/parInt1");
  }
}

REGISTER_TYPED_TEST_SUITE_P(TypeParametrizedTestWithNames, TestSuiteName);

class TypeParametrizedTestNames {
 public:
  template <typename T>
  static std::string GetName(int i) {
    if (std::is_same<T, char>::value) {
      return std::string("parChar") + ::testing::PrintToString(i);
    }
    if (std::is_same<T, int>::value) {
      return std::string("parInt") + ::testing::PrintToString(i);
    }
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(CustomName, TypeParametrizedTestWithNames,
                               TwoTypes, TypeParametrizedTestNames);

template <typename T>
class TypedTestP1 : public Test {};

TYPED_TEST_SUITE_P(TypedTestP1);

using IntAfterTypedTestSuiteP = int;

TYPED_TEST_P(TypedTestP1, A) {}
TYPED_TEST_P(TypedTestP1, B) {}

using IntBeforeRegisterTypedTestSuiteP = int;

REGISTER_TYPED_TEST_SUITE_P(TypedTestP1, A, B);

template <typename T>
class TypedTestP2 : public Test {};

TYPED_TEST_SUITE_P(TypedTestP2);

TYPED_TEST_P(TypedTestP2, A) {}

REGISTER_TYPED_TEST_SUITE_P(TypedTestP2, A);

IntAfterTypedTestSuiteP after = 0;
IntBeforeRegisterTypedTestSuiteP before = 0;

INSTANTIATE_TYPED_TEST_SUITE_P(Int, TypedTestP1, int);
INSTANTIATE_TYPED_TEST_SUITE_P(Int, TypedTestP2, Types<int>);

INSTANTIATE_TYPED_TEST_SUITE_P(Double, TypedTestP2, Types<double>);

typedef Types<std::vector<double>, std::set<char> > MyContainers;
INSTANTIATE_TYPED_TEST_SUITE_P(My, ContainerTest, MyContainers);

namespace library2 {

template <typename T>
class NumericTest : public Test {};

TYPED_TEST_SUITE_P(NumericTest);

TYPED_TEST_P(NumericTest, DefaultIsZero) { EXPECT_EQ(0, TypeParam()); }

TYPED_TEST_P(NumericTest, ZeroIsLessThanOne) {
  EXPECT_LT(TypeParam(0), TypeParam(1));
}

REGISTER_TYPED_TEST_SUITE_P(NumericTest, DefaultIsZero, ZeroIsLessThanOne);
typedef Types<int, double> NumericTypes;
INSTANTIATE_TYPED_TEST_SUITE_P(My, NumericTest, NumericTypes);

static const char* GetTestName() {
  return testing::UnitTest::GetInstance()->current_test_info()->name();
}

template <typename T>
class TrimmedTest : public Test {};
TYPED_TEST_SUITE_P(TrimmedTest);
TYPED_TEST_P(TrimmedTest, Test1) { EXPECT_STREQ("Test1", GetTestName()); }
TYPED_TEST_P(TrimmedTest, Test2) { EXPECT_STREQ("Test2", GetTestName()); }
TYPED_TEST_P(TrimmedTest, Test3) { EXPECT_STREQ("Test3", GetTestName()); }
TYPED_TEST_P(TrimmedTest, Test4) { EXPECT_STREQ("Test4", GetTestName()); }
TYPED_TEST_P(TrimmedTest, Test5) { EXPECT_STREQ("Test5", GetTestName()); }
REGISTER_TYPED_TEST_SUITE_P(TrimmedTest, Test1, Test2, Test3, Test4,
                            Test5);
template <typename T1, typename T2>
struct MyPair {};

typedef Types<int, double, MyPair<int, int> > TrimTypes;
INSTANTIATE_TYPED_TEST_SUITE_P(My, TrimmedTest, TrimTypes);

}

GTEST_DISABLE_MSC_WARNINGS_POP_()
