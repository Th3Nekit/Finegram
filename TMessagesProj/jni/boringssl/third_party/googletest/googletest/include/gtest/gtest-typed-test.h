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

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_TYPED_TEST_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_TYPED_TEST_H_

#if 0

template <typename T>
class FooTest : public testing::Test {
 public:
  ...
  typedef std::list<T> List;
  static T shared_;
  T value_;
};

typedef testing::Types<char, int, unsigned int> MyTypes;
TYPED_TEST_SUITE(FooTest, MyTypes);

TYPED_TEST(FooTest, DoesBlah) {

  TypeParam n = this->value_;

  n += TestFixture::shared_;

  typename TestFixture::List values;
  values.push_back(n);
  ...
}

TYPED_TEST(FooTest, HasPropertyA) { ... }

#endif

#if 0

template <typename T>
class FooTest : public testing::Test {
  ...
};

TYPED_TEST_SUITE_P(FooTest);

TYPED_TEST_P(FooTest, DoesBlah) {

  TypeParam n = 0;
  ...
}

TYPED_TEST_P(FooTest, HasPropertyA) { ... }

REGISTER_TYPED_TEST_SUITE_P(FooTest,
                            DoesBlah, HasPropertyA);

typedef testing::Types<char, int, unsigned int> MyTypes;
INSTANTIATE_TYPED_TEST_SUITE_P(My, FooTest, MyTypes);

#endif

#include "gtest/internal/gtest-internal.h"
#include "gtest/internal/gtest-port.h"
#include "gtest/internal/gtest-type-util.h"

#define GTEST_TYPE_PARAMS_(TestSuiteName) gtest_type_params_##TestSuiteName##_

#define GTEST_NAME_GENERATOR_(TestSuiteName) \
  gtest_type_params_##TestSuiteName##_NameGenerator

#define TYPED_TEST_SUITE(CaseName, Types, ...)                          \
  typedef ::testing::internal::GenerateTypeList<Types>::type            \
      GTEST_TYPE_PARAMS_(CaseName);                                     \
  typedef ::testing::internal::NameGeneratorSelector<__VA_ARGS__>::type \
  GTEST_NAME_GENERATOR_(CaseName)

#define TYPED_TEST(CaseName, TestName)                                        \
  static_assert(sizeof(GTEST_STRINGIFY_(TestName)) > 1,                       \
                "test-name must not be empty");                               \
  template <typename gtest_TypeParam_>                                        \
  class GTEST_TEST_CLASS_NAME_(CaseName, TestName)                            \
      : public CaseName<gtest_TypeParam_> {                                   \
   private:                                                                   \
    typedef CaseName<gtest_TypeParam_> TestFixture;                           \
    typedef gtest_TypeParam_ TypeParam;                                       \
    void TestBody() override;                                                 \
  };                                                                          \
  [[maybe_unused]] static bool gtest_##CaseName##_##TestName##_registered_ =  \
      ::testing::internal::TypeParameterizedTest<                             \
          CaseName,                                                           \
          ::testing::internal::TemplateSel<GTEST_TEST_CLASS_NAME_(CaseName,   \
                                                                  TestName)>, \
          GTEST_TYPE_PARAMS_(                                                 \
              CaseName)>::Register("",                                        \
                                   ::testing::internal::CodeLocation(         \
                                       __FILE__, __LINE__),                   \
                                   GTEST_STRINGIFY_(CaseName),                \
                                   GTEST_STRINGIFY_(TestName), 0,             \
                                   ::testing::internal::GenerateNames<        \
                                       GTEST_NAME_GENERATOR_(CaseName),       \
                                       GTEST_TYPE_PARAMS_(CaseName)>());      \
  template <typename gtest_TypeParam_>                                        \
  void GTEST_TEST_CLASS_NAME_(CaseName,                                       \
                              TestName)<gtest_TypeParam_>::TestBody()

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
#define TYPED_TEST_CASE                                                \
  static_assert(::testing::internal::TypedTestCaseIsDeprecated(), ""); \
  TYPED_TEST_SUITE
#endif

#define GTEST_SUITE_NAMESPACE_(TestSuiteName) gtest_suite_##TestSuiteName##_

#define GTEST_TYPED_TEST_SUITE_P_STATE_(TestSuiteName) \
  gtest_typed_test_suite_p_state_##TestSuiteName##_

#define GTEST_REGISTERED_TEST_NAMES_(TestSuiteName) \
  gtest_registered_test_names_##TestSuiteName##_

#define TYPED_TEST_SUITE_P(SuiteName)              \
  static ::testing::internal::TypedTestSuitePState \
  GTEST_TYPED_TEST_SUITE_P_STATE_(SuiteName)

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
#define TYPED_TEST_CASE_P                                                 \
  static_assert(::testing::internal::TypedTestCase_P_IsDeprecated(), ""); \
  TYPED_TEST_SUITE_P
#endif

#define TYPED_TEST_P(SuiteName, TestName)                     \
  namespace GTEST_SUITE_NAMESPACE_(SuiteName) {               \
  template <typename gtest_TypeParam_>                        \
  class TestName : public SuiteName<gtest_TypeParam_> {       \
   private:                                                   \
    typedef SuiteName<gtest_TypeParam_> TestFixture;          \
    typedef gtest_TypeParam_ TypeParam;                       \
    void TestBody() override;                                 \
  };                                                          \
  [[maybe_unused]] static bool gtest_##TestName##_defined_ =  \
      GTEST_TYPED_TEST_SUITE_P_STATE_(SuiteName).AddTestName( \
          __FILE__, __LINE__, GTEST_STRINGIFY_(SuiteName),    \
          GTEST_STRINGIFY_(TestName));                        \
  }                                                           \
  template <typename gtest_TypeParam_>                        \
  void GTEST_SUITE_NAMESPACE_(                                \
      SuiteName)::TestName<gtest_TypeParam_>::TestBody()

#define REGISTER_TYPED_TEST_SUITE_P(SuiteName, ...)                         \
  namespace GTEST_SUITE_NAMESPACE_(SuiteName) {                             \
  typedef ::testing::internal::Templates<__VA_ARGS__> gtest_AllTests_;      \
  }                                                                         \
  [[maybe_unused]] static const char* const GTEST_REGISTERED_TEST_NAMES_(   \
      SuiteName) =                                                          \
      GTEST_TYPED_TEST_SUITE_P_STATE_(SuiteName).VerifyRegisteredTestNames( \
          GTEST_STRINGIFY_(SuiteName), __FILE__, __LINE__, #__VA_ARGS__)

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
#define REGISTER_TYPED_TEST_CASE_P                                           \
  static_assert(::testing::internal::RegisterTypedTestCase_P_IsDeprecated(), \
                "");                                                         \
  REGISTER_TYPED_TEST_SUITE_P
#endif

#define INSTANTIATE_TYPED_TEST_SUITE_P(Prefix, SuiteName, Types, ...)     \
  static_assert(sizeof(GTEST_STRINGIFY_(Prefix)) > 1,                     \
                "test-suit-prefix must not be empty");                    \
  [[maybe_unused]] static bool gtest_##Prefix##_##SuiteName =             \
      ::testing::internal::TypeParameterizedTestSuite<                    \
          SuiteName, GTEST_SUITE_NAMESPACE_(SuiteName)::gtest_AllTests_,  \
          ::testing::internal::GenerateTypeList<Types>::type>::           \
          Register(GTEST_STRINGIFY_(Prefix),                              \
                   ::testing::internal::CodeLocation(__FILE__, __LINE__), \
                   &GTEST_TYPED_TEST_SUITE_P_STATE_(SuiteName),           \
                   GTEST_STRINGIFY_(SuiteName),                           \
                   GTEST_REGISTERED_TEST_NAMES_(SuiteName),               \
                   ::testing::internal::GenerateNames<                    \
                       ::testing::internal::NameGeneratorSelector<        \
                           __VA_ARGS__>::type,                            \
                       ::testing::internal::GenerateTypeList<Types>::type>())

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
#define INSTANTIATE_TYPED_TEST_CASE_P                                      \
  static_assert(                                                           \
      ::testing::internal::InstantiateTypedTestCase_P_IsDeprecated(), ""); \
  INSTANTIATE_TYPED_TEST_SUITE_P
#endif

#endif
