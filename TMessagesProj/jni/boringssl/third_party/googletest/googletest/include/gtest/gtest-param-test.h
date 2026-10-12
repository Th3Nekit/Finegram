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

// Macros and functions for implementing parameterized tests
// in Google C++ Testing and Mocking Framework (Google Test)

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_PARAM_TEST_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_PARAM_TEST_H_

#if 0

class FooTest : public ::testing::TestWithParam<const char*> {

};

TEST_P(FooTest, DoesBlah) {

  EXPECT_TRUE(foo.Blah(GetParam()));
  ...
}

TEST_P(FooTest, HasBlahBlah) {
  ...
}

INSTANTIATE_TEST_SUITE_P(InstantiationName,
                         FooTest,
                         Values("meeny", "miny", "moe"));

const char* pets[] = {"cat", "dog"};
INSTANTIATE_TEST_SUITE_P(AnotherInstantiationName, FooTest, ValuesIn(pets));

class BaseTest : public ::testing::Test {

};

class DerivedTest : public BaseTest, public ::testing::WithParamInterface<int> {

};

TEST_F(BaseTest, HasFoo) {

}

TEST_P(DerivedTest, DoesBlah) {

  EXPECT_TRUE(foo.Blah(GetParam()));
}

#endif

#include <iterator>
#include <utility>

#include "gtest/internal/gtest-internal.h"
#include "gtest/internal/gtest-param-util.h"
#include "gtest/internal/gtest-port.h"

namespace testing {

template <typename T, typename IncrementT>
internal::ParamGenerator<T> Range(T start, T end, IncrementT step) {
  return internal::ParamGenerator<T>(
      new internal::RangeGenerator<T, IncrementT>(start, end, step));
}

template <typename T>
internal::ParamGenerator<T> Range(T start, T end) {
  return Range(start, end, 1);
}

template <typename ForwardIterator>
internal::ParamGenerator<
    typename std::iterator_traits<ForwardIterator>::value_type>
ValuesIn(ForwardIterator begin, ForwardIterator end) {
  typedef typename std::iterator_traits<ForwardIterator>::value_type ParamType;
  return internal::ParamGenerator<ParamType>(
      new internal::ValuesInIteratorRangeGenerator<ParamType>(begin, end));
}

template <typename T, size_t N>
internal::ParamGenerator<T> ValuesIn(const T (&array)[N]) {
  return ValuesIn(array, array + N);
}

template <class Container>
internal::ParamGenerator<typename Container::value_type> ValuesIn(
    const Container& container) {
  return ValuesIn(container.begin(), container.end());
}

template <typename... T>
internal::ValueArray<T...> Values(T... v) {
  return internal::ValueArray<T...>(std::move(v)...);
}

inline internal::ParamGenerator<bool> Bool() { return Values(false, true); }

template <typename... Generator>
internal::CartesianProductHolder<Generator...> Combine(const Generator&... g) {
  return internal::CartesianProductHolder<Generator...>(g...);
}

template <typename T>
internal::ParamConverterGenerator<T> ConvertGenerator(
    internal::ParamGenerator<T> gen) {
  return internal::ParamConverterGenerator<T>(gen);
}

#define TEST_P(test_suite_name, test_name)                                     \
  class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                     \
      : public test_suite_name,                                                \
        private ::testing::internal::GTestNonCopyable {                        \
   public:                                                                     \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() {}                    \
    void TestBody() override;                                                  \
                                                                               \
   private:                                                                    \
    static int AddToRegistry() {                                               \
      ::testing::UnitTest::GetInstance()                                       \
          ->parameterized_test_registry()                                      \
          .GetTestSuitePatternHolder<test_suite_name>(                         \
              GTEST_STRINGIFY_(test_suite_name),                               \
              ::testing::internal::CodeLocation(__FILE__, __LINE__))           \
          ->AddTestPattern(                                                    \
              GTEST_STRINGIFY_(test_suite_name), GTEST_STRINGIFY_(test_name),  \
              new ::testing::internal::TestMetaFactory<GTEST_TEST_CLASS_NAME_( \
                  test_suite_name, test_name)>(),                              \
              ::testing::internal::CodeLocation(__FILE__, __LINE__));          \
      return 0;                                                                \
    }                                                                          \
    [[maybe_unused]] static int gtest_registering_dummy_;                      \
  };                                                                           \
  int GTEST_TEST_CLASS_NAME_(test_suite_name,                                  \
                             test_name)::gtest_registering_dummy_ =            \
      GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::AddToRegistry();     \
  void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody()

#define GTEST_EXPAND_(arg) arg
#define GTEST_GET_FIRST_(first, ...) first
#define GTEST_GET_SECOND_(first, second, ...) second

#define INSTANTIATE_TEST_SUITE_P(prefix, test_suite_name, ...)                \
  static ::testing::internal::ParamGenerator<test_suite_name::ParamType>      \
      gtest_##prefix##test_suite_name##_EvalGenerator_() {                    \
    return GTEST_EXPAND_(GTEST_GET_FIRST_(__VA_ARGS__, DUMMY_PARAM_));        \
  }                                                                           \
  static ::std::string gtest_##prefix##test_suite_name##_EvalGenerateName_(   \
      const ::testing::TestParamInfo<test_suite_name::ParamType>& info) {     \
    if (::testing::internal::AlwaysFalse()) {                                 \
      ::testing::internal::TestNotEmpty(GTEST_EXPAND_(GTEST_GET_SECOND_(      \
          __VA_ARGS__,                                                        \
          ::testing::internal::DefaultParamName<test_suite_name::ParamType>,  \
          DUMMY_PARAM_)));                                                    \
      auto t = std::make_tuple(__VA_ARGS__);                                  \
      static_assert(std::tuple_size<decltype(t)>::value <= 2,                 \
                    "Too Many Args!");                                        \
    }                                                                         \
    return ((GTEST_EXPAND_(GTEST_GET_SECOND_(                                 \
        __VA_ARGS__,                                                          \
        ::testing::internal::DefaultParamName<test_suite_name::ParamType>,    \
        DUMMY_PARAM_))))(info);                                               \
  }                                                                           \
  [[maybe_unused]] static int gtest_##prefix##test_suite_name##_dummy_ =      \
      ::testing::UnitTest::GetInstance()                                      \
          ->parameterized_test_registry()                                     \
          .GetTestSuitePatternHolder<test_suite_name>(                        \
              GTEST_STRINGIFY_(test_suite_name),                              \
              ::testing::internal::CodeLocation(__FILE__, __LINE__))          \
          ->AddTestSuiteInstantiation(                                        \
              GTEST_STRINGIFY_(prefix),                                       \
              &gtest_##prefix##test_suite_name##_EvalGenerator_,              \
              &gtest_##prefix##test_suite_name##_EvalGenerateName_, __FILE__, \
              __LINE__)

#define GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(T)                  \
  namespace gtest_do_not_use_outside_namespace_scope {}                   \
  static const ::testing::internal::MarkAsIgnored gtest_allow_ignore_##T( \
      GTEST_STRINGIFY_(T))

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
#define INSTANTIATE_TEST_CASE_P                                            \
  static_assert(::testing::internal::InstantiateTestCase_P_IsDeprecated(), \
                "");                                                       \
  INSTANTIATE_TEST_SUITE_P
#endif

}

#endif
