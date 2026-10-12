// Copyright 2007, Google Inc.
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

// Google Mock - a framework for writing C++ mock classes.
//
// This file tests some commonly used argument matchers.

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include "gmock/gmock.h"
#include "test/gmock-matchers_test.h"
#include "gtest/gtest.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4244 4100)

namespace testing {
namespace gmock_matchers_test {
namespace {

typedef ::std::tuple<long, int> Tuple2;

TEST(Eq2Test, MatchesEqualArguments) {
  Matcher<const Tuple2&> m = Eq();
  EXPECT_TRUE(m.Matches(Tuple2(5L, 5)));
  EXPECT_FALSE(m.Matches(Tuple2(5L, 6)));
}

TEST(Eq2Test, CanDescribeSelf) {
  Matcher<const Tuple2&> m = Eq();
  EXPECT_EQ("are an equal pair", Describe(m));
}

TEST(Ge2Test, MatchesGreaterThanOrEqualArguments) {
  Matcher<const Tuple2&> m = Ge();
  EXPECT_TRUE(m.Matches(Tuple2(5L, 4)));
  EXPECT_TRUE(m.Matches(Tuple2(5L, 5)));
  EXPECT_FALSE(m.Matches(Tuple2(5L, 6)));
}

TEST(Ge2Test, CanDescribeSelf) {
  Matcher<const Tuple2&> m = Ge();
  EXPECT_EQ("are a pair where the first >= the second", Describe(m));
}

TEST(Gt2Test, MatchesGreaterThanArguments) {
  Matcher<const Tuple2&> m = Gt();
  EXPECT_TRUE(m.Matches(Tuple2(5L, 4)));
  EXPECT_FALSE(m.Matches(Tuple2(5L, 5)));
  EXPECT_FALSE(m.Matches(Tuple2(5L, 6)));
}

TEST(Gt2Test, CanDescribeSelf) {
  Matcher<const Tuple2&> m = Gt();
  EXPECT_EQ("are a pair where the first > the second", Describe(m));
}

TEST(Le2Test, MatchesLessThanOrEqualArguments) {
  Matcher<const Tuple2&> m = Le();
  EXPECT_TRUE(m.Matches(Tuple2(5L, 6)));
  EXPECT_TRUE(m.Matches(Tuple2(5L, 5)));
  EXPECT_FALSE(m.Matches(Tuple2(5L, 4)));
}

TEST(Le2Test, CanDescribeSelf) {
  Matcher<const Tuple2&> m = Le();
  EXPECT_EQ("are a pair where the first <= the second", Describe(m));
}

TEST(Lt2Test, MatchesLessThanArguments) {
  Matcher<const Tuple2&> m = Lt();
  EXPECT_TRUE(m.Matches(Tuple2(5L, 6)));
  EXPECT_FALSE(m.Matches(Tuple2(5L, 5)));
  EXPECT_FALSE(m.Matches(Tuple2(5L, 4)));
}

TEST(Lt2Test, CanDescribeSelf) {
  Matcher<const Tuple2&> m = Lt();
  EXPECT_EQ("are a pair where the first < the second", Describe(m));
}

TEST(Ne2Test, MatchesUnequalArguments) {
  Matcher<const Tuple2&> m = Ne();
  EXPECT_TRUE(m.Matches(Tuple2(5L, 6)));
  EXPECT_TRUE(m.Matches(Tuple2(5L, 4)));
  EXPECT_FALSE(m.Matches(Tuple2(5L, 5)));
}

TEST(Ne2Test, CanDescribeSelf) {
  Matcher<const Tuple2&> m = Ne();
  EXPECT_EQ("are an unequal pair", Describe(m));
}

TEST(PairMatchBaseTest, WorksWithMoveOnly) {
  using Pointers = std::tuple<std::unique_ptr<int>, std::unique_ptr<int>>;
  Matcher<Pointers> matcher = Eq();
  Pointers pointers;

  EXPECT_TRUE(matcher.Matches(pointers));
}

TEST(IsNan, FloatMatchesNan) {
  float quiet_nan = std::numeric_limits<float>::quiet_NaN();
  float other_nan = std::nanf("1");
  float real_value = 1.0f;

  Matcher<float> m = IsNan();
  EXPECT_TRUE(m.Matches(quiet_nan));
  EXPECT_TRUE(m.Matches(other_nan));
  EXPECT_FALSE(m.Matches(real_value));

  Matcher<float&> m_ref = IsNan();
  EXPECT_TRUE(m_ref.Matches(quiet_nan));
  EXPECT_TRUE(m_ref.Matches(other_nan));
  EXPECT_FALSE(m_ref.Matches(real_value));

  Matcher<const float&> m_cref = IsNan();
  EXPECT_TRUE(m_cref.Matches(quiet_nan));
  EXPECT_TRUE(m_cref.Matches(other_nan));
  EXPECT_FALSE(m_cref.Matches(real_value));
}

TEST(IsNan, DoubleMatchesNan) {
  double quiet_nan = std::numeric_limits<double>::quiet_NaN();
  double other_nan = std::nan("1");
  double real_value = 1.0;

  Matcher<double> m = IsNan();
  EXPECT_TRUE(m.Matches(quiet_nan));
  EXPECT_TRUE(m.Matches(other_nan));
  EXPECT_FALSE(m.Matches(real_value));

  Matcher<double&> m_ref = IsNan();
  EXPECT_TRUE(m_ref.Matches(quiet_nan));
  EXPECT_TRUE(m_ref.Matches(other_nan));
  EXPECT_FALSE(m_ref.Matches(real_value));

  Matcher<const double&> m_cref = IsNan();
  EXPECT_TRUE(m_cref.Matches(quiet_nan));
  EXPECT_TRUE(m_cref.Matches(other_nan));
  EXPECT_FALSE(m_cref.Matches(real_value));
}

TEST(IsNan, LongDoubleMatchesNan) {
  long double quiet_nan = std::numeric_limits<long double>::quiet_NaN();
  long double other_nan = std::nan("1");
  long double real_value = 1.0;

  Matcher<long double> m = IsNan();
  EXPECT_TRUE(m.Matches(quiet_nan));
  EXPECT_TRUE(m.Matches(other_nan));
  EXPECT_FALSE(m.Matches(real_value));

  Matcher<long double&> m_ref = IsNan();
  EXPECT_TRUE(m_ref.Matches(quiet_nan));
  EXPECT_TRUE(m_ref.Matches(other_nan));
  EXPECT_FALSE(m_ref.Matches(real_value));

  Matcher<const long double&> m_cref = IsNan();
  EXPECT_TRUE(m_cref.Matches(quiet_nan));
  EXPECT_TRUE(m_cref.Matches(other_nan));
  EXPECT_FALSE(m_cref.Matches(real_value));
}

TEST(IsNan, NotMatchesNan) {
  Matcher<float> mf = Not(IsNan());
  EXPECT_FALSE(mf.Matches(std::numeric_limits<float>::quiet_NaN()));
  EXPECT_FALSE(mf.Matches(std::nanf("1")));
  EXPECT_TRUE(mf.Matches(1.0));

  Matcher<double> md = Not(IsNan());
  EXPECT_FALSE(md.Matches(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_FALSE(md.Matches(std::nan("1")));
  EXPECT_TRUE(md.Matches(1.0));

  Matcher<long double> mld = Not(IsNan());
  EXPECT_FALSE(mld.Matches(std::numeric_limits<long double>::quiet_NaN()));
  EXPECT_FALSE(mld.Matches(std::nanl("1")));
  EXPECT_TRUE(mld.Matches(1.0));
}

TEST(IsNan, CanDescribeSelf) {
  Matcher<float> mf = IsNan();
  EXPECT_EQ("is NaN", Describe(mf));

  Matcher<double> md = IsNan();
  EXPECT_EQ("is NaN", Describe(md));

  Matcher<long double> mld = IsNan();
  EXPECT_EQ("is NaN", Describe(mld));
}

TEST(IsNan, CanDescribeSelfWithNot) {
  Matcher<float> mf = Not(IsNan());
  EXPECT_EQ("isn't NaN", Describe(mf));

  Matcher<double> md = Not(IsNan());
  EXPECT_EQ("isn't NaN", Describe(md));

  Matcher<long double> mld = Not(IsNan());
  EXPECT_EQ("isn't NaN", Describe(mld));
}

TEST(FloatEq2Test, MatchesEqualArguments) {
  typedef ::std::tuple<float, float> Tpl;
  Matcher<const Tpl&> m = FloatEq();
  EXPECT_TRUE(m.Matches(Tpl(1.0f, 1.0f)));
  EXPECT_TRUE(m.Matches(Tpl(0.3f, 0.1f + 0.1f + 0.1f)));
  EXPECT_FALSE(m.Matches(Tpl(1.1f, 1.0f)));
}

TEST(FloatEq2Test, CanDescribeSelf) {
  Matcher<const ::std::tuple<float, float>&> m = FloatEq();
  EXPECT_EQ("are an almost-equal pair", Describe(m));
}

TEST(NanSensitiveFloatEqTest, MatchesEqualArgumentsWithNaN) {
  typedef ::std::tuple<float, float> Tpl;
  Matcher<const Tpl&> m = NanSensitiveFloatEq();
  EXPECT_TRUE(m.Matches(Tpl(1.0f, 1.0f)));
  EXPECT_TRUE(m.Matches(Tpl(std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::quiet_NaN())));
  EXPECT_FALSE(m.Matches(Tpl(1.1f, 1.0f)));
  EXPECT_FALSE(m.Matches(Tpl(1.0f, std::numeric_limits<float>::quiet_NaN())));
  EXPECT_FALSE(m.Matches(Tpl(std::numeric_limits<float>::quiet_NaN(), 1.0f)));
}

TEST(NanSensitiveFloatEqTest, CanDescribeSelfWithNaNs) {
  Matcher<const ::std::tuple<float, float>&> m = NanSensitiveFloatEq();
  EXPECT_EQ("are an almost-equal pair", Describe(m));
}

TEST(DoubleEq2Test, MatchesEqualArguments) {
  typedef ::std::tuple<double, double> Tpl;
  Matcher<const Tpl&> m = DoubleEq();
  EXPECT_TRUE(m.Matches(Tpl(1.0, 1.0)));
  EXPECT_TRUE(m.Matches(Tpl(0.3, 0.1 + 0.1 + 0.1)));
  EXPECT_FALSE(m.Matches(Tpl(1.1, 1.0)));
}

TEST(DoubleEq2Test, CanDescribeSelf) {
  Matcher<const ::std::tuple<double, double>&> m = DoubleEq();
  EXPECT_EQ("are an almost-equal pair", Describe(m));
}

TEST(NanSensitiveDoubleEqTest, MatchesEqualArgumentsWithNaN) {
  typedef ::std::tuple<double, double> Tpl;
  Matcher<const Tpl&> m = NanSensitiveDoubleEq();
  EXPECT_TRUE(m.Matches(Tpl(1.0f, 1.0f)));
  EXPECT_TRUE(m.Matches(Tpl(std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::quiet_NaN())));
  EXPECT_FALSE(m.Matches(Tpl(1.1f, 1.0f)));
  EXPECT_FALSE(m.Matches(Tpl(1.0f, std::numeric_limits<double>::quiet_NaN())));
  EXPECT_FALSE(m.Matches(Tpl(std::numeric_limits<double>::quiet_NaN(), 1.0f)));
}

TEST(NanSensitiveDoubleEqTest, CanDescribeSelfWithNaNs) {
  Matcher<const ::std::tuple<double, double>&> m = NanSensitiveDoubleEq();
  EXPECT_EQ("are an almost-equal pair", Describe(m));
}

TEST(FloatNear2Test, MatchesEqualArguments) {
  typedef ::std::tuple<float, float> Tpl;
  Matcher<const Tpl&> m = FloatNear(0.5f);
  EXPECT_TRUE(m.Matches(Tpl(1.0f, 1.0f)));
  EXPECT_TRUE(m.Matches(Tpl(1.3f, 1.0f)));
  EXPECT_FALSE(m.Matches(Tpl(1.8f, 1.0f)));
}

TEST(FloatNear2Test, CanDescribeSelf) {
  Matcher<const ::std::tuple<float, float>&> m = FloatNear(0.5f);
  EXPECT_EQ("are an almost-equal pair", Describe(m));
}

TEST(NanSensitiveFloatNearTest, MatchesNearbyArgumentsWithNaN) {
  typedef ::std::tuple<float, float> Tpl;
  Matcher<const Tpl&> m = NanSensitiveFloatNear(0.5f);
  EXPECT_TRUE(m.Matches(Tpl(1.0f, 1.0f)));
  EXPECT_TRUE(m.Matches(Tpl(1.1f, 1.0f)));
  EXPECT_TRUE(m.Matches(Tpl(std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::quiet_NaN())));
  EXPECT_FALSE(m.Matches(Tpl(1.6f, 1.0f)));
  EXPECT_FALSE(m.Matches(Tpl(1.0f, std::numeric_limits<float>::quiet_NaN())));
  EXPECT_FALSE(m.Matches(Tpl(std::numeric_limits<float>::quiet_NaN(), 1.0f)));
}

TEST(NanSensitiveFloatNearTest, CanDescribeSelfWithNaNs) {
  Matcher<const ::std::tuple<float, float>&> m = NanSensitiveFloatNear(0.5f);
  EXPECT_EQ("are an almost-equal pair", Describe(m));
}

TEST(DoubleNear2Test, MatchesEqualArguments) {
  typedef ::std::tuple<double, double> Tpl;
  Matcher<const Tpl&> m = DoubleNear(0.5);
  EXPECT_TRUE(m.Matches(Tpl(1.0, 1.0)));
  EXPECT_TRUE(m.Matches(Tpl(1.3, 1.0)));
  EXPECT_FALSE(m.Matches(Tpl(1.8, 1.0)));
}

TEST(DoubleNear2Test, CanDescribeSelf) {
  Matcher<const ::std::tuple<double, double>&> m = DoubleNear(0.5);
  EXPECT_EQ("are an almost-equal pair", Describe(m));
}

TEST(NanSensitiveDoubleNearTest, MatchesNearbyArgumentsWithNaN) {
  typedef ::std::tuple<double, double> Tpl;
  Matcher<const Tpl&> m = NanSensitiveDoubleNear(0.5f);
  EXPECT_TRUE(m.Matches(Tpl(1.0f, 1.0f)));
  EXPECT_TRUE(m.Matches(Tpl(1.1f, 1.0f)));
  EXPECT_TRUE(m.Matches(Tpl(std::numeric_limits<double>::quiet_NaN(),
                            std::numeric_limits<double>::quiet_NaN())));
  EXPECT_FALSE(m.Matches(Tpl(1.6f, 1.0f)));
  EXPECT_FALSE(m.Matches(Tpl(1.0f, std::numeric_limits<double>::quiet_NaN())));
  EXPECT_FALSE(m.Matches(Tpl(std::numeric_limits<double>::quiet_NaN(), 1.0f)));
}

TEST(NanSensitiveDoubleNearTest, CanDescribeSelfWithNaNs) {
  Matcher<const ::std::tuple<double, double>&> m = NanSensitiveDoubleNear(0.5f);
  EXPECT_EQ("are an almost-equal pair", Describe(m));
}

TEST(NotTest, NegatesMatcher) {
  Matcher<int> m;
  m = Not(Eq(2));
  EXPECT_TRUE(m.Matches(3));
  EXPECT_FALSE(m.Matches(2));
}

TEST(NotTest, CanDescribeSelf) {
  Matcher<int> m = Not(Eq(5));
  EXPECT_EQ("isn't equal to 5", Describe(m));
}

TEST(NotTest, NotMatcherSafelyCastsMonomorphicMatchers) {

  Matcher<int> greater_than_5 = Gt(5);

  Matcher<const int&> m = Not(greater_than_5);
  Matcher<int&> m2 = Not(greater_than_5);
  Matcher<int&> m3 = Not(m);
}

void AllOfMatches(int num, const Matcher<int>& m) {
  SCOPED_TRACE(Describe(m));
  EXPECT_TRUE(m.Matches(0));
  for (int i = 1; i <= num; ++i) {
    EXPECT_FALSE(m.Matches(i));
  }
  EXPECT_TRUE(m.Matches(num + 1));
}

INSTANTIATE_GTEST_MATCHER_TEST_P(AllOfTest);

TEST(AllOfTest, MatchesWhenAllMatch) {
  Matcher<int> m;
  m = AllOf(Le(2), Ge(1));
  EXPECT_TRUE(m.Matches(1));
  EXPECT_TRUE(m.Matches(2));
  EXPECT_FALSE(m.Matches(0));
  EXPECT_FALSE(m.Matches(3));

  m = AllOf(Gt(0), Ne(1), Ne(2));
  EXPECT_TRUE(m.Matches(3));
  EXPECT_FALSE(m.Matches(2));
  EXPECT_FALSE(m.Matches(1));
  EXPECT_FALSE(m.Matches(0));

  m = AllOf(Gt(0), Ne(1), Ne(2), Ne(3));
  EXPECT_TRUE(m.Matches(4));
  EXPECT_FALSE(m.Matches(3));
  EXPECT_FALSE(m.Matches(2));
  EXPECT_FALSE(m.Matches(1));
  EXPECT_FALSE(m.Matches(0));

  m = AllOf(Ge(0), Lt(10), Ne(3), Ne(5), Ne(7));
  EXPECT_TRUE(m.Matches(0));
  EXPECT_TRUE(m.Matches(1));
  EXPECT_FALSE(m.Matches(3));

  AllOfMatches(2, AllOf(Ne(1), Ne(2)));
  AllOfMatches(3, AllOf(Ne(1), Ne(2), Ne(3)));
  AllOfMatches(4, AllOf(Ne(1), Ne(2), Ne(3), Ne(4)));
  AllOfMatches(5, AllOf(Ne(1), Ne(2), Ne(3), Ne(4), Ne(5)));
  AllOfMatches(6, AllOf(Ne(1), Ne(2), Ne(3), Ne(4), Ne(5), Ne(6)));
  AllOfMatches(7, AllOf(Ne(1), Ne(2), Ne(3), Ne(4), Ne(5), Ne(6), Ne(7)));
  AllOfMatches(8,
               AllOf(Ne(1), Ne(2), Ne(3), Ne(4), Ne(5), Ne(6), Ne(7), Ne(8)));
  AllOfMatches(
      9, AllOf(Ne(1), Ne(2), Ne(3), Ne(4), Ne(5), Ne(6), Ne(7), Ne(8), Ne(9)));
  AllOfMatches(10, AllOf(Ne(1), Ne(2), Ne(3), Ne(4), Ne(5), Ne(6), Ne(7), Ne(8),
                         Ne(9), Ne(10)));
  AllOfMatches(
      50, AllOf(Ne(1), Ne(2), Ne(3), Ne(4), Ne(5), Ne(6), Ne(7), Ne(8), Ne(9),
                Ne(10), Ne(11), Ne(12), Ne(13), Ne(14), Ne(15), Ne(16), Ne(17),
                Ne(18), Ne(19), Ne(20), Ne(21), Ne(22), Ne(23), Ne(24), Ne(25),
                Ne(26), Ne(27), Ne(28), Ne(29), Ne(30), Ne(31), Ne(32), Ne(33),
                Ne(34), Ne(35), Ne(36), Ne(37), Ne(38), Ne(39), Ne(40), Ne(41),
                Ne(42), Ne(43), Ne(44), Ne(45), Ne(46), Ne(47), Ne(48), Ne(49),
                Ne(50)));
}

TEST(AllOfTest, CanDescribeSelf) {
  Matcher<int> m;
  m = AllOf(Le(2), Ge(1));
  EXPECT_EQ("(is <= 2) and (is >= 1)", Describe(m));

  m = AllOf(Gt(0), Ne(1), Ne(2));
  std::string expected_descr1 =
      "(is > 0) and (isn't equal to 1) and (isn't equal to 2)";
  EXPECT_EQ(expected_descr1, Describe(m));

  m = AllOf(Gt(0), Ne(1), Ne(2), Ne(3));
  std::string expected_descr2 =
      "(is > 0) and (isn't equal to 1) and (isn't equal to 2) and (isn't equal "
      "to 3)";
  EXPECT_EQ(expected_descr2, Describe(m));

  m = AllOf(Ge(0), Lt(10), Ne(3), Ne(5), Ne(7));
  std::string expected_descr3 =
      "(is >= 0) and (is < 10) and (isn't equal to 3) and (isn't equal to 5) "
      "and (isn't equal to 7)";
  EXPECT_EQ(expected_descr3, Describe(m));
}

TEST(AllOfTest, CanDescribeNegation) {
  Matcher<int> m;
  m = AllOf(Le(2), Ge(1));
  std::string expected_descr4 = "(isn't <= 2) or (isn't >= 1)";
  EXPECT_EQ(expected_descr4, DescribeNegation(m));

  m = AllOf(Gt(0), Ne(1), Ne(2));
  std::string expected_descr5 =
      "(isn't > 0) or (is equal to 1) or (is equal to 2)";
  EXPECT_EQ(expected_descr5, DescribeNegation(m));

  m = AllOf(Gt(0), Ne(1), Ne(2), Ne(3));
  std::string expected_descr6 =
      "(isn't > 0) or (is equal to 1) or (is equal to 2) or (is equal to 3)";
  EXPECT_EQ(expected_descr6, DescribeNegation(m));

  m = AllOf(Ge(0), Lt(10), Ne(3), Ne(5), Ne(7));
  std::string expected_desr7 =
      "(isn't >= 0) or (isn't < 10) or (is equal to 3) or (is equal to 5) or "
      "(is equal to 7)";
  EXPECT_EQ(expected_desr7, DescribeNegation(m));

  m = AllOf(Ne(1), Ne(2), Ne(3), Ne(4), Ne(5), Ne(6), Ne(7), Ne(8), Ne(9),
            Ne(10), Ne(11));
  AllOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);
  EXPECT_THAT(Describe(m), EndsWith("and (isn't equal to 11)"));
  AllOfMatches(11, m);
}

TEST(AllOfTest, AllOfMatcherSafelyCastsMonomorphicMatchers) {

  Matcher<int> greater_than_5 = Gt(5);
  Matcher<int> less_than_10 = Lt(10);

  Matcher<const int&> m = AllOf(greater_than_5, less_than_10);
  Matcher<int&> m2 = AllOf(greater_than_5, less_than_10);
  Matcher<int&> m3 = AllOf(greater_than_5, m2);

  Matcher<const int&> m4 = AllOf(greater_than_5, less_than_10, less_than_10);
  Matcher<int&> m5 = AllOf(greater_than_5, less_than_10, less_than_10);
}

TEST_P(AllOfTestP, ExplainsResult) {
  Matcher<int> m;

  m = AllOf(GreaterThan(10), Lt(30));
  EXPECT_EQ("which is 15 more than 10, and is < 30", Explain(m, 25));

  m = AllOf(GreaterThan(10), GreaterThan(20));
  EXPECT_EQ("which is 20 more than 10, and which is 10 more than 20",
            Explain(m, 30));

  m = AllOf(GreaterThan(10), Lt(30), GreaterThan(20));
  EXPECT_EQ(
      "which is 15 more than 10, and is < 30, and which is 5 more than 20",
      Explain(m, 25));

  m = AllOf(GreaterThan(10), GreaterThan(20), GreaterThan(30));
  EXPECT_EQ(
      "which is 30 more than 10, and which is 20 more than 20, "
      "and which is 10 more than 30",
      Explain(m, 40));

  m = AllOf(GreaterThan(10), GreaterThan(20));
  EXPECT_EQ("which is 5 less than 10", Explain(m, 5));

  m = AllOf(GreaterThan(10), Lt(30));
  EXPECT_EQ("which doesn't match (is < 30)", Explain(m, 40));

  m = AllOf(GreaterThan(10), GreaterThan(20));
  EXPECT_EQ("which is 5 less than 20", Explain(m, 15));
}

static void AnyOfMatches(int num, const Matcher<int>& m) {
  SCOPED_TRACE(Describe(m));
  EXPECT_FALSE(m.Matches(0));
  for (int i = 1; i <= num; ++i) {
    EXPECT_TRUE(m.Matches(i));
  }
  EXPECT_FALSE(m.Matches(num + 1));
}

static void AnyOfStringMatches(int num, const Matcher<std::string>& m) {
  SCOPED_TRACE(Describe(m));
  EXPECT_FALSE(m.Matches(std::to_string(0)));

  for (int i = 1; i <= num; ++i) {
    EXPECT_TRUE(m.Matches(std::to_string(i)));
  }
  EXPECT_FALSE(m.Matches(std::to_string(num + 1)));
}

INSTANTIATE_GTEST_MATCHER_TEST_P(AnyOfTest);

TEST(AnyOfTest, MatchesWhenAnyMatches) {
  Matcher<int> m;
  m = AnyOf(Le(1), Ge(3));
  EXPECT_TRUE(m.Matches(1));
  EXPECT_TRUE(m.Matches(4));
  EXPECT_FALSE(m.Matches(2));

  m = AnyOf(Lt(0), Eq(1), Eq(2));
  EXPECT_TRUE(m.Matches(-1));
  EXPECT_TRUE(m.Matches(1));
  EXPECT_TRUE(m.Matches(2));
  EXPECT_FALSE(m.Matches(0));

  m = AnyOf(Lt(0), Eq(1), Eq(2), Eq(3));
  EXPECT_TRUE(m.Matches(-1));
  EXPECT_TRUE(m.Matches(1));
  EXPECT_TRUE(m.Matches(2));
  EXPECT_TRUE(m.Matches(3));
  EXPECT_FALSE(m.Matches(0));

  m = AnyOf(Le(0), Gt(10), 3, 5, 7);
  EXPECT_TRUE(m.Matches(0));
  EXPECT_TRUE(m.Matches(11));
  EXPECT_TRUE(m.Matches(3));
  EXPECT_FALSE(m.Matches(2));

  AnyOfMatches(2, AnyOf(1, 2));
  AnyOfMatches(3, AnyOf(1, 2, 3));
  AnyOfMatches(4, AnyOf(1, 2, 3, 4));
  AnyOfMatches(5, AnyOf(1, 2, 3, 4, 5));
  AnyOfMatches(6, AnyOf(1, 2, 3, 4, 5, 6));
  AnyOfMatches(7, AnyOf(1, 2, 3, 4, 5, 6, 7));
  AnyOfMatches(8, AnyOf(1, 2, 3, 4, 5, 6, 7, 8));
  AnyOfMatches(9, AnyOf(1, 2, 3, 4, 5, 6, 7, 8, 9));
  AnyOfMatches(10, AnyOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
}

TEST(AnyOfTest, VariadicMatchesWhenAnyMatches) {

  Matcher<int> m = ::testing::AnyOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11);

  EXPECT_THAT(Describe(m), EndsWith("or (is equal to 11)"));
  AnyOfMatches(11, m);
  AnyOfMatches(50, AnyOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                         17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
                         31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
                         45, 46, 47, 48, 49, 50));
  AnyOfStringMatches(
      50, AnyOf("1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
                "13", "14", "15", "16", "17", "18", "19", "20", "21", "22",
                "23", "24", "25", "26", "27", "28", "29", "30", "31", "32",
                "33", "34", "35", "36", "37", "38", "39", "40", "41", "42",
                "43", "44", "45", "46", "47", "48", "49", "50"));
}

TEST(ConditionalTest, MatchesFirstIfCondition) {
  Matcher<std::string> eq_red = Eq("red");
  Matcher<std::string> ne_red = Ne("red");
  Matcher<std::string> m = Conditional(true, eq_red, ne_red);
  EXPECT_TRUE(m.Matches("red"));
  EXPECT_FALSE(m.Matches("green"));

  StringMatchResultListener listener;
  StringMatchResultListener expected;
  EXPECT_FALSE(m.MatchAndExplain("green", &listener));
  EXPECT_FALSE(eq_red.MatchAndExplain("green", &expected));
  EXPECT_THAT(listener.str(), Eq(expected.str()));
}

TEST(ConditionalTest, MatchesSecondIfCondition) {
  Matcher<std::string> eq_red = Eq("red");
  Matcher<std::string> ne_red = Ne("red");
  Matcher<std::string> m = Conditional(false, eq_red, ne_red);
  EXPECT_FALSE(m.Matches("red"));
  EXPECT_TRUE(m.Matches("green"));

  StringMatchResultListener listener;
  StringMatchResultListener expected;
  EXPECT_FALSE(m.MatchAndExplain("red", &listener));
  EXPECT_FALSE(ne_red.MatchAndExplain("red", &expected));
  EXPECT_THAT(listener.str(), Eq(expected.str()));
}

TEST(AnyOfTest, CanDescribeSelf) {
  Matcher<int> m;
  m = AnyOf(Le(1), Ge(3));

  EXPECT_EQ("(is <= 1) or (is >= 3)", Describe(m));

  m = AnyOf(Lt(0), Eq(1), Eq(2));
  EXPECT_EQ("(is < 0) or (is equal to 1) or (is equal to 2)", Describe(m));

  m = AnyOf(Lt(0), Eq(1), Eq(2), Eq(3));
  EXPECT_EQ("(is < 0) or (is equal to 1) or (is equal to 2) or (is equal to 3)",
            Describe(m));

  m = AnyOf(Le(0), Gt(10), 3, 5, 7);
  EXPECT_EQ(
      "(is <= 0) or (is > 10) or (is equal to 3) or (is equal to 5) or (is "
      "equal to 7)",
      Describe(m));
}

TEST(AnyOfTest, CanDescribeNegation) {
  Matcher<int> m;
  m = AnyOf(Le(1), Ge(3));
  EXPECT_EQ("(isn't <= 1) and (isn't >= 3)", DescribeNegation(m));

  m = AnyOf(Lt(0), Eq(1), Eq(2));
  EXPECT_EQ("(isn't < 0) and (isn't equal to 1) and (isn't equal to 2)",
            DescribeNegation(m));

  m = AnyOf(Lt(0), Eq(1), Eq(2), Eq(3));
  EXPECT_EQ(
      "(isn't < 0) and (isn't equal to 1) and (isn't equal to 2) and (isn't "
      "equal to 3)",
      DescribeNegation(m));

  m = AnyOf(Le(0), Gt(10), 3, 5, 7);
  EXPECT_EQ(
      "(isn't <= 0) and (isn't > 10) and (isn't equal to 3) and (isn't equal "
      "to 5) and (isn't equal to 7)",
      DescribeNegation(m));
}

TEST(AnyOfTest, AnyOfMatcherSafelyCastsMonomorphicMatchers) {

  Matcher<int> greater_than_5 = Gt(5);
  Matcher<int> less_than_10 = Lt(10);

  Matcher<const int&> m = AnyOf(greater_than_5, less_than_10);
  Matcher<int&> m2 = AnyOf(greater_than_5, less_than_10);
  Matcher<int&> m3 = AnyOf(greater_than_5, m2);

  Matcher<const int&> m4 = AnyOf(greater_than_5, less_than_10, less_than_10);
  Matcher<int&> m5 = AnyOf(greater_than_5, less_than_10, less_than_10);
}

TEST_P(AnyOfTestP, ExplainsResult) {
  Matcher<int> m;

  m = AnyOf(GreaterThan(10), Lt(0));
  EXPECT_EQ("which is 5 less than 10, and isn't < 0", Explain(m, 5));

  m = AnyOf(GreaterThan(10), GreaterThan(20));
  EXPECT_EQ("which is 5 less than 10, and which is 15 less than 20",
            Explain(m, 5));

  m = AnyOf(GreaterThan(10), Gt(20), GreaterThan(30));
  EXPECT_EQ(
      "which is 5 less than 10, and isn't > 20, and which is 25 less than 30",
      Explain(m, 5));

  m = AnyOf(GreaterThan(10), GreaterThan(20), GreaterThan(30));
  EXPECT_EQ(
      "which is 5 less than 10, and which is 15 less than 20, "
      "and which is 25 less than 30",
      Explain(m, 5));

  m = AnyOf(GreaterThan(10), GreaterThan(20));
  EXPECT_EQ("which is 5 more than 10", Explain(m, 15));

  m = AnyOf(GreaterThan(30), GreaterThan(20));
  EXPECT_EQ("which is 5 more than 20", Explain(m, 25));

  m = AnyOf(Gt(10), Lt(20));
  EXPECT_EQ("which matches (is > 10)", Explain(m, 15));

  m = AnyOf(Gt(30), Gt(20));
  EXPECT_EQ("which matches (is > 20)", Explain(m, 25));
}

int IsPositive(double x) { return x > 0 ? 1 : 0; }

class IsGreaterThan {
 public:
  explicit IsGreaterThan(int threshold) : threshold_(threshold) {}

  bool operator()(int n) const { return n > threshold_; }

 private:
  int threshold_;
};

const int foo = 0;

bool ReferencesFooAndIsZero(const int& n) { return (&n == &foo) && (n == 0); }

TEST(TrulyTest, MatchesWhatSatisfiesThePredicate) {
  Matcher<double> m = Truly(IsPositive);
  EXPECT_TRUE(m.Matches(2.0));
  EXPECT_FALSE(m.Matches(-1.5));
}

TEST(TrulyTest, CanBeUsedWithFunctor) {
  Matcher<int> m = Truly(IsGreaterThan(5));
  EXPECT_TRUE(m.Matches(6));
  EXPECT_FALSE(m.Matches(4));
}

class ConvertibleToBool {
 public:
  explicit ConvertibleToBool(int number) : number_(number) {}
  operator bool() const { return number_ != 0; }

 private:
  int number_;
};

ConvertibleToBool IsNotZero(int number) { return ConvertibleToBool(number); }

TEST(TrulyTest, PredicateCanReturnAClassConvertibleToBool) {
  Matcher<int> m = Truly(IsNotZero);
  EXPECT_TRUE(m.Matches(1));
  EXPECT_FALSE(m.Matches(0));
}

TEST(TrulyTest, CanDescribeSelf) {
  Matcher<double> m = Truly(IsPositive);
  EXPECT_EQ("satisfies the given predicate", Describe(m));
}

TEST(TrulyTest, WorksForByRefArguments) {
  Matcher<const int&> m = Truly(ReferencesFooAndIsZero);
  EXPECT_TRUE(m.Matches(foo));
  int n = 0;
  EXPECT_FALSE(m.Matches(n));
}

TEST(TrulyTest, ExplainsFailures) {
  StringMatchResultListener listener;
  EXPECT_FALSE(ExplainMatchResult(Truly(IsPositive), -1, &listener));
  EXPECT_EQ(listener.str(), "didn't satisfy the given predicate");
}

TEST(MatchesTest, IsSatisfiedByWhatMatchesTheMatcher) {
  EXPECT_TRUE(Matches(Ge(0))(1));
  EXPECT_FALSE(Matches(Eq('a'))('b'));
}

TEST(MatchesTest, WorksOnByRefArguments) {
  int m = 0, n = 0;
  EXPECT_TRUE(Matches(AllOf(Ref(n), Eq(0)))(n));
  EXPECT_FALSE(Matches(Ref(m))(n));
}

TEST(MatchesTest, WorksWithMatcherOnNonRefType) {
  Matcher<int> eq5 = Eq(5);
  EXPECT_TRUE(Matches(eq5)(5));
  EXPECT_FALSE(Matches(eq5)(2));
}

TEST(ValueTest, WorksWithPolymorphicMatcher) {
  EXPECT_TRUE(Value("hi", StartsWith("h")));
  EXPECT_FALSE(Value(5, Gt(10)));
}

TEST(ValueTest, WorksWithMonomorphicMatcher) {
  const Matcher<int> is_zero = Eq(0);
  EXPECT_TRUE(Value(0, is_zero));
  EXPECT_FALSE(Value('a', is_zero));

  int n = 0;
  const Matcher<const int&> ref_n = Ref(n);
  EXPECT_TRUE(Value(n, ref_n));
  EXPECT_FALSE(Value(1, ref_n));
}

TEST(AllArgsTest, WorksForTuple) {
  EXPECT_THAT(std::make_tuple(1, 2L), AllArgs(Lt()));
  EXPECT_THAT(std::make_tuple(2L, 1), Not(AllArgs(Lt())));
}

TEST(AllArgsTest, WorksForNonTuple) {
  EXPECT_THAT(42, AllArgs(Gt(0)));
  EXPECT_THAT('a', Not(AllArgs(Eq('b'))));
}

class AllArgsHelper {
 public:
  AllArgsHelper() = default;

  MOCK_METHOD2(Helper, int(char x, int y));

 private:
  AllArgsHelper(const AllArgsHelper&) = delete;
  AllArgsHelper& operator=(const AllArgsHelper&) = delete;
};

TEST(AllArgsTest, WorksInWithClause) {
  AllArgsHelper helper;
  ON_CALL(helper, Helper(_, _)).With(AllArgs(Lt())).WillByDefault(Return(1));
  EXPECT_CALL(helper, Helper(_, _));
  EXPECT_CALL(helper, Helper(_, _)).With(AllArgs(Gt())).WillOnce(Return(2));

  EXPECT_EQ(1, helper.Helper('\1', 2));
  EXPECT_EQ(2, helper.Helper('a', 1));
}

class OptionalMatchersHelper {
 public:
  OptionalMatchersHelper() = default;

  MOCK_METHOD0(NoArgs, int());

  MOCK_METHOD1(OneArg, int(int y));

  MOCK_METHOD2(TwoArgs, int(char x, int y));

  MOCK_METHOD1(Overloaded, int(char x));
  MOCK_METHOD2(Overloaded, int(char x, int y));

 private:
  OptionalMatchersHelper(const OptionalMatchersHelper&) = delete;
  OptionalMatchersHelper& operator=(const OptionalMatchersHelper&) = delete;
};

TEST(AllArgsTest, WorksWithoutMatchers) {
  OptionalMatchersHelper helper;

  ON_CALL(helper, NoArgs).WillByDefault(Return(10));
  ON_CALL(helper, OneArg).WillByDefault(Return(20));
  ON_CALL(helper, TwoArgs).WillByDefault(Return(30));

  EXPECT_EQ(10, helper.NoArgs());
  EXPECT_EQ(20, helper.OneArg(1));
  EXPECT_EQ(30, helper.TwoArgs('\1', 2));

  EXPECT_CALL(helper, NoArgs).Times(1);
  EXPECT_CALL(helper, OneArg).WillOnce(Return(100));
  EXPECT_CALL(helper, OneArg(17)).WillOnce(Return(200));
  EXPECT_CALL(helper, TwoArgs).Times(0);

  EXPECT_EQ(10, helper.NoArgs());
  EXPECT_EQ(100, helper.OneArg(1));
  EXPECT_EQ(200, helper.OneArg(17));
}

template <typename RawType>
class FloatingPointTest : public testing::Test {
 protected:
  typedef testing::internal::FloatingPoint<RawType> Floating;
  typedef typename Floating::Bits Bits;

  FloatingPointTest()
      : max_ulps_(Floating::kMaxUlps),
        zero_bits_(Floating(0).bits()),
        one_bits_(Floating(1).bits()),
        infinity_bits_(Floating(Floating::Infinity()).bits()),
        close_to_positive_zero_(
            Floating::ReinterpretBits(zero_bits_ + max_ulps_ / 2)),
        close_to_negative_zero_(
            -Floating::ReinterpretBits(zero_bits_ + max_ulps_ - max_ulps_ / 2)),
        further_from_negative_zero_(-Floating::ReinterpretBits(
            zero_bits_ + max_ulps_ + 1 - max_ulps_ / 2)),
        close_to_one_(Floating::ReinterpretBits(one_bits_ + max_ulps_)),
        further_from_one_(Floating::ReinterpretBits(one_bits_ + max_ulps_ + 1)),
        infinity_(Floating::Infinity()),
        close_to_infinity_(
            Floating::ReinterpretBits(infinity_bits_ - max_ulps_)),
        further_from_infinity_(
            Floating::ReinterpretBits(infinity_bits_ - max_ulps_ - 1)),
        max_(std::numeric_limits<RawType>::max()),
        nan1_(Floating::ReinterpretBits(Floating::kExponentBitMask | 1)),
        nan2_(Floating::ReinterpretBits(Floating::kExponentBitMask | 200)) {}

  void TestSize() { EXPECT_EQ(sizeof(RawType), sizeof(Bits)); }

  void TestMatches(
      testing::internal::FloatingEqMatcher<RawType> (*matcher_maker)(RawType)) {
    Matcher<RawType> m1 = matcher_maker(0.0);
    EXPECT_TRUE(m1.Matches(-0.0));
    EXPECT_TRUE(m1.Matches(close_to_positive_zero_));
    EXPECT_TRUE(m1.Matches(close_to_negative_zero_));
    EXPECT_FALSE(m1.Matches(1.0));

    Matcher<RawType> m2 = matcher_maker(close_to_positive_zero_);
    EXPECT_FALSE(m2.Matches(further_from_negative_zero_));

    Matcher<RawType> m3 = matcher_maker(1.0);
    EXPECT_TRUE(m3.Matches(close_to_one_));
    EXPECT_FALSE(m3.Matches(further_from_one_));

    EXPECT_FALSE(m3.Matches(0.0));

    Matcher<RawType> m4 = matcher_maker(-infinity_);
    EXPECT_TRUE(m4.Matches(-close_to_infinity_));

    Matcher<RawType> m5 = matcher_maker(infinity_);
    EXPECT_TRUE(m5.Matches(close_to_infinity_));

    EXPECT_FALSE(m5.Matches(nan1_));

    Matcher<const RawType&> m6 = matcher_maker(0.0);
    EXPECT_TRUE(m6.Matches(-0.0));
    EXPECT_TRUE(m6.Matches(close_to_positive_zero_));
    EXPECT_FALSE(m6.Matches(1.0));

    Matcher<RawType&> m7 = matcher_maker(0.0);
    RawType x = 0.0;
    EXPECT_TRUE(m7.Matches(x));
    x = 0.01f;
    EXPECT_FALSE(m7.Matches(x));
  }

  const Bits max_ulps_;

  const Bits zero_bits_;
  const Bits one_bits_;
  const Bits infinity_bits_;

  const RawType close_to_positive_zero_;
  const RawType close_to_negative_zero_;
  const RawType further_from_negative_zero_;

  const RawType close_to_one_;
  const RawType further_from_one_;

  const RawType infinity_;
  const RawType close_to_infinity_;
  const RawType further_from_infinity_;

  const RawType max_;

  const RawType nan1_;
  const RawType nan2_;
};

template <typename RawType>
class FloatingPointNearTest : public FloatingPointTest<RawType> {
 protected:
  typedef FloatingPointTest<RawType> ParentType;

  void TestNearMatches(testing::internal::FloatingEqMatcher<RawType> (
      *matcher_maker)(RawType, RawType)) {
    Matcher<RawType> m1 = matcher_maker(0.0, 0.0);
    EXPECT_TRUE(m1.Matches(0.0));
    EXPECT_TRUE(m1.Matches(-0.0));
    EXPECT_FALSE(m1.Matches(ParentType::close_to_positive_zero_));
    EXPECT_FALSE(m1.Matches(ParentType::close_to_negative_zero_));
    EXPECT_FALSE(m1.Matches(1.0));

    Matcher<RawType> m2 = matcher_maker(0.0, 1.0);
    EXPECT_TRUE(m2.Matches(0.0));
    EXPECT_TRUE(m2.Matches(-0.0));
    EXPECT_TRUE(m2.Matches(1.0));
    EXPECT_TRUE(m2.Matches(-1.0));
    EXPECT_FALSE(m2.Matches(ParentType::close_to_one_));
    EXPECT_FALSE(m2.Matches(-ParentType::close_to_one_));

    Matcher<RawType> m3 = matcher_maker(ParentType::infinity_, 0.0);
    EXPECT_TRUE(m3.Matches(ParentType::infinity_));
    EXPECT_FALSE(m3.Matches(ParentType::close_to_infinity_));
    EXPECT_FALSE(m3.Matches(-ParentType::infinity_));

    Matcher<RawType> m4 = matcher_maker(-ParentType::infinity_, 0.0);
    EXPECT_TRUE(m4.Matches(-ParentType::infinity_));
    EXPECT_FALSE(m4.Matches(-ParentType::close_to_infinity_));
    EXPECT_FALSE(m4.Matches(ParentType::infinity_));

    Matcher<RawType> m5 = matcher_maker(ParentType::max_, ParentType::max_);
    EXPECT_TRUE(m5.Matches(ParentType::max_));
    EXPECT_FALSE(m5.Matches(-ParentType::max_));

    Matcher<RawType> m6 = matcher_maker(-ParentType::max_, ParentType::max_);
    EXPECT_FALSE(m6.Matches(ParentType::max_));
    EXPECT_TRUE(m6.Matches(-ParentType::max_));

    Matcher<RawType> m7 = matcher_maker(ParentType::max_, 0);
    EXPECT_TRUE(m7.Matches(ParentType::max_));
    EXPECT_FALSE(m7.Matches(-ParentType::max_));

    Matcher<RawType> m8 = matcher_maker(-ParentType::max_, 0);
    EXPECT_FALSE(m8.Matches(ParentType::max_));
    EXPECT_TRUE(m8.Matches(-ParentType::max_));

    Matcher<RawType> m9 =
        matcher_maker(ParentType::max_, ParentType::infinity_);
    EXPECT_TRUE(m8.Matches(-ParentType::max_));

    Matcher<const RawType&> m10 = matcher_maker(0.0, 1.0);
    EXPECT_TRUE(m10.Matches(-0.0));
    EXPECT_TRUE(m10.Matches(ParentType::close_to_positive_zero_));
    EXPECT_FALSE(m10.Matches(ParentType::close_to_one_));

    Matcher<RawType&> m11 = matcher_maker(0.0, 1.0);
    RawType x = 0.0;
    EXPECT_TRUE(m11.Matches(x));
    x = 1.0f;
    EXPECT_TRUE(m11.Matches(x));
    x = -1.0f;
    EXPECT_TRUE(m11.Matches(x));
    x = 1.1f;
    EXPECT_FALSE(m11.Matches(x));
    x = -1.1f;
    EXPECT_FALSE(m11.Matches(x));
  }
};

typedef FloatingPointTest<float> FloatTest;

TEST_F(FloatTest, FloatEqApproximatelyMatchesFloats) { TestMatches(&FloatEq); }

TEST_F(FloatTest, NanSensitiveFloatEqApproximatelyMatchesFloats) {
  TestMatches(&NanSensitiveFloatEq);
}

TEST_F(FloatTest, FloatEqCannotMatchNaN) {

  Matcher<float> m = FloatEq(nan1_);
  EXPECT_FALSE(m.Matches(nan1_));
  EXPECT_FALSE(m.Matches(nan2_));
  EXPECT_FALSE(m.Matches(1.0));
}

TEST_F(FloatTest, NanSensitiveFloatEqCanMatchNaN) {

  Matcher<float> m = NanSensitiveFloatEq(nan1_);
  EXPECT_TRUE(m.Matches(nan1_));
  EXPECT_TRUE(m.Matches(nan2_));
  EXPECT_FALSE(m.Matches(1.0));
}

TEST_F(FloatTest, FloatEqCanDescribeSelf) {
  Matcher<float> m1 = FloatEq(2.0f);
  EXPECT_EQ("is approximately 2", Describe(m1));
  EXPECT_EQ("isn't approximately 2", DescribeNegation(m1));

  Matcher<float> m2 = FloatEq(0.5f);
  EXPECT_EQ("is approximately 0.5", Describe(m2));
  EXPECT_EQ("isn't approximately 0.5", DescribeNegation(m2));

  Matcher<float> m3 = FloatEq(nan1_);
  EXPECT_EQ("never matches", Describe(m3));
  EXPECT_EQ("is anything", DescribeNegation(m3));
}

TEST_F(FloatTest, NanSensitiveFloatEqCanDescribeSelf) {
  Matcher<float> m1 = NanSensitiveFloatEq(2.0f);
  EXPECT_EQ("is approximately 2", Describe(m1));
  EXPECT_EQ("isn't approximately 2", DescribeNegation(m1));

  Matcher<float> m2 = NanSensitiveFloatEq(0.5f);
  EXPECT_EQ("is approximately 0.5", Describe(m2));
  EXPECT_EQ("isn't approximately 0.5", DescribeNegation(m2));

  Matcher<float> m3 = NanSensitiveFloatEq(nan1_);
  EXPECT_EQ("is NaN", Describe(m3));
  EXPECT_EQ("isn't NaN", DescribeNegation(m3));
}

typedef FloatingPointNearTest<float> FloatNearTest;

TEST_F(FloatNearTest, FloatNearMatches) { TestNearMatches(&FloatNear); }

TEST_F(FloatNearTest, NanSensitiveFloatNearApproximatelyMatchesFloats) {
  TestNearMatches(&NanSensitiveFloatNear);
}

TEST_F(FloatNearTest, FloatNearCanDescribeSelf) {
  Matcher<float> m1 = FloatNear(2.0f, 0.5f);
  EXPECT_EQ("is approximately 2 (absolute error <= 0.5)", Describe(m1));
  EXPECT_EQ("isn't approximately 2 (absolute error > 0.5)",
            DescribeNegation(m1));

  Matcher<float> m2 = FloatNear(0.5f, 0.5f);
  EXPECT_EQ("is approximately 0.5 (absolute error <= 0.5)", Describe(m2));
  EXPECT_EQ("isn't approximately 0.5 (absolute error > 0.5)",
            DescribeNegation(m2));

  Matcher<float> m3 = FloatNear(nan1_, 0.0);
  EXPECT_EQ("never matches", Describe(m3));
  EXPECT_EQ("is anything", DescribeNegation(m3));
}

TEST_F(FloatNearTest, NanSensitiveFloatNearCanDescribeSelf) {
  Matcher<float> m1 = NanSensitiveFloatNear(2.0f, 0.5f);
  EXPECT_EQ("is approximately 2 (absolute error <= 0.5)", Describe(m1));
  EXPECT_EQ("isn't approximately 2 (absolute error > 0.5)",
            DescribeNegation(m1));

  Matcher<float> m2 = NanSensitiveFloatNear(0.5f, 0.5f);
  EXPECT_EQ("is approximately 0.5 (absolute error <= 0.5)", Describe(m2));
  EXPECT_EQ("isn't approximately 0.5 (absolute error > 0.5)",
            DescribeNegation(m2));

  Matcher<float> m3 = NanSensitiveFloatNear(nan1_, 0.1f);
  EXPECT_EQ("is NaN", Describe(m3));
  EXPECT_EQ("isn't NaN", DescribeNegation(m3));
}

TEST_F(FloatNearTest, FloatNearCannotMatchNaN) {

  Matcher<float> m = FloatNear(ParentType::nan1_, 0.1f);
  EXPECT_FALSE(m.Matches(nan1_));
  EXPECT_FALSE(m.Matches(nan2_));
  EXPECT_FALSE(m.Matches(1.0));
}

TEST_F(FloatNearTest, NanSensitiveFloatNearCanMatchNaN) {

  Matcher<float> m = NanSensitiveFloatNear(nan1_, 0.1f);
  EXPECT_TRUE(m.Matches(nan1_));
  EXPECT_TRUE(m.Matches(nan2_));
  EXPECT_FALSE(m.Matches(1.0));
}

typedef FloatingPointTest<double> DoubleTest;

TEST_F(DoubleTest, DoubleEqApproximatelyMatchesDoubles) {
  TestMatches(&DoubleEq);
}

TEST_F(DoubleTest, NanSensitiveDoubleEqApproximatelyMatchesDoubles) {
  TestMatches(&NanSensitiveDoubleEq);
}

TEST_F(DoubleTest, DoubleEqCannotMatchNaN) {

  Matcher<double> m = DoubleEq(nan1_);
  EXPECT_FALSE(m.Matches(nan1_));
  EXPECT_FALSE(m.Matches(nan2_));
  EXPECT_FALSE(m.Matches(1.0));
}

TEST_F(DoubleTest, NanSensitiveDoubleEqCanMatchNaN) {

  Matcher<double> m = NanSensitiveDoubleEq(nan1_);
  EXPECT_TRUE(m.Matches(nan1_));
  EXPECT_TRUE(m.Matches(nan2_));
  EXPECT_FALSE(m.Matches(1.0));
}

TEST_F(DoubleTest, DoubleEqCanDescribeSelf) {
  Matcher<double> m1 = DoubleEq(2.0);
  EXPECT_EQ("is approximately 2", Describe(m1));
  EXPECT_EQ("isn't approximately 2", DescribeNegation(m1));

  Matcher<double> m2 = DoubleEq(0.5);
  EXPECT_EQ("is approximately 0.5", Describe(m2));
  EXPECT_EQ("isn't approximately 0.5", DescribeNegation(m2));

  Matcher<double> m3 = DoubleEq(nan1_);
  EXPECT_EQ("never matches", Describe(m3));
  EXPECT_EQ("is anything", DescribeNegation(m3));
}

TEST_F(DoubleTest, NanSensitiveDoubleEqCanDescribeSelf) {
  Matcher<double> m1 = NanSensitiveDoubleEq(2.0);
  EXPECT_EQ("is approximately 2", Describe(m1));
  EXPECT_EQ("isn't approximately 2", DescribeNegation(m1));

  Matcher<double> m2 = NanSensitiveDoubleEq(0.5);
  EXPECT_EQ("is approximately 0.5", Describe(m2));
  EXPECT_EQ("isn't approximately 0.5", DescribeNegation(m2));

  Matcher<double> m3 = NanSensitiveDoubleEq(nan1_);
  EXPECT_EQ("is NaN", Describe(m3));
  EXPECT_EQ("isn't NaN", DescribeNegation(m3));
}

typedef FloatingPointNearTest<double> DoubleNearTest;

TEST_F(DoubleNearTest, DoubleNearMatches) { TestNearMatches(&DoubleNear); }

TEST_F(DoubleNearTest, NanSensitiveDoubleNearApproximatelyMatchesDoubles) {
  TestNearMatches(&NanSensitiveDoubleNear);
}

TEST_F(DoubleNearTest, DoubleNearCanDescribeSelf) {
  Matcher<double> m1 = DoubleNear(2.0, 0.5);
  EXPECT_EQ("is approximately 2 (absolute error <= 0.5)", Describe(m1));
  EXPECT_EQ("isn't approximately 2 (absolute error > 0.5)",
            DescribeNegation(m1));

  Matcher<double> m2 = DoubleNear(0.5, 0.5);
  EXPECT_EQ("is approximately 0.5 (absolute error <= 0.5)", Describe(m2));
  EXPECT_EQ("isn't approximately 0.5 (absolute error > 0.5)",
            DescribeNegation(m2));

  Matcher<double> m3 = DoubleNear(nan1_, 0.0);
  EXPECT_EQ("never matches", Describe(m3));
  EXPECT_EQ("is anything", DescribeNegation(m3));
}

TEST_F(DoubleNearTest, ExplainsResultWhenMatchFails) {
  EXPECT_EQ("", Explain(DoubleNear(2.0, 0.1), 2.05));
  EXPECT_EQ("which is 0.2 from 2", Explain(DoubleNear(2.0, 0.1), 2.2));
  EXPECT_EQ("which is -0.3 from 2", Explain(DoubleNear(2.0, 0.1), 1.7));

  const std::string explanation =
      Explain(DoubleNear(2.1, 1e-10), 2.1 + 1.2e-10);

  EXPECT_TRUE(explanation == "which is 1.2e-10 from 2.1" ||
              explanation == "which is 1.2e-010 from 2.1")
      << " where explanation is \"" << explanation << "\".";
}

TEST_F(DoubleNearTest, NanSensitiveDoubleNearCanDescribeSelf) {
  Matcher<double> m1 = NanSensitiveDoubleNear(2.0, 0.5);
  EXPECT_EQ("is approximately 2 (absolute error <= 0.5)", Describe(m1));
  EXPECT_EQ("isn't approximately 2 (absolute error > 0.5)",
            DescribeNegation(m1));

  Matcher<double> m2 = NanSensitiveDoubleNear(0.5, 0.5);
  EXPECT_EQ("is approximately 0.5 (absolute error <= 0.5)", Describe(m2));
  EXPECT_EQ("isn't approximately 0.5 (absolute error > 0.5)",
            DescribeNegation(m2));

  Matcher<double> m3 = NanSensitiveDoubleNear(nan1_, 0.1);
  EXPECT_EQ("is NaN", Describe(m3));
  EXPECT_EQ("isn't NaN", DescribeNegation(m3));
}

TEST_F(DoubleNearTest, DoubleNearCannotMatchNaN) {

  Matcher<double> m = DoubleNear(ParentType::nan1_, 0.1);
  EXPECT_FALSE(m.Matches(nan1_));
  EXPECT_FALSE(m.Matches(nan2_));
  EXPECT_FALSE(m.Matches(1.0));
}

TEST_F(DoubleNearTest, NanSensitiveDoubleNearCanMatchNaN) {

  Matcher<double> m = NanSensitiveDoubleNear(nan1_, 0.1);
  EXPECT_TRUE(m.Matches(nan1_));
  EXPECT_TRUE(m.Matches(nan2_));
  EXPECT_FALSE(m.Matches(1.0));
}

TEST(NotTest, WorksOnMoveOnlyType) {
  std::unique_ptr<int> p(new int(3));
  EXPECT_THAT(p, Pointee(Eq(3)));
  EXPECT_THAT(p, Not(Pointee(Eq(2))));
}

TEST(AllOfTest, HugeMatcher) {

  EXPECT_THAT(0, testing::AllOf(_, _, _, _, _, _, _, _, _,
                                testing::AllOf(_, _, _, _, _, _, _, _, _, _)));
}

TEST(AnyOfTest, HugeMatcher) {

  EXPECT_THAT(0, testing::AnyOf(_, _, _, _, _, _, _, _, _,
                                testing::AnyOf(_, _, _, _, _, _, _, _, _, _)));
}

namespace adl_test {

MATCHER(M, "") {
  (void)arg;
  return true;
}

template <typename T1, typename T2>
bool AllOf(const T1&       , const T2&       ) {
  return true;
}

TEST(AllOfTest, DoesNotCallAllOfUnqualified) {
  EXPECT_THAT(42,
              testing::AllOf(M(), M(), M(), M(), M(), M(), M(), M(), M(), M()));
}

template <typename T1, typename T2>
bool AnyOf(const T1&, const T2&) {
  return true;
}

TEST(AnyOfTest, DoesNotCallAnyOfUnqualified) {
  EXPECT_THAT(42,
              testing::AnyOf(M(), M(), M(), M(), M(), M(), M(), M(), M(), M()));
}

}

TEST(AllOfTest, WorksOnMoveOnlyType) {
  std::unique_ptr<int> p(new int(3));
  EXPECT_THAT(p, AllOf(Pointee(Eq(3)), Pointee(Gt(0)), Pointee(Lt(5))));
  EXPECT_THAT(p, Not(AllOf(Pointee(Eq(3)), Pointee(Gt(0)), Pointee(Lt(3)))));
}

TEST(AnyOfTest, WorksOnMoveOnlyType) {
  std::unique_ptr<int> p(new int(3));
  EXPECT_THAT(p, AnyOf(Pointee(Eq(5)), Pointee(Lt(0)), Pointee(Lt(5))));
  EXPECT_THAT(p, Not(AnyOf(Pointee(Eq(5)), Pointee(Lt(0)), Pointee(Gt(5)))));
}

}
}
}

GTEST_DISABLE_MSC_WARNINGS_POP_()
