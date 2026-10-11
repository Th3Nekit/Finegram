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
// The MATCHER* family of macros can be used in a namespace scope to
// define custom matchers easily.
//
// Basic Usage
// ===========
//
// The syntax
//
//   MATCHER(name, description_string) { statements; }
//
// defines a matcher with the given name that executes the statements,
// which must return a bool to indicate if the match succeeds.  Inside
// the statements, you can refer to the value being matched by 'arg',
// and refer to its type by 'arg_type'.
//
// The description string documents what the matcher does, and is used
// to generate the failure message when the match fails.  Since a
// MATCHER() is usually defined in a header file shared by multiple
// C++ source files, we require the description to be a C-string
// literal to avoid possible side effects.  It can be empty, in which
// case we'll use the sequence of words in the matcher name as the
// description.
//
// For example:
//
//   MATCHER(IsEven, "") { return (arg % 2) == 0; }
//
// allows you to write
//
//   // Expects mock_foo.Bar(n) to be called where n is even.
//   EXPECT_CALL(mock_foo, Bar(IsEven()));
//
// or,
//
//   // Verifies that the value of some_expression is even.
//   EXPECT_THAT(some_expression, IsEven());
//
// If the above assertion fails, it will print something like:
//
//   Value of: some_expression
//   Expected: is even
//     Actual: 7
//
// where the description "is even" is automatically calculated from the
// matcher name IsEven.
//
// Argument Type
// =============
//
// Note that the type of the value being matched (arg_type) is
// determined by the context in which you use the matcher and is
// supplied to you by the compiler, so you don't need to worry about
// declaring it (nor can you).  This allows the matcher to be
// polymorphic.  For example, IsEven() can be used to match any type
// where the value of "(arg % 2) == 0" can be implicitly converted to
// a bool.  In the "Bar(IsEven())" example above, if method Bar()
// takes an int, 'arg_type' will be int; if it takes an unsigned long,
// 'arg_type' will be unsigned long; and so on.
//
// Parameterizing Matchers
// =======================
//
// Sometimes you'll want to parameterize the matcher.  For that you
// can use another macro:
//
//   MATCHER_P(name, param_name, description_string) { statements; }
//
// For example:
//
//   MATCHER_P(HasAbsoluteValue, value, "") { return abs(arg) == value; }
//
// will allow you to write:
//
//   EXPECT_THAT(Blah("a"), HasAbsoluteValue(n));
//
// which may lead to this message (assuming n is 10):
//
//   Value of: Blah("a")
//   Expected: has absolute value 10
//     Actual: -9
//
// Note that both the matcher description and its parameter are
// printed, making the message human-friendly.
//
// In the matcher definition body, you can write 'foo_type' to
// reference the type of a parameter named 'foo'.  For example, in the
// body of MATCHER_P(HasAbsoluteValue, value) above, you can write
// 'value_type' to refer to the type of 'value'.
//
// We also provide MATCHER_P2, MATCHER_P3, ..., up to MATCHER_P$n to
// support multi-parameter matchers.
//
// Describing Parameterized Matchers
// =================================
//
// The last argument to MATCHER*() is a string-typed expression.  The
// expression can reference all of the matcher's parameters and a
// special bool-typed variable named 'negation'.  When 'negation' is
// false, the expression should evaluate to the matcher's description;
// otherwise it should evaluate to the description of the negation of
// the matcher.  For example,
//
//   using testing::PrintToString;
//
//   MATCHER_P2(InClosedRange, low, hi,
//       std::string(negation ? "is not" : "is") + " in range [" +
//       PrintToString(low) + ", " + PrintToString(hi) + "]") {
//     return low <= arg && arg <= hi;
//   }
//   ...
//   EXPECT_THAT(3, InClosedRange(4, 6));
//   EXPECT_THAT(3, Not(InClosedRange(2, 4)));
//
// would generate two failures that contain the text:
//
//   Expected: is in range [4, 6]
//   ...
//   Expected: is not in range [2, 4]
//
// If you specify "" as the description, the failure message will
// contain the sequence of words in the matcher name followed by the
// parameter values printed as a tuple.  For example,
//
//   MATCHER_P2(InClosedRange, low, hi, "") { ... }
//   ...
//   EXPECT_THAT(3, InClosedRange(4, 6));
//   EXPECT_THAT(3, Not(InClosedRange(2, 4)));
//
// would generate two failures that contain the text:
//
//   Expected: in closed range (4, 6)
//   ...
//   Expected: not (in closed range (2, 4))
//
// Types of Matcher Parameters
// ===========================
//
// For the purpose of typing, you can view
//
//   MATCHER_Pk(Foo, p1, ..., pk, description_string) { ... }
//
// as shorthand for
//
//   template <typename p1_type, ..., typename pk_type>
//   FooMatcherPk<p1_type, ..., pk_type>
//   Foo(p1_type p1, ..., pk_type pk) { ... }
//
// When you write Foo(v1, ..., vk), the compiler infers the types of
// the parameters v1, ..., and vk for you.  If you are not happy with
// the result of the type inference, you can specify the types by
// explicitly instantiating the template, as in Foo<long, bool>(5,
// false).  As said earlier, you don't get to (or need to) specify
// 'arg_type' as that's determined by the context in which the matcher
// is used.  You can assign the result of expression Foo(p1, ..., pk)
// to a variable of type FooMatcherPk<p1_type, ..., pk_type>.  This
// can be useful when composing matchers.
//
// While you can instantiate a matcher template with reference types,
// passing the parameters by pointer usually makes your code more
// readable.  If, however, you still want to pass a parameter by
// reference, be aware that in the failure message generated by the
// matcher you will see the value of the referenced object but not its
// address.
//
// Explaining Match Results
// ========================
//
// Sometimes the matcher description alone isn't enough to explain why
// the match has failed or succeeded.  For example, when expecting a
// long string, it can be very helpful to also print the diff between
// the expected string and the actual one.  To achieve that, you can
// optionally stream additional information to a special variable
// named result_listener, whose type is a pointer to class
// MatchResultListener:
//
//   MATCHER_P(EqualsLongString, str, "") {
//     if (arg == str) return true;
//
//     *result_listener << "the difference: "
///                     << DiffStrings(str, arg);
//     return false;
//   }
//
// Overloading Matchers
// ====================
//
// You can overload matchers with different numbers of parameters:
//
//   MATCHER_P(Blah, a, description_string1) { ... }
//   MATCHER_P2(Blah, a, b, description_string2) { ... }
//
// Caveats
// =======
//
// When defining a new matcher, you should also consider implementing
// MatcherInterface or using MakePolymorphicMatcher().  These
// approaches require more work than the MATCHER* macros, but also
// give you more control on the types of the value being matched and
// the matcher parameters, which may leads to better compiler error
// messages when the matcher is used wrong.  They also allow
// overloading matchers based on parameter types (as opposed to just
// based on the number of parameters).
//
// MATCHER*() can only be used in a namespace scope as templates cannot be
// declared inside of a local class.
//
// More Information
// ================
//
// To learn more about using these macros, please search for 'MATCHER'
// on
// https://github.com/google/googletest/blob/main/docs/gmock_cook_book.md
//
// This file also implements some commonly used argument matchers.  More
// matchers can be defined by the user implementing the
// MatcherInterface<T> interface if necessary.
//
// See googletest/include/gtest/gtest-matchers.h for the definition of class
// Matcher, class MatcherInterface, and others.

// IWYU pragma: private, include "gmock/gmock.h"
// IWYU pragma: friend gmock/.*

#ifndef GOOGLEMOCK_INCLUDE_GMOCK_GMOCK_MATCHERS_H_
#define GOOGLEMOCK_INCLUDE_GMOCK_GMOCK_MATCHERS_H_

#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <initializer_list>
#include <ios>
#include <iterator>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/internal/gmock-internal-utils.h"
#include "gmock/internal/gmock-port.h"
#include "gmock/internal/gmock-pp.h"
#include "gtest/gtest.h"

#if defined(_MSC_VER) && _MSC_VER >= 1915
#define GMOCK_MAYBE_5046_ 5046
#else
#define GMOCK_MAYBE_5046_
#endif

GTEST_DISABLE_MSC_WARNINGS_PUSH_(
    4251 GMOCK_MAYBE_5046_

                                                                 )

namespace testing {

class StringMatchResultListener : public MatchResultListener {
 public:
  StringMatchResultListener() : MatchResultListener(&ss_) {}

  std::string str() const { return ss_.str(); }

  void Clear() { ss_.str(""); }

 private:
  ::std::stringstream ss_;

  StringMatchResultListener(const StringMatchResultListener&) = delete;
  StringMatchResultListener& operator=(const StringMatchResultListener&) =
      delete;
};

namespace internal {

template <typename T, typename M>
class MatcherCastImpl {
 public:
  static Matcher<T> Cast(const M& polymorphic_matcher_or_value) {

    return CastImpl(polymorphic_matcher_or_value,
                    std::is_convertible<M, Matcher<T>>{},
                    std::is_convertible<M, T>{});
  }

 private:
  template <bool Ignore>
  static Matcher<T> CastImpl(const M& polymorphic_matcher_or_value,
                             std::true_type                             ,
                             std::integral_constant<bool, Ignore>) {

    return polymorphic_matcher_or_value;
  }

  static Matcher<T> CastImpl(const M& value,
                             std::false_type                             ,
                             std::true_type                       ) {
    return Matcher<T>(ImplicitCast_<T>(value));
  }

  static Matcher<T> CastImpl(const M& value,
                             std::false_type                             ,
                             std::false_type                       );
};

template <typename T, typename U>
class MatcherCastImpl<T, Matcher<U>> {
 public:
  static Matcher<T> Cast(const Matcher<U>& source_matcher) {
    return Matcher<T>(new Impl(source_matcher));
  }

 private:

  using ImplArgT =
      typename std::conditional<std::is_convertible<const T&, const U&>::value,
                                const T&, T>::type;

  class Impl : public MatcherInterface<ImplArgT> {
   public:
    explicit Impl(const Matcher<U>& source_matcher)
        : source_matcher_(source_matcher) {}

    bool MatchAndExplain(ImplArgT x,
                         MatchResultListener* listener) const override {
      using FromType = typename std::remove_cv<typename std::remove_pointer<
          typename std::remove_reference<T>::type>::type>::type;
      using ToType = typename std::remove_cv<typename std::remove_pointer<
          typename std::remove_reference<U>::type>::type>::type;

      static_assert(

          (std::is_pointer<typename std::remove_reference<T>::type>::value !=
           std::is_pointer<typename std::remove_reference<U>::type>::value) ||
              std::is_same<FromType, ToType>::value ||
              !std::is_base_of<FromType, ToType>::value,
          "Can't implicitly convert from <base> to <derived>");

      using CastType = typename std::conditional<
          std::is_convertible<ImplArgT&, const U&>::value, ImplArgT&, U>::type;

      return source_matcher_.MatchAndExplain(static_cast<CastType>(x),
                                             listener);
    }

    void DescribeTo(::std::ostream* os) const override {
      source_matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(::std::ostream* os) const override {
      source_matcher_.DescribeNegationTo(os);
    }

   private:
    const Matcher<U> source_matcher_;
  };
};

template <typename T>
class MatcherCastImpl<T, Matcher<T>> {
 public:
  static Matcher<T> Cast(const Matcher<T>& matcher) { return matcher; }
};

template <typename Derived>
class MatcherBaseImpl {
 public:
  MatcherBaseImpl() = default;

  template <typename T>
  operator ::testing::Matcher<T>() const {
    return ::testing::Matcher<T>(new
                                 typename Derived::template gmock_Impl<T>());
  }
};

template <template <typename...> class Derived, typename... Ts>
class MatcherBaseImpl<Derived<Ts...>> {
 public:

  template <typename E = std::enable_if<sizeof...(Ts) == 1>,
            typename E::type* = nullptr>
  explicit MatcherBaseImpl(Ts... params)
      : params_(std::forward<Ts>(params)...) {}
  template <typename E = std::enable_if<sizeof...(Ts) != 1>,
            typename = typename E::type>
  MatcherBaseImpl(Ts... params)
      : params_(std::forward<Ts>(params)...) {}

  template <typename F>
  operator ::testing::Matcher<F>() const {
    return Apply<F>(std::make_index_sequence<sizeof...(Ts)>{});
  }

 private:
  template <typename F, std::size_t... tuple_ids>
  ::testing::Matcher<F> Apply(std::index_sequence<tuple_ids...>) const {
    return ::testing::Matcher<F>(
        new typename Derived<Ts...>::template gmock_Impl<F>(
            std::get<tuple_ids>(params_)...));
  }

  const std::tuple<Ts...> params_;
};

}

template <typename T, typename M>
inline Matcher<T> MatcherCast(const M& matcher) {
  return internal::MatcherCastImpl<T, M>::Cast(matcher);
}

template <typename T, typename M>
inline Matcher<T> SafeMatcherCast(const M& polymorphic_matcher_or_value) {
  return MatcherCast<T>(polymorphic_matcher_or_value);
}

template <typename T, typename U>
inline Matcher<T> SafeMatcherCast(const Matcher<U>& matcher) {

  static_assert(std::is_convertible<const T&, const U&>::value,
                "T must be implicitly convertible to U (and T must be a "
                "non-const reference if U is a non-const reference)");

  typedef GTEST_REMOVE_REFERENCE_AND_CONST_(T) RawT;
  typedef GTEST_REMOVE_REFERENCE_AND_CONST_(U) RawU;
  constexpr bool kTIsOther = GMOCK_KIND_OF_(RawT) == internal::kOther;
  constexpr bool kUIsOther = GMOCK_KIND_OF_(RawU) == internal::kOther;
  static_assert(
      kTIsOther || kUIsOther ||
          (internal::LosslessArithmeticConvertible<RawT, RawU>::value),
      "conversion of arithmetic types must be lossless");
  return MatcherCast<T>(matcher);
}

template <typename T>
Matcher<T> A();

namespace internal {

struct Rank0 {};
struct Rank1 : Rank0 {};
using HighestRank = Rank1;

inline void PrintIfNotEmpty(const std::string& explanation,
                            ::std::ostream* os) {
  if (!explanation.empty() && os != nullptr) {
    *os << ", " << explanation;
  }
}

inline bool IsReadableTypeName(const std::string& type_name) {

  return (type_name.length() <= 20 ||
          type_name.find_first_of("<(") == std::string::npos);
}

template <typename Value, typename T>
bool MatchPrintAndExplain(Value& value, const Matcher<T>& matcher,
                          MatchResultListener* listener) {
  if (!listener->IsInterested()) {

    return matcher.Matches(value);
  }

  StringMatchResultListener inner_listener;
  const bool match = matcher.MatchAndExplain(value, &inner_listener);

  UniversalPrint(value, listener->stream());
#if GTEST_HAS_RTTI
  const std::string& type_name = GetTypeName<Value>();
  if (IsReadableTypeName(type_name))
    *listener->stream() << " (of type " << type_name << ")";
#endif
  PrintIfNotEmpty(inner_listener.str(), listener->stream());

  return match;
}

template <size_t N>
class TuplePrefix {
 public:

  template <typename MatcherTuple, typename ValueTuple>
  static bool Matches(const MatcherTuple& matcher_tuple,
                      const ValueTuple& value_tuple) {
    return TuplePrefix<N - 1>::Matches(matcher_tuple, value_tuple) &&
           std::get<N - 1>(matcher_tuple).Matches(std::get<N - 1>(value_tuple));
  }

  template <typename MatcherTuple, typename ValueTuple>
  static void ExplainMatchFailuresTo(const MatcherTuple& matchers,
                                     const ValueTuple& values,
                                     ::std::ostream* os) {

    TuplePrefix<N - 1>::ExplainMatchFailuresTo(matchers, values, os);

    typename std::tuple_element<N - 1, MatcherTuple>::type matcher =
        std::get<N - 1>(matchers);
    typedef typename std::tuple_element<N - 1, ValueTuple>::type Value;
    const Value& value = std::get<N - 1>(values);
    StringMatchResultListener listener;
    if (!matcher.MatchAndExplain(value, &listener)) {
      *os << "  Expected arg #" << N - 1 << ": ";
      std::get<N - 1>(matchers).DescribeTo(os);
      *os << "\n           Actual: ";

      internal::UniversalPrint(value, os);
      PrintIfNotEmpty(listener.str(), os);
      *os << "\n";
    }
  }
};

template <>
class TuplePrefix<0> {
 public:
  template <typename MatcherTuple, typename ValueTuple>
  static bool Matches(const MatcherTuple&                    ,
                      const ValueTuple&                  ) {
    return true;
  }

  template <typename MatcherTuple, typename ValueTuple>
  static void ExplainMatchFailuresTo(const MatcherTuple&               ,
                                     const ValueTuple&             ,
                                     ::std::ostream*         ) {}
};

template <typename MatcherTuple, typename ValueTuple>
bool TupleMatches(const MatcherTuple& matcher_tuple,
                  const ValueTuple& value_tuple) {

  static_assert(std::tuple_size<MatcherTuple>::value ==
                    std::tuple_size<ValueTuple>::value,
                "matcher and value have different numbers of fields");
  return TuplePrefix<std::tuple_size<ValueTuple>::value>::Matches(matcher_tuple,
                                                                  value_tuple);
}

template <typename MatcherTuple, typename ValueTuple>
void ExplainMatchFailureTupleTo(const MatcherTuple& matchers,
                                const ValueTuple& values, ::std::ostream* os) {
  TuplePrefix<std::tuple_size<MatcherTuple>::value>::ExplainMatchFailuresTo(
      matchers, values, os);
}

template <typename Tuple, typename Func, typename OutIter>
class TransformTupleValuesHelper {
 private:
  typedef ::std::tuple_size<Tuple> TupleSize;

 public:

  static OutIter Run(Func f, const Tuple& t, OutIter out) {
    return IterateOverTuple<Tuple, TupleSize::value>()(f, t, out);
  }

 private:
  template <typename Tup, size_t kRemainingSize>
  struct IterateOverTuple {
    OutIter operator()(Func f, const Tup& t, OutIter out) const {
      *out++ = f(::std::get<TupleSize::value - kRemainingSize>(t));
      return IterateOverTuple<Tup, kRemainingSize - 1>()(f, t, out);
    }
  };
  template <typename Tup>
  struct IterateOverTuple<Tup, 0> {
    OutIter operator()(Func        , const Tup&        , OutIter out) const {
      return out;
    }
  };
};

template <typename Tuple, typename Func, typename OutIter>
OutIter TransformTupleValues(Func f, const Tuple& t, OutIter out) {
  return TransformTupleValuesHelper<Tuple, Func, OutIter>::Run(f, t, out);
}

class AnythingMatcher {
 public:
  using is_gtest_matcher = void;

  template <typename T>
  bool MatchAndExplain(const T&        , std::ostream*               ) const {
    return true;
  }
  void DescribeTo(std::ostream* os) const { *os << "is anything"; }
  void DescribeNegationTo(::std::ostream* os) const {

    *os << "never matches";
  }
};

class IsNullMatcher {
 public:
  template <typename Pointer>
  bool MatchAndExplain(const Pointer& p,
                       MatchResultListener*               ) const {
    return p == nullptr;
  }

  void DescribeTo(::std::ostream* os) const { *os << "is NULL"; }
  void DescribeNegationTo(::std::ostream* os) const { *os << "isn't NULL"; }
};

class NotNullMatcher {
 public:
  template <typename Pointer>
  bool MatchAndExplain(const Pointer& p,
                       MatchResultListener*               ) const {
    return p != nullptr;
  }

  void DescribeTo(::std::ostream* os) const { *os << "isn't NULL"; }
  void DescribeNegationTo(::std::ostream* os) const { *os << "is NULL"; }
};

template <typename T>
class RefMatcher;

template <typename T>
class RefMatcher<T&> {

 public:

  explicit RefMatcher(T& x) : object_(x) {}

  template <typename Super>
  operator Matcher<Super&>() const {

    return MakeMatcher(new Impl<Super>(object_));
  }

 private:
  template <typename Super>
  class Impl : public MatcherInterface<Super&> {
   public:
    explicit Impl(Super& x) : object_(x) {}

    bool MatchAndExplain(Super& x,
                         MatchResultListener* listener) const override {
      *listener << "which is located @" << static_cast<const void*>(&x);
      return &x == &object_;
    }

    void DescribeTo(::std::ostream* os) const override {
      *os << "references the variable ";
      UniversalPrinter<Super&>::Print(object_, os);
    }

    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "does not reference the variable ";
      UniversalPrinter<Super&>::Print(object_, os);
    }

   private:
    const Super& object_;
  };

  T& object_;
};

inline bool CaseInsensitiveCStringEquals(const char* lhs, const char* rhs) {
  return String::CaseInsensitiveCStringEquals(lhs, rhs);
}

inline bool CaseInsensitiveCStringEquals(const wchar_t* lhs,
                                         const wchar_t* rhs) {
  return String::CaseInsensitiveWideCStringEquals(lhs, rhs);
}

template <typename StringType>
bool CaseInsensitiveStringEquals(const StringType& s1, const StringType& s2) {

  if (!CaseInsensitiveCStringEquals(s1.c_str(), s2.c_str())) {
    return false;
  }

  const typename StringType::value_type nul = 0;
  const size_t i1 = s1.find(nul), i2 = s2.find(nul);

  if (i1 == StringType::npos || i2 == StringType::npos) {
    return i1 == i2;
  }

  return CaseInsensitiveStringEquals(s1.substr(i1 + 1), s2.substr(i2 + 1));
}

template <typename StringType>
class StrEqualityMatcher {
 public:
  StrEqualityMatcher(StringType str, bool expect_eq, bool case_sensitive)
      : string_(std::move(str)),
        expect_eq_(expect_eq),
        case_sensitive_(case_sensitive) {}

#if GTEST_INTERNAL_HAS_STRING_VIEW
  bool MatchAndExplain(const internal::StringView& s,
                       MatchResultListener* listener) const {

    const StringType& str = std::string(s);
    return MatchAndExplain(str, listener);
  }
#endif

  template <typename CharType>
  bool MatchAndExplain(CharType* s, MatchResultListener* listener) const {
    if (s == nullptr) {
      return !expect_eq_;
    }
    return MatchAndExplain(StringType(s), listener);
  }

  template <typename MatcheeStringType>
  bool MatchAndExplain(const MatcheeStringType& s,
                       MatchResultListener*               ) const {
    const StringType s2(s);
    const bool eq = case_sensitive_ ? s2 == string_
                                    : CaseInsensitiveStringEquals(s2, string_);
    return expect_eq_ == eq;
  }

  void DescribeTo(::std::ostream* os) const {
    DescribeToHelper(expect_eq_, os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    DescribeToHelper(!expect_eq_, os);
  }

 private:
  void DescribeToHelper(bool expect_eq, ::std::ostream* os) const {
    *os << (expect_eq ? "is " : "isn't ");
    *os << "equal to ";
    if (!case_sensitive_) {
      *os << "(ignoring case) ";
    }
    UniversalPrint(string_, os);
  }

  const StringType string_;
  const bool expect_eq_;
  const bool case_sensitive_;
};

template <typename StringType>
class HasSubstrMatcher {
 public:
  explicit HasSubstrMatcher(const StringType& substring)
      : substring_(substring) {}

#if GTEST_INTERNAL_HAS_STRING_VIEW
  bool MatchAndExplain(const internal::StringView& s,
                       MatchResultListener* listener) const {

    const StringType& str = std::string(s);
    return MatchAndExplain(str, listener);
  }
#endif

  template <typename CharType>
  bool MatchAndExplain(CharType* s, MatchResultListener* listener) const {
    return s != nullptr && MatchAndExplain(StringType(s), listener);
  }

  template <typename MatcheeStringType>
  bool MatchAndExplain(const MatcheeStringType& s,
                       MatchResultListener*               ) const {
    return StringType(s).find(substring_) != StringType::npos;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "has substring ";
    UniversalPrint(substring_, os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "has no substring ";
    UniversalPrint(substring_, os);
  }

 private:
  const StringType substring_;
};

template <typename StringType>
class StartsWithMatcher {
 public:
  explicit StartsWithMatcher(const StringType& prefix) : prefix_(prefix) {}

#if GTEST_INTERNAL_HAS_STRING_VIEW
  bool MatchAndExplain(const internal::StringView& s,
                       MatchResultListener* listener) const {

    const StringType& str = std::string(s);
    return MatchAndExplain(str, listener);
  }
#endif

  template <typename CharType>
  bool MatchAndExplain(CharType* s, MatchResultListener* listener) const {
    return s != nullptr && MatchAndExplain(StringType(s), listener);
  }

  template <typename MatcheeStringType>
  bool MatchAndExplain(const MatcheeStringType& s,
                       MatchResultListener*               ) const {
    const StringType s2(s);
    return s2.length() >= prefix_.length() &&
           s2.substr(0, prefix_.length()) == prefix_;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "starts with ";
    UniversalPrint(prefix_, os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "doesn't start with ";
    UniversalPrint(prefix_, os);
  }

 private:
  const StringType prefix_;
};

template <typename StringType>
class EndsWithMatcher {
 public:
  explicit EndsWithMatcher(const StringType& suffix) : suffix_(suffix) {}

#if GTEST_INTERNAL_HAS_STRING_VIEW
  bool MatchAndExplain(const internal::StringView& s,
                       MatchResultListener* listener) const {

    const StringType& str = std::string(s);
    return MatchAndExplain(str, listener);
  }
#endif

  template <typename CharType>
  bool MatchAndExplain(CharType* s, MatchResultListener* listener) const {
    return s != nullptr && MatchAndExplain(StringType(s), listener);
  }

  template <typename MatcheeStringType>
  bool MatchAndExplain(const MatcheeStringType& s,
                       MatchResultListener*               ) const {
    const StringType s2(s);
    return s2.length() >= suffix_.length() &&
           s2.substr(s2.length() - suffix_.length()) == suffix_;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "ends with ";
    UniversalPrint(suffix_, os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "doesn't end with ";
    UniversalPrint(suffix_, os);
  }

 private:
  const StringType suffix_;
};

class WhenBase64UnescapedMatcher {
 public:
  using is_gtest_matcher = void;

  explicit WhenBase64UnescapedMatcher(
      const Matcher<const std::string&>& internal_matcher)
      : internal_matcher_(internal_matcher) {}

  template <typename MatcheeStringType>
  bool MatchAndExplain(const MatcheeStringType& s,
                       MatchResultListener* listener) const {
    const std::string s2(s);
    std::string unescaped;
    if (!internal::Base64Unescape(s2, &unescaped)) {
      if (listener != nullptr) {
        *listener << "is not a valid base64 escaped string";
      }
      return false;
    }
    return MatchPrintAndExplain(unescaped, internal_matcher_, listener);
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "matches after Base64Unescape ";
    internal_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not match after Base64Unescape ";
    internal_matcher_.DescribeTo(os);
  }

 private:
  const Matcher<const std::string&> internal_matcher_;
};

template <typename D, typename Op>
class PairMatchBase {
 public:
  template <typename T1, typename T2>
  operator Matcher<::std::tuple<T1, T2>>() const {
    return Matcher<::std::tuple<T1, T2>>(new Impl<const ::std::tuple<T1, T2>&>);
  }
  template <typename T1, typename T2>
  operator Matcher<const ::std::tuple<T1, T2>&>() const {
    return MakeMatcher(new Impl<const ::std::tuple<T1, T2>&>);
  }

 private:
  static ::std::ostream& GetDesc(::std::ostream& os) {
    return os << D::Desc();
  }

  template <typename Tuple>
  class Impl : public MatcherInterface<Tuple> {
   public:
    bool MatchAndExplain(Tuple args,
                         MatchResultListener*               ) const override {
      return Op()(::std::get<0>(args), ::std::get<1>(args));
    }
    void DescribeTo(::std::ostream* os) const override {
      *os << "are " << GetDesc;
    }
    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "aren't " << GetDesc;
    }
  };
};

class Eq2Matcher : public PairMatchBase<Eq2Matcher, std::equal_to<>> {
 public:
  static const char* Desc() { return "an equal pair"; }
};
class Ne2Matcher : public PairMatchBase<Ne2Matcher, std::not_equal_to<>> {
 public:
  static const char* Desc() { return "an unequal pair"; }
};
class Lt2Matcher : public PairMatchBase<Lt2Matcher, std::less<>> {
 public:
  static const char* Desc() { return "a pair where the first < the second"; }
};
class Gt2Matcher : public PairMatchBase<Gt2Matcher, std::greater<>> {
 public:
  static const char* Desc() { return "a pair where the first > the second"; }
};
class Le2Matcher : public PairMatchBase<Le2Matcher, std::less_equal<>> {
 public:
  static const char* Desc() { return "a pair where the first <= the second"; }
};
class Ge2Matcher : public PairMatchBase<Ge2Matcher, std::greater_equal<>> {
 public:
  static const char* Desc() { return "a pair where the first >= the second"; }
};

template <typename T>
class NotMatcherImpl : public MatcherInterface<const T&> {
 public:
  explicit NotMatcherImpl(const Matcher<T>& matcher) : matcher_(matcher) {}

  bool MatchAndExplain(const T& x,
                       MatchResultListener* listener) const override {
    return !matcher_.MatchAndExplain(x, listener);
  }

  void DescribeTo(::std::ostream* os) const override {
    matcher_.DescribeNegationTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    matcher_.DescribeTo(os);
  }

 private:
  const Matcher<T> matcher_;
};

template <typename InnerMatcher>
class NotMatcher {
 public:
  explicit NotMatcher(InnerMatcher matcher) : matcher_(matcher) {}

  template <typename T>
  operator Matcher<T>() const {
    return Matcher<T>(new NotMatcherImpl<T>(SafeMatcherCast<T>(matcher_)));
  }

 private:
  InnerMatcher matcher_;
};

template <typename T>
class AllOfMatcherImpl : public MatcherInterface<const T&> {
 public:
  explicit AllOfMatcherImpl(std::vector<Matcher<T>> matchers)
      : matchers_(std::move(matchers)) {}

  void DescribeTo(::std::ostream* os) const override {
    *os << "(";
    for (size_t i = 0; i < matchers_.size(); ++i) {
      if (i != 0) *os << ") and (";
      matchers_[i].DescribeTo(os);
    }
    *os << ")";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "(";
    for (size_t i = 0; i < matchers_.size(); ++i) {
      if (i != 0) *os << ") or (";
      matchers_[i].DescribeNegationTo(os);
    }
    *os << ")";
  }

  bool MatchAndExplain(const T& x,
                       MatchResultListener* listener) const override {

    std::string all_match_result;
    for (const Matcher<T>& matcher : matchers_) {
      StringMatchResultListener slistener;

      if (!matcher.MatchAndExplain(x, &slistener)) {
        const std::string explanation = slistener.str();
        if (!explanation.empty()) {
          *listener << explanation;
        } else {
          *listener << "which doesn't match (" << Describe(matcher) << ")";
        }
        return false;
      }

      std::string explanation = slistener.str();
      if (explanation.empty()) {
        explanation = Describe(matcher);
      }
      if (all_match_result.empty()) {
        all_match_result = explanation;
      } else {
        if (!explanation.empty()) {
          all_match_result += ", and ";
          all_match_result += explanation;
        }
      }
    }

    *listener << all_match_result;
    return true;
  }

 private:

  std::string Describe(const Matcher<T>& matcher) const {
    StringMatchResultListener listener;
    matcher.DescribeTo(listener.stream());
    return listener.str();
  }
  const std::vector<Matcher<T>> matchers_;
};

template <template <typename T> class CombiningMatcher, typename... Args>
class VariadicMatcher {
 public:
  VariadicMatcher(const Args&... matchers)
      : matchers_(matchers...) {
    static_assert(sizeof...(Args) > 0, "Must have at least one matcher.");
  }

  VariadicMatcher(const VariadicMatcher&) = default;
  VariadicMatcher& operator=(const VariadicMatcher&) = delete;

  template <typename T>
  operator Matcher<T>() const {
    std::vector<Matcher<T>> values;
    CreateVariadicMatcher<T>(&values, std::integral_constant<size_t, 0>());
    return Matcher<T>(new CombiningMatcher<T>(std::move(values)));
  }

 private:
  template <typename T, size_t I>
  void CreateVariadicMatcher(std::vector<Matcher<T>>* values,
                             std::integral_constant<size_t, I>) const {
    values->push_back(SafeMatcherCast<T>(std::get<I>(matchers_)));
    CreateVariadicMatcher<T>(values, std::integral_constant<size_t, I + 1>());
  }

  template <typename T>
  void CreateVariadicMatcher(
      std::vector<Matcher<T>>*,
      std::integral_constant<size_t, sizeof...(Args)>) const {}

  std::tuple<Args...> matchers_;
};

template <typename... Args>
using AllOfMatcher = VariadicMatcher<AllOfMatcherImpl, Args...>;

template <typename T>
class AnyOfMatcherImpl : public MatcherInterface<const T&> {
 public:
  explicit AnyOfMatcherImpl(std::vector<Matcher<T>> matchers)
      : matchers_(std::move(matchers)) {}

  void DescribeTo(::std::ostream* os) const override {
    *os << "(";
    for (size_t i = 0; i < matchers_.size(); ++i) {
      if (i != 0) *os << ") or (";
      matchers_[i].DescribeTo(os);
    }
    *os << ")";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "(";
    for (size_t i = 0; i < matchers_.size(); ++i) {
      if (i != 0) *os << ") and (";
      matchers_[i].DescribeNegationTo(os);
    }
    *os << ")";
  }

  bool MatchAndExplain(const T& x,
                       MatchResultListener* listener) const override {

    std::string no_match_result;
    for (const Matcher<T>& matcher : matchers_) {
      StringMatchResultListener slistener;

      if (matcher.MatchAndExplain(x, &slistener)) {
        const std::string explanation = slistener.str();
        if (!explanation.empty()) {
          *listener << explanation;
        } else {
          *listener << "which matches (" << Describe(matcher) << ")";
        }
        return true;
      }

      std::string explanation = slistener.str();
      if (explanation.empty()) {
        explanation = DescribeNegation(matcher);
      }
      if (no_match_result.empty()) {
        no_match_result = explanation;
      } else {
        if (!explanation.empty()) {
          no_match_result += ", and ";
          no_match_result += explanation;
        }
      }
    }

    *listener << no_match_result;
    return false;
  }

 private:

  std::string Describe(const Matcher<T>& matcher) const {
    StringMatchResultListener listener;
    matcher.DescribeTo(listener.stream());
    return listener.str();
  }

  std::string DescribeNegation(const Matcher<T>& matcher) const {
    StringMatchResultListener listener;
    matcher.DescribeNegationTo(listener.stream());
    return listener.str();
  }

  const std::vector<Matcher<T>> matchers_;
};

template <typename... Args>
using AnyOfMatcher = VariadicMatcher<AnyOfMatcherImpl, Args...>;

template <typename MatcherTrue, typename MatcherFalse>
class ConditionalMatcher {
 public:
  ConditionalMatcher(bool condition, MatcherTrue matcher_true,
                     MatcherFalse matcher_false)
      : condition_(condition),
        matcher_true_(std::move(matcher_true)),
        matcher_false_(std::move(matcher_false)) {}

  template <typename T>
  operator Matcher<T>() const {
    return condition_ ? SafeMatcherCast<T>(matcher_true_)
                      : SafeMatcherCast<T>(matcher_false_);
  }

 private:
  bool condition_;
  MatcherTrue matcher_true_;
  MatcherFalse matcher_false_;
};

template <template <class> class MatcherImpl, typename T>
class SomeOfArrayMatcher {
 public:

  template <typename Iter>
  SomeOfArrayMatcher(Iter first, Iter last) : matchers_(first, last) {}

  template <typename U>
  operator Matcher<U>() const {
    using RawU = typename std::decay<U>::type;
    std::vector<Matcher<RawU>> matchers;
    matchers.reserve(matchers_.size());
    for (const auto& matcher : matchers_) {
      matchers.push_back(MatcherCast<RawU>(matcher));
    }
    return Matcher<U>(new MatcherImpl<RawU>(std::move(matchers)));
  }

 private:
  const std::vector<std::remove_const_t<T>> matchers_;
};

template <typename T>
using AllOfArrayMatcher = SomeOfArrayMatcher<AllOfMatcherImpl, T>;

template <typename T>
using AnyOfArrayMatcher = SomeOfArrayMatcher<AnyOfMatcherImpl, T>;

template <typename Predicate>
class TrulyMatcher {
 public:
  explicit TrulyMatcher(Predicate pred) : predicate_(pred) {}

  template <typename T>
  bool MatchAndExplain(T& x,
                       MatchResultListener* listener) const {

    if (predicate_(x)) return true;
    *listener << "didn't satisfy the given predicate";
    return false;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "satisfies the given predicate";
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "doesn't satisfy the given predicate";
  }

 private:
  Predicate predicate_;
};

template <typename M>
class MatcherAsPredicate {
 public:
  explicit MatcherAsPredicate(M matcher) : matcher_(matcher) {}

  template <typename T>
  bool operator()(const T& x) const {

    return MatcherCast<const T&>(matcher_).Matches(x);
  }

 private:
  M matcher_;
};

template <typename M>
class PredicateFormatterFromMatcher {
 public:
  explicit PredicateFormatterFromMatcher(M m) : matcher_(std::move(m)) {}

  template <typename T>
  AssertionResult operator()(const char* value_text, const T& x) const {

    const Matcher<const T&> matcher = SafeMatcherCast<const T&>(matcher_);

    if (matcher.Matches(x)) {
      return AssertionSuccess();
    }

    ::std::stringstream ss;
    ss << "Value of: " << value_text << "\n"
       << "Expected: ";
    matcher.DescribeTo(&ss);

    StringMatchResultListener listener;
    if (MatchPrintAndExplain(x, matcher, &listener)) {
      ss << "\n  The matcher failed on the initial attempt; but passed when "
            "rerun to generate the explanation.";
    }
    ss << "\n  Actual: " << listener.str();
    return AssertionFailure() << ss.str();
  }

 private:
  const M matcher_;
};

template <typename M>
inline PredicateFormatterFromMatcher<M> MakePredicateFormatterFromMatcher(
    M matcher) {
  return PredicateFormatterFromMatcher<M>(std::move(matcher));
}

class IsNanMatcher {
 public:
  template <typename FloatType>
  bool MatchAndExplain(const FloatType& f,
                       MatchResultListener*               ) const {
    return (::std::isnan)(f);
  }

  void DescribeTo(::std::ostream* os) const { *os << "is NaN"; }
  void DescribeNegationTo(::std::ostream* os) const { *os << "isn't NaN"; }
};

template <typename FloatType>
class FloatingEqMatcher {
 public:

  FloatingEqMatcher(FloatType expected, bool nan_eq_nan)
      : expected_(expected), nan_eq_nan_(nan_eq_nan), max_abs_error_(-1) {}

  FloatingEqMatcher(FloatType expected, bool nan_eq_nan,
                    FloatType max_abs_error)
      : expected_(expected),
        nan_eq_nan_(nan_eq_nan),
        max_abs_error_(max_abs_error) {
    GTEST_CHECK_(max_abs_error >= 0)
        << ", where max_abs_error is" << max_abs_error;
  }

  template <typename T>
  class Impl : public MatcherInterface<T> {
   public:
    Impl(FloatType expected, bool nan_eq_nan, FloatType max_abs_error)
        : expected_(expected),
          nan_eq_nan_(nan_eq_nan),
          max_abs_error_(max_abs_error) {}

    bool MatchAndExplain(T value,
                         MatchResultListener* listener) const override {
      const FloatingPoint<FloatType> actual(value), expected(expected_);

      if (actual.is_nan() || expected.is_nan()) {
        if (actual.is_nan() && expected.is_nan()) {
          return nan_eq_nan_;
        }

        return false;
      }
      if (HasMaxAbsError()) {

        if (value == expected_) {
          return true;
        }

        const FloatType diff = value - expected_;
        if (::std::fabs(diff) <= max_abs_error_) {
          return true;
        }

        if (listener->IsInterested()) {
          *listener << "which is " << diff << " from " << expected_;
        }
        return false;
      } else {
        return actual.AlmostEquals(expected);
      }
    }

    void DescribeTo(::std::ostream* os) const override {

      const ::std::streamsize old_precision =
          os->precision(::std::numeric_limits<FloatType>::digits10 + 2);
      if (FloatingPoint<FloatType>(expected_).is_nan()) {
        if (nan_eq_nan_) {
          *os << "is NaN";
        } else {
          *os << "never matches";
        }
      } else {
        *os << "is approximately " << expected_;
        if (HasMaxAbsError()) {
          *os << " (absolute error <= " << max_abs_error_ << ")";
        }
      }
      os->precision(old_precision);
    }

    void DescribeNegationTo(::std::ostream* os) const override {

      const ::std::streamsize old_precision =
          os->precision(::std::numeric_limits<FloatType>::digits10 + 2);
      if (FloatingPoint<FloatType>(expected_).is_nan()) {
        if (nan_eq_nan_) {
          *os << "isn't NaN";
        } else {
          *os << "is anything";
        }
      } else {
        *os << "isn't approximately " << expected_;
        if (HasMaxAbsError()) {
          *os << " (absolute error > " << max_abs_error_ << ")";
        }
      }

      os->precision(old_precision);
    }

   private:
    bool HasMaxAbsError() const { return max_abs_error_ >= 0; }

    const FloatType expected_;
    const bool nan_eq_nan_;

    const FloatType max_abs_error_;
  };

  operator Matcher<FloatType>() const {
    return MakeMatcher(
        new Impl<FloatType>(expected_, nan_eq_nan_, max_abs_error_));
  }

  operator Matcher<const FloatType&>() const {
    return MakeMatcher(
        new Impl<const FloatType&>(expected_, nan_eq_nan_, max_abs_error_));
  }

  operator Matcher<FloatType&>() const {
    return MakeMatcher(
        new Impl<FloatType&>(expected_, nan_eq_nan_, max_abs_error_));
  }

 private:
  const FloatType expected_;
  const bool nan_eq_nan_;

  const FloatType max_abs_error_;
};

template <typename FloatType>
class FloatingEq2Matcher {
 public:
  FloatingEq2Matcher() { Init(-1, false); }

  explicit FloatingEq2Matcher(bool nan_eq_nan) { Init(-1, nan_eq_nan); }

  explicit FloatingEq2Matcher(FloatType max_abs_error) {
    Init(max_abs_error, false);
  }

  FloatingEq2Matcher(FloatType max_abs_error, bool nan_eq_nan) {
    Init(max_abs_error, nan_eq_nan);
  }

  template <typename T1, typename T2>
  operator Matcher<::std::tuple<T1, T2>>() const {
    return MakeMatcher(
        new Impl<::std::tuple<T1, T2>>(max_abs_error_, nan_eq_nan_));
  }
  template <typename T1, typename T2>
  operator Matcher<const ::std::tuple<T1, T2>&>() const {
    return MakeMatcher(
        new Impl<const ::std::tuple<T1, T2>&>(max_abs_error_, nan_eq_nan_));
  }

 private:
  static ::std::ostream& GetDesc(::std::ostream& os) {
    return os << "an almost-equal pair";
  }

  template <typename Tuple>
  class Impl : public MatcherInterface<Tuple> {
   public:
    Impl(FloatType max_abs_error, bool nan_eq_nan)
        : max_abs_error_(max_abs_error), nan_eq_nan_(nan_eq_nan) {}

    bool MatchAndExplain(Tuple args,
                         MatchResultListener* listener) const override {
      if (max_abs_error_ == -1) {
        FloatingEqMatcher<FloatType> fm(::std::get<0>(args), nan_eq_nan_);
        return static_cast<Matcher<FloatType>>(fm).MatchAndExplain(
            ::std::get<1>(args), listener);
      } else {
        FloatingEqMatcher<FloatType> fm(::std::get<0>(args), nan_eq_nan_,
                                        max_abs_error_);
        return static_cast<Matcher<FloatType>>(fm).MatchAndExplain(
            ::std::get<1>(args), listener);
      }
    }
    void DescribeTo(::std::ostream* os) const override {
      *os << "are " << GetDesc;
    }
    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "aren't " << GetDesc;
    }

   private:
    FloatType max_abs_error_;
    const bool nan_eq_nan_;
  };

  void Init(FloatType max_abs_error_val, bool nan_eq_nan_val) {
    max_abs_error_ = max_abs_error_val;
    nan_eq_nan_ = nan_eq_nan_val;
  }
  FloatType max_abs_error_;
  bool nan_eq_nan_;
};

template <typename InnerMatcher>
class PointeeMatcher {
 public:
  explicit PointeeMatcher(const InnerMatcher& matcher) : matcher_(matcher) {}

  template <typename Pointer>
  operator Matcher<Pointer>() const {
    return Matcher<Pointer>(new Impl<const Pointer&>(matcher_));
  }

 private:

  template <typename Pointer>
  class Impl : public MatcherInterface<Pointer> {
   public:
    using Pointee =
        typename std::pointer_traits<GTEST_REMOVE_REFERENCE_AND_CONST_(
            Pointer)>::element_type;

    explicit Impl(const InnerMatcher& matcher)
        : matcher_(MatcherCast<const Pointee&>(matcher)) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "points to a value that ";
      matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "does not point to a value that ";
      matcher_.DescribeTo(os);
    }

    bool MatchAndExplain(Pointer pointer,
                         MatchResultListener* listener) const override {
      if (GetRawPointer(pointer) == nullptr) return false;

      *listener << "which points to ";
      return MatchPrintAndExplain(*pointer, matcher_, listener);
    }

   private:
    const Matcher<const Pointee&> matcher_;
  };

  const InnerMatcher matcher_;
};

template <typename InnerMatcher>
class PointerMatcher {
 public:
  explicit PointerMatcher(const InnerMatcher& matcher) : matcher_(matcher) {}

  template <typename PointerType>
  operator Matcher<PointerType>() const {
    return Matcher<PointerType>(new Impl<const PointerType&>(matcher_));
  }

 private:

  template <typename PointerType>
  class Impl : public MatcherInterface<PointerType> {
   public:
    using Pointer =
        const typename std::pointer_traits<GTEST_REMOVE_REFERENCE_AND_CONST_(
            PointerType)>::element_type*;

    explicit Impl(const InnerMatcher& matcher)
        : matcher_(MatcherCast<Pointer>(matcher)) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "is a pointer that ";
      matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "is not a pointer that ";
      matcher_.DescribeTo(os);
    }

    bool MatchAndExplain(PointerType pointer,
                         MatchResultListener* listener) const override {
      *listener << "which is a pointer that ";
      Pointer p = GetRawPointer(pointer);
      return MatchPrintAndExplain(p, matcher_, listener);
    }

   private:
    Matcher<Pointer> matcher_;
  };

  const InnerMatcher matcher_;
};

#if GTEST_HAS_RTTI

template <typename To>
class WhenDynamicCastToMatcherBase {
 public:
  explicit WhenDynamicCastToMatcherBase(const Matcher<To>& matcher)
      : matcher_(matcher) {}

  void DescribeTo(::std::ostream* os) const {
    GetCastTypeDescription(os);
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    GetCastTypeDescription(os);
    matcher_.DescribeNegationTo(os);
  }

 protected:
  const Matcher<To> matcher_;

  static std::string GetToName() { return GetTypeName<To>(); }

 private:
  static void GetCastTypeDescription(::std::ostream* os) {
    *os << "when dynamic_cast to " << GetToName() << ", ";
  }
};

template <typename To>
class WhenDynamicCastToMatcher : public WhenDynamicCastToMatcherBase<To> {
 public:
  explicit WhenDynamicCastToMatcher(const Matcher<To>& matcher)
      : WhenDynamicCastToMatcherBase<To>(matcher) {}

  template <typename From>
  bool MatchAndExplain(From from, MatchResultListener* listener) const {
    To to = dynamic_cast<To>(from);
    return MatchPrintAndExplain(to, this->matcher_, listener);
  }
};

template <typename To>
class WhenDynamicCastToMatcher<To&> : public WhenDynamicCastToMatcherBase<To&> {
 public:
  explicit WhenDynamicCastToMatcher(const Matcher<To&>& matcher)
      : WhenDynamicCastToMatcherBase<To&>(matcher) {}

  template <typename From>
  bool MatchAndExplain(From& from, MatchResultListener* listener) const {

    To* to = dynamic_cast<To*>(&from);
    if (to == nullptr) {
      *listener << "which cannot be dynamic_cast to " << this->GetToName();
      return false;
    }
    return MatchPrintAndExplain(*to, this->matcher_, listener);
  }
};
#endif

template <typename Class, typename FieldType>
class FieldMatcher {
 public:
  FieldMatcher(FieldType Class::*field,
               const Matcher<const FieldType&>& matcher)
      : field_(field), matcher_(matcher), whose_field_("whose given field ") {}

  FieldMatcher(const std::string& field_name, FieldType Class::*field,
               const Matcher<const FieldType&>& matcher)
      : field_(field),
        matcher_(matcher),
        whose_field_("whose field `" + field_name + "` ") {}

  void DescribeTo(::std::ostream* os) const {
    *os << "is an object " << whose_field_;
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "is an object " << whose_field_;
    matcher_.DescribeNegationTo(os);
  }

  template <typename T>
  bool MatchAndExplain(const T& value, MatchResultListener* listener) const {

    return MatchAndExplainImpl(
        typename std::is_pointer<typename std::remove_const<T>::type>::type(),
        value, listener);
  }

 private:
  bool MatchAndExplainImpl(std::false_type                     ,
                           const Class& obj,
                           MatchResultListener* listener) const {
    *listener << whose_field_ << "is ";
    return MatchPrintAndExplain(obj.*field_, matcher_, listener);
  }

  bool MatchAndExplainImpl(std::true_type                 , const Class* p,
                           MatchResultListener* listener) const {
    if (p == nullptr) return false;

    *listener << "which points to an object ";

    return MatchAndExplainImpl(std::false_type(), *p, listener);
  }

  const FieldType Class::*field_;
  const Matcher<const FieldType&> matcher_;

  const std::string whose_field_;
};

template <typename Class, typename PropertyType, typename Property>
class PropertyMatcher {
 public:
  typedef const PropertyType& RefToConstProperty;

  PropertyMatcher(Property property, const Matcher<RefToConstProperty>& matcher)
      : property_(property),
        matcher_(matcher),
        whose_property_("whose given property ") {}

  PropertyMatcher(const std::string& property_name, Property property,
                  const Matcher<RefToConstProperty>& matcher)
      : property_(property),
        matcher_(matcher),
        whose_property_("whose property `" + property_name + "` ") {}

  void DescribeTo(::std::ostream* os) const {
    *os << "is an object " << whose_property_;
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "is an object " << whose_property_;
    matcher_.DescribeNegationTo(os);
  }

  template <typename T>
  bool MatchAndExplain(const T& value, MatchResultListener* listener) const {
    return MatchAndExplainImpl(
        typename std::is_pointer<typename std::remove_const<T>::type>::type(),
        value, listener);
  }

 private:
  bool MatchAndExplainImpl(std::false_type                     ,
                           const Class& obj,
                           MatchResultListener* listener) const {
    *listener << whose_property_ << "is ";

    RefToConstProperty result = (obj.*property_)();
    return MatchPrintAndExplain(result, matcher_, listener);
  }

  bool MatchAndExplainImpl(std::true_type                 , const Class* p,
                           MatchResultListener* listener) const {
    if (p == nullptr) return false;

    *listener << "which points to an object ";

    return MatchAndExplainImpl(std::false_type(), *p, listener);
  }

  Property property_;
  const Matcher<RefToConstProperty> matcher_;

  const std::string whose_property_;
};

template <typename Functor>
struct CallableTraits {
  typedef Functor StorageType;

  static void CheckIsValid(Functor              ) {}

  template <typename T>
  static auto Invoke(Functor f, const T& arg) -> decltype(f(arg)) {
    return f(arg);
  }
};

template <typename ArgType, typename ResType>
struct CallableTraits<ResType (*)(ArgType)> {
  typedef ResType ResultType;
  typedef ResType (*StorageType)(ArgType);

  static void CheckIsValid(ResType (*f)(ArgType)) {
    GTEST_CHECK_(f != nullptr)
        << "NULL function pointer is passed into ResultOf().";
  }
  template <typename T>
  static ResType Invoke(ResType (*f)(ArgType), T arg) {
    return (*f)(arg);
  }
};

template <typename Callable, typename InnerMatcher>
class ResultOfMatcher {
 public:
  ResultOfMatcher(Callable callable, InnerMatcher matcher)
      : ResultOfMatcher(                       "", std::move(callable),
                        std::move(matcher)) {}

  ResultOfMatcher(const std::string& result_description, Callable callable,
                  InnerMatcher matcher)
      : result_description_(result_description),
        callable_(std::move(callable)),
        matcher_(std::move(matcher)) {
    CallableTraits<Callable>::CheckIsValid(callable_);
  }

  template <typename T>
  operator Matcher<T>() const {
    return Matcher<T>(
        new Impl<const T&>(result_description_, callable_, matcher_));
  }

 private:
  typedef typename CallableTraits<Callable>::StorageType CallableStorageType;

  template <typename T>
  class Impl : public MatcherInterface<T> {
    using ResultType = decltype(CallableTraits<Callable>::template Invoke<T>(
        std::declval<CallableStorageType>(), std::declval<T>()));
    using InnerType = std::conditional_t<
        std::is_lvalue_reference<ResultType>::value,
        const typename std::remove_reference<ResultType>::type&, ResultType>;

   public:
    template <typename M>
    Impl(const std::string& result_description,
         const CallableStorageType& callable, const M& matcher)
        : result_description_(result_description),
          callable_(callable),
          matcher_(MatcherCast<InnerType>(matcher)) {}

    void DescribeTo(::std::ostream* os) const override {
      if (result_description_.empty()) {
        *os << "is mapped by the given callable to a value that ";
      } else {
        *os << "whose " << result_description_ << " ";
      }
      matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(::std::ostream* os) const override {
      if (result_description_.empty()) {
        *os << "is mapped by the given callable to a value that ";
      } else {
        *os << "whose " << result_description_ << " ";
      }
      matcher_.DescribeNegationTo(os);
    }

    bool MatchAndExplain(T obj, MatchResultListener* listener) const override {
      if (result_description_.empty()) {
        *listener << "which is mapped by the given callable to ";
      } else {
        *listener << "whose " << result_description_ << " is ";
      }

      InnerType result =
          CallableTraits<Callable>::template Invoke<T>(callable_, obj);
      return MatchPrintAndExplain(result, matcher_, listener);
    }

   private:
    const std::string result_description_;

    mutable CallableStorageType callable_;
    const Matcher<InnerType> matcher_;
  };

  const std::string result_description_;
  const CallableStorageType callable_;
  const InnerMatcher matcher_;
};

template <typename SizeMatcher>
class SizeIsMatcher {
 public:
  explicit SizeIsMatcher(const SizeMatcher& size_matcher)
      : size_matcher_(size_matcher) {}

  template <typename Container>
  operator Matcher<Container>() const {
    return Matcher<Container>(new Impl<const Container&>(size_matcher_));
  }

  template <typename Container>
  class Impl : public MatcherInterface<Container> {
   public:
    using SizeType = decltype(std::declval<Container>().size());
    explicit Impl(const SizeMatcher& size_matcher)
        : size_matcher_(MatcherCast<SizeType>(size_matcher)) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "has a size that ";
      size_matcher_.DescribeTo(os);
    }
    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "has a size that ";
      size_matcher_.DescribeNegationTo(os);
    }

    bool MatchAndExplain(Container container,
                         MatchResultListener* listener) const override {
      SizeType size = container.size();
      StringMatchResultListener size_listener;
      const bool result = size_matcher_.MatchAndExplain(size, &size_listener);
      *listener << "whose size " << size
                << (result ? " matches" : " doesn't match");
      PrintIfNotEmpty(size_listener.str(), listener->stream());
      return result;
    }

   private:
    const Matcher<SizeType> size_matcher_;
  };

 private:
  const SizeMatcher size_matcher_;
};

template <typename DistanceMatcher>
class BeginEndDistanceIsMatcher {
 public:
  explicit BeginEndDistanceIsMatcher(const DistanceMatcher& distance_matcher)
      : distance_matcher_(distance_matcher) {}

  template <typename Container>
  operator Matcher<Container>() const {
    return Matcher<Container>(new Impl<const Container&>(distance_matcher_));
  }

  template <typename Container>
  class Impl : public MatcherInterface<Container> {
   public:
    typedef internal::StlContainerView<GTEST_REMOVE_REFERENCE_AND_CONST_(
        Container)>
        ContainerView;
    typedef typename std::iterator_traits<
        typename ContainerView::type::const_iterator>::difference_type
        DistanceType;
    explicit Impl(const DistanceMatcher& distance_matcher)
        : distance_matcher_(MatcherCast<DistanceType>(distance_matcher)) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "distance between begin() and end() ";
      distance_matcher_.DescribeTo(os);
    }
    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "distance between begin() and end() ";
      distance_matcher_.DescribeNegationTo(os);
    }

    bool MatchAndExplain(Container container,
                         MatchResultListener* listener) const override {
      using std::begin;
      using std::end;
      DistanceType distance = std::distance(begin(container), end(container));
      StringMatchResultListener distance_listener;
      const bool result =
          distance_matcher_.MatchAndExplain(distance, &distance_listener);
      *listener << "whose distance between begin() and end() " << distance
                << (result ? " matches" : " doesn't match");
      PrintIfNotEmpty(distance_listener.str(), listener->stream());
      return result;
    }

   private:
    const Matcher<DistanceType> distance_matcher_;
  };

 private:
  const DistanceMatcher distance_matcher_;
};

template <typename Container>
class ContainerEqMatcher {
 public:
  typedef internal::StlContainerView<Container> View;
  typedef typename View::type StlContainer;
  typedef typename View::const_reference StlContainerReference;

  static_assert(!std::is_const<Container>::value,
                "Container type must not be const");
  static_assert(!std::is_reference<Container>::value,
                "Container type must not be a reference");

  explicit ContainerEqMatcher(const Container& expected)
      : expected_(View::Copy(expected)) {}

  void DescribeTo(::std::ostream* os) const {
    *os << "equals ";
    UniversalPrint(expected_, os);
  }
  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal ";
    UniversalPrint(expected_, os);
  }

  template <typename LhsContainer>
  bool MatchAndExplain(const LhsContainer& lhs,
                       MatchResultListener* listener) const {
    typedef internal::StlContainerView<
        typename std::remove_const<LhsContainer>::type>
        LhsView;
    StlContainerReference lhs_stl_container = LhsView::ConstReference(lhs);
    if (lhs_stl_container == expected_) return true;

    ::std::ostream* const os = listener->stream();
    if (os != nullptr) {

      bool printed_header = false;
      for (auto it = lhs_stl_container.begin(); it != lhs_stl_container.end();
           ++it) {
        if (internal::ArrayAwareFind(expected_.begin(), expected_.end(), *it) ==
            expected_.end()) {
          if (printed_header) {
            *os << ", ";
          } else {
            *os << "which has these unexpected elements: ";
            printed_header = true;
          }
          UniversalPrint(*it, os);
        }
      }

      bool printed_header2 = false;
      for (auto it = expected_.begin(); it != expected_.end(); ++it) {
        if (internal::ArrayAwareFind(lhs_stl_container.begin(),
                                     lhs_stl_container.end(),
                                     *it) == lhs_stl_container.end()) {
          if (printed_header2) {
            *os << ", ";
          } else {
            *os << (printed_header ? ",\nand" : "which")
                << " doesn't have these expected elements: ";
            printed_header2 = true;
          }
          UniversalPrint(*it, os);
        }
      }
    }

    return false;
  }

 private:
  const StlContainer expected_;
};

struct LessComparator {
  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    return lhs < rhs;
  }
};

template <typename Comparator, typename ContainerMatcher>
class WhenSortedByMatcher {
 public:
  WhenSortedByMatcher(const Comparator& comparator,
                      const ContainerMatcher& matcher)
      : comparator_(comparator), matcher_(matcher) {}

  template <typename LhsContainer>
  operator Matcher<LhsContainer>() const {
    return MakeMatcher(new Impl<LhsContainer>(comparator_, matcher_));
  }

  template <typename LhsContainer>
  class Impl : public MatcherInterface<LhsContainer> {
   public:
    typedef internal::StlContainerView<GTEST_REMOVE_REFERENCE_AND_CONST_(
        LhsContainer)>
        LhsView;
    typedef typename LhsView::type LhsStlContainer;
    typedef typename LhsView::const_reference LhsStlContainerReference;

    typedef
        typename RemoveConstFromKey<typename LhsStlContainer::value_type>::type
            LhsValue;

    Impl(const Comparator& comparator, const ContainerMatcher& matcher)
        : comparator_(comparator), matcher_(matcher) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "(when sorted) ";
      matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "(when sorted) ";
      matcher_.DescribeNegationTo(os);
    }

    bool MatchAndExplain(LhsContainer lhs,
                         MatchResultListener* listener) const override {
      LhsStlContainerReference lhs_stl_container = LhsView::ConstReference(lhs);
      ::std::vector<LhsValue> sorted_container(lhs_stl_container.begin(),
                                               lhs_stl_container.end());
      ::std::sort(sorted_container.begin(), sorted_container.end(),
                  comparator_);

      if (!listener->IsInterested()) {

        return matcher_.Matches(sorted_container);
      }

      *listener << "which is ";
      UniversalPrint(sorted_container, listener->stream());
      *listener << " when sorted";

      StringMatchResultListener inner_listener;
      const bool match =
          matcher_.MatchAndExplain(sorted_container, &inner_listener);
      PrintIfNotEmpty(inner_listener.str(), listener->stream());
      return match;
    }

   private:
    const Comparator comparator_;
    const Matcher<const ::std::vector<LhsValue>&> matcher_;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
  };

 private:
  const Comparator comparator_;
  const ContainerMatcher matcher_;
};

template <typename TupleMatcher, typename RhsContainer>
class PointwiseMatcher {
  static_assert(
      !IsHashTable<GTEST_REMOVE_REFERENCE_AND_CONST_(RhsContainer)>::value,
      "use UnorderedPointwise with hash tables");

 public:
  typedef internal::StlContainerView<RhsContainer> RhsView;
  typedef typename RhsView::type RhsStlContainer;
  typedef typename RhsStlContainer::value_type RhsValue;

  static_assert(!std::is_const<RhsContainer>::value,
                "RhsContainer type must not be const");
  static_assert(!std::is_reference<RhsContainer>::value,
                "RhsContainer type must not be a reference");

  PointwiseMatcher(const TupleMatcher& tuple_matcher, const RhsContainer& rhs)
      : tuple_matcher_(tuple_matcher), rhs_(RhsView::Copy(rhs)) {}

  template <typename LhsContainer>
  operator Matcher<LhsContainer>() const {
    static_assert(
        !IsHashTable<GTEST_REMOVE_REFERENCE_AND_CONST_(LhsContainer)>::value,
        "use UnorderedPointwise with hash tables");

    return Matcher<LhsContainer>(
        new Impl<const LhsContainer&>(tuple_matcher_, rhs_));
  }

  template <typename LhsContainer>
  class Impl : public MatcherInterface<LhsContainer> {
   public:
    typedef internal::StlContainerView<GTEST_REMOVE_REFERENCE_AND_CONST_(
        LhsContainer)>
        LhsView;
    typedef typename LhsView::type LhsStlContainer;
    typedef typename LhsView::const_reference LhsStlContainerReference;
    typedef typename LhsStlContainer::value_type LhsValue;

    typedef ::std::tuple<const LhsValue&, const RhsValue&> InnerMatcherArg;

    Impl(const TupleMatcher& tuple_matcher, const RhsStlContainer& rhs)

        : mono_tuple_matcher_(SafeMatcherCast<InnerMatcherArg>(tuple_matcher)),
          rhs_(rhs) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "contains " << rhs_.size()
          << " values, where each value and its corresponding value in ";
      UniversalPrinter<RhsStlContainer>::Print(rhs_, os);
      *os << " ";
      mono_tuple_matcher_.DescribeTo(os);
    }
    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "doesn't contain exactly " << rhs_.size()
          << " values, or contains a value x at some index i"
          << " where x and the i-th value of ";
      UniversalPrint(rhs_, os);
      *os << " ";
      mono_tuple_matcher_.DescribeNegationTo(os);
    }

    bool MatchAndExplain(LhsContainer lhs,
                         MatchResultListener* listener) const override {
      LhsStlContainerReference lhs_stl_container = LhsView::ConstReference(lhs);
      const size_t actual_size = lhs_stl_container.size();
      if (actual_size != rhs_.size()) {
        *listener << "which contains " << actual_size << " values";
        return false;
      }

      auto left = lhs_stl_container.begin();
      auto right = rhs_.begin();
      for (size_t i = 0; i != actual_size; ++i, ++left, ++right) {
        if (listener->IsInterested()) {
          StringMatchResultListener inner_listener;

          if (!mono_tuple_matcher_.MatchAndExplain(
                  InnerMatcherArg(ImplicitCast_<const LhsValue&>(*left),
                                  ImplicitCast_<const RhsValue&>(*right)),
                  &inner_listener)) {
            *listener << "where the value pair (";
            UniversalPrint(*left, listener->stream());
            *listener << ", ";
            UniversalPrint(*right, listener->stream());
            *listener << ") at index #" << i << " don't match";
            PrintIfNotEmpty(inner_listener.str(), listener->stream());
            return false;
          }
        } else {
          if (!mono_tuple_matcher_.Matches(
                  InnerMatcherArg(ImplicitCast_<const LhsValue&>(*left),
                                  ImplicitCast_<const RhsValue&>(*right))))
            return false;
        }
      }

      return true;
    }

   private:
    const Matcher<InnerMatcherArg> mono_tuple_matcher_;
    const RhsStlContainer rhs_;
  };

 private:
  const TupleMatcher tuple_matcher_;
  const RhsStlContainer rhs_;
};

template <typename Container>
class QuantifierMatcherImpl : public MatcherInterface<Container> {
 public:
  typedef GTEST_REMOVE_REFERENCE_AND_CONST_(Container) RawContainer;
  typedef StlContainerView<RawContainer> View;
  typedef typename View::type StlContainer;
  typedef typename View::const_reference StlContainerReference;
  typedef typename StlContainer::value_type Element;

  template <typename InnerMatcher>
  explicit QuantifierMatcherImpl(InnerMatcher inner_matcher)
      : inner_matcher_(
            testing::SafeMatcherCast<const Element&>(inner_matcher)) {}

  bool MatchAndExplainImpl(bool all_elements_should_match, Container container,
                           MatchResultListener* listener) const {
    StlContainerReference stl_container = View::ConstReference(container);
    size_t i = 0;
    for (auto it = stl_container.begin(); it != stl_container.end();
         ++it, ++i) {
      StringMatchResultListener inner_listener;
      const bool matches = inner_matcher_.MatchAndExplain(*it, &inner_listener);

      if (matches != all_elements_should_match) {
        *listener << "whose element #" << i
                  << (matches ? " matches" : " doesn't match");
        PrintIfNotEmpty(inner_listener.str(), listener->stream());
        return !all_elements_should_match;
      }
    }
    return all_elements_should_match;
  }

  bool MatchAndExplainImpl(const Matcher<size_t>& count_matcher,
                           Container container,
                           MatchResultListener* listener) const {
    StlContainerReference stl_container = View::ConstReference(container);
    size_t i = 0;
    std::vector<size_t> match_elements;
    for (auto it = stl_container.begin(); it != stl_container.end();
         ++it, ++i) {
      StringMatchResultListener inner_listener;
      const bool matches = inner_matcher_.MatchAndExplain(*it, &inner_listener);
      if (matches) {
        match_elements.push_back(i);
      }
    }
    if (listener->IsInterested()) {
      if (match_elements.empty()) {
        *listener << "has no element that matches";
      } else if (match_elements.size() == 1) {
        *listener << "whose element #" << match_elements[0] << " matches";
      } else {
        *listener << "whose elements (";
        std::string sep = "";
        for (size_t e : match_elements) {
          *listener << sep << e;
          sep = ", ";
        }
        *listener << ") match";
      }
    }
    StringMatchResultListener count_listener;
    if (count_matcher.MatchAndExplain(match_elements.size(), &count_listener)) {
      *listener << " and whose match quantity of " << match_elements.size()
                << " matches";
      PrintIfNotEmpty(count_listener.str(), listener->stream());
      return true;
    } else {
      if (match_elements.empty()) {
        *listener << " and";
      } else {
        *listener << " but";
      }
      *listener << " whose match quantity of " << match_elements.size()
                << " does not match";
      PrintIfNotEmpty(count_listener.str(), listener->stream());
      return false;
    }
  }

 protected:
  const Matcher<const Element&> inner_matcher_;
};

template <typename Container>
class ContainsMatcherImpl : public QuantifierMatcherImpl<Container> {
 public:
  template <typename InnerMatcher>
  explicit ContainsMatcherImpl(InnerMatcher inner_matcher)
      : QuantifierMatcherImpl<Container>(inner_matcher) {}

  void DescribeTo(::std::ostream* os) const override {
    *os << "contains at least one element that ";
    this->inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "doesn't contain any element that ";
    this->inner_matcher_.DescribeTo(os);
  }

  bool MatchAndExplain(Container container,
                       MatchResultListener* listener) const override {
    return this->MatchAndExplainImpl(false, container, listener);
  }
};

template <typename Container>
class EachMatcherImpl : public QuantifierMatcherImpl<Container> {
 public:
  template <typename InnerMatcher>
  explicit EachMatcherImpl(InnerMatcher inner_matcher)
      : QuantifierMatcherImpl<Container>(inner_matcher) {}

  void DescribeTo(::std::ostream* os) const override {
    *os << "only contains elements that ";
    this->inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "contains some element that ";
    this->inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(Container container,
                       MatchResultListener* listener) const override {
    return this->MatchAndExplainImpl(true, container, listener);
  }
};

template <typename Container>
class ContainsTimesMatcherImpl : public QuantifierMatcherImpl<Container> {
 public:
  template <typename InnerMatcher>
  explicit ContainsTimesMatcherImpl(InnerMatcher inner_matcher,
                                    Matcher<size_t> count_matcher)
      : QuantifierMatcherImpl<Container>(inner_matcher),
        count_matcher_(std::move(count_matcher)) {}

  void DescribeTo(::std::ostream* os) const override {
    *os << "quantity of elements that match ";
    this->inner_matcher_.DescribeTo(os);
    *os << " ";
    count_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "quantity of elements that match ";
    this->inner_matcher_.DescribeTo(os);
    *os << " ";
    count_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(Container container,
                       MatchResultListener* listener) const override {
    return this->MatchAndExplainImpl(count_matcher_, container, listener);
  }

 private:
  const Matcher<size_t> count_matcher_;
};

template <typename M>
class ContainsTimesMatcher {
 public:
  explicit ContainsTimesMatcher(M m, Matcher<size_t> count_matcher)
      : inner_matcher_(m), count_matcher_(std::move(count_matcher)) {}

  template <typename Container>
  operator Matcher<Container>() const {
    return Matcher<Container>(new ContainsTimesMatcherImpl<const Container&>(
        inner_matcher_, count_matcher_));
  }

 private:
  const M inner_matcher_;
  const Matcher<size_t> count_matcher_;
};

template <typename M>
class ContainsMatcher {
 public:
  explicit ContainsMatcher(M m) : inner_matcher_(m) {}

  template <typename Container>
  operator Matcher<Container>() const {
    return Matcher<Container>(
        new ContainsMatcherImpl<const Container&>(inner_matcher_));
  }

  ContainsTimesMatcher<M> Times(Matcher<size_t> count_matcher) const {
    return ContainsTimesMatcher<M>(inner_matcher_, std::move(count_matcher));
  }

 private:
  const M inner_matcher_;
};

template <typename M>
class EachMatcher {
 public:
  explicit EachMatcher(M m) : inner_matcher_(m) {}

  template <typename Container>
  operator Matcher<Container>() const {
    return Matcher<Container>(
        new EachMatcherImpl<const Container&>(inner_matcher_));
  }

 private:
  const M inner_matcher_;
};

namespace pair_getters {
using std::get;
template <typename T>
auto First(T& x, Rank0) -> decltype(get<0>(x)) {
  return get<0>(x);
}
template <typename T>
auto First(T& x, Rank1) -> decltype((x.first)) {
  return x.first;
}

template <typename T>
auto Second(T& x, Rank0) -> decltype(get<1>(x)) {
  return get<1>(x);
}
template <typename T>
auto Second(T& x, Rank1) -> decltype((x.second)) {
  return x.second;
}
}

template <typename PairType>
class KeyMatcherImpl : public MatcherInterface<PairType> {
 public:
  typedef GTEST_REMOVE_REFERENCE_AND_CONST_(PairType) RawPairType;
  typedef typename RawPairType::first_type KeyType;

  template <typename InnerMatcher>
  explicit KeyMatcherImpl(InnerMatcher inner_matcher)
      : inner_matcher_(
            testing::SafeMatcherCast<const KeyType&>(inner_matcher)) {}

  bool MatchAndExplain(PairType key_value,
                       MatchResultListener* listener) const override {
    StringMatchResultListener inner_listener;
    const bool match = inner_matcher_.MatchAndExplain(
        pair_getters::First(key_value, Rank1()), &inner_listener);
    const std::string explanation = inner_listener.str();
    if (!explanation.empty()) {
      *listener << "whose first field is a value " << explanation;
    }
    return match;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "has a key that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "doesn't have a key that ";
    inner_matcher_.DescribeTo(os);
  }

 private:
  const Matcher<const KeyType&> inner_matcher_;
};

template <typename M>
class KeyMatcher {
 public:
  explicit KeyMatcher(M m) : matcher_for_key_(m) {}

  template <typename PairType>
  operator Matcher<PairType>() const {
    return Matcher<PairType>(
        new KeyMatcherImpl<const PairType&>(matcher_for_key_));
  }

 private:
  const M matcher_for_key_;
};

template <typename InnerMatcher>
class AddressMatcher {
 public:
  explicit AddressMatcher(InnerMatcher m) : matcher_(m) {}

  template <typename Type>
  operator Matcher<Type>() const {
    return Matcher<Type>(new Impl<const Type&>(matcher_));
  }

 private:

  template <typename Type>
  class Impl : public MatcherInterface<Type> {
   public:
    using Address = const GTEST_REMOVE_REFERENCE_AND_CONST_(Type) *;
    explicit Impl(const InnerMatcher& matcher)
        : matcher_(MatcherCast<Address>(matcher)) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "has address that ";
      matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "does not have address that ";
      matcher_.DescribeTo(os);
    }

    bool MatchAndExplain(Type object,
                         MatchResultListener* listener) const override {
      *listener << "which has address ";
      Address address = std::addressof(object);
      return MatchPrintAndExplain(address, matcher_, listener);
    }

   private:
    const Matcher<Address> matcher_;
  };
  const InnerMatcher matcher_;
};

template <typename PairType>
class PairMatcherImpl : public MatcherInterface<PairType> {
 public:
  typedef GTEST_REMOVE_REFERENCE_AND_CONST_(PairType) RawPairType;
  typedef typename RawPairType::first_type FirstType;
  typedef typename RawPairType::second_type SecondType;

  template <typename FirstMatcher, typename SecondMatcher>
  PairMatcherImpl(FirstMatcher first_matcher, SecondMatcher second_matcher)
      : first_matcher_(
            testing::SafeMatcherCast<const FirstType&>(first_matcher)),
        second_matcher_(
            testing::SafeMatcherCast<const SecondType&>(second_matcher)) {}

  void DescribeTo(::std::ostream* os) const override {
    *os << "has a first field that ";
    first_matcher_.DescribeTo(os);
    *os << ", and has a second field that ";
    second_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "has a first field that ";
    first_matcher_.DescribeNegationTo(os);
    *os << ", or has a second field that ";
    second_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(PairType a_pair,
                       MatchResultListener* listener) const override {
    if (!listener->IsInterested()) {

      return first_matcher_.Matches(pair_getters::First(a_pair, Rank1())) &&
             second_matcher_.Matches(pair_getters::Second(a_pair, Rank1()));
    }
    StringMatchResultListener first_inner_listener;
    if (!first_matcher_.MatchAndExplain(pair_getters::First(a_pair, Rank1()),
                                        &first_inner_listener)) {
      *listener << "whose first field does not match";
      PrintIfNotEmpty(first_inner_listener.str(), listener->stream());
      return false;
    }
    StringMatchResultListener second_inner_listener;
    if (!second_matcher_.MatchAndExplain(pair_getters::Second(a_pair, Rank1()),
                                         &second_inner_listener)) {
      *listener << "whose second field does not match";
      PrintIfNotEmpty(second_inner_listener.str(), listener->stream());
      return false;
    }
    ExplainSuccess(first_inner_listener.str(), second_inner_listener.str(),
                   listener);
    return true;
  }

 private:
  void ExplainSuccess(const std::string& first_explanation,
                      const std::string& second_explanation,
                      MatchResultListener* listener) const {
    *listener << "whose both fields match";
    if (!first_explanation.empty()) {
      *listener << ", where the first field is a value " << first_explanation;
    }
    if (!second_explanation.empty()) {
      *listener << ", ";
      if (!first_explanation.empty()) {
        *listener << "and ";
      } else {
        *listener << "where ";
      }
      *listener << "the second field is a value " << second_explanation;
    }
  }

  const Matcher<const FirstType&> first_matcher_;
  const Matcher<const SecondType&> second_matcher_;
};

template <typename FirstMatcher, typename SecondMatcher>
class PairMatcher {
 public:
  PairMatcher(FirstMatcher first_matcher, SecondMatcher second_matcher)
      : first_matcher_(first_matcher), second_matcher_(second_matcher) {}

  template <typename PairType>
  operator Matcher<PairType>() const {
    return Matcher<PairType>(
        new PairMatcherImpl<const PairType&>(first_matcher_, second_matcher_));
  }

 private:
  const FirstMatcher first_matcher_;
  const SecondMatcher second_matcher_;
};

template <typename T, size_t... I>
auto UnpackStructImpl(const T& t, std::index_sequence<I...>,
                      int) -> decltype(std::tie(get<I>(t)...)) {
  static_assert(std::tuple_size<T>::value == sizeof...(I),
                "Number of arguments doesn't match the number of fields.");
  return std::tie(get<I>(t)...);
}

#if defined(__cpp_structured_bindings) && __cpp_structured_bindings >= 201606
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<1>, char) {
  const auto& [a] = t;
  return std::tie(a);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<2>, char) {
  const auto& [a, b] = t;
  return std::tie(a, b);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<3>, char) {
  const auto& [a, b, c] = t;
  return std::tie(a, b, c);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<4>, char) {
  const auto& [a, b, c, d] = t;
  return std::tie(a, b, c, d);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<5>, char) {
  const auto& [a, b, c, d, e] = t;
  return std::tie(a, b, c, d, e);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<6>, char) {
  const auto& [a, b, c, d, e, f] = t;
  return std::tie(a, b, c, d, e, f);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<7>, char) {
  const auto& [a, b, c, d, e, f, g] = t;
  return std::tie(a, b, c, d, e, f, g);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<8>, char) {
  const auto& [a, b, c, d, e, f, g, h] = t;
  return std::tie(a, b, c, d, e, f, g, h);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<9>, char) {
  const auto& [a, b, c, d, e, f, g, h, i] = t;
  return std::tie(a, b, c, d, e, f, g, h, i);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<10>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<11>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<12>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<13>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l, m] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l, m);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<14>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<15>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<16>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<17>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<18>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r);
}
template <typename T>
auto UnpackStructImpl(const T& t, std::make_index_sequence<19>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s] = t;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s);
}
template <typename T>
auto UnpackStructImpl(const T& u, std::make_index_sequence<20>, char) {
  const auto& [a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t] = u;
  return std::tie(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t);
}
#endif

template <size_t I, typename T>
auto UnpackStruct(const T& t)
    -> decltype((UnpackStructImpl)(t, std::make_index_sequence<I>{}, 0)) {
  return (UnpackStructImpl)(t, std::make_index_sequence<I>{}, 0);
}

template <typename T, size_t N>
void VariadicExpand(const T (&)[N]) {}

template <typename Struct, typename StructSize>
class FieldsAreMatcherImpl;

template <typename Struct, size_t... I>
class FieldsAreMatcherImpl<Struct, std::index_sequence<I...>>
    : public MatcherInterface<Struct> {
  using UnpackedType =
      decltype(UnpackStruct<sizeof...(I)>(std::declval<const Struct&>()));
  using MatchersType = std::tuple<
      Matcher<const typename std::tuple_element<I, UnpackedType>::type&>...>;

 public:
  template <typename Inner>
  explicit FieldsAreMatcherImpl(const Inner& matchers)
      : matchers_(testing::SafeMatcherCast<
                  const typename std::tuple_element<I, UnpackedType>::type&>(
            std::get<I>(matchers))...) {}

  void DescribeTo(::std::ostream* os) const override {
    const char* separator = "";
    VariadicExpand(
        {(*os << separator << "has field #" << I << " that ",
          std::get<I>(matchers_).DescribeTo(os), separator = ", and ")...});
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    const char* separator = "";
    VariadicExpand({(*os << separator << "has field #" << I << " that ",
                     std::get<I>(matchers_).DescribeNegationTo(os),
                     separator = ", or ")...});
  }

  bool MatchAndExplain(Struct t, MatchResultListener* listener) const override {
    return MatchInternal((UnpackStruct<sizeof...(I)>)(t), listener);
  }

 private:
  bool MatchInternal(UnpackedType tuple, MatchResultListener* listener) const {
    if (!listener->IsInterested()) {

      bool good = true;
      VariadicExpand({good = good && std::get<I>(matchers_).Matches(
                                         std::get<I>(tuple))...});
      return good;
    }

    size_t failed_pos = ~size_t{};

    std::vector<StringMatchResultListener> inner_listener(sizeof...(I));

    VariadicExpand(
        {failed_pos == ~size_t{} && !std::get<I>(matchers_).MatchAndExplain(
                                        std::get<I>(tuple), &inner_listener[I])
             ? failed_pos = I
             : 0 ...});
    if (failed_pos != ~size_t{}) {
      *listener << "whose field #" << failed_pos << " does not match";
      PrintIfNotEmpty(inner_listener[failed_pos].str(), listener->stream());
      return false;
    }

    *listener << "whose all elements match";
    const char* separator = ", where";
    for (size_t index = 0; index < sizeof...(I); ++index) {
      const std::string str = inner_listener[index].str();
      if (!str.empty()) {
        *listener << separator << " field #" << index << " is a value " << str;
        separator = ", and";
      }
    }

    return true;
  }

  MatchersType matchers_;
};

template <typename... Inner>
class FieldsAreMatcher {
 public:
  explicit FieldsAreMatcher(Inner... inner) : matchers_(std::move(inner)...) {}

  template <typename Struct>
  operator Matcher<Struct>() const {
    return Matcher<Struct>(
        new FieldsAreMatcherImpl<const Struct&,
                                 std::index_sequence_for<Inner...>>(matchers_));
  }

 private:
  std::tuple<Inner...> matchers_;
};

template <typename Container>
class ElementsAreMatcherImpl : public MatcherInterface<Container> {
 public:
  typedef GTEST_REMOVE_REFERENCE_AND_CONST_(Container) RawContainer;
  typedef internal::StlContainerView<RawContainer> View;
  typedef typename View::type StlContainer;
  typedef typename View::const_reference StlContainerReference;
  typedef typename StlContainer::value_type Element;

  template <typename InputIter>
  ElementsAreMatcherImpl(InputIter first, InputIter last) {
    while (first != last) {
      matchers_.push_back(MatcherCast<const Element&>(*first++));
    }
  }

  void DescribeTo(::std::ostream* os) const override {
    if (count() == 0) {
      *os << "is empty";
    } else if (count() == 1) {
      *os << "has 1 element that ";
      matchers_[0].DescribeTo(os);
    } else {
      *os << "has " << Elements(count()) << " where\n";
      for (size_t i = 0; i != count(); ++i) {
        *os << "element #" << i << " ";
        matchers_[i].DescribeTo(os);
        if (i + 1 < count()) {
          *os << ",\n";
        }
      }
    }
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    if (count() == 0) {
      *os << "isn't empty";
      return;
    }

    *os << "doesn't have " << Elements(count()) << ", or\n";
    for (size_t i = 0; i != count(); ++i) {
      *os << "element #" << i << " ";
      matchers_[i].DescribeNegationTo(os);
      if (i + 1 < count()) {
        *os << ", or\n";
      }
    }
  }

  bool MatchAndExplain(Container container,
                       MatchResultListener* listener) const override {

    const bool listener_interested = listener->IsInterested();

    ::std::vector<std::string> explanations(count());
    StlContainerReference stl_container = View::ConstReference(container);
    auto it = stl_container.begin();
    size_t exam_pos = 0;
    bool mismatch_found = false;

    for (; it != stl_container.end() && exam_pos != count(); ++it, ++exam_pos) {
      bool match;
      if (listener_interested) {
        StringMatchResultListener s;
        match = matchers_[exam_pos].MatchAndExplain(*it, &s);
        explanations[exam_pos] = s.str();
      } else {
        match = matchers_[exam_pos].Matches(*it);
      }

      if (!match) {
        mismatch_found = true;
        break;
      }
    }

    size_t actual_count = exam_pos;
    for (; it != stl_container.end(); ++it) {
      ++actual_count;
    }

    if (actual_count != count()) {

      if (listener_interested && (actual_count != 0)) {
        *listener << "which has " << Elements(actual_count);
      }
      return false;
    }

    if (mismatch_found) {

      if (listener_interested) {
        *listener << "whose element #" << exam_pos << " doesn't match";
        PrintIfNotEmpty(explanations[exam_pos], listener->stream());
      }
      return false;
    }

    if (listener_interested) {
      bool reason_printed = false;
      for (size_t i = 0; i != count(); ++i) {
        const std::string& s = explanations[i];
        if (!s.empty()) {
          if (reason_printed) {
            *listener << ",\nand ";
          }
          *listener << "whose element #" << i << " matches, " << s;
          reason_printed = true;
        }
      }
    }
    return true;
  }

 private:
  static Message Elements(size_t count) {
    return Message() << count << (count == 1 ? " element" : " elements");
  }

  size_t count() const { return matchers_.size(); }

  ::std::vector<Matcher<const Element&>> matchers_;
};

class GTEST_API_ MatchMatrix {
 public:
  MatchMatrix(size_t num_elements, size_t num_matchers)
      : num_elements_(num_elements),
        num_matchers_(num_matchers),
        matched_(num_elements_ * num_matchers_, 0) {}

  size_t LhsSize() const { return num_elements_; }
  size_t RhsSize() const { return num_matchers_; }
  bool HasEdge(size_t ilhs, size_t irhs) const {
    return matched_[SpaceIndex(ilhs, irhs)] == 1;
  }
  void SetEdge(size_t ilhs, size_t irhs, bool b) {
    matched_[SpaceIndex(ilhs, irhs)] = b ? 1 : 0;
  }

  bool NextGraph();

  void Randomize();

  std::string DebugString() const;

 private:
  size_t SpaceIndex(size_t ilhs, size_t irhs) const {
    return ilhs * num_matchers_ + irhs;
  }

  size_t num_elements_;
  size_t num_matchers_;

  ::std::vector<char> matched_;
};

typedef ::std::pair<size_t, size_t> ElementMatcherPair;
typedef ::std::vector<ElementMatcherPair> ElementMatcherPairs;

GTEST_API_ ElementMatcherPairs FindMaxBipartiteMatching(const MatchMatrix& g);

struct UnorderedMatcherRequire {
  enum Flags {
    Superset = 1 << 0,
    Subset = 1 << 1,
    ExactMatch = Superset | Subset,
  };
};

class GTEST_API_ UnorderedElementsAreMatcherImplBase {
 protected:
  explicit UnorderedElementsAreMatcherImplBase(
      UnorderedMatcherRequire::Flags matcher_flags)
      : match_flags_(matcher_flags) {}

  typedef ::std::vector<const MatcherDescriberInterface*> MatcherDescriberVec;

  void DescribeToImpl(::std::ostream* os) const;

  void DescribeNegationToImpl(::std::ostream* os) const;

  bool VerifyMatchMatrix(const ::std::vector<std::string>& element_printouts,
                         const MatchMatrix& matrix,
                         MatchResultListener* listener) const;

  bool FindPairing(const MatchMatrix& matrix,
                   MatchResultListener* listener) const;

  MatcherDescriberVec& matcher_describers() { return matcher_describers_; }

  static Message Elements(size_t n) {
    return Message() << n << " element" << (n == 1 ? "" : "s");
  }

  UnorderedMatcherRequire::Flags match_flags() const { return match_flags_; }

 private:
  UnorderedMatcherRequire::Flags match_flags_;
  MatcherDescriberVec matcher_describers_;
};

template <typename Container>
class UnorderedElementsAreMatcherImpl
    : public MatcherInterface<Container>,
      public UnorderedElementsAreMatcherImplBase {
 public:
  typedef GTEST_REMOVE_REFERENCE_AND_CONST_(Container) RawContainer;
  typedef internal::StlContainerView<RawContainer> View;
  typedef typename View::type StlContainer;
  typedef typename View::const_reference StlContainerReference;
  typedef typename StlContainer::value_type Element;

  template <typename InputIter>
  UnorderedElementsAreMatcherImpl(UnorderedMatcherRequire::Flags matcher_flags,
                                  InputIter first, InputIter last)
      : UnorderedElementsAreMatcherImplBase(matcher_flags) {
    for (; first != last; ++first) {
      matchers_.push_back(MatcherCast<const Element&>(*first));
    }
    for (const auto& m : matchers_) {
      matcher_describers().push_back(m.GetDescriber());
    }
  }

  void DescribeTo(::std::ostream* os) const override {
    return UnorderedElementsAreMatcherImplBase::DescribeToImpl(os);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    return UnorderedElementsAreMatcherImplBase::DescribeNegationToImpl(os);
  }

  bool MatchAndExplain(Container container,
                       MatchResultListener* listener) const override {
    StlContainerReference stl_container = View::ConstReference(container);
    ::std::vector<std::string> element_printouts;
    MatchMatrix matrix =
        AnalyzeElements(stl_container.begin(), stl_container.end(),
                        &element_printouts, listener);

    return VerifyMatchMatrix(element_printouts, matrix, listener) &&
           FindPairing(matrix, listener);
  }

 private:
  template <typename ElementIter>
  MatchMatrix AnalyzeElements(ElementIter elem_first, ElementIter elem_last,
                              ::std::vector<std::string>* element_printouts,
                              MatchResultListener* listener) const {
    element_printouts->clear();
    ::std::vector<char> did_match;
    size_t num_elements = 0;
    DummyMatchResultListener dummy;
    for (; elem_first != elem_last; ++num_elements, ++elem_first) {
      if (listener->IsInterested()) {
        element_printouts->push_back(PrintToString(*elem_first));
      }
      for (size_t irhs = 0; irhs != matchers_.size(); ++irhs) {
        did_match.push_back(
            matchers_[irhs].MatchAndExplain(*elem_first, &dummy));
      }
    }

    MatchMatrix matrix(num_elements, matchers_.size());
    ::std::vector<char>::const_iterator did_match_iter = did_match.begin();
    for (size_t ilhs = 0; ilhs != num_elements; ++ilhs) {
      for (size_t irhs = 0; irhs != matchers_.size(); ++irhs) {
        matrix.SetEdge(ilhs, irhs, *did_match_iter++ != 0);
      }
    }
    return matrix;
  }

  ::std::vector<Matcher<const Element&>> matchers_;
};

template <typename Target>
struct CastAndAppendTransform {
  template <typename Arg>
  Matcher<Target> operator()(const Arg& a) const {
    return MatcherCast<Target>(a);
  }
};

template <typename MatcherTuple>
class UnorderedElementsAreMatcher {
 public:
  explicit UnorderedElementsAreMatcher(const MatcherTuple& args)
      : matchers_(args) {}

  template <typename Container>
  operator Matcher<Container>() const {
    typedef GTEST_REMOVE_REFERENCE_AND_CONST_(Container) RawContainer;
    typedef typename internal::StlContainerView<RawContainer>::type View;
    typedef typename View::value_type Element;
    typedef ::std::vector<Matcher<const Element&>> MatcherVec;
    MatcherVec matchers;
    matchers.reserve(::std::tuple_size<MatcherTuple>::value);
    TransformTupleValues(CastAndAppendTransform<const Element&>(), matchers_,
                         ::std::back_inserter(matchers));
    return Matcher<Container>(
        new UnorderedElementsAreMatcherImpl<const Container&>(
            UnorderedMatcherRequire::ExactMatch, matchers.begin(),
            matchers.end()));
  }

 private:
  const MatcherTuple matchers_;
};

template <typename MatcherTuple>
class ElementsAreMatcher {
 public:
  explicit ElementsAreMatcher(const MatcherTuple& args) : matchers_(args) {}

  template <typename Container>
  operator Matcher<Container>() const {
    static_assert(
        !IsHashTable<GTEST_REMOVE_REFERENCE_AND_CONST_(Container)>::value ||
            ::std::tuple_size<MatcherTuple>::value < 2,
        "use UnorderedElementsAre with hash tables");

    typedef GTEST_REMOVE_REFERENCE_AND_CONST_(Container) RawContainer;
    typedef typename internal::StlContainerView<RawContainer>::type View;
    typedef typename View::value_type Element;
    typedef ::std::vector<Matcher<const Element&>> MatcherVec;
    MatcherVec matchers;
    matchers.reserve(::std::tuple_size<MatcherTuple>::value);
    TransformTupleValues(CastAndAppendTransform<const Element&>(), matchers_,
                         ::std::back_inserter(matchers));
    return Matcher<Container>(new ElementsAreMatcherImpl<const Container&>(
        matchers.begin(), matchers.end()));
  }

 private:
  const MatcherTuple matchers_;
};

template <typename T>
class UnorderedElementsAreArrayMatcher {
 public:
  template <typename Iter>
  UnorderedElementsAreArrayMatcher(UnorderedMatcherRequire::Flags match_flags,
                                   Iter first, Iter last)
      : match_flags_(match_flags), matchers_(first, last) {}

  template <typename Container>
  operator Matcher<Container>() const {
    return Matcher<Container>(
        new UnorderedElementsAreMatcherImpl<const Container&>(
            match_flags_, matchers_.begin(), matchers_.end()));
  }

 private:
  UnorderedMatcherRequire::Flags match_flags_;
  std::vector<std::remove_const_t<T>> matchers_;
};

template <typename T>
class ElementsAreArrayMatcher {
 public:
  template <typename Iter>
  ElementsAreArrayMatcher(Iter first, Iter last) : matchers_(first, last) {}

  template <typename Container>
  operator Matcher<Container>() const {
    static_assert(
        !IsHashTable<GTEST_REMOVE_REFERENCE_AND_CONST_(Container)>::value,
        "use UnorderedElementsAreArray with hash tables");

    return Matcher<Container>(new ElementsAreMatcherImpl<const Container&>(
        matchers_.begin(), matchers_.end()));
  }

 private:
  const std::vector<std::remove_const_t<T>> matchers_;
};

template <typename Tuple2Matcher, typename Second>
class BoundSecondMatcher {
 public:
  BoundSecondMatcher(const Tuple2Matcher& tm, const Second& second)
      : tuple2_matcher_(tm), second_value_(second) {}

  BoundSecondMatcher(const BoundSecondMatcher& other) = default;

  template <typename T>
  operator Matcher<T>() const {
    return MakeMatcher(new Impl<T>(tuple2_matcher_, second_value_));
  }

  void operator=(const BoundSecondMatcher&        ) {
    GTEST_LOG_(FATAL) << "BoundSecondMatcher should never be assigned.";
  }

 private:
  template <typename T>
  class Impl : public MatcherInterface<T> {
   public:
    typedef ::std::tuple<T, Second> ArgTuple;

    Impl(const Tuple2Matcher& tm, const Second& second)
        : mono_tuple2_matcher_(SafeMatcherCast<const ArgTuple&>(tm)),
          second_value_(second) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "and ";
      UniversalPrint(second_value_, os);
      *os << " ";
      mono_tuple2_matcher_.DescribeTo(os);
    }

    bool MatchAndExplain(T x, MatchResultListener* listener) const override {
      return mono_tuple2_matcher_.MatchAndExplain(ArgTuple(x, second_value_),
                                                  listener);
    }

   private:
    const Matcher<const ArgTuple&> mono_tuple2_matcher_;
    const Second second_value_;
  };

  const Tuple2Matcher tuple2_matcher_;
  const Second second_value_;
};

template <typename Tuple2Matcher, typename Second>
BoundSecondMatcher<Tuple2Matcher, Second> MatcherBindSecond(
    const Tuple2Matcher& tm, const Second& second) {
  return BoundSecondMatcher<Tuple2Matcher, Second>(tm, second);
}

GTEST_API_ std::string FormatMatcherDescription(
    bool negation, const char* matcher_name,
    const std::vector<const char*>& param_names, const Strings& param_values);

template <typename Optional>
auto IsOptionalEngaged(const Optional& optional,
                       Rank1) -> decltype(!!optional) {

  return !static_cast<bool>(!optional);
}
template <typename Optional>
auto IsOptionalEngaged(const Optional& optional,
                       Rank0) -> decltype(!optional.has_value()) {
  return optional.has_value();
}

template <typename ValueMatcher>
class OptionalMatcher {
 public:
  explicit OptionalMatcher(const ValueMatcher& value_matcher)
      : value_matcher_(value_matcher) {}

  template <typename Optional>
  operator Matcher<Optional>() const {
    return Matcher<Optional>(new Impl<const Optional&>(value_matcher_));
  }

  template <typename Optional>
  class Impl : public MatcherInterface<Optional> {
   public:
    typedef GTEST_REMOVE_REFERENCE_AND_CONST_(Optional) OptionalView;
    typedef typename OptionalView::value_type ValueType;
    explicit Impl(const ValueMatcher& value_matcher)
        : value_matcher_(MatcherCast<ValueType>(value_matcher)) {}

    void DescribeTo(::std::ostream* os) const override {
      *os << "value ";
      value_matcher_.DescribeTo(os);
    }

    void DescribeNegationTo(::std::ostream* os) const override {
      *os << "value ";
      value_matcher_.DescribeNegationTo(os);
    }

    bool MatchAndExplain(Optional optional,
                         MatchResultListener* listener) const override {
      if (!IsOptionalEngaged(optional, HighestRank())) {
        *listener << "which is not engaged";
        return false;
      }
      const ValueType& value = *optional;
      StringMatchResultListener value_listener;
      const bool match = value_matcher_.MatchAndExplain(value, &value_listener);
      *listener << "whose value " << PrintToString(value)
                << (match ? " matches" : " doesn't match");
      PrintIfNotEmpty(value_listener.str(), listener->stream());
      return match;
    }

   private:
    const Matcher<ValueType> value_matcher_;
  };

 private:
  const ValueMatcher value_matcher_;
};

namespace variant_matcher {

template <typename T>
void holds_alternative() {}
template <typename T>
void get() {}

template <typename T>
class VariantMatcher {
 public:
  explicit VariantMatcher(::testing::Matcher<const T&> matcher)
      : matcher_(std::move(matcher)) {}

  template <typename Variant>
  bool MatchAndExplain(const Variant& value,
                       ::testing::MatchResultListener* listener) const {
    using std::get;
    if (!listener->IsInterested()) {
      return holds_alternative<T>(value) && matcher_.Matches(get<T>(value));
    }

    if (!holds_alternative<T>(value)) {
      *listener << "whose value is not of type '" << GetTypeName() << "'";
      return false;
    }

    const T& elem = get<T>(value);
    StringMatchResultListener elem_listener;
    const bool match = matcher_.MatchAndExplain(elem, &elem_listener);
    *listener << "whose value " << PrintToString(elem)
              << (match ? " matches" : " doesn't match");
    PrintIfNotEmpty(elem_listener.str(), listener->stream());
    return match;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is a variant<> with value of type '" << GetTypeName()
        << "' and the value ";
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "is a variant<> with value of type other than '" << GetTypeName()
        << "' or the value ";
    matcher_.DescribeNegationTo(os);
  }

 private:
  static std::string GetTypeName() {
#if GTEST_HAS_RTTI
    GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(
        return internal::GetTypeName<T>());
#endif
    return "the element type";
  }

  const ::testing::Matcher<const T&> matcher_;
};

}

namespace any_cast_matcher {

template <typename T>
void any_cast() {}

template <typename T>
class AnyCastMatcher {
 public:
  explicit AnyCastMatcher(const ::testing::Matcher<const T&>& matcher)
      : matcher_(matcher) {}

  template <typename AnyType>
  bool MatchAndExplain(const AnyType& value,
                       ::testing::MatchResultListener* listener) const {
    if (!listener->IsInterested()) {
      const T* ptr = any_cast<T>(&value);
      return ptr != nullptr && matcher_.Matches(*ptr);
    }

    const T* elem = any_cast<T>(&value);
    if (elem == nullptr) {
      *listener << "whose value is not of type '" << GetTypeName() << "'";
      return false;
    }

    StringMatchResultListener elem_listener;
    const bool match = matcher_.MatchAndExplain(*elem, &elem_listener);
    *listener << "whose value " << PrintToString(*elem)
              << (match ? " matches" : " doesn't match");
    PrintIfNotEmpty(elem_listener.str(), listener->stream());
    return match;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is an 'any' type with value of type '" << GetTypeName()
        << "' and the value ";
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "is an 'any' type with value of type other than '" << GetTypeName()
        << "' or the value ";
    matcher_.DescribeNegationTo(os);
  }

 private:
  static std::string GetTypeName() {
#if GTEST_HAS_RTTI
    GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(
        return internal::GetTypeName<T>());
#endif
    return "the element type";
  }

  const ::testing::Matcher<const T&> matcher_;
};

}

template <class ArgsTuple, size_t... k>
class ArgsMatcherImpl : public MatcherInterface<ArgsTuple> {
 public:
  using RawArgsTuple = typename std::decay<ArgsTuple>::type;
  using SelectedArgs =
      std::tuple<typename std::tuple_element<k, RawArgsTuple>::type...>;
  using MonomorphicInnerMatcher = Matcher<const SelectedArgs&>;

  template <typename InnerMatcher>
  explicit ArgsMatcherImpl(const InnerMatcher& inner_matcher)
      : inner_matcher_(SafeMatcherCast<const SelectedArgs&>(inner_matcher)) {}

  bool MatchAndExplain(ArgsTuple args,
                       MatchResultListener* listener) const override {

    (void)args;
    const SelectedArgs& selected_args =
        std::forward_as_tuple(std::get<k>(args)...);
    if (!listener->IsInterested()) return inner_matcher_.Matches(selected_args);

    PrintIndices(listener->stream());
    *listener << "are " << PrintToString(selected_args);

    StringMatchResultListener inner_listener;
    const bool match =
        inner_matcher_.MatchAndExplain(selected_args, &inner_listener);
    PrintIfNotEmpty(inner_listener.str(), listener->stream());
    return match;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "are a tuple ";
    PrintIndices(os);
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "are a tuple ";
    PrintIndices(os);
    inner_matcher_.DescribeNegationTo(os);
  }

 private:

  static void PrintIndices(::std::ostream* os) {
    *os << "whose fields (";
    const char* sep = "";

    (void)sep;

    const char* dummy[] = {
        "", (static_cast<void>(*os << sep << "#" << k), sep = ", ")...};
    (void)dummy;
    *os << ") ";
  }

  MonomorphicInnerMatcher inner_matcher_;
};

template <class InnerMatcher, size_t... k>
class ArgsMatcher {
 public:
  explicit ArgsMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  template <typename ArgsTuple>
  operator Matcher<ArgsTuple>() const {
    return MakeMatcher(new ArgsMatcherImpl<ArgsTuple, k...>(inner_matcher_));
  }

 private:
  InnerMatcher inner_matcher_;
};

}

template <typename Iter>
inline internal::ElementsAreArrayMatcher<
    typename ::std::iterator_traits<Iter>::value_type>
ElementsAreArray(Iter first, Iter last) {
  typedef typename ::std::iterator_traits<Iter>::value_type T;
  return internal::ElementsAreArrayMatcher<T>(first, last);
}

template <typename T>
inline auto ElementsAreArray(const T* pointer, size_t count)
    -> decltype(ElementsAreArray(pointer, pointer + count)) {
  return ElementsAreArray(pointer, pointer + count);
}

template <typename T, size_t N>
inline auto ElementsAreArray(const T (&array)[N])
    -> decltype(ElementsAreArray(array, N)) {
  return ElementsAreArray(array, N);
}

template <typename Container>
inline auto ElementsAreArray(const Container& container)
    -> decltype(ElementsAreArray(container.begin(), container.end())) {
  return ElementsAreArray(container.begin(), container.end());
}

template <typename T>
inline auto ElementsAreArray(::std::initializer_list<T> xs)
    -> decltype(ElementsAreArray(xs.begin(), xs.end())) {
  return ElementsAreArray(xs.begin(), xs.end());
}

template <typename Iter>
inline internal::UnorderedElementsAreArrayMatcher<
    typename ::std::iterator_traits<Iter>::value_type>
UnorderedElementsAreArray(Iter first, Iter last) {
  typedef typename ::std::iterator_traits<Iter>::value_type T;
  return internal::UnorderedElementsAreArrayMatcher<T>(
      internal::UnorderedMatcherRequire::ExactMatch, first, last);
}

template <typename T>
inline internal::UnorderedElementsAreArrayMatcher<T> UnorderedElementsAreArray(
    const T* pointer, size_t count) {
  return UnorderedElementsAreArray(pointer, pointer + count);
}

template <typename T, size_t N>
inline internal::UnorderedElementsAreArrayMatcher<T> UnorderedElementsAreArray(
    const T (&array)[N]) {
  return UnorderedElementsAreArray(array, N);
}

template <typename Container>
inline internal::UnorderedElementsAreArrayMatcher<
    typename Container::value_type>
UnorderedElementsAreArray(const Container& container) {
  return UnorderedElementsAreArray(container.begin(), container.end());
}

template <typename T>
inline internal::UnorderedElementsAreArrayMatcher<T> UnorderedElementsAreArray(
    ::std::initializer_list<T> xs) {
  return UnorderedElementsAreArray(xs.begin(), xs.end());
}

const internal::AnythingMatcher _ = {};

template <typename T>
inline Matcher<T> A() {
  return _;
}

template <typename T>
inline Matcher<T> An() {
  return _;
}

template <typename T, typename M>
Matcher<T> internal::MatcherCastImpl<T, M>::CastImpl(
    const M& value, std::false_type                             ,
    std::false_type                       ) {
  return Eq(value);
}

inline PolymorphicMatcher<internal::IsNullMatcher> IsNull() {
  return MakePolymorphicMatcher(internal::IsNullMatcher());
}

inline PolymorphicMatcher<internal::NotNullMatcher> NotNull() {
  return MakePolymorphicMatcher(internal::NotNullMatcher());
}

template <typename T>
inline internal::RefMatcher<T&> Ref(T& x) {
  return internal::RefMatcher<T&>(x);
}

inline PolymorphicMatcher<internal::IsNanMatcher> IsNan() {
  return MakePolymorphicMatcher(internal::IsNanMatcher());
}

inline internal::FloatingEqMatcher<double> DoubleEq(double rhs) {
  return internal::FloatingEqMatcher<double>(rhs, false);
}

inline internal::FloatingEqMatcher<double> NanSensitiveDoubleEq(double rhs) {
  return internal::FloatingEqMatcher<double>(rhs, true);
}

inline internal::FloatingEqMatcher<double> DoubleNear(double rhs,
                                                      double max_abs_error) {
  return internal::FloatingEqMatcher<double>(rhs, false, max_abs_error);
}

inline internal::FloatingEqMatcher<double> NanSensitiveDoubleNear(
    double rhs, double max_abs_error) {
  return internal::FloatingEqMatcher<double>(rhs, true, max_abs_error);
}

inline internal::FloatingEqMatcher<float> FloatEq(float rhs) {
  return internal::FloatingEqMatcher<float>(rhs, false);
}

inline internal::FloatingEqMatcher<float> NanSensitiveFloatEq(float rhs) {
  return internal::FloatingEqMatcher<float>(rhs, true);
}

inline internal::FloatingEqMatcher<float> FloatNear(float rhs,
                                                    float max_abs_error) {
  return internal::FloatingEqMatcher<float>(rhs, false, max_abs_error);
}

inline internal::FloatingEqMatcher<float> NanSensitiveFloatNear(
    float rhs, float max_abs_error) {
  return internal::FloatingEqMatcher<float>(rhs, true, max_abs_error);
}

template <typename InnerMatcher>
inline internal::PointeeMatcher<InnerMatcher> Pointee(
    const InnerMatcher& inner_matcher) {
  return internal::PointeeMatcher<InnerMatcher>(inner_matcher);
}

#if GTEST_HAS_RTTI

template <typename To>
inline PolymorphicMatcher<internal::WhenDynamicCastToMatcher<To>>
WhenDynamicCastTo(const Matcher<To>& inner_matcher) {
  return MakePolymorphicMatcher(
      internal::WhenDynamicCastToMatcher<To>(inner_matcher));
}
#endif

template <typename Class, typename FieldType, typename FieldMatcher>
inline PolymorphicMatcher<internal::FieldMatcher<Class, FieldType>> Field(
    FieldType Class::*field, const FieldMatcher& matcher) {
  return MakePolymorphicMatcher(internal::FieldMatcher<Class, FieldType>(
      field, MatcherCast<const FieldType&>(matcher)));

}

template <typename Class, typename FieldType, typename FieldMatcher>
inline PolymorphicMatcher<internal::FieldMatcher<Class, FieldType>> Field(
    const std::string& field_name, FieldType Class::*field,
    const FieldMatcher& matcher) {
  return MakePolymorphicMatcher(internal::FieldMatcher<Class, FieldType>(
      field_name, field, MatcherCast<const FieldType&>(matcher)));
}

template <typename Class, typename PropertyType, typename PropertyMatcher>
inline PolymorphicMatcher<internal::PropertyMatcher<
    Class, PropertyType, PropertyType (Class::*)() const>>
Property(PropertyType (Class::*property)() const,
         const PropertyMatcher& matcher) {
  return MakePolymorphicMatcher(
      internal::PropertyMatcher<Class, PropertyType,
                                PropertyType (Class::*)() const>(
          property, MatcherCast<const PropertyType&>(matcher)));

}

template <typename Class, typename PropertyType, typename PropertyMatcher>
inline PolymorphicMatcher<internal::PropertyMatcher<
    Class, PropertyType, PropertyType (Class::*)() const>>
Property(const std::string& property_name,
         PropertyType (Class::*property)() const,
         const PropertyMatcher& matcher) {
  return MakePolymorphicMatcher(
      internal::PropertyMatcher<Class, PropertyType,
                                PropertyType (Class::*)() const>(
          property_name, property, MatcherCast<const PropertyType&>(matcher)));
}

template <typename Class, typename PropertyType, typename PropertyMatcher>
inline PolymorphicMatcher<internal::PropertyMatcher<
    Class, PropertyType, PropertyType (Class::*)() const&>>
Property(PropertyType (Class::*property)() const&,
         const PropertyMatcher& matcher) {
  return MakePolymorphicMatcher(
      internal::PropertyMatcher<Class, PropertyType,
                                PropertyType (Class::*)() const&>(
          property, MatcherCast<const PropertyType&>(matcher)));
}

template <typename Class, typename PropertyType, typename PropertyMatcher>
inline PolymorphicMatcher<internal::PropertyMatcher<
    Class, PropertyType, PropertyType (Class::*)() const&>>
Property(const std::string& property_name,
         PropertyType (Class::*property)() const&,
         const PropertyMatcher& matcher) {
  return MakePolymorphicMatcher(
      internal::PropertyMatcher<Class, PropertyType,
                                PropertyType (Class::*)() const&>(
          property_name, property, MatcherCast<const PropertyType&>(matcher)));
}

template <typename Callable, typename InnerMatcher>
internal::ResultOfMatcher<Callable, InnerMatcher> ResultOf(
    Callable callable, InnerMatcher matcher) {
  return internal::ResultOfMatcher<Callable, InnerMatcher>(std::move(callable),
                                                           std::move(matcher));
}

template <typename Callable, typename InnerMatcher>
internal::ResultOfMatcher<Callable, InnerMatcher> ResultOf(
    const std::string& result_description, Callable callable,
    InnerMatcher matcher) {
  return internal::ResultOfMatcher<Callable, InnerMatcher>(
      result_description, std::move(callable), std::move(matcher));
}

template <typename T = std::string>
PolymorphicMatcher<internal::StrEqualityMatcher<std::string>> StrEq(
    const internal::StringLike<T>& str) {
  return MakePolymorphicMatcher(
      internal::StrEqualityMatcher<std::string>(std::string(str), true, true));
}

template <typename T = std::string>
PolymorphicMatcher<internal::StrEqualityMatcher<std::string>> StrNe(
    const internal::StringLike<T>& str) {
  return MakePolymorphicMatcher(
      internal::StrEqualityMatcher<std::string>(std::string(str), false, true));
}

template <typename T = std::string>
PolymorphicMatcher<internal::StrEqualityMatcher<std::string>> StrCaseEq(
    const internal::StringLike<T>& str) {
  return MakePolymorphicMatcher(
      internal::StrEqualityMatcher<std::string>(std::string(str), true, false));
}

template <typename T = std::string>
PolymorphicMatcher<internal::StrEqualityMatcher<std::string>> StrCaseNe(
    const internal::StringLike<T>& str) {
  return MakePolymorphicMatcher(internal::StrEqualityMatcher<std::string>(
      std::string(str), false, false));
}

template <typename T = std::string>
PolymorphicMatcher<internal::HasSubstrMatcher<std::string>> HasSubstr(
    const internal::StringLike<T>& substring) {
  return MakePolymorphicMatcher(
      internal::HasSubstrMatcher<std::string>(std::string(substring)));
}

template <typename T = std::string>
PolymorphicMatcher<internal::StartsWithMatcher<std::string>> StartsWith(
    const internal::StringLike<T>& prefix) {
  return MakePolymorphicMatcher(
      internal::StartsWithMatcher<std::string>(std::string(prefix)));
}

template <typename T = std::string>
PolymorphicMatcher<internal::EndsWithMatcher<std::string>> EndsWith(
    const internal::StringLike<T>& suffix) {
  return MakePolymorphicMatcher(
      internal::EndsWithMatcher<std::string>(std::string(suffix)));
}

#if GTEST_HAS_STD_WSTRING

inline PolymorphicMatcher<internal::StrEqualityMatcher<std::wstring>> StrEq(
    const std::wstring& str) {
  return MakePolymorphicMatcher(
      internal::StrEqualityMatcher<std::wstring>(str, true, true));
}

inline PolymorphicMatcher<internal::StrEqualityMatcher<std::wstring>> StrNe(
    const std::wstring& str) {
  return MakePolymorphicMatcher(
      internal::StrEqualityMatcher<std::wstring>(str, false, true));
}

inline PolymorphicMatcher<internal::StrEqualityMatcher<std::wstring>> StrCaseEq(
    const std::wstring& str) {
  return MakePolymorphicMatcher(
      internal::StrEqualityMatcher<std::wstring>(str, true, false));
}

inline PolymorphicMatcher<internal::StrEqualityMatcher<std::wstring>> StrCaseNe(
    const std::wstring& str) {
  return MakePolymorphicMatcher(
      internal::StrEqualityMatcher<std::wstring>(str, false, false));
}

inline PolymorphicMatcher<internal::HasSubstrMatcher<std::wstring>> HasSubstr(
    const std::wstring& substring) {
  return MakePolymorphicMatcher(
      internal::HasSubstrMatcher<std::wstring>(substring));
}

inline PolymorphicMatcher<internal::StartsWithMatcher<std::wstring>> StartsWith(
    const std::wstring& prefix) {
  return MakePolymorphicMatcher(
      internal::StartsWithMatcher<std::wstring>(prefix));
}

inline PolymorphicMatcher<internal::EndsWithMatcher<std::wstring>> EndsWith(
    const std::wstring& suffix) {
  return MakePolymorphicMatcher(
      internal::EndsWithMatcher<std::wstring>(suffix));
}

#endif

inline internal::Eq2Matcher Eq() { return internal::Eq2Matcher(); }

inline internal::Ge2Matcher Ge() { return internal::Ge2Matcher(); }

inline internal::Gt2Matcher Gt() { return internal::Gt2Matcher(); }

inline internal::Le2Matcher Le() { return internal::Le2Matcher(); }

inline internal::Lt2Matcher Lt() { return internal::Lt2Matcher(); }

inline internal::Ne2Matcher Ne() { return internal::Ne2Matcher(); }

inline internal::FloatingEq2Matcher<float> FloatEq() {
  return internal::FloatingEq2Matcher<float>();
}

inline internal::FloatingEq2Matcher<double> DoubleEq() {
  return internal::FloatingEq2Matcher<double>();
}

inline internal::FloatingEq2Matcher<float> NanSensitiveFloatEq() {
  return internal::FloatingEq2Matcher<float>(true);
}

inline internal::FloatingEq2Matcher<double> NanSensitiveDoubleEq() {
  return internal::FloatingEq2Matcher<double>(true);
}

inline internal::FloatingEq2Matcher<float> FloatNear(float max_abs_error) {
  return internal::FloatingEq2Matcher<float>(max_abs_error);
}

inline internal::FloatingEq2Matcher<double> DoubleNear(double max_abs_error) {
  return internal::FloatingEq2Matcher<double>(max_abs_error);
}

inline internal::FloatingEq2Matcher<float> NanSensitiveFloatNear(
    float max_abs_error) {
  return internal::FloatingEq2Matcher<float>(max_abs_error, true);
}

inline internal::FloatingEq2Matcher<double> NanSensitiveDoubleNear(
    double max_abs_error) {
  return internal::FloatingEq2Matcher<double>(max_abs_error, true);
}

template <typename InnerMatcher>
inline internal::NotMatcher<InnerMatcher> Not(InnerMatcher m) {
  return internal::NotMatcher<InnerMatcher>(m);
}

template <typename Predicate>
inline PolymorphicMatcher<internal::TrulyMatcher<Predicate>> Truly(
    Predicate pred) {
  return MakePolymorphicMatcher(internal::TrulyMatcher<Predicate>(pred));
}

template <typename SizeMatcher>
inline internal::SizeIsMatcher<SizeMatcher> SizeIs(
    const SizeMatcher& size_matcher) {
  return internal::SizeIsMatcher<SizeMatcher>(size_matcher);
}

template <typename DistanceMatcher>
inline internal::BeginEndDistanceIsMatcher<DistanceMatcher> BeginEndDistanceIs(
    const DistanceMatcher& distance_matcher) {
  return internal::BeginEndDistanceIsMatcher<DistanceMatcher>(distance_matcher);
}

template <typename Container>
inline PolymorphicMatcher<
    internal::ContainerEqMatcher<typename std::remove_const<Container>::type>>
ContainerEq(const Container& rhs) {
  return MakePolymorphicMatcher(internal::ContainerEqMatcher<Container>(rhs));
}

template <typename Comparator, typename ContainerMatcher>
inline internal::WhenSortedByMatcher<Comparator, ContainerMatcher> WhenSortedBy(
    const Comparator& comparator, const ContainerMatcher& container_matcher) {
  return internal::WhenSortedByMatcher<Comparator, ContainerMatcher>(
      comparator, container_matcher);
}

template <typename ContainerMatcher>
inline internal::WhenSortedByMatcher<internal::LessComparator, ContainerMatcher>
WhenSorted(const ContainerMatcher& container_matcher) {
  return internal::WhenSortedByMatcher<internal::LessComparator,
                                       ContainerMatcher>(
      internal::LessComparator(), container_matcher);
}

template <typename TupleMatcher, typename Container>
inline internal::PointwiseMatcher<TupleMatcher,
                                  typename std::remove_const<Container>::type>
Pointwise(const TupleMatcher& tuple_matcher, const Container& rhs) {
  return internal::PointwiseMatcher<TupleMatcher, Container>(tuple_matcher,
                                                             rhs);
}

template <typename TupleMatcher, typename T>
inline internal::PointwiseMatcher<TupleMatcher,
                                  std::vector<std::remove_const_t<T>>>
Pointwise(const TupleMatcher& tuple_matcher, std::initializer_list<T> rhs) {
  return Pointwise(tuple_matcher, std::vector<std::remove_const_t<T>>(rhs));
}

template <typename Tuple2Matcher, typename RhsContainer>
inline internal::UnorderedElementsAreArrayMatcher<
    typename internal::BoundSecondMatcher<
        Tuple2Matcher,
        typename internal::StlContainerView<
            typename std::remove_const<RhsContainer>::type>::type::value_type>>
UnorderedPointwise(const Tuple2Matcher& tuple2_matcher,
                   const RhsContainer& rhs_container) {

  typedef typename internal::StlContainerView<RhsContainer> RhsView;
  typedef typename RhsView::type RhsStlContainer;
  typedef typename RhsStlContainer::value_type Second;
  const RhsStlContainer& rhs_stl_container =
      RhsView::ConstReference(rhs_container);

  ::std::vector<internal::BoundSecondMatcher<Tuple2Matcher, Second>> matchers;
  for (auto it = rhs_stl_container.begin(); it != rhs_stl_container.end();
       ++it) {
    matchers.push_back(internal::MatcherBindSecond(tuple2_matcher, *it));
  }

  return UnorderedElementsAreArray(matchers);
}

template <typename Tuple2Matcher, typename T>
inline internal::UnorderedElementsAreArrayMatcher<
    typename internal::BoundSecondMatcher<Tuple2Matcher, T>>
UnorderedPointwise(const Tuple2Matcher& tuple2_matcher,
                   std::initializer_list<T> rhs) {
  return UnorderedPointwise(tuple2_matcher, std::vector<T>(rhs));
}

template <typename M>
inline internal::ContainsMatcher<M> Contains(M matcher) {
  return internal::ContainsMatcher<M>(matcher);
}

template <typename Iter>
inline internal::UnorderedElementsAreArrayMatcher<
    typename ::std::iterator_traits<Iter>::value_type>
IsSupersetOf(Iter first, Iter last) {
  typedef typename ::std::iterator_traits<Iter>::value_type T;
  return internal::UnorderedElementsAreArrayMatcher<T>(
      internal::UnorderedMatcherRequire::Superset, first, last);
}

template <typename T>
inline internal::UnorderedElementsAreArrayMatcher<T> IsSupersetOf(
    const T* pointer, size_t count) {
  return IsSupersetOf(pointer, pointer + count);
}

template <typename T, size_t N>
inline internal::UnorderedElementsAreArrayMatcher<T> IsSupersetOf(
    const T (&array)[N]) {
  return IsSupersetOf(array, N);
}

template <typename Container>
inline internal::UnorderedElementsAreArrayMatcher<
    typename Container::value_type>
IsSupersetOf(const Container& container) {
  return IsSupersetOf(container.begin(), container.end());
}

template <typename T>
inline internal::UnorderedElementsAreArrayMatcher<T> IsSupersetOf(
    ::std::initializer_list<T> xs) {
  return IsSupersetOf(xs.begin(), xs.end());
}

template <typename Iter>
inline internal::UnorderedElementsAreArrayMatcher<
    typename ::std::iterator_traits<Iter>::value_type>
IsSubsetOf(Iter first, Iter last) {
  typedef typename ::std::iterator_traits<Iter>::value_type T;
  return internal::UnorderedElementsAreArrayMatcher<T>(
      internal::UnorderedMatcherRequire::Subset, first, last);
}

template <typename T>
inline internal::UnorderedElementsAreArrayMatcher<T> IsSubsetOf(
    const T* pointer, size_t count) {
  return IsSubsetOf(pointer, pointer + count);
}

template <typename T, size_t N>
inline internal::UnorderedElementsAreArrayMatcher<T> IsSubsetOf(
    const T (&array)[N]) {
  return IsSubsetOf(array, N);
}

template <typename Container>
inline internal::UnorderedElementsAreArrayMatcher<
    typename Container::value_type>
IsSubsetOf(const Container& container) {
  return IsSubsetOf(container.begin(), container.end());
}

template <typename T>
inline internal::UnorderedElementsAreArrayMatcher<T> IsSubsetOf(
    ::std::initializer_list<T> xs) {
  return IsSubsetOf(xs.begin(), xs.end());
}

template <typename M>
inline internal::EachMatcher<M> Each(M matcher) {
  return internal::EachMatcher<M>(matcher);
}

template <typename M>
inline internal::KeyMatcher<M> Key(M inner_matcher) {
  return internal::KeyMatcher<M>(inner_matcher);
}

template <typename FirstMatcher, typename SecondMatcher>
inline internal::PairMatcher<FirstMatcher, SecondMatcher> Pair(
    FirstMatcher first_matcher, SecondMatcher second_matcher) {
  return internal::PairMatcher<FirstMatcher, SecondMatcher>(first_matcher,
                                                            second_matcher);
}

namespace no_adl {

template <typename MatcherTrue, typename MatcherFalse>
internal::ConditionalMatcher<MatcherTrue, MatcherFalse> Conditional(
    bool condition, MatcherTrue matcher_true, MatcherFalse matcher_false) {
  return internal::ConditionalMatcher<MatcherTrue, MatcherFalse>(
      condition, std::move(matcher_true), std::move(matcher_false));
}

template <typename... M>
internal::FieldsAreMatcher<typename std::decay<M>::type...> FieldsAre(
    M&&... matchers) {
  return internal::FieldsAreMatcher<typename std::decay<M>::type...>(
      std::forward<M>(matchers)...);
}

template <typename InnerMatcher>
inline internal::PointerMatcher<InnerMatcher> Pointer(
    const InnerMatcher& inner_matcher) {
  return internal::PointerMatcher<InnerMatcher>(inner_matcher);
}

template <typename InnerMatcher>
inline internal::AddressMatcher<InnerMatcher> Address(
    const InnerMatcher& inner_matcher) {
  return internal::AddressMatcher<InnerMatcher>(inner_matcher);
}

template <typename MatcherType>
internal::WhenBase64UnescapedMatcher WhenBase64Unescaped(
    const MatcherType& internal_matcher) {
  return internal::WhenBase64UnescapedMatcher(internal_matcher);
}
}

template <typename M>
inline internal::MatcherAsPredicate<M> Matches(M matcher) {
  return internal::MatcherAsPredicate<M>(matcher);
}

template <typename T, typename M>
inline bool Value(const T& value, M matcher) {
  return testing::Matches(matcher)(value);
}

template <typename T, typename M>
inline bool ExplainMatchResult(M matcher, const T& value,
                               MatchResultListener* listener) {
  return SafeMatcherCast<const T&>(matcher).MatchAndExplain(value, listener);
}

template <typename T, typename M>
std::string DescribeMatcher(const M& matcher, bool negation = false) {
  ::std::stringstream ss;
  Matcher<T> monomorphic_matcher = SafeMatcherCast<T>(matcher);
  if (negation) {
    monomorphic_matcher.DescribeNegationTo(&ss);
  } else {
    monomorphic_matcher.DescribeTo(&ss);
  }
  return ss.str();
}

template <typename... Args>
internal::ElementsAreMatcher<
    std::tuple<typename std::decay<const Args&>::type...>>
ElementsAre(const Args&... matchers) {
  return internal::ElementsAreMatcher<
      std::tuple<typename std::decay<const Args&>::type...>>(
      std::make_tuple(matchers...));
}

template <typename... Args>
internal::UnorderedElementsAreMatcher<
    std::tuple<typename std::decay<const Args&>::type...>>
UnorderedElementsAre(const Args&... matchers) {
  return internal::UnorderedElementsAreMatcher<
      std::tuple<typename std::decay<const Args&>::type...>>(
      std::make_tuple(matchers...));
}

template <typename... Args>
internal::AllOfMatcher<typename std::decay<const Args&>::type...> AllOf(
    const Args&... matchers) {
  return internal::AllOfMatcher<typename std::decay<const Args&>::type...>(
      matchers...);
}

template <typename... Args>
internal::AnyOfMatcher<typename std::decay<const Args&>::type...> AnyOf(
    const Args&... matchers) {
  return internal::AnyOfMatcher<typename std::decay<const Args&>::type...>(
      matchers...);
}

template <typename Iter>
inline internal::AnyOfArrayMatcher<
    typename ::std::iterator_traits<Iter>::value_type>
AnyOfArray(Iter first, Iter last) {
  return internal::AnyOfArrayMatcher<
      typename ::std::iterator_traits<Iter>::value_type>(first, last);
}

template <typename Iter>
inline internal::AllOfArrayMatcher<
    typename ::std::iterator_traits<Iter>::value_type>
AllOfArray(Iter first, Iter last) {
  return internal::AllOfArrayMatcher<
      typename ::std::iterator_traits<Iter>::value_type>(first, last);
}

template <typename T>
inline internal::AnyOfArrayMatcher<T> AnyOfArray(const T* ptr, size_t count) {
  return AnyOfArray(ptr, ptr + count);
}

template <typename T>
inline internal::AllOfArrayMatcher<T> AllOfArray(const T* ptr, size_t count) {
  return AllOfArray(ptr, ptr + count);
}

template <typename T, size_t N>
inline internal::AnyOfArrayMatcher<T> AnyOfArray(const T (&array)[N]) {
  return AnyOfArray(array, N);
}

template <typename T, size_t N>
inline internal::AllOfArrayMatcher<T> AllOfArray(const T (&array)[N]) {
  return AllOfArray(array, N);
}

template <typename Container>
inline internal::AnyOfArrayMatcher<typename Container::value_type> AnyOfArray(
    const Container& container) {
  return AnyOfArray(container.begin(), container.end());
}

template <typename Container>
inline internal::AllOfArrayMatcher<typename Container::value_type> AllOfArray(
    const Container& container) {
  return AllOfArray(container.begin(), container.end());
}

template <typename T>
inline internal::AnyOfArrayMatcher<T> AnyOfArray(
    ::std::initializer_list<T> xs) {
  return AnyOfArray(xs.begin(), xs.end());
}

template <typename T>
inline internal::AllOfArrayMatcher<T> AllOfArray(
    ::std::initializer_list<T> xs) {
  return AllOfArray(xs.begin(), xs.end());
}

template <size_t... k, typename InnerMatcher>
internal::ArgsMatcher<typename std::decay<InnerMatcher>::type, k...> Args(
    InnerMatcher&& matcher) {
  return internal::ArgsMatcher<typename std::decay<InnerMatcher>::type, k...>(
      std::forward<InnerMatcher>(matcher));
}

template <typename InnerMatcher>
inline InnerMatcher AllArgs(const InnerMatcher& matcher) {
  return matcher;
}

template <typename ValueMatcher>
inline internal::OptionalMatcher<ValueMatcher> Optional(
    const ValueMatcher& value_matcher) {
  return internal::OptionalMatcher<ValueMatcher>(value_matcher);
}

template <typename T>
PolymorphicMatcher<internal::any_cast_matcher::AnyCastMatcher<T>> AnyWith(
    const Matcher<const T&>& matcher) {
  return MakePolymorphicMatcher(
      internal::any_cast_matcher::AnyCastMatcher<T>(matcher));
}

template <typename T>
PolymorphicMatcher<internal::variant_matcher::VariantMatcher<T>> VariantWith(
    const Matcher<const T&>& matcher) {
  return MakePolymorphicMatcher(
      internal::variant_matcher::VariantMatcher<T>(matcher));
}

#if GTEST_HAS_EXCEPTIONS

namespace internal {

class WithWhatMatcherImpl {
 public:
  WithWhatMatcherImpl(Matcher<std::string> matcher)
      : matcher_(std::move(matcher)) {}

  void DescribeTo(std::ostream* os) const {
    *os << "contains .what() that ";
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "contains .what() that does not ";
    matcher_.DescribeTo(os);
  }

  template <typename Err>
  bool MatchAndExplain(const Err& err, MatchResultListener* listener) const {
    *listener << "which contains .what() (of value = " << err.what()
              << ") that ";
    return matcher_.MatchAndExplain(err.what(), listener);
  }

 private:
  const Matcher<std::string> matcher_;
};

inline PolymorphicMatcher<WithWhatMatcherImpl> WithWhat(
    Matcher<std::string> m) {
  return MakePolymorphicMatcher(WithWhatMatcherImpl(std::move(m)));
}

template <typename Err>
class ExceptionMatcherImpl {
  class NeverThrown {
   public:
    const char* what() const noexcept {
      return "this exception should never be thrown";
    }
  };

  using DefaultExceptionType = typename std::conditional<
      std::is_same<typename std::remove_cv<
                       typename std::remove_reference<Err>::type>::type,
                   std::exception>::value,
      const NeverThrown&, const std::exception&>::type;

 public:
  ExceptionMatcherImpl(Matcher<const Err&> matcher)
      : matcher_(std::move(matcher)) {}

  void DescribeTo(std::ostream* os) const {
    *os << "throws an exception which is a " << GetTypeName<Err>();
    *os << " which ";
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "throws an exception which is not a " << GetTypeName<Err>();
    *os << " which ";
    matcher_.DescribeNegationTo(os);
  }

  template <typename T>
  bool MatchAndExplain(T&& x, MatchResultListener* listener) const {
    try {
      (void)(std::forward<T>(x)());
    } catch (const Err& err) {
      *listener << "throws an exception which is a " << GetTypeName<Err>();
      *listener << " ";
      return matcher_.MatchAndExplain(err, listener);
    } catch (DefaultExceptionType err) {
#if GTEST_HAS_RTTI
      *listener << "throws an exception of type " << GetTypeName(typeid(err));
      *listener << " ";
#else
      *listener << "throws an std::exception-derived type ";
#endif
      *listener << "with description \"" << err.what() << "\"";
      return false;
    } catch (...) {
      *listener << "throws an exception of an unknown type";
      return false;
    }

    *listener << "does not throw any exception";
    return false;
  }

 private:
  const Matcher<const Err&> matcher_;
};

}

template <typename Err>
PolymorphicMatcher<internal::ExceptionMatcherImpl<Err>> Throws() {
  return MakePolymorphicMatcher(
      internal::ExceptionMatcherImpl<Err>(A<const Err&>()));
}

template <typename Err, typename ExceptionMatcher>
PolymorphicMatcher<internal::ExceptionMatcherImpl<Err>> Throws(
    const ExceptionMatcher& exception_matcher) {

  return MakePolymorphicMatcher(internal::ExceptionMatcherImpl<Err>(
      SafeMatcherCast<const Err&>(exception_matcher)));
}

template <typename Err, typename MessageMatcher>
PolymorphicMatcher<internal::ExceptionMatcherImpl<Err>> ThrowsMessage(
    MessageMatcher&& message_matcher) {
  static_assert(std::is_base_of<std::exception, Err>::value,
                "expected an std::exception-derived type");
  return Throws<Err>(internal::WithWhat(
      MatcherCast<std::string>(std::forward<MessageMatcher>(message_matcher))));
}

#endif

#define ASSERT_THAT(value, matcher) \
  ASSERT_PRED_FORMAT1(              \
      ::testing::internal::MakePredicateFormatterFromMatcher(matcher), value)
#define EXPECT_THAT(value, matcher) \
  EXPECT_PRED_FORMAT1(              \
      ::testing::internal::MakePredicateFormatterFromMatcher(matcher), value)

#define MATCHER(name, description)                                            \
  class name##Matcher                                                         \
      : public ::testing::internal::MatcherBaseImpl<name##Matcher> {          \
   public:                                                                    \
    template <typename arg_type>                                              \
    class gmock_Impl : public ::testing::MatcherInterface<const arg_type&> {  \
     public:                                                                  \
      gmock_Impl() {}                                                         \
      bool MatchAndExplain(                                                   \
          const arg_type& arg,                                                \
          ::testing::MatchResultListener* result_listener) const override;    \
      void DescribeTo(::std::ostream* gmock_os) const override {              \
        *gmock_os << FormatDescription(false);                                \
      }                                                                       \
      void DescribeNegationTo(::std::ostream* gmock_os) const override {      \
        *gmock_os << FormatDescription(true);                                 \
      }                                                                       \
                                                                              \
     private:                                                                 \
      ::std::string FormatDescription(bool negation) const {                  \
                                                                              \
        ::std::string gmock_description = (description);                      \
        if (!gmock_description.empty()) {                                     \
          return gmock_description;                                           \
        }                                                                     \
        return ::testing::internal::FormatMatcherDescription(negation, #name, \
                                                             {}, {});         \
      }                                                                       \
    };                                                                        \
  };                                                                          \
  inline name##Matcher GMOCK_INTERNAL_WARNING_PUSH()                          \
      GMOCK_INTERNAL_WARNING_CLANG(ignored, "-Wunused-function")              \
          GMOCK_INTERNAL_WARNING_CLANG(ignored, "-Wunused-member-function")   \
              name GMOCK_INTERNAL_WARNING_POP()() {                           \
    return {};                                                                \
  }                                                                           \
  template <typename arg_type>                                                \
  bool name##Matcher::gmock_Impl<arg_type>::MatchAndExplain(                  \
      const arg_type& arg,                                                    \
      [[maybe_unused]] ::testing::MatchResultListener* result_listener) const

#define MATCHER_P(name, p0, description) \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP, description, (#p0), (p0))
#define MATCHER_P2(name, p0, p1, description)                            \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP2, description, (#p0, #p1), \
                         (p0, p1))
#define MATCHER_P3(name, p0, p1, p2, description)                             \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP3, description, (#p0, #p1, #p2), \
                         (p0, p1, p2))
#define MATCHER_P4(name, p0, p1, p2, p3, description)        \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP4, description, \
                         (#p0, #p1, #p2, #p3), (p0, p1, p2, p3))
#define MATCHER_P5(name, p0, p1, p2, p3, p4, description)    \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP5, description, \
                         (#p0, #p1, #p2, #p3, #p4), (p0, p1, p2, p3, p4))
#define MATCHER_P6(name, p0, p1, p2, p3, p4, p5, description) \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP6, description,  \
                         (#p0, #p1, #p2, #p3, #p4, #p5),      \
                         (p0, p1, p2, p3, p4, p5))
#define MATCHER_P7(name, p0, p1, p2, p3, p4, p5, p6, description) \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP7, description,      \
                         (#p0, #p1, #p2, #p3, #p4, #p5, #p6),     \
                         (p0, p1, p2, p3, p4, p5, p6))
#define MATCHER_P8(name, p0, p1, p2, p3, p4, p5, p6, p7, description) \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP8, description,          \
                         (#p0, #p1, #p2, #p3, #p4, #p5, #p6, #p7),    \
                         (p0, p1, p2, p3, p4, p5, p6, p7))
#define MATCHER_P9(name, p0, p1, p2, p3, p4, p5, p6, p7, p8, description) \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP9, description,              \
                         (#p0, #p1, #p2, #p3, #p4, #p5, #p6, #p7, #p8),   \
                         (p0, p1, p2, p3, p4, p5, p6, p7, p8))
#define MATCHER_P10(name, p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, description) \
  GMOCK_INTERNAL_MATCHER(name, name##MatcherP10, description,                  \
                         (#p0, #p1, #p2, #p3, #p4, #p5, #p6, #p7, #p8, #p9),   \
                         (p0, p1, p2, p3, p4, p5, p6, p7, p8, p9))

#define GMOCK_INTERNAL_MATCHER(name, full_name, description, arg_names, args)  \
  template <GMOCK_INTERNAL_MATCHER_TEMPLATE_PARAMS(args)>                      \
  class full_name : public ::testing::internal::MatcherBaseImpl<               \
                        full_name<GMOCK_INTERNAL_MATCHER_TYPE_PARAMS(args)>> { \
   public:                                                                     \
    using full_name::MatcherBaseImpl::MatcherBaseImpl;                         \
    template <typename arg_type>                                               \
    class gmock_Impl : public ::testing::MatcherInterface<const arg_type&> {   \
     public:                                                                   \
      explicit gmock_Impl(GMOCK_INTERNAL_MATCHER_FUNCTION_ARGS(args))          \
          : GMOCK_INTERNAL_MATCHER_FORWARD_ARGS(args) {}                       \
      bool MatchAndExplain(                                                    \
          const arg_type& arg,                                                 \
          ::testing::MatchResultListener* result_listener) const override;     \
      void DescribeTo(::std::ostream* gmock_os) const override {               \
        *gmock_os << FormatDescription(false);                                 \
      }                                                                        \
      void DescribeNegationTo(::std::ostream* gmock_os) const override {       \
        *gmock_os << FormatDescription(true);                                  \
      }                                                                        \
      GMOCK_INTERNAL_MATCHER_MEMBERS(args)                                     \
                                                                               \
     private:                                                                  \
      ::std::string FormatDescription(bool negation) const {                   \
        ::std::string gmock_description;                                       \
        gmock_description = (description);                                     \
        if (!gmock_description.empty()) {                                      \
          return gmock_description;                                            \
        }                                                                      \
        return ::testing::internal::FormatMatcherDescription(                  \
            negation, #name, {GMOCK_PP_REMOVE_PARENS(arg_names)},              \
            ::testing::internal::UniversalTersePrintTupleFieldsToStrings(      \
                ::std::tuple<GMOCK_INTERNAL_MATCHER_TYPE_PARAMS(args)>(        \
                    GMOCK_INTERNAL_MATCHER_MEMBERS_USAGE(args))));             \
      }                                                                        \
    };                                                                         \
  };                                                                           \
  template <GMOCK_INTERNAL_MATCHER_TEMPLATE_PARAMS(args)>                      \
  inline full_name<GMOCK_INTERNAL_MATCHER_TYPE_PARAMS(args)> name(             \
      GMOCK_INTERNAL_MATCHER_FUNCTION_ARGS(args)) {                            \
    return full_name<GMOCK_INTERNAL_MATCHER_TYPE_PARAMS(args)>(                \
        GMOCK_INTERNAL_MATCHER_ARGS_USAGE(args));                              \
  }                                                                            \
  template <GMOCK_INTERNAL_MATCHER_TEMPLATE_PARAMS(args)>                      \
  template <typename arg_type>                                                 \
  bool full_name<GMOCK_INTERNAL_MATCHER_TYPE_PARAMS(args)>::                   \
      gmock_Impl<arg_type>::MatchAndExplain(                                   \
          const arg_type& arg,                                                 \
          [[maybe_unused]] ::testing::MatchResultListener* result_listener)    \
          const

#define GMOCK_INTERNAL_MATCHER_TEMPLATE_PARAMS(args) \
  GMOCK_PP_TAIL(                                     \
      GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_MATCHER_TEMPLATE_PARAM, , args))
#define GMOCK_INTERNAL_MATCHER_TEMPLATE_PARAM(i_unused, data_unused, arg) \
  , typename arg##_type

#define GMOCK_INTERNAL_MATCHER_TYPE_PARAMS(args) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_MATCHER_TYPE_PARAM, , args))
#define GMOCK_INTERNAL_MATCHER_TYPE_PARAM(i_unused, data_unused, arg) \
  , arg##_type

#define GMOCK_INTERNAL_MATCHER_FUNCTION_ARGS(args) \
  GMOCK_PP_TAIL(dummy_first GMOCK_PP_FOR_EACH(     \
      GMOCK_INTERNAL_MATCHER_FUNCTION_ARG, , args))
#define GMOCK_INTERNAL_MATCHER_FUNCTION_ARG(i, data_unused, arg) \
  , arg##_type gmock_p##i

#define GMOCK_INTERNAL_MATCHER_FORWARD_ARGS(args) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_MATCHER_FORWARD_ARG, , args))
#define GMOCK_INTERNAL_MATCHER_FORWARD_ARG(i, data_unused, arg) \
  , arg(::std::forward<arg##_type>(gmock_p##i))

#define GMOCK_INTERNAL_MATCHER_MEMBERS(args) \
  GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_MATCHER_MEMBER, , args)
#define GMOCK_INTERNAL_MATCHER_MEMBER(i_unused, data_unused, arg) \
  const arg##_type arg;

#define GMOCK_INTERNAL_MATCHER_MEMBERS_USAGE(args) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_MATCHER_MEMBER_USAGE, , args))
#define GMOCK_INTERNAL_MATCHER_MEMBER_USAGE(i_unused, data_unused, arg) , arg

#define GMOCK_INTERNAL_MATCHER_ARGS_USAGE(args) \
  GMOCK_PP_TAIL(GMOCK_PP_FOR_EACH(GMOCK_INTERNAL_MATCHER_ARG_USAGE, , args))
#define GMOCK_INTERNAL_MATCHER_ARG_USAGE(i, data_unused, arg) \
  , ::std::forward<arg##_type>(gmock_p##i)

using namespace no_adl;

}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#include "gmock/internal/custom/gmock-matchers.h"

#endif
