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

// The Google C++ Testing and Mocking Framework (Google Test)
//
// This header file declares functions and macros used internally by
// Google Test.  They are subject to change without notice.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_INTERNAL_H_
#define GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_INTERNAL_H_

#include "gtest/internal/gtest-port.h"

#ifdef GTEST_OS_LINUX
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#if GTEST_HAS_EXCEPTIONS
#include <stdexcept>
#endif

#include <ctype.h>
#include <float.h>
#include <string.h>

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gtest/gtest-message.h"
#include "gtest/internal/gtest-filepath.h"
#include "gtest/internal/gtest-string.h"
#include "gtest/internal/gtest-type-util.h"

#define GTEST_CONCAT_TOKEN_(foo, bar) GTEST_CONCAT_TOKEN_IMPL_(foo, bar)
#define GTEST_CONCAT_TOKEN_IMPL_(foo, bar) foo##bar

#define GTEST_STRINGIFY_HELPER_(name, ...) #name
#define GTEST_STRINGIFY_(...) GTEST_STRINGIFY_HELPER_(__VA_ARGS__, )

namespace proto2 {
class MessageLite;
}

namespace testing {

class AssertionResult;
class Message;
class Test;
class TestInfo;
class TestPartResult;
class UnitTest;

template <typename T>
::std::string PrintToString(const T& value);

namespace internal {

struct TraceInfo;
class TestInfoImpl;
class UnitTestImpl;

GTEST_API_ extern const char kStackTraceMarker[];

class IgnoredValue {
  struct Sink {};

 public:

  template <typename T,
            typename std::enable_if<!std::is_convertible<T, Sink>::value,
                                    int>::type = 0>
  IgnoredValue(const T&              ) {}
};

GTEST_API_ std::string AppendUserMessage(const std::string& gtest_msg,
                                         const Message& user_msg);

#if GTEST_HAS_EXCEPTIONS

GTEST_DISABLE_MSC_WARNINGS_PUSH_(
    4275                                                                       )

class GTEST_API_ GoogleTestFailureException : public ::std::runtime_error {
 public:
  explicit GoogleTestFailureException(const TestPartResult& failure);
};

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif

namespace edit_distance {

enum EditType { kMatch, kAdd, kRemove, kReplace };
GTEST_API_ std::vector<EditType> CalculateOptimalEdits(
    const std::vector<size_t>& left, const std::vector<size_t>& right);

GTEST_API_ std::vector<EditType> CalculateOptimalEdits(
    const std::vector<std::string>& left,
    const std::vector<std::string>& right);

GTEST_API_ std::string CreateUnifiedDiff(const std::vector<std::string>& left,
                                         const std::vector<std::string>& right,
                                         size_t context = 2);

}

GTEST_API_ AssertionResult EqFailure(const char* expected_expression,
                                     const char* actual_expression,
                                     const std::string& expected_value,
                                     const std::string& actual_value,
                                     bool ignoring_case);

GTEST_API_ std::string GetBoolAssertionFailureMessage(
    const AssertionResult& assertion_result, const char* expression_text,
    const char* actual_predicate_value, const char* expected_predicate_value);

template <typename RawType>
class FloatingPoint {
 public:

  typedef typename TypeWithSize<sizeof(RawType)>::UInt Bits;

  static const size_t kBitCount = 8 * sizeof(RawType);

  static const size_t kFractionBitCount =
      std::numeric_limits<RawType>::digits - 1;

  static const size_t kExponentBitCount = kBitCount - 1 - kFractionBitCount;

  static const Bits kSignBitMask = static_cast<Bits>(1) << (kBitCount - 1);

  static const Bits kFractionBitMask = ~static_cast<Bits>(0) >>
                                       (kExponentBitCount + 1);

  static const Bits kExponentBitMask = ~(kSignBitMask | kFractionBitMask);

  static const uint32_t kMaxUlps = 4;

  explicit FloatingPoint(const RawType& x) { u_.value_ = x; }

  static RawType ReinterpretBits(const Bits bits) {
    FloatingPoint fp(0);
    fp.u_.bits_ = bits;
    return fp.u_.value_;
  }

  static RawType Infinity() { return ReinterpretBits(kExponentBitMask); }

  const Bits& bits() const { return u_.bits_; }

  Bits exponent_bits() const { return kExponentBitMask & u_.bits_; }

  Bits fraction_bits() const { return kFractionBitMask & u_.bits_; }

  Bits sign_bit() const { return kSignBitMask & u_.bits_; }

  bool is_nan() const {

    return (exponent_bits() == kExponentBitMask) && (fraction_bits() != 0);
  }

  bool AlmostEquals(const FloatingPoint& rhs) const {

    if (is_nan() || rhs.is_nan()) return false;

    return DistanceBetweenSignAndMagnitudeNumbers(u_.bits_, rhs.u_.bits_) <=
           kMaxUlps;
  }

 private:

  union FloatingPointUnion {
    RawType value_;
    Bits bits_;
  };

  static Bits SignAndMagnitudeToBiased(const Bits& sam) {
    if (kSignBitMask & sam) {

      return ~sam + 1;
    } else {

      return kSignBitMask | sam;
    }
  }

  static Bits DistanceBetweenSignAndMagnitudeNumbers(const Bits& sam1,
                                                     const Bits& sam2) {
    const Bits biased1 = SignAndMagnitudeToBiased(sam1);
    const Bits biased2 = SignAndMagnitudeToBiased(sam2);
    return (biased1 >= biased2) ? (biased1 - biased2) : (biased2 - biased1);
  }

  FloatingPointUnion u_;
};

typedef FloatingPoint<float> Float;
typedef FloatingPoint<double> Double;

typedef const void* TypeId;

template <typename T>
class TypeIdHelper {
 public:

  static bool dummy_;
};

template <typename T>
bool TypeIdHelper<T>::dummy_ = false;

template <typename T>
TypeId GetTypeId() {

  return &(TypeIdHelper<T>::dummy_);
}

GTEST_API_ TypeId GetTestTypeId();

class TestFactoryBase {
 public:
  virtual ~TestFactoryBase() = default;

  virtual Test* CreateTest() = 0;

 protected:
  TestFactoryBase() {}

 private:
  TestFactoryBase(const TestFactoryBase&) = delete;
  TestFactoryBase& operator=(const TestFactoryBase&) = delete;
};

template <class TestClass>
class TestFactoryImpl : public TestFactoryBase {
 public:
  Test* CreateTest() override { return new TestClass; }
};

#ifdef GTEST_OS_WINDOWS

GTEST_API_ AssertionResult IsHRESULTSuccess(const char* expr,
                                            long hr);
GTEST_API_ AssertionResult IsHRESULTFailure(const char* expr,
                                            long hr);

#endif

using SetUpTestSuiteFunc = void (*)();
using TearDownTestSuiteFunc = void (*)();

struct CodeLocation {
  CodeLocation(std::string a_file, int a_line)
      : file(std::move(a_file)), line(a_line) {}

  std::string file;
  int line;
};

using SetUpTearDownSuiteFuncType = void (*)();

inline SetUpTearDownSuiteFuncType GetNotDefaultOrNull(
    SetUpTearDownSuiteFuncType a, SetUpTearDownSuiteFuncType def) {
  return a == def ? nullptr : a;
}

template <typename T>

struct SuiteApiResolver : T {

  using Test =
      typename std::conditional<sizeof(T) != 0, ::testing::Test, void>::type;

  static SetUpTearDownSuiteFuncType GetSetUpCaseOrSuite(const char* filename,
                                                        int line_num) {
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
    SetUpTearDownSuiteFuncType test_case_fp =
        GetNotDefaultOrNull(&T::SetUpTestCase, &Test::SetUpTestCase);
    SetUpTearDownSuiteFuncType test_suite_fp =
        GetNotDefaultOrNull(&T::SetUpTestSuite, &Test::SetUpTestSuite);

    GTEST_CHECK_(!test_case_fp || !test_suite_fp)
        << "Test can not provide both SetUpTestSuite and SetUpTestCase, please "
           "make sure there is only one present at "
        << filename << ":" << line_num;

    return test_case_fp != nullptr ? test_case_fp : test_suite_fp;
#else
    (void)(filename);
    (void)(line_num);
    return &T::SetUpTestSuite;
#endif
  }

  static SetUpTearDownSuiteFuncType GetTearDownCaseOrSuite(const char* filename,
                                                           int line_num) {
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
    SetUpTearDownSuiteFuncType test_case_fp =
        GetNotDefaultOrNull(&T::TearDownTestCase, &Test::TearDownTestCase);
    SetUpTearDownSuiteFuncType test_suite_fp =
        GetNotDefaultOrNull(&T::TearDownTestSuite, &Test::TearDownTestSuite);

    GTEST_CHECK_(!test_case_fp || !test_suite_fp)
        << "Test can not provide both TearDownTestSuite and TearDownTestCase,"
           " please make sure there is only one present at"
        << filename << ":" << line_num;

    return test_case_fp != nullptr ? test_case_fp : test_suite_fp;
#else
    (void)(filename);
    (void)(line_num);
    return &T::TearDownTestSuite;
#endif
  }
};

GTEST_API_ TestInfo* MakeAndRegisterTestInfo(
    std::string test_suite_name, const char* name, const char* type_param,
    const char* value_param, CodeLocation code_location,
    TypeId fixture_class_id, SetUpTestSuiteFunc set_up_tc,
    TearDownTestSuiteFunc tear_down_tc, TestFactoryBase* factory);

GTEST_API_ bool SkipPrefix(const char* prefix, const char** pstr);

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

class GTEST_API_ TypedTestSuitePState {
 public:
  TypedTestSuitePState() : registered_(false) {}

  bool AddTestName(const char* file, int line, const char* case_name,
                   const char* test_name) {
    if (registered_) {
      fprintf(stderr,
              "%s Test %s must be defined before "
              "REGISTER_TYPED_TEST_SUITE_P(%s, ...).\n",
              FormatFileLocation(file, line).c_str(), test_name, case_name);
      fflush(stderr);
      posix::Abort();
    }
    registered_tests_.emplace(test_name, CodeLocation(file, line));
    return true;
  }

  bool TestExists(const std::string& test_name) const {
    return registered_tests_.count(test_name) > 0;
  }

  const CodeLocation& GetCodeLocation(const std::string& test_name) const {
    RegisteredTestsMap::const_iterator it = registered_tests_.find(test_name);
    GTEST_CHECK_(it != registered_tests_.end());
    return it->second;
  }

  const char* VerifyRegisteredTestNames(const char* test_suite_name,
                                        const char* file, int line,
                                        const char* registered_tests);

 private:
  typedef ::std::map<std::string, CodeLocation, std::less<>> RegisteredTestsMap;

  bool registered_;
  RegisteredTestsMap registered_tests_;
};

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
using TypedTestCasePState = TypedTestSuitePState;
#endif

GTEST_DISABLE_MSC_WARNINGS_POP_()

inline const char* SkipComma(const char* str) {
  const char* comma = strchr(str, ',');
  if (comma == nullptr) {
    return nullptr;
  }
  while (IsSpace(*(++comma))) {
  }
  return comma;
}

inline std::string GetPrefixUntilComma(const char* str) {
  const char* comma = strchr(str, ',');
  return comma == nullptr ? str : std::string(str, comma);
}

void SplitString(const ::std::string& str, char delimiter,
                 ::std::vector<::std::string>* dest);

struct DefaultNameGenerator {
  template <typename T>
  static std::string GetName(int i) {
    return StreamableToString(i);
  }
};

template <typename Provided = DefaultNameGenerator>
struct NameGeneratorSelector {
  typedef Provided type;
};

template <typename NameGenerator>
void GenerateNamesRecursively(internal::None, std::vector<std::string>*, int) {}

template <typename NameGenerator, typename Types>
void GenerateNamesRecursively(Types, std::vector<std::string>* result, int i) {
  result->push_back(NameGenerator::template GetName<typename Types::Head>(i));
  GenerateNamesRecursively<NameGenerator>(typename Types::Tail(), result,
                                          i + 1);
}

template <typename NameGenerator, typename Types>
std::vector<std::string> GenerateNames() {
  std::vector<std::string> result;
  GenerateNamesRecursively<NameGenerator>(Types(), &result, 0);
  return result;
}

template <GTEST_TEMPLATE_ Fixture, class TestSel, typename Types>
class TypeParameterizedTest {
 public:

  static bool Register(const char* prefix, CodeLocation code_location,
                       const char* case_name, const char* test_names, int index,
                       const std::vector<std::string>& type_names =
                           GenerateNames<DefaultNameGenerator, Types>()) {
    typedef typename Types::Head Type;
    typedef Fixture<Type> FixtureClass;
    typedef typename GTEST_BIND_(TestSel, Type) TestClass;

    MakeAndRegisterTestInfo(
        (std::string(prefix) + (prefix[0] == '\0' ? "" : "/") + case_name +
         "/" + type_names[static_cast<size_t>(index)]),
        StripTrailingSpaces(GetPrefixUntilComma(test_names)).c_str(),
        GetTypeName<Type>().c_str(),
        nullptr,
        code_location, GetTypeId<FixtureClass>(),
        SuiteApiResolver<TestClass>::GetSetUpCaseOrSuite(
            code_location.file.c_str(), code_location.line),
        SuiteApiResolver<TestClass>::GetTearDownCaseOrSuite(
            code_location.file.c_str(), code_location.line),
        new TestFactoryImpl<TestClass>);

    return TypeParameterizedTest<Fixture, TestSel, typename Types::Tail>::
        Register(prefix, std::move(code_location), case_name, test_names,
                 index + 1, type_names);
  }
};

template <GTEST_TEMPLATE_ Fixture, class TestSel>
class TypeParameterizedTest<Fixture, TestSel, internal::None> {
 public:
  static bool Register(const char*           , CodeLocation,
                       const char*              , const char*               ,
                       int          ,
                       const std::vector<std::string>& =
                           std::vector<std::string>()               ) {
    return true;
  }
};

GTEST_API_ void RegisterTypeParameterizedTestSuite(const char* test_suite_name,
                                                   CodeLocation code_location);
GTEST_API_ void RegisterTypeParameterizedTestSuiteInstantiation(
    const char* case_name);

template <GTEST_TEMPLATE_ Fixture, typename Tests, typename Types>
class TypeParameterizedTestSuite {
 public:
  static bool Register(const char* prefix, CodeLocation code_location,
                       const TypedTestSuitePState* state, const char* case_name,
                       const char* test_names,
                       const std::vector<std::string>& type_names =
                           GenerateNames<DefaultNameGenerator, Types>()) {
    RegisterTypeParameterizedTestSuiteInstantiation(case_name);
    std::string test_name =
        StripTrailingSpaces(GetPrefixUntilComma(test_names));
    if (!state->TestExists(test_name)) {
      fprintf(stderr, "Failed to get code location for test %s.%s at %s.",
              case_name, test_name.c_str(),
              FormatFileLocation(code_location.file.c_str(), code_location.line)
                  .c_str());
      fflush(stderr);
      posix::Abort();
    }
    const CodeLocation& test_location = state->GetCodeLocation(test_name);

    typedef typename Tests::Head Head;

    TypeParameterizedTest<Fixture, Head, Types>::Register(
        prefix, test_location, case_name, test_names, 0, type_names);

    return TypeParameterizedTestSuite<Fixture, typename Tests::Tail,
                                      Types>::Register(prefix,
                                                       std::move(code_location),
                                                       state, case_name,
                                                       SkipComma(test_names),
                                                       type_names);
  }
};

template <GTEST_TEMPLATE_ Fixture, typename Types>
class TypeParameterizedTestSuite<Fixture, internal::None, Types> {
 public:
  static bool Register(const char*           , const CodeLocation&,
                       const TypedTestSuitePState*          ,
                       const char*              , const char*               ,
                       const std::vector<std::string>& =
                           std::vector<std::string>()               ) {
    return true;
  }
};

GTEST_API_ std::string GetCurrentOsStackTraceExceptTop(int skip_count);

GTEST_API_ bool AlwaysTrue();

inline bool AlwaysFalse() { return !AlwaysTrue(); }

struct GTEST_API_ ConstCharPtr {
  ConstCharPtr(const char* str) : value(str) {}
  operator bool() const { return true; }
  const char* value;
};

struct TrueWithString {
  TrueWithString() = default;
  explicit TrueWithString(const char* str) : value(str) {}
  explicit TrueWithString(const std::string& str) : value(str) {}
  explicit operator bool() const { return true; }
  std::string value;
};

class GTEST_API_ Random {
 public:
  static const uint32_t kMaxRange = 1u << 31;

  explicit Random(uint32_t seed) : state_(seed) {}

  void Reseed(uint32_t seed) { state_ = seed; }

  uint32_t Generate(uint32_t range);

 private:
  uint32_t state_;
  Random(const Random&) = delete;
  Random& operator=(const Random&) = delete;
};

#define GTEST_REMOVE_REFERENCE_AND_CONST_(T) \
  typename std::remove_const<typename std::remove_reference<T>::type>::type

template <typename T>
class HasDebugStringAndShortDebugString {
 private:
  template <typename C>
  static auto CheckDebugString(C*) -> typename std::is_same<
      std::string, decltype(std::declval<const C>().DebugString())>::type;
  template <typename>
  static std::false_type CheckDebugString(...);

  template <typename C>
  static auto CheckShortDebugString(C*) -> typename std::is_same<
      std::string, decltype(std::declval<const C>().ShortDebugString())>::type;
  template <typename>
  static std::false_type CheckShortDebugString(...);

  using HasDebugStringType = decltype(CheckDebugString<T>(nullptr));
  using HasShortDebugStringType = decltype(CheckShortDebugString<T>(nullptr));

 public:
  static constexpr bool value =
      HasDebugStringType::value && HasShortDebugStringType::value;
};

typedef int IsContainer;
template <class C,
          class Iterator = decltype(::std::declval<const C&>().begin()),
          class = decltype(::std::declval<const C&>().end()),
          class = decltype(++::std::declval<Iterator&>()),
          class = decltype(*::std::declval<Iterator>()),
          class = typename C::const_iterator>
IsContainer IsContainerTest(int            ) {
  return 0;
}

typedef char IsNotContainer;
template <class C>
IsNotContainer IsContainerTest(long            ) {
  return '\0';
}

template <typename T>
struct IsHashTable {
 private:
  template <typename U>
  static char test(typename U::hasher*, typename U::reverse_iterator*);
  template <typename U>
  static int test(typename U::hasher*, ...);
  template <typename U>
  static char test(...);

 public:
  static const bool value = sizeof(test<T>(nullptr, nullptr)) == sizeof(int);
};

template <typename T>
const bool IsHashTable<T>::value;

template <typename C,
          bool = sizeof(IsContainerTest<C>(0)) == sizeof(IsContainer)>
struct IsRecursiveContainerImpl;

template <typename C>
struct IsRecursiveContainerImpl<C, false> : public std::false_type {};

template <typename C>
struct IsRecursiveContainerImpl<C, true> {
  using value_type = decltype(*std::declval<typename C::const_iterator>());
  using type =
      std::is_same<typename std::remove_const<
                       typename std::remove_reference<value_type>::type>::type,
                   C>;
};

template <typename C>
struct IsRecursiveContainer : public IsRecursiveContainerImpl<C>::type {};

template <typename T, typename U>
bool ArrayEq(const T* lhs, size_t size, const U* rhs);

template <typename T, typename U>
inline bool ArrayEq(const T& lhs, const U& rhs) {
  return lhs == rhs;
}

template <typename T, typename U, size_t N>
inline bool ArrayEq(const T (&lhs)[N], const U (&rhs)[N]) {
  return internal::ArrayEq(lhs, N, rhs);
}

template <typename T, typename U>
bool ArrayEq(const T* lhs, size_t size, const U* rhs) {
  for (size_t i = 0; i != size; i++) {
    if (!internal::ArrayEq(lhs[i], rhs[i])) return false;
  }
  return true;
}

template <typename Iter, typename Element>
Iter ArrayAwareFind(Iter begin, Iter end, const Element& elem) {
  for (Iter it = begin; it != end; ++it) {
    if (internal::ArrayEq(*it, elem)) return it;
  }
  return end;
}

template <typename T, typename U>
void CopyArray(const T* from, size_t size, U* to);

template <typename T, typename U>
inline void CopyArray(const T& from, U* to) {
  *to = from;
}

template <typename T, typename U, size_t N>
inline void CopyArray(const T (&from)[N], U (*to)[N]) {
  internal::CopyArray(from, N, *to);
}

template <typename T, typename U>
void CopyArray(const T* from, size_t size, U* to) {
  for (size_t i = 0; i != size; i++) {
    internal::CopyArray(from[i], to + i);
  }
}

struct RelationToSourceReference {};
struct RelationToSourceCopy {};

template <typename Element>
class NativeArray {
 public:

  typedef Element value_type;
  typedef Element* iterator;
  typedef const Element* const_iterator;

  NativeArray(const Element* array, size_t count, RelationToSourceReference) {
    InitRef(array, count);
  }

  NativeArray(const Element* array, size_t count, RelationToSourceCopy) {
    InitCopy(array, count);
  }

  NativeArray(const NativeArray& rhs) {
    (this->*rhs.clone_)(rhs.array_, rhs.size_);
  }

  ~NativeArray() {
    if (clone_ != &NativeArray::InitRef) delete[] array_;
  }

  size_t size() const { return size_; }
  const_iterator begin() const { return array_; }
  const_iterator end() const { return array_ + size_; }
  bool operator==(const NativeArray& rhs) const {
    return size() == rhs.size() && ArrayEq(begin(), size(), rhs.begin());
  }

 private:
  static_assert(!std::is_const<Element>::value, "Type must not be const");
  static_assert(!std::is_reference<Element>::value,
                "Type must not be a reference");

  void InitCopy(const Element* array, size_t a_size) {
    Element* const copy = new Element[a_size];
    CopyArray(array, a_size, copy);
    array_ = copy;
    size_ = a_size;
    clone_ = &NativeArray::InitCopy;
  }

  void InitRef(const Element* array, size_t a_size) {
    array_ = array;
    size_ = a_size;
    clone_ = &NativeArray::InitRef;
  }

  const Element* array_;
  size_t size_;
  void (NativeArray::*clone_)(const Element*, size_t);
};

template <size_t>
struct Ignore {
  Ignore(...);
};

template <typename>
struct ElemFromListImpl;
template <size_t... I>
struct ElemFromListImpl<std::index_sequence<I...>> {

  template <typename R>
  static R Apply(Ignore<0 * I>..., R (*)(), ...);
};

template <size_t N, typename... T>
struct ElemFromList {
  using type = decltype(ElemFromListImpl<std::make_index_sequence<N>>::Apply(
      static_cast<T (*)()>(nullptr)...));
};

struct FlatTupleConstructTag {};

template <typename... T>
class FlatTuple;

template <typename Derived, size_t I>
struct FlatTupleElemBase;

template <typename... T, size_t I>
struct FlatTupleElemBase<FlatTuple<T...>, I> {
  using value_type = typename ElemFromList<I, T...>::type;
  FlatTupleElemBase() = default;
  template <typename Arg>
  explicit FlatTupleElemBase(FlatTupleConstructTag, Arg&& t)
      : value(std::forward<Arg>(t)) {}
  value_type value;
};

template <typename Derived, typename Idx>
struct FlatTupleBase;

template <size_t... Idx, typename... T>
struct FlatTupleBase<FlatTuple<T...>, std::index_sequence<Idx...>>
    : FlatTupleElemBase<FlatTuple<T...>, Idx>... {
  using Indices = std::index_sequence<Idx...>;
  FlatTupleBase() = default;
  template <typename... Args>
  explicit FlatTupleBase(FlatTupleConstructTag, Args&&... args)
      : FlatTupleElemBase<FlatTuple<T...>, Idx>(FlatTupleConstructTag{},
                                                std::forward<Args>(args))... {}

  template <size_t I>
  const typename ElemFromList<I, T...>::type& Get() const {
    return FlatTupleElemBase<FlatTuple<T...>, I>::value;
  }

  template <size_t I>
  typename ElemFromList<I, T...>::type& Get() {
    return FlatTupleElemBase<FlatTuple<T...>, I>::value;
  }

  template <typename F>
  auto Apply(F&& f) -> decltype(std::forward<F>(f)(this->Get<Idx>()...)) {
    return std::forward<F>(f)(Get<Idx>()...);
  }

  template <typename F>
  auto Apply(F&& f) const -> decltype(std::forward<F>(f)(this->Get<Idx>()...)) {
    return std::forward<F>(f)(Get<Idx>()...);
  }
};

template <typename... T>
class FlatTuple
    : private FlatTupleBase<FlatTuple<T...>,
                            std::make_index_sequence<sizeof...(T)>> {
  using Indices =
      typename FlatTupleBase<FlatTuple<T...>,
                             std::make_index_sequence<sizeof...(T)>>::Indices;

 public:
  FlatTuple() = default;
  template <typename... Args>
  explicit FlatTuple(FlatTupleConstructTag tag, Args&&... args)
      : FlatTuple::FlatTupleBase(tag, std::forward<Args>(args)...) {}

  using FlatTuple::FlatTupleBase::Apply;
  using FlatTuple::FlatTupleBase::Get;
};

[[deprecated(
    "INSTANTIATE_TEST_CASE_P is deprecated, please use "
    "INSTANTIATE_TEST_SUITE_P")]]
constexpr bool InstantiateTestCase_P_IsDeprecated() {
  return true;
}

[[deprecated(
    "TYPED_TEST_CASE_P is deprecated, please use "
    "TYPED_TEST_SUITE_P")]]
constexpr bool TypedTestCase_P_IsDeprecated() {
  return true;
}

[[deprecated(
    "TYPED_TEST_CASE is deprecated, please use "
    "TYPED_TEST_SUITE")]]
constexpr bool TypedTestCaseIsDeprecated() {
  return true;
}

[[deprecated(
    "REGISTER_TYPED_TEST_CASE_P is deprecated, please use "
    "REGISTER_TYPED_TEST_SUITE_P")]]
constexpr bool RegisterTypedTestCase_P_IsDeprecated() {
  return true;
}

[[deprecated(
    "INSTANTIATE_TYPED_TEST_CASE_P is deprecated, please use "
    "INSTANTIATE_TYPED_TEST_SUITE_P")]]
constexpr bool InstantiateTypedTestCase_P_IsDeprecated() {
  return true;
}

}
}

namespace std {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmismatched-tags"
#endif
template <typename... Ts>
struct tuple_size<testing::internal::FlatTuple<Ts...>>
    : std::integral_constant<size_t, sizeof...(Ts)> {};
#ifdef __clang__
#pragma clang diagnostic pop
#endif
}

#define GTEST_MESSAGE_AT_(file, line, message, result_type)             \
  ::testing::internal::AssertHelper(result_type, file, line, message) = \
      ::testing::Message()

#define GTEST_MESSAGE_(message, result_type) \
  GTEST_MESSAGE_AT_(__FILE__, __LINE__, message, result_type)

#define GTEST_FATAL_FAILURE_(message) \
  return GTEST_MESSAGE_(message, ::testing::TestPartResult::kFatalFailure)

#define GTEST_NONFATAL_FAILURE_(message) \
  GTEST_MESSAGE_(message, ::testing::TestPartResult::kNonFatalFailure)

#define GTEST_SUCCESS_(message) \
  GTEST_MESSAGE_(message, ::testing::TestPartResult::kSuccess)

#define GTEST_SKIP_(message) \
  return GTEST_MESSAGE_(message, ::testing::TestPartResult::kSkip)

#define GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement) \
  if (::testing::internal::AlwaysTrue()) {                        \
    statement;                                                    \
  } else                                                          \
    static_assert(true, "")                                                

#if GTEST_HAS_EXCEPTIONS

namespace testing {
namespace internal {

class NeverThrown {
 public:
  const char* what() const noexcept {
    return "this exception should never be thrown";
  }
};

}
}

#if GTEST_HAS_RTTI

#define GTEST_EXCEPTION_TYPE_(e) ::testing::internal::GetTypeName(typeid(e))

#else

#define GTEST_EXCEPTION_TYPE_(e) \
  std::string { "an std::exception-derived error" }

#endif

#define GTEST_TEST_THROW_CATCH_STD_EXCEPTION_(statement, expected_exception)   \
  catch (typename std::conditional<                                            \
         std::is_same<typename std::remove_cv<typename std::remove_reference<  \
                          expected_exception>::type>::type,                    \
                      std::exception>::value,                                  \
         const ::testing::internal::NeverThrown&, const std::exception&>::type \
             e) {                                                              \
    gtest_msg.value = "Expected: " #statement                                  \
                      " throws an exception of type " #expected_exception      \
                      ".\n  Actual: it throws ";                               \
    gtest_msg.value += GTEST_EXCEPTION_TYPE_(e);                               \
    gtest_msg.value += " with description \"";                                 \
    gtest_msg.value += e.what();                                               \
    gtest_msg.value += "\".";                                                  \
    goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__);                \
  }

#else

#define GTEST_TEST_THROW_CATCH_STD_EXCEPTION_(statement, expected_exception)

#endif

#define GTEST_TEST_THROW_(statement, expected_exception, fail)              \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                             \
  if (::testing::internal::TrueWithString gtest_msg{}) {                    \
    bool gtest_caught_expected = false;                                     \
    try {                                                                   \
      GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);            \
    } catch (expected_exception const&) {                                   \
      gtest_caught_expected = true;                                         \
    }                                                                       \
    GTEST_TEST_THROW_CATCH_STD_EXCEPTION_(statement, expected_exception)    \
    catch (...) {                                                           \
      gtest_msg.value = "Expected: " #statement                             \
                        " throws an exception of type " #expected_exception \
                        ".\n  Actual: it throws a different type.";         \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__);           \
    }                                                                       \
    if (!gtest_caught_expected) {                                           \
      gtest_msg.value = "Expected: " #statement                             \
                        " throws an exception of type " #expected_exception \
                        ".\n  Actual: it throws nothing.";                  \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__);           \
    }                                                                       \
  } else                                                                    \
    GTEST_CONCAT_TOKEN_(gtest_label_testthrow_, __LINE__)                   \
        : fail(gtest_msg.value.c_str())

#if GTEST_HAS_EXCEPTIONS

#define GTEST_TEST_NO_THROW_CATCH_STD_EXCEPTION_()                \
  catch (std::exception const& e) {                               \
    gtest_msg.value = "it throws ";                               \
    gtest_msg.value += GTEST_EXCEPTION_TYPE_(e);                  \
    gtest_msg.value += " with description \"";                    \
    gtest_msg.value += e.what();                                  \
    gtest_msg.value += "\".";                                     \
    goto GTEST_CONCAT_TOKEN_(gtest_label_testnothrow_, __LINE__); \
  }

#else

#define GTEST_TEST_NO_THROW_CATCH_STD_EXCEPTION_()

#endif

#define GTEST_TEST_NO_THROW_(statement, fail)                            \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                          \
  if (::testing::internal::TrueWithString gtest_msg{}) {                 \
    try {                                                                \
      GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);         \
    }                                                                    \
    GTEST_TEST_NO_THROW_CATCH_STD_EXCEPTION_()                           \
    catch (...) {                                                        \
      gtest_msg.value = "it throws.";                                    \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testnothrow_, __LINE__);      \
    }                                                                    \
  } else                                                                 \
    GTEST_CONCAT_TOKEN_(gtest_label_testnothrow_, __LINE__)              \
        : fail(("Expected: " #statement " doesn't throw an exception.\n" \
                "  Actual: " +                                           \
                gtest_msg.value)                                         \
                   .c_str())

#define GTEST_TEST_ANY_THROW_(statement, fail)                       \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                      \
  if (::testing::internal::AlwaysTrue()) {                           \
    bool gtest_caught_any = false;                                   \
    try {                                                            \
      GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);     \
    } catch (...) {                                                  \
      gtest_caught_any = true;                                       \
    }                                                                \
    if (!gtest_caught_any) {                                         \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testanythrow_, __LINE__); \
    }                                                                \
  } else                                                             \
    GTEST_CONCAT_TOKEN_(gtest_label_testanythrow_, __LINE__)         \
        : fail("Expected: " #statement                               \
               " throws an exception.\n"                             \
               "  Actual: it doesn't.")

#define GTEST_TEST_BOOLEAN_(expression, text, actual, expected, fail) \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                       \
  if (const ::testing::AssertionResult gtest_ar_ =                    \
          ::testing::AssertionResult(expression))                     \
    ;                                                                 \
  else                                                                \
    fail(::testing::internal::GetBoolAssertionFailureMessage(         \
             gtest_ar_, text, #actual, #expected)                     \
             .c_str())

#define GTEST_TEST_NO_FATAL_FAILURE_(statement, fail)               \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                     \
  if (::testing::internal::AlwaysTrue()) {                          \
    const ::testing::internal::HasNewFatalFailureHelper             \
        gtest_fatal_failure_checker;                                \
    GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);      \
    if (gtest_fatal_failure_checker.has_new_fatal_failure()) {      \
      goto GTEST_CONCAT_TOKEN_(gtest_label_testnofatal_, __LINE__); \
    }                                                               \
  } else                                                            \
    GTEST_CONCAT_TOKEN_(gtest_label_testnofatal_, __LINE__)         \
        : fail("Expected: " #statement                              \
               " doesn't generate new fatal "                       \
               "failures in the current thread.\n"                  \
               "  Actual: it does.")

#define GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) \
  test_suite_name##_##test_name##_Test

#define GTEST_TEST_(test_suite_name, test_name, parent_class, parent_id)       \
  static_assert(sizeof(GTEST_STRINGIFY_(test_suite_name)) > 1,                 \
                "test_suite_name must not be empty");                          \
  static_assert(sizeof(GTEST_STRINGIFY_(test_name)) > 1,                       \
                "test_name must not be empty");                                \
  class GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                     \
      : public parent_class {                                                  \
   public:                                                                     \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() = default;            \
    ~GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)() override = default;  \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                         \
    (const GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &) = delete;     \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) & operator=(            \
        const GTEST_TEST_CLASS_NAME_(test_suite_name,                          \
                                     test_name) &) = delete;                   \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)                         \
    (GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) &&) noexcept = delete; \
    GTEST_TEST_CLASS_NAME_(test_suite_name, test_name) & operator=(            \
        GTEST_TEST_CLASS_NAME_(test_suite_name,                                \
                               test_name) &&) noexcept = delete;               \
                                                                               \
   private:                                                                    \
    void TestBody() override;                                                  \
    [[maybe_unused]] static ::testing::TestInfo* const test_info_;             \
  };                                                                           \
                                                                               \
  ::testing::TestInfo* const GTEST_TEST_CLASS_NAME_(test_suite_name,           \
                                                    test_name)::test_info_ =   \
      ::testing::internal::MakeAndRegisterTestInfo(                            \
          #test_suite_name, #test_name, nullptr, nullptr,                      \
          ::testing::internal::CodeLocation(__FILE__, __LINE__), (parent_id),  \
          ::testing::internal::SuiteApiResolver<                               \
              parent_class>::GetSetUpCaseOrSuite(__FILE__, __LINE__),          \
          ::testing::internal::SuiteApiResolver<                               \
              parent_class>::GetTearDownCaseOrSuite(__FILE__, __LINE__),       \
          new ::testing::internal::TestFactoryImpl<GTEST_TEST_CLASS_NAME_(     \
              test_suite_name, test_name)>);                                   \
  void GTEST_TEST_CLASS_NAME_(test_suite_name, test_name)::TestBody()

#endif
