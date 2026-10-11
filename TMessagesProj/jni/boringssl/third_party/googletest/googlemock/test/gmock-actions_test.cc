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
// This file tests the built-in actions.

#include "gmock/gmock-actions.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gmock/internal/gmock-port.h"
#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"
#include "gtest/internal/gtest-port.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4100 4503)
#if defined(_MSC_VER) && (_MSC_VER == 1900)

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4800)
#endif

namespace testing {
namespace {

using ::testing::internal::BuiltInDefaultValue;

TEST(TypeTraits, Negation) {

  static_assert(std::is_base_of<std::false_type,
                                internal::negation<std::true_type>>::value,
                "");

  static_assert(std::is_base_of<std::true_type,
                                internal::negation<std::false_type>>::value,
                "");

  static_assert(std::is_base_of<
                    std::true_type,
                    internal::negation<std::integral_constant<int, 0>>>::value,
                "");

  static_assert(std::is_base_of<
                    std::false_type,
                    internal::negation<std::integral_constant<int, 1>>>::value,
                "");

  static_assert(std::is_base_of<
                    std::false_type,
                    internal::negation<std::integral_constant<int, -1>>>::value,
                "");
}

template <int>
struct MyFalse : std::integral_constant<int, 0> {};

template <int>
struct MyTrue : std::integral_constant<int, -1> {};

TEST(TypeTraits, Conjunction) {

  static_assert(std::is_base_of<std::true_type, internal::conjunction<>>::value,
                "");

  static_assert(
      std::is_base_of<MyFalse<0>, internal::conjunction<MyFalse<0>>>::value,
      "");

  static_assert(
      std::is_base_of<MyTrue<0>, internal::conjunction<MyTrue<0>>>::value, "");

  static_assert(
      std::is_base_of<MyFalse<1>, internal::conjunction<MyTrue<0>, MyFalse<1>,
                                                        MyTrue<2>>>::value,
      "");

  static_assert(
      std::is_base_of<MyFalse<1>, internal::conjunction<MyTrue<0>, MyFalse<1>,
                                                        MyFalse<2>>>::value,
      "");

  struct Empty {};
  static_assert(
      std::is_base_of<MyFalse<1>, internal::conjunction<MyTrue<0>, MyFalse<1>,
                                                        Empty>>::value,
      "");

  static_assert(
      std::is_base_of<MyTrue<2>, internal::conjunction<MyTrue<0>, MyTrue<1>,
                                                       MyTrue<2>>>::value,
      "");
}

TEST(TypeTraits, Disjunction) {

  static_assert(
      std::is_base_of<std::false_type, internal::disjunction<>>::value, "");

  static_assert(
      std::is_base_of<MyFalse<0>, internal::disjunction<MyFalse<0>>>::value,
      "");

  static_assert(
      std::is_base_of<MyTrue<0>, internal::disjunction<MyTrue<0>>>::value, "");

  static_assert(
      std::is_base_of<MyTrue<1>, internal::disjunction<MyFalse<0>, MyTrue<1>,
                                                       MyFalse<2>>>::value,
      "");

  static_assert(
      std::is_base_of<MyTrue<1>, internal::disjunction<MyFalse<0>, MyTrue<1>,
                                                       MyTrue<2>>>::value,
      "");

  struct Empty {};
  static_assert(
      std::is_base_of<MyTrue<1>, internal::disjunction<MyFalse<0>, MyTrue<1>,
                                                       Empty>>::value,
      "");

  static_assert(
      std::is_base_of<MyFalse<2>, internal::disjunction<MyFalse<0>, MyFalse<1>,
                                                        MyFalse<2>>>::value,
      "");
}

TEST(TypeTraits, IsInvocableRV) {
  struct C {
    int operator()() const { return 0; }
    void operator()(int) & {}
    std::string operator()(int) && { return ""; };
  };

  static_assert(internal::is_callable_r<int, C>::value, "");
  static_assert(internal::is_callable_r<int, C&>::value, "");
  static_assert(internal::is_callable_r<int, const C>::value, "");
  static_assert(internal::is_callable_r<int, const C&>::value, "");

  static_assert(internal::is_callable_r<void, C>::value, "");
  static_assert(internal::is_callable_r<const volatile void, C>::value, "");
  static_assert(internal::is_callable_r<char, C>::value, "");

  static_assert(internal::is_callable_r<void, C&, int>::value, "");
  static_assert(!internal::is_callable_r<int, C&, int>::value, "");
  static_assert(!internal::is_callable_r<std::string, C&, int>::value, "");
  static_assert(!internal::is_callable_r<void, const C&, int>::value, "");

  static_assert(internal::is_callable_r<std::string, C, int>::value, "");
  static_assert(internal::is_callable_r<void, C, int>::value, "");
  static_assert(!internal::is_callable_r<int, C, int>::value, "");

  static_assert(!internal::is_callable_r<void, C, std::string>::value, "");
  static_assert(!internal::is_callable_r<void, C, int, int>::value, "");

#ifndef _MSC_VER
  {
    struct NonMoveable {
      NonMoveable() = default;
      NonMoveable(NonMoveable&&) = delete;
    };

    static_assert(!std::is_move_constructible_v<NonMoveable>);

    struct Callable {
      NonMoveable operator()() { return NonMoveable(); }
    };

    static_assert(internal::is_callable_r<NonMoveable, Callable>::value);
    static_assert(internal::is_callable_r<void, Callable>::value);
    static_assert(
        internal::is_callable_r<const volatile void, Callable>::value);

    static_assert(!internal::is_callable_r<int, Callable>::value);
    static_assert(!internal::is_callable_r<NonMoveable, Callable, int>::value);
  }
#endif

  static_assert(!internal::is_callable_r<void, int>::value, "");
  static_assert(!internal::is_callable_r<void, void (C::*)()>::value, "");
  static_assert(!internal::is_callable_r<void, void (C::*)(), C*>::value, "");
}

TEST(BuiltInDefaultValueTest, IsNullForPointerTypes) {
  EXPECT_TRUE(BuiltInDefaultValue<int*>::Get() == nullptr);
  EXPECT_TRUE(BuiltInDefaultValue<const char*>::Get() == nullptr);
  EXPECT_TRUE(BuiltInDefaultValue<void*>::Get() == nullptr);
}

TEST(BuiltInDefaultValueTest, ExistsForPointerTypes) {
  EXPECT_TRUE(BuiltInDefaultValue<int*>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<const char*>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<void*>::Exists());
}

TEST(BuiltInDefaultValueTest, IsZeroForNumericTypes) {
  EXPECT_EQ(0U, BuiltInDefaultValue<unsigned char>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<signed char>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<char>::Get());
#if GMOCK_WCHAR_T_IS_NATIVE_
#if !defined(__WCHAR_UNSIGNED__)
  EXPECT_EQ(0, BuiltInDefaultValue<wchar_t>::Get());
#else
  EXPECT_EQ(0U, BuiltInDefaultValue<wchar_t>::Get());
#endif
#endif
  EXPECT_EQ(0U, BuiltInDefaultValue<unsigned short>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<signed short>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<short>::Get());
  EXPECT_EQ(0U, BuiltInDefaultValue<unsigned int>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<signed int>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<int>::Get());
  EXPECT_EQ(0U, BuiltInDefaultValue<unsigned long>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<signed long>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<long>::Get());
  EXPECT_EQ(0U, BuiltInDefaultValue<unsigned long long>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<signed long long>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<long long>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<float>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<double>::Get());
}

TEST(BuiltInDefaultValueTest, ExistsForNumericTypes) {
  EXPECT_TRUE(BuiltInDefaultValue<unsigned char>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<signed char>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<char>::Exists());
#if GMOCK_WCHAR_T_IS_NATIVE_
  EXPECT_TRUE(BuiltInDefaultValue<wchar_t>::Exists());
#endif
  EXPECT_TRUE(BuiltInDefaultValue<unsigned short>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<signed short>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<short>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<unsigned int>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<signed int>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<int>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<unsigned long>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<signed long>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<long>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<unsigned long long>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<signed long long>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<long long>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<float>::Exists());
  EXPECT_TRUE(BuiltInDefaultValue<double>::Exists());
}

TEST(BuiltInDefaultValueTest, IsFalseForBool) {
  EXPECT_FALSE(BuiltInDefaultValue<bool>::Get());
}

TEST(BuiltInDefaultValueTest, BoolExists) {
  EXPECT_TRUE(BuiltInDefaultValue<bool>::Exists());
}

TEST(BuiltInDefaultValueTest, IsEmptyStringForString) {
  EXPECT_EQ("", BuiltInDefaultValue<::std::string>::Get());
}

TEST(BuiltInDefaultValueTest, ExistsForString) {
  EXPECT_TRUE(BuiltInDefaultValue<::std::string>::Exists());
}

TEST(BuiltInDefaultValueTest, WorksForConstTypes) {
  EXPECT_EQ("", BuiltInDefaultValue<const std::string>::Get());
  EXPECT_EQ(0, BuiltInDefaultValue<const int>::Get());
  EXPECT_TRUE(BuiltInDefaultValue<char* const>::Get() == nullptr);
  EXPECT_FALSE(BuiltInDefaultValue<const bool>::Get());
}

class MyDefaultConstructible {
 public:
  MyDefaultConstructible() : value_(42) {}

  int value() const { return value_; }

 private:
  int value_;
};

class MyNonDefaultConstructible {
 public:

  explicit MyNonDefaultConstructible(int a_value) : value_(a_value) {}

  int value() const { return value_; }

 private:
  int value_;
};

TEST(BuiltInDefaultValueTest, ExistsForDefaultConstructibleType) {
  EXPECT_TRUE(BuiltInDefaultValue<MyDefaultConstructible>::Exists());
}

TEST(BuiltInDefaultValueTest, IsDefaultConstructedForDefaultConstructibleType) {
  EXPECT_EQ(42, BuiltInDefaultValue<MyDefaultConstructible>::Get().value());
}

TEST(BuiltInDefaultValueTest, DoesNotExistForNonDefaultConstructibleType) {
  EXPECT_FALSE(BuiltInDefaultValue<MyNonDefaultConstructible>::Exists());
}

TEST(BuiltInDefaultValueDeathTest, IsUndefinedForReferences) {
  EXPECT_DEATH_IF_SUPPORTED({ BuiltInDefaultValue<int&>::Get(); }, "");
  EXPECT_DEATH_IF_SUPPORTED({ BuiltInDefaultValue<const char&>::Get(); }, "");
}

TEST(BuiltInDefaultValueDeathTest, IsUndefinedForNonDefaultConstructibleType) {
  EXPECT_DEATH_IF_SUPPORTED(
      { BuiltInDefaultValue<MyNonDefaultConstructible>::Get(); }, "");
}

TEST(DefaultValueTest, IsInitiallyUnset) {
  EXPECT_FALSE(DefaultValue<int>::IsSet());
  EXPECT_FALSE(DefaultValue<MyDefaultConstructible>::IsSet());
  EXPECT_FALSE(DefaultValue<const MyNonDefaultConstructible>::IsSet());
}

TEST(DefaultValueTest, CanBeSetAndUnset) {
  EXPECT_TRUE(DefaultValue<int>::Exists());
  EXPECT_FALSE(DefaultValue<const MyNonDefaultConstructible>::Exists());

  DefaultValue<int>::Set(1);
  DefaultValue<const MyNonDefaultConstructible>::Set(
      MyNonDefaultConstructible(42));

  EXPECT_EQ(1, DefaultValue<int>::Get());
  EXPECT_EQ(42, DefaultValue<const MyNonDefaultConstructible>::Get().value());

  EXPECT_TRUE(DefaultValue<int>::Exists());
  EXPECT_TRUE(DefaultValue<const MyNonDefaultConstructible>::Exists());

  DefaultValue<int>::Clear();
  DefaultValue<const MyNonDefaultConstructible>::Clear();

  EXPECT_FALSE(DefaultValue<int>::IsSet());
  EXPECT_FALSE(DefaultValue<const MyNonDefaultConstructible>::IsSet());

  EXPECT_TRUE(DefaultValue<int>::Exists());
  EXPECT_FALSE(DefaultValue<const MyNonDefaultConstructible>::Exists());
}

TEST(DefaultValueDeathTest, GetReturnsBuiltInDefaultValueWhenUnset) {
  EXPECT_FALSE(DefaultValue<int>::IsSet());
  EXPECT_TRUE(DefaultValue<int>::Exists());
  EXPECT_FALSE(DefaultValue<MyNonDefaultConstructible>::IsSet());
  EXPECT_FALSE(DefaultValue<MyNonDefaultConstructible>::Exists());

  EXPECT_EQ(0, DefaultValue<int>::Get());

  EXPECT_DEATH_IF_SUPPORTED(
      { DefaultValue<MyNonDefaultConstructible>::Get(); }, "");
}

TEST(DefaultValueTest, GetWorksForMoveOnlyIfSet) {
  EXPECT_TRUE(DefaultValue<std::unique_ptr<int>>::Exists());
  EXPECT_TRUE(DefaultValue<std::unique_ptr<int>>::Get() == nullptr);
  DefaultValue<std::unique_ptr<int>>::SetFactory(
      [] { return std::make_unique<int>(42); });
  EXPECT_TRUE(DefaultValue<std::unique_ptr<int>>::Exists());
  std::unique_ptr<int> i = DefaultValue<std::unique_ptr<int>>::Get();
  EXPECT_EQ(42, *i);
}

TEST(DefaultValueTest, GetWorksForVoid) { return DefaultValue<void>::Get(); }

TEST(DefaultValueOfReferenceTest, IsInitiallyUnset) {
  EXPECT_FALSE(DefaultValue<int&>::IsSet());
  EXPECT_FALSE(DefaultValue<MyDefaultConstructible&>::IsSet());
  EXPECT_FALSE(DefaultValue<MyNonDefaultConstructible&>::IsSet());
}

TEST(DefaultValueOfReferenceTest, IsInitiallyNotExisting) {
  EXPECT_FALSE(DefaultValue<int&>::Exists());
  EXPECT_FALSE(DefaultValue<MyDefaultConstructible&>::Exists());
  EXPECT_FALSE(DefaultValue<MyNonDefaultConstructible&>::Exists());
}

TEST(DefaultValueOfReferenceTest, CanBeSetAndUnset) {
  int n = 1;
  DefaultValue<const int&>::Set(n);
  MyNonDefaultConstructible x(42);
  DefaultValue<MyNonDefaultConstructible&>::Set(x);

  EXPECT_TRUE(DefaultValue<const int&>::Exists());
  EXPECT_TRUE(DefaultValue<MyNonDefaultConstructible&>::Exists());

  EXPECT_EQ(&n, &(DefaultValue<const int&>::Get()));
  EXPECT_EQ(&x, &(DefaultValue<MyNonDefaultConstructible&>::Get()));

  DefaultValue<const int&>::Clear();
  DefaultValue<MyNonDefaultConstructible&>::Clear();

  EXPECT_FALSE(DefaultValue<const int&>::Exists());
  EXPECT_FALSE(DefaultValue<MyNonDefaultConstructible&>::Exists());

  EXPECT_FALSE(DefaultValue<const int&>::IsSet());
  EXPECT_FALSE(DefaultValue<MyNonDefaultConstructible&>::IsSet());
}

TEST(DefaultValueOfReferenceDeathTest, GetReturnsBuiltInDefaultValueWhenUnset) {
  EXPECT_FALSE(DefaultValue<int&>::IsSet());
  EXPECT_FALSE(DefaultValue<MyNonDefaultConstructible&>::IsSet());

  EXPECT_DEATH_IF_SUPPORTED({ DefaultValue<int&>::Get(); }, "");
  EXPECT_DEATH_IF_SUPPORTED(
      { DefaultValue<MyNonDefaultConstructible>::Get(); }, "");
}

typedef int MyGlobalFunction(bool, int);

class MyActionImpl : public ActionInterface<MyGlobalFunction> {
 public:
  int Perform(const std::tuple<bool, int>& args) override {
    return std::get<0>(args) ? std::get<1>(args) : 0;
  }
};

TEST(ActionInterfaceTest, CanBeImplementedByDefiningPerform) {
  MyActionImpl my_action_impl;
  (void)my_action_impl;
}

TEST(ActionInterfaceTest, MakeAction) {
  Action<MyGlobalFunction> action = MakeAction(new MyActionImpl);

  EXPECT_EQ(5, action.Perform(std::make_tuple(true, 5)));
}

TEST(ActionTest, CanBeConstructedFromActionInterface) {
  Action<MyGlobalFunction> action(new MyActionImpl);
}

TEST(ActionTest, DelegatesWorkToActionInterface) {
  const Action<MyGlobalFunction> action(new MyActionImpl);

  EXPECT_EQ(5, action.Perform(std::make_tuple(true, 5)));
  EXPECT_EQ(0, action.Perform(std::make_tuple(false, 1)));
}

TEST(ActionTest, IsCopyable) {
  Action<MyGlobalFunction> a1(new MyActionImpl);
  Action<MyGlobalFunction> a2(a1);

  EXPECT_EQ(5, a1.Perform(std::make_tuple(true, 5)));
  EXPECT_EQ(0, a1.Perform(std::make_tuple(false, 1)));

  EXPECT_EQ(5, a2.Perform(std::make_tuple(true, 5)));
  EXPECT_EQ(0, a2.Perform(std::make_tuple(false, 1)));

  a2 = a1;

  EXPECT_EQ(5, a1.Perform(std::make_tuple(true, 5)));
  EXPECT_EQ(0, a1.Perform(std::make_tuple(false, 1)));

  EXPECT_EQ(5, a2.Perform(std::make_tuple(true, 5)));
  EXPECT_EQ(0, a2.Perform(std::make_tuple(false, 1)));
}

class IsNotZero : public ActionInterface<bool(int)> {
 public:
  bool Perform(const std::tuple<int>& arg) override {
    return std::get<0>(arg) != 0;
  }
};

TEST(ActionTest, CanBeConvertedToOtherActionType) {
  const Action<bool(int)> a1(new IsNotZero);
  const Action<int(char)> a2 = Action<int(char)>(a1);
  EXPECT_EQ(1, a2.Perform(std::make_tuple('a')));
  EXPECT_EQ(0, a2.Perform(std::make_tuple('\0')));
}

class ReturnSecondArgumentAction {
 public:

  template <typename Result, typename ArgumentTuple>
  Result Perform(const ArgumentTuple& args) {
    return std::get<1>(args);
  }
};

class ReturnZeroFromNullaryFunctionAction {
 public:

  template <typename Result>
  Result Perform(const std::tuple<>&) const {
    return 0;
  }
};

PolymorphicAction<ReturnSecondArgumentAction> ReturnSecondArgument() {
  return MakePolymorphicAction(ReturnSecondArgumentAction());
}

PolymorphicAction<ReturnZeroFromNullaryFunctionAction>
ReturnZeroFromNullaryFunction() {
  return MakePolymorphicAction(ReturnZeroFromNullaryFunctionAction());
}

TEST(MakePolymorphicActionTest, ConstructsActionFromImpl) {
  Action<int(bool, int, double)> a1 = ReturnSecondArgument();
  EXPECT_EQ(5, a1.Perform(std::make_tuple(false, 5, 2.0)));
}

TEST(MakePolymorphicActionTest, WorksWhenPerformHasOneTemplateParameter) {
  Action<int()> a1 = ReturnZeroFromNullaryFunction();
  EXPECT_EQ(0, a1.Perform(std::make_tuple()));

  Action<void*()> a2 = ReturnZeroFromNullaryFunction();
  EXPECT_TRUE(a2.Perform(std::make_tuple()) == nullptr);
}

TEST(ReturnTest, WorksForVoid) {
  const Action<void(int)> ret = Return();
  return ret.Perform(std::make_tuple(1));
}

TEST(ReturnTest, ReturnsGivenValue) {
  Action<int()> ret = Return(1);
  EXPECT_EQ(1, ret.Perform(std::make_tuple()));

  ret = Return(-5);
  EXPECT_EQ(-5, ret.Perform(std::make_tuple()));
}

TEST(ReturnTest, AcceptsStringLiteral) {
  Action<const char*()> a1 = Return("Hello");
  EXPECT_STREQ("Hello", a1.Perform(std::make_tuple()));

  Action<std::string()> a2 = Return("world");
  EXPECT_EQ("world", a2.Perform(std::make_tuple()));
}

TEST(ReturnTest, SupportsReferenceLikeReturnType) {

  struct Result {
    const std::vector<int>* v;
    Result(const std::vector<int>& vec) : v(&vec) {}
  };

  MockFunction<Result()> mock;
  EXPECT_CALL(mock, Call)
      .WillOnce(Return(std::vector<int>{17, 19, 23}))
      .WillRepeatedly(Return(std::vector<int>{29, 31, 37}));

  EXPECT_THAT(mock.AsStdFunction()(),
              Field(&Result::v, Pointee(ElementsAre(17, 19, 23))));

  EXPECT_THAT(mock.AsStdFunction()(),
              Field(&Result::v, Pointee(ElementsAre(29, 31, 37))));
}

TEST(ReturnTest, PrefersConversionOperator) {

  struct In;
  struct Out {
    int x;

    explicit Out(const int val) : x(val) {}
    explicit Out(const In&) : x(0) {}
  };

  struct In {
    operator Out() const { return Out{19}; }
  };

  EXPECT_THAT([]() -> Out { return In(); }(), Field(&Out::x, 19));

  MockFunction<Out()> mock;
  EXPECT_CALL(mock, Call).WillOnce(Return(In()));
  EXPECT_THAT(mock.AsStdFunction()(), Field(&Out::x, 19));
}

TEST(ReturnTest, ConversionRequiresConstLvalueReference) {
  using R = int;
  using U = std::reference_wrapper<const int>;

  static_assert(std::is_convertible<const R&, U>::value, "");
  static_assert(!std::is_convertible<R, U>::value, "");

  MockFunction<U()> mock;
  EXPECT_CALL(mock, Call).WillOnce(Return(17)).WillRepeatedly(Return(19));

  EXPECT_EQ(17, mock.AsStdFunction()());
  EXPECT_EQ(19, mock.AsStdFunction()());
}

TEST(ReturnTest, ConversionRequiresMutableLvalueReference) {

  struct S {
    S(std::string&) {}
  };

  static_assert(std::is_convertible<std::string&, S>::value, "");
#ifndef _MSC_VER
  static_assert(!std::is_convertible<std::string&&, S>::value, "");
#endif
  static_assert(!std::is_convertible<const std::string&, S>::value, "");

  using RA = decltype(Return(std::string()));

  static_assert(!std::is_convertible<RA, Action<S()>>::value, "");
#ifndef _MSC_VER
  static_assert(!std::is_convertible<RA, OnceAction<S()>>::value, "");
#endif
}

TEST(ReturnTest, MoveOnlyResultType) {

  {
    MockFunction<std::unique_ptr<int>()> mock;
    EXPECT_CALL(mock, Call)

        .WillOnce(Return(std::unique_ptr<int>(new int(17))));

    EXPECT_THAT(mock.AsStdFunction()(), Pointee(17));
  }

  static_assert(!std::is_convertible<decltype(Return(std::unique_ptr<int>())),
                                     Action<std::unique_ptr<int>()>>::value,
                "");
}

struct Base {
  bool operator==(const Base&) { return true; }
};

struct Derived : public Base {
  bool operator==(const Derived&) { return true; }
};

TEST(ReturnTest, IsCovariant) {
  Base base;
  Derived derived;
  Action<Base*()> ret = Return(&base);
  EXPECT_EQ(&base, ret.Perform(std::make_tuple()));

  ret = Return(&derived);
  EXPECT_EQ(&derived, ret.Perform(std::make_tuple()));
}

class FromType {
 public:
  explicit FromType(bool* is_converted) : converted_(is_converted) {}
  bool* converted() const { return converted_; }

 private:
  bool* const converted_;
};

class ToType {
 public:

  ToType(const FromType& x) { *x.converted() = true; }
};

TEST(ReturnTest, ConvertsArgumentWhenConverted) {
  bool converted = false;
  FromType x(&converted);
  Action<ToType()> action(Return(x));
  EXPECT_TRUE(converted) << "Return must convert its argument in its own "
                         << "conversion operator.";
  converted = false;
  action.Perform(std::tuple<>());
  EXPECT_FALSE(converted) << "Action must NOT convert its argument "
                          << "when performed.";
}

TEST(ReturnNullTest, WorksInPointerReturningFunction) {
  const Action<int*()> a1 = ReturnNull();
  EXPECT_TRUE(a1.Perform(std::make_tuple()) == nullptr);

  const Action<const char*(bool)> a2 = ReturnNull();
  EXPECT_TRUE(a2.Perform(std::make_tuple(true)) == nullptr);
}

TEST(ReturnNullTest, WorksInSmartPointerReturningFunction) {
  const Action<std::unique_ptr<const int>()> a1 = ReturnNull();
  EXPECT_TRUE(a1.Perform(std::make_tuple()) == nullptr);

  const Action<std::shared_ptr<int>(std::string)> a2 = ReturnNull();
  EXPECT_TRUE(a2.Perform(std::make_tuple("foo")) == nullptr);
}

TEST(ReturnRefTest, WorksForReference) {
  const int n = 0;
  const Action<const int&(bool)> ret = ReturnRef(n);

  EXPECT_EQ(&n, &ret.Perform(std::make_tuple(true)));
}

TEST(ReturnRefTest, IsCovariant) {
  Base base;
  Derived derived;
  Action<Base&()> a = ReturnRef(base);
  EXPECT_EQ(&base, &a.Perform(std::make_tuple()));

  a = ReturnRef(derived);
  EXPECT_EQ(&derived, &a.Perform(std::make_tuple()));
}

template <typename T, typename = decltype(ReturnRef(std::declval<T&&>()))>
bool CanCallReturnRef(T&&) {
  return true;
}
bool CanCallReturnRef(Unused) { return false; }

TEST(ReturnRefTest, WorksForNonTemporary) {
  int scalar_value = 123;
  EXPECT_TRUE(CanCallReturnRef(scalar_value));

  std::string non_scalar_value("ABC");
  EXPECT_TRUE(CanCallReturnRef(non_scalar_value));

  const int const_scalar_value{321};
  EXPECT_TRUE(CanCallReturnRef(const_scalar_value));

  const std::string const_non_scalar_value("CBA");
  EXPECT_TRUE(CanCallReturnRef(const_non_scalar_value));
}

TEST(ReturnRefTest, DoesNotWorkForTemporary) {
  auto scalar_value = []() -> int { return 123; };
  EXPECT_FALSE(CanCallReturnRef(scalar_value()));

  auto non_scalar_value = []() -> std::string { return "ABC"; };
  EXPECT_FALSE(CanCallReturnRef(non_scalar_value()));

  EXPECT_FALSE(CanCallReturnRef(static_cast<const int>(321)));

  auto const_non_scalar_value = []() -> const std::string { return "CBA"; };
  EXPECT_FALSE(CanCallReturnRef(const_non_scalar_value()));
}

TEST(ReturnRefOfCopyTest, WorksForReference) {
  int n = 42;
  const Action<const int&()> ret = ReturnRefOfCopy(n);

  EXPECT_NE(&n, &ret.Perform(std::make_tuple()));
  EXPECT_EQ(42, ret.Perform(std::make_tuple()));

  n = 43;
  EXPECT_NE(&n, &ret.Perform(std::make_tuple()));
  EXPECT_EQ(42, ret.Perform(std::make_tuple()));
}

TEST(ReturnRefOfCopyTest, IsCovariant) {
  Base base;
  Derived derived;
  Action<Base&()> a = ReturnRefOfCopy(base);
  EXPECT_NE(&base, &a.Perform(std::make_tuple()));

  a = ReturnRefOfCopy(derived);
  EXPECT_NE(&derived, &a.Perform(std::make_tuple()));
}

TEST(ReturnRoundRobinTest, WorksForInitList) {
  Action<int()> ret = ReturnRoundRobin({1, 2, 3});

  EXPECT_EQ(1, ret.Perform(std::make_tuple()));
  EXPECT_EQ(2, ret.Perform(std::make_tuple()));
  EXPECT_EQ(3, ret.Perform(std::make_tuple()));
  EXPECT_EQ(1, ret.Perform(std::make_tuple()));
  EXPECT_EQ(2, ret.Perform(std::make_tuple()));
  EXPECT_EQ(3, ret.Perform(std::make_tuple()));
}

TEST(ReturnRoundRobinTest, WorksForVector) {
  std::vector<double> v = {4.4, 5.5, 6.6};
  Action<double()> ret = ReturnRoundRobin(v);

  EXPECT_EQ(4.4, ret.Perform(std::make_tuple()));
  EXPECT_EQ(5.5, ret.Perform(std::make_tuple()));
  EXPECT_EQ(6.6, ret.Perform(std::make_tuple()));
  EXPECT_EQ(4.4, ret.Perform(std::make_tuple()));
  EXPECT_EQ(5.5, ret.Perform(std::make_tuple()));
  EXPECT_EQ(6.6, ret.Perform(std::make_tuple()));
}

class MockClass {
 public:
  MockClass() = default;

  MOCK_METHOD1(IntFunc, int(bool flag));
  MOCK_METHOD0(Foo, MyNonDefaultConstructible());
  MOCK_METHOD0(MakeUnique, std::unique_ptr<int>());
  MOCK_METHOD0(MakeUniqueBase, std::unique_ptr<Base>());
  MOCK_METHOD0(MakeVectorUnique, std::vector<std::unique_ptr<int>>());
  MOCK_METHOD1(TakeUnique, int(std::unique_ptr<int>));
  MOCK_METHOD2(TakeUnique,
               int(const std::unique_ptr<int>&, std::unique_ptr<int>));

 private:
  MockClass(const MockClass&) = delete;
  MockClass& operator=(const MockClass&) = delete;
};

TEST(DoDefaultTest, ReturnsBuiltInDefaultValueByDefault) {
  MockClass mock;
  EXPECT_CALL(mock, IntFunc(_)).WillOnce(DoDefault());
  EXPECT_EQ(0, mock.IntFunc(true));
}

TEST(DoDefaultDeathTest, DiesForUnknowType) {
  MockClass mock;
  EXPECT_CALL(mock, Foo()).WillRepeatedly(DoDefault());
#if GTEST_HAS_EXCEPTIONS
  EXPECT_ANY_THROW(mock.Foo());
#else
  EXPECT_DEATH_IF_SUPPORTED({ mock.Foo(); }, "");
#endif
}

void VoidFunc(bool           ) {}

TEST(DoDefaultDeathTest, DiesIfUsedInCompositeAction) {
  MockClass mock;
  EXPECT_CALL(mock, IntFunc(_))
      .WillRepeatedly(DoAll(Invoke(VoidFunc), DoDefault()));

  EXPECT_DEATH_IF_SUPPORTED({ mock.IntFunc(true); }, "");
}

TEST(DoDefaultTest, ReturnsUserSpecifiedPerTypeDefaultValueWhenThereIsOne) {
  DefaultValue<int>::Set(1);
  MockClass mock;
  EXPECT_CALL(mock, IntFunc(_)).WillOnce(DoDefault());
  EXPECT_EQ(1, mock.IntFunc(false));
  DefaultValue<int>::Clear();
}

TEST(DoDefaultTest, DoesWhatOnCallSpecifies) {
  MockClass mock;
  ON_CALL(mock, IntFunc(_)).WillByDefault(Return(2));
  EXPECT_CALL(mock, IntFunc(_)).WillOnce(DoDefault());
  EXPECT_EQ(2, mock.IntFunc(false));
}

TEST(DoDefaultTest, CannotBeUsedInOnCall) {
  MockClass mock;
  EXPECT_NONFATAL_FAILURE(
      {
        ON_CALL(mock, IntFunc(_)).WillByDefault(DoDefault());
      },
      "DoDefault() cannot be used in ON_CALL()");
}

TEST(SetArgPointeeTest, SetsTheNthPointee) {
  typedef void MyFunction(bool, int*, char*);
  Action<MyFunction> a = SetArgPointee<1>(2);

  int n = 0;
  char ch = '\0';
  a.Perform(std::make_tuple(true, &n, &ch));
  EXPECT_EQ(2, n);
  EXPECT_EQ('\0', ch);

  a = SetArgPointee<2>('a');
  n = 0;
  ch = '\0';
  a.Perform(std::make_tuple(true, &n, &ch));
  EXPECT_EQ(0, n);
  EXPECT_EQ('a', ch);
}

TEST(SetArgPointeeTest, AcceptsStringLiteral) {
  typedef void MyFunction(std::string*, const char**);
  Action<MyFunction> a = SetArgPointee<0>("hi");
  std::string str;
  const char* ptr = nullptr;
  a.Perform(std::make_tuple(&str, &ptr));
  EXPECT_EQ("hi", str);
  EXPECT_TRUE(ptr == nullptr);

  a = SetArgPointee<1>("world");
  str = "";
  a.Perform(std::make_tuple(&str, &ptr));
  EXPECT_EQ("", str);
  EXPECT_STREQ("world", ptr);
}

TEST(SetArgPointeeTest, AcceptsWideStringLiteral) {
  typedef void MyFunction(const wchar_t**);
  Action<MyFunction> a = SetArgPointee<0>(L"world");
  const wchar_t* ptr = nullptr;
  a.Perform(std::make_tuple(&ptr));
  EXPECT_STREQ(L"world", ptr);

#if GTEST_HAS_STD_WSTRING

  typedef void MyStringFunction(std::wstring*);
  Action<MyStringFunction> a2 = SetArgPointee<0>(L"world");
  std::wstring str = L"";
  a2.Perform(std::make_tuple(&str));
  EXPECT_EQ(L"world", str);

#endif
}

TEST(SetArgPointeeTest, AcceptsCharPointer) {
  typedef void MyFunction(bool, std::string*, const char**);
  const char* const hi = "hi";
  Action<MyFunction> a = SetArgPointee<1>(hi);
  std::string str;
  const char* ptr = nullptr;
  a.Perform(std::make_tuple(true, &str, &ptr));
  EXPECT_EQ("hi", str);
  EXPECT_TRUE(ptr == nullptr);

  char world_array[] = "world";
  char* const world = world_array;
  a = SetArgPointee<2>(world);
  str = "";
  a.Perform(std::make_tuple(true, &str, &ptr));
  EXPECT_EQ("", str);
  EXPECT_EQ(world, ptr);
}

TEST(SetArgPointeeTest, AcceptsWideCharPointer) {
  typedef void MyFunction(bool, const wchar_t**);
  const wchar_t* const hi = L"hi";
  Action<MyFunction> a = SetArgPointee<1>(hi);
  const wchar_t* ptr = nullptr;
  a.Perform(std::make_tuple(true, &ptr));
  EXPECT_EQ(hi, ptr);

#if GTEST_HAS_STD_WSTRING

  typedef void MyStringFunction(bool, std::wstring*);
  wchar_t world_array[] = L"world";
  wchar_t* const world = world_array;
  Action<MyStringFunction> a2 = SetArgPointee<1>(world);
  std::wstring str;
  a2.Perform(std::make_tuple(true, &str));
  EXPECT_EQ(world_array, str);
#endif
}

TEST(SetArgumentPointeeTest, SetsTheNthPointee) {
  typedef void MyFunction(bool, int*, char*);
  Action<MyFunction> a = SetArgumentPointee<1>(2);

  int n = 0;
  char ch = '\0';
  a.Perform(std::make_tuple(true, &n, &ch));
  EXPECT_EQ(2, n);
  EXPECT_EQ('\0', ch);

  a = SetArgumentPointee<2>('a');
  n = 0;
  ch = '\0';
  a.Perform(std::make_tuple(true, &n, &ch));
  EXPECT_EQ(0, n);
  EXPECT_EQ('a', ch);
}

int Nullary() { return 1; }

class NullaryFunctor {
 public:
  int operator()() { return 2; }
};

bool g_done = false;
void VoidNullary() { g_done = true; }

class VoidNullaryFunctor {
 public:
  void operator()() { g_done = true; }
};

short Short(short n) { return n; }
char Char(char ch) { return ch; }

const char* CharPtr(const char* s) { return s; }

bool Unary(int x) { return x < 0; }

const char* Binary(const char* input, short n) { return input + n; }

void VoidBinary(int, char) { g_done = true; }

int Ternary(int x, char y, short z) { return x + y + z; }

int SumOf4(int a, int b, int c, int d) { return a + b + c + d; }

class Foo {
 public:
  Foo() : value_(123) {}

  int Nullary() const { return value_; }

 private:
  int value_;
};

TEST(InvokeWithoutArgsTest, Function) {

  Action<int(int)> a = InvokeWithoutArgs(Nullary);
  EXPECT_EQ(1, a.Perform(std::make_tuple(2)));

  Action<int(int, double)> a2 = InvokeWithoutArgs(Nullary);
  EXPECT_EQ(1, a2.Perform(std::make_tuple(2, 3.5)));

  Action<void(int)> a3 = InvokeWithoutArgs(VoidNullary);
  g_done = false;
  a3.Perform(std::make_tuple(1));
  EXPECT_TRUE(g_done);
}

TEST(InvokeWithoutArgsTest, Functor) {

  Action<int()> a = InvokeWithoutArgs(NullaryFunctor());
  EXPECT_EQ(2, a.Perform(std::make_tuple()));

  Action<int(int, double, char)> a2 =
      InvokeWithoutArgs(NullaryFunctor());
  EXPECT_EQ(2, a2.Perform(std::make_tuple(3, 3.5, 'a')));

  Action<void()> a3 = InvokeWithoutArgs(VoidNullaryFunctor());
  g_done = false;
  a3.Perform(std::make_tuple());
  EXPECT_TRUE(g_done);
}

TEST(InvokeWithoutArgsTest, Method) {
  Foo foo;
  Action<int(bool, char)> a =
      InvokeWithoutArgs(&foo, &Foo::Nullary);
  EXPECT_EQ(123, a.Perform(std::make_tuple(true, 'a')));
}

TEST(IgnoreResultTest, PolymorphicAction) {
  Action<void(int)> a = IgnoreResult(Return(5));
  a.Perform(std::make_tuple(1));
}

int ReturnOne() {
  g_done = true;
  return 1;
}

TEST(IgnoreResultTest, MonomorphicAction) {
  g_done = false;
  Action<void()> a = IgnoreResult(Invoke(ReturnOne));
  a.Perform(std::make_tuple());
  EXPECT_TRUE(g_done);
}

MyNonDefaultConstructible ReturnMyNonDefaultConstructible(double        ) {
  g_done = true;
  return MyNonDefaultConstructible(42);
}

TEST(IgnoreResultTest, ActionReturningClass) {
  g_done = false;
  Action<void(int)> a =
      IgnoreResult(Invoke(ReturnMyNonDefaultConstructible));
  a.Perform(std::make_tuple(2));
  EXPECT_TRUE(g_done);
}

TEST(AssignTest, Int) {
  int x = 0;
  Action<void(int)> a = Assign(&x, 5);
  a.Perform(std::make_tuple(0));
  EXPECT_EQ(5, x);
}

TEST(AssignTest, String) {
  ::std::string x;
  Action<void(void)> a = Assign(&x, "Hello, world");
  a.Perform(std::make_tuple());
  EXPECT_EQ("Hello, world", x);
}

TEST(AssignTest, CompatibleTypes) {
  double x = 0;
  Action<void(int)> a = Assign(&x, 5);
  a.Perform(std::make_tuple(0));
  EXPECT_DOUBLE_EQ(5, x);
}

TEST(DoAll, SupportsRefQualifiedActions) {
  struct InitialAction {
    void operator()(const int arg) && { EXPECT_EQ(17, arg); }
  };

  struct FinalAction {
    int operator()() && { return 19; }
  };

  MockFunction<int(int)> mock;
  EXPECT_CALL(mock, Call).WillOnce(DoAll(InitialAction{}, FinalAction{}));
  EXPECT_EQ(19, mock.AsStdFunction()(17));
}

TEST(DoAll, ProvidesLvalueReferencesToInitialActions) {
  struct Obj {};

  {
    struct InitialAction {
      void operator()(Obj&) const { FAIL() << "Unexpected call"; }
      void operator()(const Obj&) const {}
      void operator()(Obj&&) const { FAIL() << "Unexpected call"; }
      void operator()(const Obj&&) const { FAIL() << "Unexpected call"; }
    };

    MockFunction<void(Obj)> mock;
    EXPECT_CALL(mock, Call)
        .WillOnce(DoAll(InitialAction{}, InitialAction{}, [](Obj&&) {}))
        .WillRepeatedly(DoAll(InitialAction{}, InitialAction{}, [](Obj&&) {}));

    mock.AsStdFunction()(Obj{});
    mock.AsStdFunction()(Obj{});
  }

  {
    struct InitialAction {
      void operator()(Obj&) const { FAIL() << "Unexpected call"; }
      void operator()(const Obj&) const {}
      void operator()(Obj&&) const { FAIL() << "Unexpected call"; }
      void operator()(const Obj&&) const { FAIL() << "Unexpected call"; }
    };

    MockFunction<void(const Obj&)> mock;
    EXPECT_CALL(mock, Call)
        .WillOnce(DoAll(InitialAction{}, InitialAction{}, [](const Obj&) {}))
        .WillRepeatedly(
            DoAll(InitialAction{}, InitialAction{}, [](const Obj&) {}));

    mock.AsStdFunction()(Obj{});
    mock.AsStdFunction()(Obj{});
  }

  {
    struct InitialAction {
      void operator()(Obj&) const {}
      void operator()(Obj&&) const { FAIL() << "Unexpected call"; }
    };

    MockFunction<void(Obj&)> mock;
    EXPECT_CALL(mock, Call)
        .WillOnce(DoAll(InitialAction{}, InitialAction{}, [](Obj&) {}))
        .WillRepeatedly(DoAll(InitialAction{}, InitialAction{}, [](Obj&) {}));

    Obj obj;
    mock.AsStdFunction()(obj);
    mock.AsStdFunction()(obj);
  }

  {
    struct InitialAction {
      void operator()(Obj&) const {}
      void operator()(Obj&&) const { FAIL() << "Unexpected call"; }
    };

    MockFunction<void(Obj&&)> mock;
    EXPECT_CALL(mock, Call)
        .WillOnce(DoAll(InitialAction{}, InitialAction{}, [](Obj&&) {}))
        .WillRepeatedly(DoAll(InitialAction{}, InitialAction{}, [](Obj&&) {}));

    mock.AsStdFunction()(Obj{});
    mock.AsStdFunction()(Obj{});
  }

  {
    struct InitialAction {
      void operator()(Obj&) && {}
    };

    MockFunction<void(Obj&)> mock;
    EXPECT_CALL(mock, Call)
        .WillOnce(DoAll(InitialAction{}, InitialAction{}, [](Obj&) {}));

    Obj obj;
    mock.AsStdFunction()(obj);
  }

  {
    struct InitialAction {
      void operator()(Obj&) && {}
    };

    MockFunction<void(Obj&&)> mock;
    EXPECT_CALL(mock, Call)
        .WillOnce(DoAll(InitialAction{}, InitialAction{}, [](Obj&&) {}));

    mock.AsStdFunction()(Obj{});
  }
}

TEST(DoAll, SupportsTypeErasedActions) {

  const Action<void()> initial_action = [] {};
  const Action<int()> final_action = [] { return 17; };

  MockFunction<int()> mock;
  EXPECT_CALL(mock, Call)
      .WillOnce(DoAll(initial_action, initial_action, final_action))
      .WillRepeatedly(DoAll(initial_action, initial_action, final_action));

  EXPECT_EQ(17, mock.AsStdFunction()());

  {
    struct FinalAction {
      FinalAction() = default;
      FinalAction(FinalAction&&) = default;

      int operator()() && { return 17; }
    };

    EXPECT_CALL(mock, Call)
        .WillOnce(DoAll(initial_action, initial_action, FinalAction{}));

    EXPECT_EQ(17, mock.AsStdFunction()());
  }
}

TEST(DoAll, ConvertibleToOnceActionWithUserProvidedActionConversion) {

  struct CustomFinal final {
    operator Action<int()>() {
      return Return(17);
    }

    operator Action<int(int, char)>() {
      return Return(19);
    }
  };

  {
    OnceAction<int()> action = DoAll(CustomFinal{});
    EXPECT_EQ(17, std::move(action).Call());
  }

  {
    OnceAction<int(int, char)> action = DoAll(CustomFinal{});
    EXPECT_EQ(19, std::move(action).Call(0, 0));
  }

  struct CustomInitial final {
    operator Action<void()>() {
      return [] {};
    }

    operator Action<void(int, char)>() {
      return [] {};
    }
  };

  {
    OnceAction<int()> action = DoAll(CustomInitial{}, CustomFinal{});
    EXPECT_EQ(17, std::move(action).Call());
  }

  {
    OnceAction<int(int, char)> action = DoAll(CustomInitial{}, CustomFinal{});
    EXPECT_EQ(19, std::move(action).Call(0, 0));
  }
}

TEST(WithArgsTest, OneArg) {
  Action<bool(double x, int n)> a = WithArgs<1>(Invoke(Unary));
  EXPECT_TRUE(a.Perform(std::make_tuple(1.5, -1)));
  EXPECT_FALSE(a.Perform(std::make_tuple(1.5, 1)));
}

TEST(WithArgsTest, TwoArgs) {
  Action<const char*(const char* s, double x, short n)> a =
      WithArgs<0, 2>(Invoke(Binary));
  const char s[] = "Hello";
  EXPECT_EQ(s + 2, a.Perform(std::make_tuple(CharPtr(s), 0.5, Short(2))));
}

struct ConcatAll {
  std::string operator()() const { return {}; }
  template <typename... I>
  std::string operator()(const char* a, I... i) const {
    return a + ConcatAll()(i...);
  }
};

TEST(WithArgsTest, TenArgs) {
  Action<std::string(const char*, const char*, const char*, const char*)> a =
      WithArgs<0, 1, 2, 3, 2, 1, 0, 1, 2, 3>(Invoke(ConcatAll{}));
  EXPECT_EQ("0123210123",
            a.Perform(std::make_tuple(CharPtr("0"), CharPtr("1"), CharPtr("2"),
                                      CharPtr("3"))));
}

class SubtractAction : public ActionInterface<int(int, int)> {
 public:
  int Perform(const std::tuple<int, int>& args) override {
    return std::get<0>(args) - std::get<1>(args);
  }
};

TEST(WithArgsTest, NonInvokeAction) {
  Action<int(const std::string&, int, int)> a =
      WithArgs<2, 1>(MakeAction(new SubtractAction));
  std::tuple<std::string, int, int> dummy =
      std::make_tuple(std::string("hi"), 2, 10);
  EXPECT_EQ(8, a.Perform(dummy));
}

TEST(WithArgsTest, Identity) {
  Action<int(int x, char y, short z)> a =
      WithArgs<0, 1, 2>(Invoke(Ternary));
  EXPECT_EQ(123, a.Perform(std::make_tuple(100, Char(20), Short(3))));
}

TEST(WithArgsTest, RepeatedArguments) {
  Action<int(bool, int m, int n)> a =
      WithArgs<1, 1, 1, 1>(Invoke(SumOf4));
  EXPECT_EQ(4, a.Perform(std::make_tuple(false, 1, 10)));
}

TEST(WithArgsTest, ReversedArgumentOrder) {
  Action<const char*(short n, const char* input)> a =
      WithArgs<1, 0>(Invoke(Binary));
  const char s[] = "Hello";
  EXPECT_EQ(s + 2, a.Perform(std::make_tuple(Short(2), CharPtr(s))));
}

TEST(WithArgsTest, ArgsOfCompatibleTypes) {
  Action<long(short x, char y, double z, char c)> a =
      WithArgs<0, 1, 3>(Invoke(Ternary));
  EXPECT_EQ(123,
            a.Perform(std::make_tuple(Short(100), Char(20), 5.6, Char(3))));
}

TEST(WithArgsTest, VoidAction) {
  Action<void(double x, char c, int n)> a = WithArgs<2, 1>(Invoke(VoidBinary));
  g_done = false;
  a.Perform(std::make_tuple(1.5, 'a', 3));
  EXPECT_TRUE(g_done);
}

TEST(WithArgsTest, ReturnReference) {
  Action<int&(int&, void*)> aa = WithArgs<0>([](int& a) -> int& { return a; });
  int i = 0;
  const int& res = aa.Perform(std::forward_as_tuple(i, nullptr));
  EXPECT_EQ(&i, &res);
}

TEST(WithArgsTest, InnerActionWithConversion) {
  Action<Derived*()> inner = [] { return nullptr; };

  MockFunction<Base*(double)> mock;
  EXPECT_CALL(mock, Call)
      .WillOnce(WithoutArgs(inner))
      .WillRepeatedly(WithoutArgs(inner));

  EXPECT_EQ(nullptr, mock.AsStdFunction()(1.1));
  EXPECT_EQ(nullptr, mock.AsStdFunction()(1.1));
}

TEST(WithArgsTest, RefQualifiedInnerAction) {
  struct SomeAction {
    int operator()(const int arg) && {
      EXPECT_EQ(17, arg);
      return 19;
    }
  };

  MockFunction<int(int, int)> mock;
  EXPECT_CALL(mock, Call).WillOnce(WithArg<1>(SomeAction{}));
  EXPECT_EQ(19, mock.AsStdFunction()(0, 17));
}

#ifndef GTEST_OS_WINDOWS_MOBILE

class SetErrnoAndReturnTest : public testing::Test {
 protected:
  void SetUp() override { errno = 0; }
  void TearDown() override { errno = 0; }
};

TEST_F(SetErrnoAndReturnTest, Int) {
  Action<int(void)> a = SetErrnoAndReturn(ENOTTY, -5);
  EXPECT_EQ(-5, a.Perform(std::make_tuple()));
  EXPECT_EQ(ENOTTY, errno);
}

TEST_F(SetErrnoAndReturnTest, Ptr) {
  int x;
  Action<int*(void)> a = SetErrnoAndReturn(ENOTTY, &x);
  EXPECT_EQ(&x, a.Perform(std::make_tuple()));
  EXPECT_EQ(ENOTTY, errno);
}

TEST_F(SetErrnoAndReturnTest, CompatibleTypes) {
  Action<double()> a = SetErrnoAndReturn(EINVAL, 5);
  EXPECT_DOUBLE_EQ(5.0, a.Perform(std::make_tuple()));
  EXPECT_EQ(EINVAL, errno);
}

#endif

TEST(ByRefTest, IsCopyable) {
  const std::string s1 = "Hi";
  const std::string s2 = "Hello";

  auto ref_wrapper = ByRef(s1);
  const std::string& r1 = ref_wrapper;
  EXPECT_EQ(&s1, &r1);

  ref_wrapper = ByRef(s2);
  const std::string& r2 = ref_wrapper;
  EXPECT_EQ(&s2, &r2);

  auto ref_wrapper1 = ByRef(s1);

  ref_wrapper = ref_wrapper1;
  const std::string& r3 = ref_wrapper;
  EXPECT_EQ(&s1, &r3);
}

TEST(ByRefTest, ConstValue) {
  const int n = 0;

  const int& const_ref = ByRef(n);
  EXPECT_EQ(&n, &const_ref);
}

TEST(ByRefTest, NonConstValue) {
  int n = 0;

  int& ref = ByRef(n);
  EXPECT_EQ(&n, &ref);

  const int& const_ref = ByRef(n);
  EXPECT_EQ(&n, &const_ref);
}

TEST(ByRefTest, ExplicitType) {
  int n = 0;
  const int& r1 = ByRef<const int>(n);
  EXPECT_EQ(&n, &r1);

  Derived d;
  Derived& r2 = ByRef<Derived>(d);
  EXPECT_EQ(&d, &r2);

  const Derived& r3 = ByRef<const Derived>(d);
  EXPECT_EQ(&d, &r3);

  Base& r4 = ByRef<Base>(d);
  EXPECT_EQ(&d, &r4);

  const Base& r5 = ByRef<const Base>(d);
  EXPECT_EQ(&d, &r5);

}

TEST(ByRefTest, PrintsCorrectly) {
  int n = 42;
  ::std::stringstream expected, actual;
  testing::internal::UniversalPrinter<const int&>::Print(n, &expected);
  testing::internal::UniversalPrint(ByRef(n), &actual);
  EXPECT_EQ(expected.str(), actual.str());
}

struct UnaryConstructorClass {
  explicit UnaryConstructorClass(int v) : value(v) {}
  int value;
};

TEST(ReturnNewTest, Unary) {
  Action<UnaryConstructorClass*()> a = ReturnNew<UnaryConstructorClass>(4000);
  UnaryConstructorClass* c = a.Perform(std::make_tuple());
  EXPECT_EQ(4000, c->value);
  delete c;
}

TEST(ReturnNewTest, UnaryWorksWhenMockMethodHasArgs) {
  Action<UnaryConstructorClass*(bool, int)> a =
      ReturnNew<UnaryConstructorClass>(4000);
  UnaryConstructorClass* c = a.Perform(std::make_tuple(false, 5));
  EXPECT_EQ(4000, c->value);
  delete c;
}

TEST(ReturnNewTest, UnaryWorksWhenMockMethodReturnsPointerToConst) {
  Action<const UnaryConstructorClass*()> a =
      ReturnNew<UnaryConstructorClass>(4000);
  const UnaryConstructorClass* c = a.Perform(std::make_tuple());
  EXPECT_EQ(4000, c->value);
  delete c;
}

class TenArgConstructorClass {
 public:
  TenArgConstructorClass(int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                         int a8, int a9, int a10)
      : value_(a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10) {}
  int value_;
};

TEST(ReturnNewTest, ConstructorThatTakes10Arguments) {
  Action<TenArgConstructorClass*()> a = ReturnNew<TenArgConstructorClass>(
      1000000000, 200000000, 30000000, 4000000, 500000, 60000, 7000, 800, 90,
      0);
  TenArgConstructorClass* c = a.Perform(std::make_tuple());
  EXPECT_EQ(1234567890, c->value_);
  delete c;
}

std::unique_ptr<int> UniquePtrSource() { return std::make_unique<int>(19); }

std::vector<std::unique_ptr<int>> VectorUniquePtrSource() {
  std::vector<std::unique_ptr<int>> out;
  out.emplace_back(new int(7));
  return out;
}

TEST(MockMethodTest, CanReturnMoveOnlyValue_Return) {
  MockClass mock;
  std::unique_ptr<int> i(new int(19));
  EXPECT_CALL(mock, MakeUnique()).WillOnce(Return(ByMove(std::move(i))));
  EXPECT_CALL(mock, MakeVectorUnique())
      .WillOnce(Return(ByMove(VectorUniquePtrSource())));
  Derived* d = new Derived;
  EXPECT_CALL(mock, MakeUniqueBase())
      .WillOnce(Return(ByMove(std::unique_ptr<Derived>(d))));

  std::unique_ptr<int> result1 = mock.MakeUnique();
  EXPECT_EQ(19, *result1);

  std::vector<std::unique_ptr<int>> vresult = mock.MakeVectorUnique();
  EXPECT_EQ(1u, vresult.size());
  EXPECT_NE(nullptr, vresult[0]);
  EXPECT_EQ(7, *vresult[0]);

  std::unique_ptr<Base> result2 = mock.MakeUniqueBase();
  EXPECT_EQ(d, result2.get());
}

TEST(MockMethodTest, CanReturnMoveOnlyValue_DoAllReturn) {
  testing::MockFunction<void()> mock_function;
  MockClass mock;
  std::unique_ptr<int> i(new int(19));
  EXPECT_CALL(mock_function, Call());
  EXPECT_CALL(mock, MakeUnique())
      .WillOnce(DoAll(InvokeWithoutArgs(&mock_function,
                                        &testing::MockFunction<void()>::Call),
                      Return(ByMove(std::move(i)))));

  std::unique_ptr<int> result1 = mock.MakeUnique();
  EXPECT_EQ(19, *result1);
}

TEST(MockMethodTest, CanReturnMoveOnlyValue_Invoke) {
  MockClass mock;

  DefaultValue<std::unique_ptr<int>>::SetFactory(
      [] { return std::make_unique<int>(42); });
  EXPECT_EQ(42, *mock.MakeUnique());

  EXPECT_CALL(mock, MakeUnique()).WillRepeatedly(Invoke(UniquePtrSource));
  EXPECT_CALL(mock, MakeVectorUnique())
      .WillRepeatedly(Invoke(VectorUniquePtrSource));
  std::unique_ptr<int> result1 = mock.MakeUnique();
  EXPECT_EQ(19, *result1);
  std::unique_ptr<int> result2 = mock.MakeUnique();
  EXPECT_EQ(19, *result2);
  EXPECT_NE(result1, result2);

  std::vector<std::unique_ptr<int>> vresult = mock.MakeVectorUnique();
  EXPECT_EQ(1u, vresult.size());
  EXPECT_NE(nullptr, vresult[0]);
  EXPECT_EQ(7, *vresult[0]);
}

TEST(MockMethodTest, CanTakeMoveOnlyValue) {
  MockClass mock;
  auto make = [](int i) { return std::make_unique<int>(i); };

  EXPECT_CALL(mock, TakeUnique(_)).WillRepeatedly([](std::unique_ptr<int> i) {
    return *i;
  });

  EXPECT_CALL(mock, TakeUnique(testing::Pointee(7)))
      .WillOnce(Return(-7))
      .RetiresOnSaturation();
  EXPECT_CALL(mock, TakeUnique(testing::IsNull()))
      .WillOnce(Return(-1))
      .RetiresOnSaturation();

  EXPECT_EQ(5, mock.TakeUnique(make(5)));
  EXPECT_EQ(-7, mock.TakeUnique(make(7)));
  EXPECT_EQ(7, mock.TakeUnique(make(7)));
  EXPECT_EQ(7, mock.TakeUnique(make(7)));
  EXPECT_EQ(-1, mock.TakeUnique({}));

  auto lvalue = make(6);
  EXPECT_CALL(mock, TakeUnique(_, _))
      .WillOnce([](const std::unique_ptr<int>& i, std::unique_ptr<int> j) {
        return *i * *j;
      });
  EXPECT_EQ(42, mock.TakeUnique(lvalue, make(7)));

  std::unique_ptr<int> saved;
  EXPECT_CALL(mock, TakeUnique(_)).WillOnce([&saved](std::unique_ptr<int> i) {
    saved = std::move(i);
    return 0;
  });
  EXPECT_EQ(0, mock.TakeUnique(make(42)));
  EXPECT_EQ(42, *saved);
}

TEST(MockMethodTest, ActionHasRvalueRefQualifiedCallOperator) {
  struct Return17 {
    int operator()() && { return 17; }
  };

  {
    MockFunction<int()> mock;
    EXPECT_CALL(mock, Call).WillOnce(Return17());

    EXPECT_EQ(17, mock.AsStdFunction()());
  }

  {
    MockFunction<int(int)> mock;
    EXPECT_CALL(mock, Call).WillOnce(Return17());

    EXPECT_EQ(17, mock.AsStdFunction()(0));
  }
}

TEST(MockMethodTest, ActionHasMultipleCallOperators) {
  struct ReturnInt {
    int operator()() && { return 17; }
    int operator()() const& { return 19; }
  };

  {
    MockFunction<int()> mock;
    EXPECT_CALL(mock, Call).WillOnce(ReturnInt()).WillRepeatedly(ReturnInt());

    EXPECT_EQ(17, mock.AsStdFunction()());
    EXPECT_EQ(19, mock.AsStdFunction()());
    EXPECT_EQ(19, mock.AsStdFunction()());
  }

  {
    MockFunction<int(int)> mock;
    EXPECT_CALL(mock, Call).WillOnce(ReturnInt()).WillRepeatedly(ReturnInt());

    EXPECT_EQ(17, mock.AsStdFunction()(0));
    EXPECT_EQ(19, mock.AsStdFunction()(0));
    EXPECT_EQ(19, mock.AsStdFunction()(0));
  }
}

TEST(MockMethodTest, MoveOnlyAction) {

  {
    struct Return17 {
      Return17() = default;
      Return17(Return17&&) = default;

      Return17(const Return17&) = delete;
      Return17 operator=(const Return17&) = delete;

      int operator()() && { return 17; }
    };

    MockFunction<int()> mock;
    EXPECT_CALL(mock, Call).WillOnce(Return17());
    EXPECT_EQ(17, mock.AsStdFunction()());
  }

  {
    struct Return17 {
      Return17() = default;
      Return17(Return17&&) = default;

      Return17(const Return17&) = delete;
      Return17 operator=(const Return17&) = delete;

      int operator()() const { return 17; }
    };

    MockFunction<int()> mock;
    EXPECT_CALL(mock, Call).WillOnce(Return17());
    EXPECT_EQ(17, mock.AsStdFunction()());
  }
}

TEST(MockMethodTest, ActionReturnsIgnoredValue) {
  struct ReturnInt {
    int operator()() const { return 0; }
  };

  MockFunction<void()> mock;
  EXPECT_CALL(mock, Call).WillOnce(ReturnInt()).WillRepeatedly(ReturnInt());

  mock.AsStdFunction()();
  mock.AsStdFunction()();
}

TEST(MockMethodTest, WillOnceCanAcceptLvalueReference) {
  MockFunction<int()> mock;

  const auto action = [] { return 17; };
  EXPECT_CALL(mock, Call).WillOnce(action);

  EXPECT_EQ(17, mock.AsStdFunction()());
}

struct StaticAssertSingleArgument {
  template <typename... Args>
  static constexpr bool CheckArgs() {
    static_assert(sizeof...(Args) == 1, "");
    return true;
  }

  template <typename... Args, bool = CheckArgs<Args...>()>
  int operator()(Args...) const {
    return 17;
  }
};

TEST(MockMethodTest, ActionSwallowsAllArguments) {
  MockFunction<int(int)> mock;
  EXPECT_CALL(mock, Call)
      .WillOnce(StaticAssertSingleArgument{})
      .WillRepeatedly(StaticAssertSingleArgument{});

  EXPECT_EQ(17, mock.AsStdFunction()(0));
  EXPECT_EQ(17, mock.AsStdFunction()(0));
}

struct ActionWithTemplatedConversionOperators {
  template <typename... Args>
  operator OnceAction<int(Args...)>() && {
    return [] { return 17; };
  }

  template <typename... Args>
  operator Action<int(Args...)>() const {
    return [] { return 19; };
  }
};

TEST(MockMethodTest, ActionHasTemplatedConversionOperators) {
  MockFunction<int()> mock;
  EXPECT_CALL(mock, Call)
      .WillOnce(ActionWithTemplatedConversionOperators{})
      .WillRepeatedly(ActionWithTemplatedConversionOperators{});

  EXPECT_EQ(17, mock.AsStdFunction()());
  EXPECT_EQ(19, mock.AsStdFunction()());
}

int Add(int val, int& ref, int* ptr) {
  int result = val + ref + *ptr;
  ref = 42;
  *ptr = 43;
  return result;
}

int Deref(std::unique_ptr<int> ptr) { return *ptr; }

struct Double {
  template <typename T>
  T operator()(T t) {
    return 2 * t;
  }
};

std::unique_ptr<int> UniqueInt(int i) { return std::make_unique<int>(i); }

TEST(FunctorActionTest, ActionFromFunction) {
  Action<int(int, int&, int*)> a = &Add;
  int x = 1, y = 2, z = 3;
  EXPECT_EQ(6, a.Perform(std::forward_as_tuple(x, y, &z)));
  EXPECT_EQ(42, y);
  EXPECT_EQ(43, z);

  Action<int(std::unique_ptr<int>)> a1 = &Deref;
  EXPECT_EQ(7, a1.Perform(std::make_tuple(UniqueInt(7))));
}

TEST(FunctorActionTest, ActionFromLambda) {
  Action<int(bool, int)> a1 = [](bool b, int i) { return b ? i : 0; };
  EXPECT_EQ(5, a1.Perform(std::make_tuple(true, 5)));
  EXPECT_EQ(0, a1.Perform(std::make_tuple(false, 5)));

  std::unique_ptr<int> saved;
  Action<void(std::unique_ptr<int>)> a2 = [&saved](std::unique_ptr<int> p) {
    saved = std::move(p);
  };
  a2.Perform(std::make_tuple(UniqueInt(5)));
  EXPECT_EQ(5, *saved);
}

TEST(FunctorActionTest, PolymorphicFunctor) {
  Action<int(int)> ai = Double();
  EXPECT_EQ(2, ai.Perform(std::make_tuple(1)));
  Action<double(double)> ad = Double();
  EXPECT_EQ(3.0, ad.Perform(std::make_tuple(1.5)));
}

TEST(FunctorActionTest, TypeConversion) {

  const Action<bool(int)> a1 = [](int i) { return i > 1; };
  const Action<int(bool)> a2 = Action<int(bool)>(a1);
  EXPECT_EQ(1, a1.Perform(std::make_tuple(42)));
  EXPECT_EQ(0, a2.Perform(std::make_tuple(42)));

  const Action<bool(std::string)> s1 = [](std::string s) { return !s.empty(); };
  const Action<int(const char*)> s2 = Action<int(const char*)>(s1);
  EXPECT_EQ(0, s2.Perform(std::make_tuple("")));
  EXPECT_EQ(1, s2.Perform(std::make_tuple("hello")));

  const Action<bool(std::string)> x1 = [](Unused) { return 42; };
  const Action<bool(std::string)> x2 = [] { return 42; };
  EXPECT_TRUE(x1.Perform(std::make_tuple("hello")));
  EXPECT_TRUE(x2.Perform(std::make_tuple("hello")));

  std::function<int()> f = [] { return 7; };
  Action<int(int)> d = f;
  f = nullptr;
  EXPECT_EQ(7, d.Perform(std::make_tuple(1)));

  Action<void(int)>(nullptr);
}

TEST(FunctorActionTest, UnusedArguments) {

  Action<int(int, double y, double z)> a = [](int i, Unused, Unused) {
    return 2 * i;
  };
  std::tuple<int, double, double> dummy = std::make_tuple(3, 7.3, 9.44);
  EXPECT_EQ(6, a.Perform(dummy));
}

TEST(MoveOnlyArgumentsTest, ReturningActions) {
  Action<int(std::unique_ptr<int>)> a = Return(1);
  EXPECT_EQ(1, a.Perform(std::make_tuple(nullptr)));

  a = testing::WithoutArgs([]() { return 7; });
  EXPECT_EQ(7, a.Perform(std::make_tuple(nullptr)));

  Action<void(std::unique_ptr<int>, int*)> a2 = testing::SetArgPointee<1>(3);
  int x = 0;
  a2.Perform(std::make_tuple(nullptr, &x));
  EXPECT_EQ(x, 3);
}

ACTION(ReturnArity) { return std::tuple_size<args_type>::value; }

TEST(ActionMacro, LargeArity) {
  EXPECT_EQ(
      1, testing::Action<int(int)>(ReturnArity()).Perform(std::make_tuple(0)));
  EXPECT_EQ(
      10,
      testing::Action<int(int, int, int, int, int, int, int, int, int, int)>(
          ReturnArity())
          .Perform(std::make_tuple(0, 1, 2, 3, 4, 5, 6, 7, 8, 9)));
  EXPECT_EQ(
      20,
      testing::Action<int(int, int, int, int, int, int, int, int, int, int, int,
                          int, int, int, int, int, int, int, int, int)>(
          ReturnArity())
          .Perform(std::make_tuple(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                                   14, 15, 16, 17, 18, 19)));
}

}
}

#if defined(_MSC_VER) && (_MSC_VER == 1900)
GTEST_DISABLE_MSC_WARNINGS_POP_()
#endif
GTEST_DISABLE_MSC_WARNINGS_POP_()
