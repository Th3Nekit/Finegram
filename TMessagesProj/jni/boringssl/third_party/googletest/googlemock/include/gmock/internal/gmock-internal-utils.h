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
// This file defines some utilities useful for implementing Google
// Mock.  They are subject to change without notice, so please DO NOT
// USE THEM IN USER CODE.

// IWYU pragma: private, include "gmock/gmock.h"
// IWYU pragma: friend gmock/.*

#ifndef GOOGLEMOCK_INCLUDE_GMOCK_INTERNAL_GMOCK_INTERNAL_UTILS_H_
#define GOOGLEMOCK_INCLUDE_GMOCK_INTERNAL_GMOCK_INTERNAL_UTILS_H_

#include <stdio.h>

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/internal/gmock-port.h"
#include "gtest/gtest.h"

namespace testing {

template <typename>
class Matcher;

namespace internal {

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4100 4805)

GTEST_API_ std::string JoinAsKeyValueTuple(
    const std::vector<const char*>& names, const Strings& values);

GTEST_API_ std::string ConvertIdentifierNameToWords(const char* id_name);

template <typename Pointer>
inline const typename Pointer::element_type* GetRawPointer(const Pointer& p) {
  return p.get();
}

template <typename Element>
inline const Element* GetRawPointer(const std::reference_wrapper<Element>& r) {
  return &r.get();
}

template <typename Element>
inline Element* GetRawPointer(Element* p) {
  return p;
}

#define GMOCK_INTERNAL_WARNING_PUSH()
#define GMOCK_INTERNAL_WARNING_CLANG(Level, Name)
#define GMOCK_INTERNAL_WARNING_POP()

#if defined(__clang__)
#undef GMOCK_INTERNAL_WARNING_PUSH
#define GMOCK_INTERNAL_WARNING_PUSH() _Pragma("clang diagnostic push")
#undef GMOCK_INTERNAL_WARNING_CLANG
#define GMOCK_INTERNAL_WARNING_CLANG(Level, Warning) \
  _Pragma(GMOCK_PP_INTERNAL_STRINGIZE(clang diagnostic Level Warning))
#undef GMOCK_INTERNAL_WARNING_POP
#define GMOCK_INTERNAL_WARNING_POP() _Pragma("clang diagnostic pop")
#endif

#if defined(_MSC_VER) && !defined(_NATIVE_WCHAR_T_DEFINED)

#else
#define GMOCK_WCHAR_T_IS_NATIVE_ 1
#endif

enum TypeKind { kBool, kInteger, kFloatingPoint, kOther };

template <typename T>
struct KindOf {
  enum { value = kOther };
};

#define GMOCK_DECLARE_KIND_(type, kind) \
  template <>                           \
  struct KindOf<type> {                 \
    enum { value = kind };              \
  }

GMOCK_DECLARE_KIND_(bool, kBool);

GMOCK_DECLARE_KIND_(char, kInteger);
GMOCK_DECLARE_KIND_(signed char, kInteger);
GMOCK_DECLARE_KIND_(unsigned char, kInteger);
GMOCK_DECLARE_KIND_(short, kInteger);
GMOCK_DECLARE_KIND_(unsigned short, kInteger);
GMOCK_DECLARE_KIND_(int, kInteger);
GMOCK_DECLARE_KIND_(unsigned int, kInteger);
GMOCK_DECLARE_KIND_(long, kInteger);
GMOCK_DECLARE_KIND_(unsigned long, kInteger);
GMOCK_DECLARE_KIND_(long long, kInteger);
GMOCK_DECLARE_KIND_(unsigned long long, kInteger);

#if GMOCK_WCHAR_T_IS_NATIVE_
GMOCK_DECLARE_KIND_(wchar_t, kInteger);
#endif

GMOCK_DECLARE_KIND_(float, kFloatingPoint);
GMOCK_DECLARE_KIND_(double, kFloatingPoint);
GMOCK_DECLARE_KIND_(long double, kFloatingPoint);

#undef GMOCK_DECLARE_KIND_

#define GMOCK_KIND_OF_(type)                   \
  static_cast< ::testing::internal::TypeKind>( \
      ::testing::internal::KindOf<type>::value)

template <TypeKind kFromKind, typename From, TypeKind kToKind, typename To>
using LosslessArithmeticConvertibleImpl = std::integral_constant<
    bool,

      (kFromKind == kBool) ? true

    : (kFromKind != kToKind) ? false
    : (kFromKind == kInteger &&

      (((sizeof(From) < sizeof(To)) &&
        !(std::is_signed<From>::value && !std::is_signed<To>::value)) ||

       ((sizeof(From) == sizeof(To)) &&
        (std::is_signed<From>::value == std::is_signed<To>::value)))
       ) ? true

    : (kFromKind == kFloatingPoint && (sizeof(From) <= sizeof(To))) ? true
    : false

    >;

template <typename From, typename To>
using LosslessArithmeticConvertible =
    LosslessArithmeticConvertibleImpl<GMOCK_KIND_OF_(From), From,
                                      GMOCK_KIND_OF_(To), To>;

class FailureReporterInterface {
 public:

  enum FailureType { kNonfatal, kFatal };

  virtual ~FailureReporterInterface() = default;

  virtual void ReportFailure(FailureType type, const char* file, int line,
                             const std::string& message) = 0;
};

GTEST_API_ FailureReporterInterface* GetFailureReporter();

inline void Assert(bool condition, const char* file, int line,
                   const std::string& msg) {
  if (!condition) {
    GetFailureReporter()->ReportFailure(FailureReporterInterface::kFatal, file,
                                        line, msg);
  }
}
inline void Assert(bool condition, const char* file, int line) {
  Assert(condition, file, line, "Assertion failed.");
}

inline void Expect(bool condition, const char* file, int line,
                   const std::string& msg) {
  if (!condition) {
    GetFailureReporter()->ReportFailure(FailureReporterInterface::kNonfatal,
                                        file, line, msg);
  }
}
inline void Expect(bool condition, const char* file, int line) {
  Expect(condition, file, line, "Expectation failed.");
}

enum LogSeverity { kInfo = 0, kWarning = 1 };

const char kInfoVerbosity[] = "info";

const char kWarningVerbosity[] = "warning";

const char kErrorVerbosity[] = "error";

GTEST_API_ bool LogIsVisible(LogSeverity severity);

GTEST_API_ void Log(LogSeverity severity, const std::string& message,
                    int stack_frames_to_skip);

class WithoutMatchers {
 private:
  WithoutMatchers() {}
  friend GTEST_API_ WithoutMatchers GetWithoutMatchers();
};

GTEST_API_ WithoutMatchers GetWithoutMatchers();

template <typename T>
inline T Invalid() {
  Assert(              false,          "",          -1,
         "Internal error: attempt to return invalid value");
#if defined(__GNUC__) || defined(__clang__)
  __builtin_unreachable();
#elif defined(_MSC_VER)
  __assume(0);
#else
  return Invalid<T>();
#endif
}

template <class RawContainer>
class StlContainerView {
 public:
  typedef RawContainer type;
  typedef const type& const_reference;

  static const_reference ConstReference(const RawContainer& container) {
    static_assert(!std::is_const<RawContainer>::value,
                  "RawContainer type must not be const");
    return container;
  }
  static type Copy(const RawContainer& container) { return container; }
};

template <typename Element, size_t N>
class StlContainerView<Element[N]> {
 public:
  typedef typename std::remove_const<Element>::type RawElement;
  typedef internal::NativeArray<RawElement> type;

  typedef const type const_reference;

  static const_reference ConstReference(const Element (&array)[N]) {
    static_assert(std::is_same<Element, RawElement>::value,
                  "Element type must not be const");
    return type(array, N, RelationToSourceReference());
  }
  static type Copy(const Element (&array)[N]) {
    return type(array, N, RelationToSourceCopy());
  }
};

template <typename ElementPointer, typename Size>
class StlContainerView< ::std::tuple<ElementPointer, Size> > {
 public:
  typedef typename std::remove_const<
      typename std::pointer_traits<ElementPointer>::element_type>::type
      RawElement;
  typedef internal::NativeArray<RawElement> type;
  typedef const type const_reference;

  static const_reference ConstReference(
      const ::std::tuple<ElementPointer, Size>& array) {
    return type(std::get<0>(array), std::get<1>(array),
                RelationToSourceReference());
  }
  static type Copy(const ::std::tuple<ElementPointer, Size>& array) {
    return type(std::get<0>(array), std::get<1>(array), RelationToSourceCopy());
  }
};

template <typename T>
class StlContainerView<T&>;

template <typename T>
struct RemoveConstFromKey {
  typedef T type;
};

template <typename K, typename V>
struct RemoveConstFromKey<std::pair<const K, V> > {
  typedef std::pair<K, V> type;
};

GTEST_API_ void IllegalDoDefault(const char* file, int line);

template <typename F, typename Tuple, size_t... Idx>
auto ApplyImpl(F&& f, Tuple&& args, std::index_sequence<Idx...>)
    -> decltype(std::forward<F>(f)(
        std::get<Idx>(std::forward<Tuple>(args))...)) {
  return std::forward<F>(f)(std::get<Idx>(std::forward<Tuple>(args))...);
}

template <typename F, typename Tuple>
auto Apply(F&& f, Tuple&& args)
    -> decltype(ApplyImpl(
        std::forward<F>(f), std::forward<Tuple>(args),
        std::make_index_sequence<std::tuple_size<
            typename std::remove_reference<Tuple>::type>::value>())) {
  return ApplyImpl(std::forward<F>(f), std::forward<Tuple>(args),
                   std::make_index_sequence<std::tuple_size<
                       typename std::remove_reference<Tuple>::type>::value>());
}

template <typename T>
struct Function;

template <typename R, typename... Args>
struct Function<R(Args...)> {
  using Result = R;
  static constexpr size_t ArgumentCount = sizeof...(Args);
  template <size_t I>
  using Arg = ElemFromList<I, Args...>;
  using ArgumentTuple = std::tuple<Args...>;
  using ArgumentMatcherTuple = std::tuple<Matcher<Args>...>;
  using MakeResultVoid = void(Args...);
  using MakeResultIgnoredValue = IgnoredValue(Args...);
};

template <size_t I, typename T>
using TupleElement = typename std::tuple_element<I, T>::type;

bool Base64Unescape(const std::string& encoded, std::string* decoded);

GTEST_DISABLE_MSC_WARNINGS_POP_()

}
}

#endif
