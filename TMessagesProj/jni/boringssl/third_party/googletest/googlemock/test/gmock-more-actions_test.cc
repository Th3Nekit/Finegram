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
// This file tests the built-in actions in gmock-actions.h.

#include "gmock/gmock-more-actions.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4577)

namespace testing {
namespace gmock_more_actions_test {

using ::std::plus;
using ::std::string;
using testing::Action;
using testing::DeleteArg;
using testing::Invoke;
using testing::ReturnArg;
using testing::ReturnPointee;
using testing::SaveArg;
using testing::SaveArgByMove;
using testing::SaveArgPointee;
using testing::SetArgReferee;
using testing::Unused;
using testing::WithArg;
using testing::WithoutArgs;

inline short Short(short n) { return n; }
inline char Char(char ch) { return ch; }

int Nullary() { return 1; }

bool g_done = false;

bool Unary(int x) { return x < 0; }

bool ByConstRef(const std::string& s) { return s == "Hi"; }

const double g_double = 0;
bool ReferencesGlobalDouble(const double& x) { return &x == &g_double; }

struct UnaryFunctor {
  int operator()(bool x) { return x ? 1 : -1; }
};

struct UnaryMoveOnlyFunctor : UnaryFunctor {
  UnaryMoveOnlyFunctor() = default;
  UnaryMoveOnlyFunctor(const UnaryMoveOnlyFunctor&) = delete;
  UnaryMoveOnlyFunctor(UnaryMoveOnlyFunctor&&) = default;
};

struct OneShotUnaryFunctor {
  int operator()(bool x) && { return x ? 1 : -1; }
};

const char* Binary(const char* input, short n) { return input + n; }

int Ternary(int x, char y, short z) { return x + y + z; }

int SumOf4(int a, int b, int c, int d) { return a + b + c + d; }

int SumOfFirst2(int a, int b, Unused, Unused) { return a + b; }

int SumOf5(int a, int b, int c, int d, int e) { return a + b + c + d + e; }

struct SumOf5Functor {
  int operator()(int a, int b, int c, int d, int e) {
    return a + b + c + d + e;
  }
};

int SumOf6(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

struct SumOf6Functor {
  int operator()(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
  }
};

std::string Concat7(const char* s1, const char* s2, const char* s3,
                    const char* s4, const char* s5, const char* s6,
                    const char* s7) {
  return std::string(s1) + s2 + s3 + s4 + s5 + s6 + s7;
}

std::string Concat8(const char* s1, const char* s2, const char* s3,
                    const char* s4, const char* s5, const char* s6,
                    const char* s7, const char* s8) {
  return std::string(s1) + s2 + s3 + s4 + s5 + s6 + s7 + s8;
}

std::string Concat9(const char* s1, const char* s2, const char* s3,
                    const char* s4, const char* s5, const char* s6,
                    const char* s7, const char* s8, const char* s9) {
  return std::string(s1) + s2 + s3 + s4 + s5 + s6 + s7 + s8 + s9;
}

std::string Concat10(const char* s1, const char* s2, const char* s3,
                     const char* s4, const char* s5, const char* s6,
                     const char* s7, const char* s8, const char* s9,
                     const char* s10) {
  return std::string(s1) + s2 + s3 + s4 + s5 + s6 + s7 + s8 + s9 + s10;
}

class Foo {
 public:
  Foo() : value_(123) {}

  int Nullary() const { return value_; }

  short Unary(long x) { return static_cast<short>(value_ + x); }

  std::string Binary(const std::string& str, char c) const { return str + c; }

  int Ternary(int x, bool y, char z) { return value_ + x + y * z; }

  int SumOf4(int a, int b, int c, int d) const {
    return a + b + c + d + value_;
  }

  int SumOfLast2(Unused, Unused, int a, int b) const { return a + b; }

  int SumOf5(int a, int b, int c, int d, int e) { return a + b + c + d + e; }

  int SumOf6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
  }

  std::string Concat7(const char* s1, const char* s2, const char* s3,
                      const char* s4, const char* s5, const char* s6,
                      const char* s7) {
    return std::string(s1) + s2 + s3 + s4 + s5 + s6 + s7;
  }

  std::string Concat8(const char* s1, const char* s2, const char* s3,
                      const char* s4, const char* s5, const char* s6,
                      const char* s7, const char* s8) {
    return std::string(s1) + s2 + s3 + s4 + s5 + s6 + s7 + s8;
  }

  std::string Concat9(const char* s1, const char* s2, const char* s3,
                      const char* s4, const char* s5, const char* s6,
                      const char* s7, const char* s8, const char* s9) {
    return std::string(s1) + s2 + s3 + s4 + s5 + s6 + s7 + s8 + s9;
  }

  std::string Concat10(const char* s1, const char* s2, const char* s3,
                       const char* s4, const char* s5, const char* s6,
                       const char* s7, const char* s8, const char* s9,
                       const char* s10) {
    return std::string(s1) + s2 + s3 + s4 + s5 + s6 + s7 + s8 + s9 + s10;
  }

 private:
  int value_;
};

TEST(InvokeTest, Nullary) {
  Action<int()> a = Invoke(Nullary);
  EXPECT_EQ(1, a.Perform(std::make_tuple()));
}

TEST(InvokeTest, Unary) {
  Action<bool(int)> a = Invoke(Unary);
  EXPECT_FALSE(a.Perform(std::make_tuple(1)));
  EXPECT_TRUE(a.Perform(std::make_tuple(-1)));
}

TEST(InvokeTest, Binary) {
  Action<const char*(const char*, short)> a = Invoke(Binary);
  const char* p = "Hello";
  EXPECT_EQ(p + 2, a.Perform(std::make_tuple(p, Short(2))));
}

TEST(InvokeTest, Ternary) {
  Action<int(int, char, short)> a = Invoke(Ternary);
  EXPECT_EQ(6, a.Perform(std::make_tuple(1, '\2', Short(3))));
}

TEST(InvokeTest, FunctionThatTakes4Arguments) {
  Action<int(int, int, int, int)> a = Invoke(SumOf4);
  EXPECT_EQ(1234, a.Perform(std::make_tuple(1000, 200, 30, 4)));
}

TEST(InvokeTest, FunctionThatTakes5Arguments) {
  Action<int(int, int, int, int, int)> a = Invoke(SumOf5);
  EXPECT_EQ(12345, a.Perform(std::make_tuple(10000, 2000, 300, 40, 5)));
}

TEST(InvokeTest, FunctionThatTakes6Arguments) {
  Action<int(int, int, int, int, int, int)> a = Invoke(SumOf6);
  EXPECT_EQ(123456,
            a.Perform(std::make_tuple(100000, 20000, 3000, 400, 50, 6)));
}

inline const char* CharPtr(const char* s) { return s; }

TEST(InvokeTest, FunctionThatTakes7Arguments) {
  Action<std::string(const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*)>
      a = Invoke(Concat7);
  EXPECT_EQ("1234567",
            a.Perform(std::make_tuple(CharPtr("1"), CharPtr("2"), CharPtr("3"),
                                      CharPtr("4"), CharPtr("5"), CharPtr("6"),
                                      CharPtr("7"))));
}

TEST(InvokeTest, FunctionThatTakes8Arguments) {
  Action<std::string(const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*)>
      a = Invoke(Concat8);
  EXPECT_EQ("12345678",
            a.Perform(std::make_tuple(CharPtr("1"), CharPtr("2"), CharPtr("3"),
                                      CharPtr("4"), CharPtr("5"), CharPtr("6"),
                                      CharPtr("7"), CharPtr("8"))));
}

TEST(InvokeTest, FunctionThatTakes9Arguments) {
  Action<std::string(const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*,
                     const char*)>
      a = Invoke(Concat9);
  EXPECT_EQ("123456789", a.Perform(std::make_tuple(
                             CharPtr("1"), CharPtr("2"), CharPtr("3"),
                             CharPtr("4"), CharPtr("5"), CharPtr("6"),
                             CharPtr("7"), CharPtr("8"), CharPtr("9"))));
}

TEST(InvokeTest, FunctionThatTakes10Arguments) {
  Action<std::string(const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*,
                     const char*, const char*)>
      a = Invoke(Concat10);
  EXPECT_EQ("1234567890",
            a.Perform(std::make_tuple(CharPtr("1"), CharPtr("2"), CharPtr("3"),
                                      CharPtr("4"), CharPtr("5"), CharPtr("6"),
                                      CharPtr("7"), CharPtr("8"), CharPtr("9"),
                                      CharPtr("0"))));
}

TEST(InvokeTest, FunctionWithUnusedParameters) {
  Action<int(int, int, double, const std::string&)> a1 = Invoke(SumOfFirst2);
  std::tuple<int, int, double, std::string> dummy =
      std::make_tuple(10, 2, 5.6, std::string("hi"));
  EXPECT_EQ(12, a1.Perform(dummy));

  Action<int(int, int, bool, int*)> a2 = Invoke(SumOfFirst2);
  EXPECT_EQ(
      23, a2.Perform(std::make_tuple(20, 3, true, static_cast<int*>(nullptr))));
}

TEST(InvokeTest, MethodWithUnusedParameters) {
  Foo foo;
  Action<int(std::string, bool, int, int)> a1 = Invoke(&foo, &Foo::SumOfLast2);
  EXPECT_EQ(12, a1.Perform(std::make_tuple(CharPtr("hi"), true, 10, 2)));

  Action<int(char, double, int, int)> a2 = Invoke(&foo, &Foo::SumOfLast2);
  EXPECT_EQ(23, a2.Perform(std::make_tuple('a', 2.5, 20, 3)));
}

TEST(InvokeTest, Functor) {
  Action<long(long, int)> a = Invoke(plus<long>());
  EXPECT_EQ(3L, a.Perform(std::make_tuple(1, 2)));
}

TEST(InvokeTest, FunctionWithCompatibleType) {
  Action<long(int, short, char, bool)> a = Invoke(SumOf4);
  EXPECT_EQ(4321, a.Perform(std::make_tuple(4000, Short(300), Char(20), true)));
}

TEST(InvokeMethodTest, Nullary) {
  Foo foo;
  Action<int()> a = Invoke(&foo, &Foo::Nullary);
  EXPECT_EQ(123, a.Perform(std::make_tuple()));
}

TEST(InvokeMethodTest, Unary) {
  Foo foo;
  Action<short(long)> a = Invoke(&foo, &Foo::Unary);
  EXPECT_EQ(4123, a.Perform(std::make_tuple(4000)));
}

TEST(InvokeMethodTest, Binary) {
  Foo foo;
  Action<std::string(const std::string&, char)> a = Invoke(&foo, &Foo::Binary);
  std::string s("Hell");
  std::tuple<std::string, char> dummy = std::make_tuple(s, 'o');
  EXPECT_EQ("Hello", a.Perform(dummy));
}

TEST(InvokeMethodTest, Ternary) {
  Foo foo;
  Action<int(int, bool, char)> a = Invoke(&foo, &Foo::Ternary);
  EXPECT_EQ(1124, a.Perform(std::make_tuple(1000, true, Char(1))));
}

TEST(InvokeMethodTest, MethodThatTakes4Arguments) {
  Foo foo;
  Action<int(int, int, int, int)> a = Invoke(&foo, &Foo::SumOf4);
  EXPECT_EQ(1357, a.Perform(std::make_tuple(1000, 200, 30, 4)));
}

TEST(InvokeMethodTest, MethodThatTakes5Arguments) {
  Foo foo;
  Action<int(int, int, int, int, int)> a =
      Invoke(&foo, &Foo::SumOf5);
  EXPECT_EQ(12345, a.Perform(std::make_tuple(10000, 2000, 300, 40, 5)));
}

TEST(InvokeMethodTest, MethodThatTakes6Arguments) {
  Foo foo;
  Action<int(int, int, int, int, int, int)> a =
      Invoke(&foo, &Foo::SumOf6);
  EXPECT_EQ(123456,
            a.Perform(std::make_tuple(100000, 20000, 3000, 400, 50, 6)));
}

TEST(InvokeMethodTest, MethodThatTakes7Arguments) {
  Foo foo;
  Action<std::string(const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*)>
      a = Invoke(&foo, &Foo::Concat7);
  EXPECT_EQ("1234567",
            a.Perform(std::make_tuple(CharPtr("1"), CharPtr("2"), CharPtr("3"),
                                      CharPtr("4"), CharPtr("5"), CharPtr("6"),
                                      CharPtr("7"))));
}

TEST(InvokeMethodTest, MethodThatTakes8Arguments) {
  Foo foo;
  Action<std::string(const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*)>
      a = Invoke(&foo, &Foo::Concat8);
  EXPECT_EQ("12345678",
            a.Perform(std::make_tuple(CharPtr("1"), CharPtr("2"), CharPtr("3"),
                                      CharPtr("4"), CharPtr("5"), CharPtr("6"),
                                      CharPtr("7"), CharPtr("8"))));
}

TEST(InvokeMethodTest, MethodThatTakes9Arguments) {
  Foo foo;
  Action<std::string(const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*,
                     const char*)>
      a = Invoke(&foo, &Foo::Concat9);
  EXPECT_EQ("123456789", a.Perform(std::make_tuple(
                             CharPtr("1"), CharPtr("2"), CharPtr("3"),
                             CharPtr("4"), CharPtr("5"), CharPtr("6"),
                             CharPtr("7"), CharPtr("8"), CharPtr("9"))));
}

TEST(InvokeMethodTest, MethodThatTakes10Arguments) {
  Foo foo;
  Action<std::string(const char*, const char*, const char*, const char*,
                     const char*, const char*, const char*, const char*,
                     const char*, const char*)>
      a = Invoke(&foo, &Foo::Concat10);
  EXPECT_EQ("1234567890",
            a.Perform(std::make_tuple(CharPtr("1"), CharPtr("2"), CharPtr("3"),
                                      CharPtr("4"), CharPtr("5"), CharPtr("6"),
                                      CharPtr("7"), CharPtr("8"), CharPtr("9"),
                                      CharPtr("0"))));
}

TEST(InvokeMethodTest, MethodWithCompatibleType) {
  Foo foo;
  Action<long(int, short, char, bool)> a =
      Invoke(&foo, &Foo::SumOf4);
  EXPECT_EQ(4444, a.Perform(std::make_tuple(4000, Short(300), Char(20), true)));
}

TEST(WithoutArgsTest, NoArg) {
  Action<int(int n)> a = WithoutArgs(Invoke(Nullary));
  EXPECT_EQ(1, a.Perform(std::make_tuple(2)));
}

TEST(WithArgTest, OneArg) {
  Action<bool(double x, int n)> b = WithArg<1>(Invoke(Unary));
  EXPECT_TRUE(b.Perform(std::make_tuple(1.5, -1)));
  EXPECT_FALSE(b.Perform(std::make_tuple(1.5, 1)));
}

TEST(ReturnArgActionTest, WorksForOneArgIntArg0) {
  const Action<int(int)> a = ReturnArg<0>();
  EXPECT_EQ(5, a.Perform(std::make_tuple(5)));
}

TEST(ReturnArgActionTest, WorksForMultiArgBoolArg0) {
  const Action<bool(bool, bool, bool)> a = ReturnArg<0>();
  EXPECT_TRUE(a.Perform(std::make_tuple(true, false, false)));
}

TEST(ReturnArgActionTest, WorksForMultiArgStringArg2) {
  const Action<std::string(int, int, std::string, int)> a = ReturnArg<2>();
  EXPECT_EQ("seven", a.Perform(std::make_tuple(5, 6, std::string("seven"), 8)));
}

TEST(ReturnArgActionTest, WorksForNonConstRefArg0) {
  const Action<std::string&(std::string&)> a = ReturnArg<0>();
  std::string s = "12345";
  EXPECT_EQ(&s, &a.Perform(std::forward_as_tuple(s)));
}

TEST(SaveArgActionTest, WorksForSameType) {
  int result = 0;
  const Action<void(int n)> a1 = SaveArg<0>(&result);
  a1.Perform(std::make_tuple(5));
  EXPECT_EQ(5, result);
}

TEST(SaveArgActionTest, WorksForCompatibleType) {
  int result = 0;
  const Action<void(bool, char)> a1 = SaveArg<1>(&result);
  a1.Perform(std::make_tuple(true, 'a'));
  EXPECT_EQ('a', result);
}

struct MoveOnly {
  explicit MoveOnly(int v) : i(v) {}
  MoveOnly(MoveOnly&& o) {
    i = o.i;
    o.i = -1;
  }
  MoveOnly& operator=(MoveOnly&& o) {
    i = o.i;
    o.i = -1;
    return *this;
  }
  int i;
};

TEST(SaveArgByMoveActionTest, WorksForSameType) {
  MoveOnly result{0};
  const Action<void(MoveOnly v)> a1 = SaveArgByMove<0>(&result);
  a1.Perform(std::make_tuple(MoveOnly{5}));
  EXPECT_EQ(5, result.i);
}

TEST(SaveArgByMoveActionTest, WorksForCompatibleType) {
  MoveOnly result{0};
  const Action<void(bool, MoveOnly)> a1 = SaveArgByMove<1>(&result);
  a1.Perform(std::make_tuple(true, MoveOnly{7}));
  EXPECT_EQ(7, result.i);
}

TEST(SaveArgPointeeActionTest, WorksForSameType) {
  int result = 0;
  const int value = 5;
  const Action<void(const int*)> a1 = SaveArgPointee<0>(&result);
  a1.Perform(std::make_tuple(&value));
  EXPECT_EQ(5, result);
}

TEST(SaveArgPointeeActionTest, WorksForCompatibleType) {
  int result = 0;
  char value = 'a';
  const Action<void(bool, char*)> a1 = SaveArgPointee<1>(&result);
  a1.Perform(std::make_tuple(true, &value));
  EXPECT_EQ('a', result);
}

TEST(SetArgRefereeActionTest, WorksForSameType) {
  int value = 0;
  const Action<void(int&)> a1 = SetArgReferee<0>(1);
  a1.Perform(std::tuple<int&>(value));
  EXPECT_EQ(1, value);
}

TEST(SetArgRefereeActionTest, WorksForCompatibleType) {
  int value = 0;
  const Action<void(int, int&)> a1 = SetArgReferee<1>('a');
  a1.Perform(std::tuple<int, int&>(0, value));
  EXPECT_EQ('a', value);
}

TEST(SetArgRefereeActionTest, WorksWithExtraArguments) {
  int value = 0;
  const Action<void(bool, int, int&, const char*)> a1 = SetArgReferee<2>('a');
  a1.Perform(std::tuple<bool, int, int&, const char*>(true, 0, value, "hi"));
  EXPECT_EQ('a', value);
}

class DeletionTester {
 public:
  explicit DeletionTester(bool* is_deleted) : is_deleted_(is_deleted) {

    *is_deleted_ = false;
  }

  ~DeletionTester() { *is_deleted_ = true; }

 private:
  bool* is_deleted_;
};

TEST(DeleteArgActionTest, OneArg) {
  bool is_deleted = false;
  DeletionTester* t = new DeletionTester(&is_deleted);
  const Action<void(DeletionTester*)> a1 = DeleteArg<0>();
  EXPECT_FALSE(is_deleted);
  a1.Perform(std::make_tuple(t));
  EXPECT_TRUE(is_deleted);
}

TEST(DeleteArgActionTest, TenArgs) {
  bool is_deleted = false;
  DeletionTester* t = new DeletionTester(&is_deleted);
  const Action<void(bool, int, int, const char*, bool, int, int, int, int,
                    DeletionTester*)>
      a1 = DeleteArg<9>();
  EXPECT_FALSE(is_deleted);
  a1.Perform(std::make_tuple(true, 5, 6, CharPtr("hi"), false, 7, 8, 9, 10, t));
  EXPECT_TRUE(is_deleted);
}

#if GTEST_HAS_EXCEPTIONS

TEST(ThrowActionTest, ThrowsGivenExceptionInVoidFunction) {
  const Action<void(int n)> a = Throw('a');
  EXPECT_THROW(a.Perform(std::make_tuple(0)), char);
}

class MyException {};

TEST(ThrowActionTest, ThrowsGivenExceptionInNonVoidFunction) {
  const Action<double(char ch)> a = Throw(MyException());
  EXPECT_THROW(a.Perform(std::make_tuple('0')), MyException);
}

TEST(ThrowActionTest, ThrowsGivenExceptionInNullaryFunction) {
  const Action<double()> a = Throw(MyException());
  EXPECT_THROW(a.Perform(std::make_tuple()), MyException);
}

class Object {
 public:
  virtual ~Object() {}
  virtual void Func() {}
};

class MockObject : public Object {
 public:
  ~MockObject() override {}
  MOCK_METHOD(void, Func, (), (override));
};

TEST(ThrowActionTest, Times0) {
  EXPECT_NONFATAL_FAILURE(
      [] {
        try {
          MockObject m;
          ON_CALL(m, Func()).WillByDefault([] { throw "something"; });
          EXPECT_CALL(m, Func()).Times(0);
          m.Func();
        } catch (...) {

        }
      }(),
      "");
}

#endif

TEST(SetArrayArgumentTest, SetsTheNthArray) {
  using MyFunction = void(bool, int*, char*);
  int numbers[] = {1, 2, 3};
  Action<MyFunction> a = SetArrayArgument<1>(numbers, numbers + 3);

  int n[4] = {};
  int* pn = n;
  char ch[4] = {};
  char* pch = ch;
  a.Perform(std::make_tuple(true, pn, pch));
  EXPECT_EQ(1, n[0]);
  EXPECT_EQ(2, n[1]);
  EXPECT_EQ(3, n[2]);
  EXPECT_EQ(0, n[3]);
  EXPECT_EQ('\0', ch[0]);
  EXPECT_EQ('\0', ch[1]);
  EXPECT_EQ('\0', ch[2]);
  EXPECT_EQ('\0', ch[3]);

  std::string letters = "abc";
  a = SetArrayArgument<2>(letters.begin(), letters.end());
  std::fill_n(n, 4, 0);
  std::fill_n(ch, 4, '\0');
  a.Perform(std::make_tuple(true, pn, pch));
  EXPECT_EQ(0, n[0]);
  EXPECT_EQ(0, n[1]);
  EXPECT_EQ(0, n[2]);
  EXPECT_EQ(0, n[3]);
  EXPECT_EQ('a', ch[0]);
  EXPECT_EQ('b', ch[1]);
  EXPECT_EQ('c', ch[2]);
  EXPECT_EQ('\0', ch[3]);
}

TEST(SetArrayArgumentTest, SetsTheNthArrayWithEmptyRange) {
  using MyFunction = void(bool, int*);
  int numbers[] = {1, 2, 3};
  Action<MyFunction> a = SetArrayArgument<1>(numbers, numbers);

  int n[4] = {};
  int* pn = n;
  a.Perform(std::make_tuple(true, pn));
  EXPECT_EQ(0, n[0]);
  EXPECT_EQ(0, n[1]);
  EXPECT_EQ(0, n[2]);
  EXPECT_EQ(0, n[3]);
}

TEST(SetArrayArgumentTest, SetsTheNthArrayWithConvertibleType) {
  using MyFunction = void(bool, int*);
  char chars[] = {97, 98, 99};
  Action<MyFunction> a = SetArrayArgument<1>(chars, chars + 3);

  int codes[4] = {111, 222, 333, 444};
  int* pcodes = codes;
  a.Perform(std::make_tuple(true, pcodes));
  EXPECT_EQ(97, codes[0]);
  EXPECT_EQ(98, codes[1]);
  EXPECT_EQ(99, codes[2]);
  EXPECT_EQ(444, codes[3]);
}

TEST(SetArrayArgumentTest, SetsTheNthArrayWithIteratorArgument) {
  using MyFunction = void(bool, std::back_insert_iterator<std::string>);
  std::string letters = "abc";
  Action<MyFunction> a = SetArrayArgument<1>(letters.begin(), letters.end());

  std::string s;
  a.Perform(std::make_tuple(true, std::back_inserter(s)));
  EXPECT_EQ(letters, s);
}

TEST(ReturnPointeeTest, Works) {
  int n = 42;
  const Action<int()> a = ReturnPointee(&n);
  EXPECT_EQ(42, a.Perform(std::make_tuple()));

  n = 43;
  EXPECT_EQ(43, a.Perform(std::make_tuple()));
}

TEST(InvokeArgumentTest, Function0) {
  Action<int(int, int (*)())> a = InvokeArgument<1>();
  EXPECT_EQ(1, a.Perform(std::make_tuple(2, &Nullary)));
}

TEST(InvokeArgumentTest, Functor1) {
  Action<int(UnaryFunctor)> a = InvokeArgument<0>(true);
  EXPECT_EQ(1, a.Perform(std::make_tuple(UnaryFunctor())));
}

TEST(InvokeArgumentTest, Functor1MoveOnly) {
  Action<int(UnaryMoveOnlyFunctor)> a = InvokeArgument<0>(true);
  EXPECT_EQ(1, a.Perform(std::make_tuple(UnaryMoveOnlyFunctor())));
}

TEST(InvokeArgumentTest, OneShotFunctor1) {
  Action<int(OneShotUnaryFunctor)> a = InvokeArgument<0>(true);
  EXPECT_EQ(1, a.Perform(std::make_tuple(OneShotUnaryFunctor())));
}

TEST(InvokeArgumentTest, Function5) {
  Action<int(int (*)(int, int, int, int, int))> a =
      InvokeArgument<0>(10000, 2000, 300, 40, 5);
  EXPECT_EQ(12345, a.Perform(std::make_tuple(&SumOf5)));
}

TEST(InvokeArgumentTest, Functor5) {
  Action<int(SumOf5Functor)> a =
      InvokeArgument<0>(10000, 2000, 300, 40, 5);
  EXPECT_EQ(12345, a.Perform(std::make_tuple(SumOf5Functor())));
}

TEST(InvokeArgumentTest, Function6) {
  Action<int(int (*)(int, int, int, int, int, int))> a =
      InvokeArgument<0>(100000, 20000, 3000, 400, 50, 6);
  EXPECT_EQ(123456, a.Perform(std::make_tuple(&SumOf6)));
}

TEST(InvokeArgumentTest, Functor6) {
  Action<int(SumOf6Functor)> a =
      InvokeArgument<0>(100000, 20000, 3000, 400, 50, 6);
  EXPECT_EQ(123456, a.Perform(std::make_tuple(SumOf6Functor())));
}

TEST(InvokeArgumentTest, Function7) {
  Action<std::string(std::string (*)(const char*, const char*, const char*,
                                     const char*, const char*, const char*,
                                     const char*))>
      a = InvokeArgument<0>("1", "2", "3", "4", "5", "6", "7");
  EXPECT_EQ("1234567", a.Perform(std::make_tuple(&Concat7)));
}

TEST(InvokeArgumentTest, Function8) {
  Action<std::string(std::string (*)(const char*, const char*, const char*,
                                     const char*, const char*, const char*,
                                     const char*, const char*))>
      a = InvokeArgument<0>("1", "2", "3", "4", "5", "6", "7", "8");
  EXPECT_EQ("12345678", a.Perform(std::make_tuple(&Concat8)));
}

TEST(InvokeArgumentTest, Function9) {
  Action<std::string(std::string (*)(const char*, const char*, const char*,
                                     const char*, const char*, const char*,
                                     const char*, const char*, const char*))>
      a = InvokeArgument<0>("1", "2", "3", "4", "5", "6", "7", "8", "9");
  EXPECT_EQ("123456789", a.Perform(std::make_tuple(&Concat9)));
}

TEST(InvokeArgumentTest, Function10) {
  Action<std::string(std::string (*)(
      const char*, const char*, const char*, const char*, const char*,
      const char*, const char*, const char*, const char*, const char*))>
      a = InvokeArgument<0>("1", "2", "3", "4", "5", "6", "7", "8", "9", "0");
  EXPECT_EQ("1234567890", a.Perform(std::make_tuple(&Concat10)));
}

TEST(InvokeArgumentTest, ByPointerFunction) {
  Action<const char*(const char* (*)(const char* input, short n))>
      a = InvokeArgument<0>(static_cast<const char*>("Hi"), Short(1));
  EXPECT_STREQ("i", a.Perform(std::make_tuple(&Binary)));
}

TEST(InvokeArgumentTest, FunctionWithCStringLiteral) {
  Action<const char*(const char* (*)(const char* input, short n))>
      a = InvokeArgument<0>("Hi", Short(1));
  EXPECT_STREQ("i", a.Perform(std::make_tuple(&Binary)));
}

TEST(InvokeArgumentTest, ByConstReferenceFunction) {
  Action<bool(bool (*function)(const std::string& s))> a =
      InvokeArgument<0>(std::string("Hi"));

  EXPECT_TRUE(a.Perform(std::make_tuple(&ByConstRef)));
}

TEST(InvokeArgumentTest, ByExplicitConstReferenceFunction) {
  Action<bool(bool (*)(const double& x))> a =
      InvokeArgument<0>(ByRef(g_double));

  EXPECT_TRUE(a.Perform(std::make_tuple(&ReferencesGlobalDouble)));

  double x = 0;
  a = InvokeArgument<0>(ByRef(x));
  EXPECT_FALSE(a.Perform(std::make_tuple(&ReferencesGlobalDouble)));
}

TEST(InvokeArgumentTest, MoveOnlyType) {
  struct Marker {};
  struct {

    MOCK_METHOD(bool, MockMethod,
                (std::unique_ptr<Marker>, std::function<int()>), ());
  } mock;

  ON_CALL(mock, MockMethod(_, _)).WillByDefault(InvokeArgument<1>());

  ON_CALL(mock, MockMethod(_, _))
      .WillByDefault(WithArg<1>(InvokeArgument<0>()));
}

TEST(DoAllTest, TwoActions) {
  int n = 0;
  Action<int(int*)> a = DoAll(SetArgPointee<0>(1),
                              Return(2));
  EXPECT_EQ(2, a.Perform(std::make_tuple(&n)));
  EXPECT_EQ(1, n);
}

TEST(DoAllTest, ThreeActions) {
  int m = 0, n = 0;
  Action<int(int*, int*)> a = DoAll(SetArgPointee<0>(1),
                                    SetArgPointee<1>(2), Return(3));
  EXPECT_EQ(3, a.Perform(std::make_tuple(&m, &n)));
  EXPECT_EQ(1, m);
  EXPECT_EQ(2, n);
}

TEST(DoAllTest, FourActions) {
  int m = 0, n = 0;
  char ch = '\0';
  Action<int(int*, int*, char*)> a =
      DoAll(SetArgPointee<0>(1), SetArgPointee<1>(2), SetArgPointee<2>('a'),
            Return(3));
  EXPECT_EQ(3, a.Perform(std::make_tuple(&m, &n, &ch)));
  EXPECT_EQ(1, m);
  EXPECT_EQ(2, n);
  EXPECT_EQ('a', ch);
}

TEST(DoAllTest, FiveActions) {
  int m = 0, n = 0;
  char a = '\0', b = '\0';
  Action<int(int*, int*, char*, char*)> action =
      DoAll(SetArgPointee<0>(1), SetArgPointee<1>(2), SetArgPointee<2>('a'),
            SetArgPointee<3>('b'), Return(3));
  EXPECT_EQ(3, action.Perform(std::make_tuple(&m, &n, &a, &b)));
  EXPECT_EQ(1, m);
  EXPECT_EQ(2, n);
  EXPECT_EQ('a', a);
  EXPECT_EQ('b', b);
}

TEST(DoAllTest, SixActions) {
  int m = 0, n = 0;
  char a = '\0', b = '\0', c = '\0';
  Action<int(int*, int*, char*, char*, char*)> action =
      DoAll(SetArgPointee<0>(1), SetArgPointee<1>(2), SetArgPointee<2>('a'),
            SetArgPointee<3>('b'), SetArgPointee<4>('c'), Return(3));
  EXPECT_EQ(3, action.Perform(std::make_tuple(&m, &n, &a, &b, &c)));
  EXPECT_EQ(1, m);
  EXPECT_EQ(2, n);
  EXPECT_EQ('a', a);
  EXPECT_EQ('b', b);
  EXPECT_EQ('c', c);
}

TEST(DoAllTest, SevenActions) {
  int m = 0, n = 0;
  char a = '\0', b = '\0', c = '\0', d = '\0';
  Action<int(int*, int*, char*, char*, char*, char*)> action =
      DoAll(SetArgPointee<0>(1), SetArgPointee<1>(2), SetArgPointee<2>('a'),
            SetArgPointee<3>('b'), SetArgPointee<4>('c'), SetArgPointee<5>('d'),
            Return(3));
  EXPECT_EQ(3, action.Perform(std::make_tuple(&m, &n, &a, &b, &c, &d)));
  EXPECT_EQ(1, m);
  EXPECT_EQ(2, n);
  EXPECT_EQ('a', a);
  EXPECT_EQ('b', b);
  EXPECT_EQ('c', c);
  EXPECT_EQ('d', d);
}

TEST(DoAllTest, EightActions) {
  int m = 0, n = 0;
  char a = '\0', b = '\0', c = '\0', d = '\0', e = '\0';
  Action<int(int*, int*, char*, char*, char*, char*,
             char*)>
      action =
          DoAll(SetArgPointee<0>(1), SetArgPointee<1>(2), SetArgPointee<2>('a'),
                SetArgPointee<3>('b'), SetArgPointee<4>('c'),
                SetArgPointee<5>('d'), SetArgPointee<6>('e'), Return(3));
  EXPECT_EQ(3, action.Perform(std::make_tuple(&m, &n, &a, &b, &c, &d, &e)));
  EXPECT_EQ(1, m);
  EXPECT_EQ(2, n);
  EXPECT_EQ('a', a);
  EXPECT_EQ('b', b);
  EXPECT_EQ('c', c);
  EXPECT_EQ('d', d);
  EXPECT_EQ('e', e);
}

TEST(DoAllTest, NineActions) {
  int m = 0, n = 0;
  char a = '\0', b = '\0', c = '\0', d = '\0', e = '\0', f = '\0';
  Action<int(int*, int*, char*, char*, char*, char*,
             char*, char*)>
      action = DoAll(SetArgPointee<0>(1), SetArgPointee<1>(2),
                     SetArgPointee<2>('a'), SetArgPointee<3>('b'),
                     SetArgPointee<4>('c'), SetArgPointee<5>('d'),
                     SetArgPointee<6>('e'), SetArgPointee<7>('f'), Return(3));
  EXPECT_EQ(3, action.Perform(std::make_tuple(&m, &n, &a, &b, &c, &d, &e, &f)));
  EXPECT_EQ(1, m);
  EXPECT_EQ(2, n);
  EXPECT_EQ('a', a);
  EXPECT_EQ('b', b);
  EXPECT_EQ('c', c);
  EXPECT_EQ('d', d);
  EXPECT_EQ('e', e);
  EXPECT_EQ('f', f);
}

TEST(DoAllTest, TenActions) {
  int m = 0, n = 0;
  char a = '\0', b = '\0', c = '\0', d = '\0';
  char e = '\0', f = '\0', g = '\0';
  Action<int(int*, int*, char*, char*, char*, char*,
             char*, char*, char*)>
      action =
          DoAll(SetArgPointee<0>(1), SetArgPointee<1>(2), SetArgPointee<2>('a'),
                SetArgPointee<3>('b'), SetArgPointee<4>('c'),
                SetArgPointee<5>('d'), SetArgPointee<6>('e'),
                SetArgPointee<7>('f'), SetArgPointee<8>('g'), Return(3));
  EXPECT_EQ(
      3, action.Perform(std::make_tuple(&m, &n, &a, &b, &c, &d, &e, &f, &g)));
  EXPECT_EQ(1, m);
  EXPECT_EQ(2, n);
  EXPECT_EQ('a', a);
  EXPECT_EQ('b', b);
  EXPECT_EQ('c', c);
  EXPECT_EQ('d', d);
  EXPECT_EQ('e', e);
  EXPECT_EQ('f', f);
  EXPECT_EQ('g', g);
}

TEST(DoAllTest, NoArgs) {
  bool ran_first = false;
  Action<bool()> a =
      DoAll([&] { ran_first = true; }, [&] { return ran_first; });
  EXPECT_TRUE(a.Perform({}));
}

TEST(DoAllTest, MoveOnlyArgs) {
  bool ran_first = false;
  Action<int(std::unique_ptr<int>)> a =
      DoAll(InvokeWithoutArgs([&] { ran_first = true; }),
            [](std::unique_ptr<int> p) { return *p; });
  EXPECT_EQ(7, a.Perform(std::make_tuple(std::unique_ptr<int>(new int(7)))));
  EXPECT_TRUE(ran_first);
}

TEST(DoAllTest, ImplicitlyConvertsActionArguments) {
  bool ran_first = false;

  Action<void(std::vector<int>)> first = [&] { ran_first = true; };
  Action<int(std::vector<int>)> a =
      DoAll(first, [](std::vector<int> arg) { return arg.front(); });
  EXPECT_EQ(7, a.Perform(std::make_tuple(std::vector<int>{7})));
  EXPECT_TRUE(ran_first);
}

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4100 4503)

ACTION(Return5) { return 5; }

TEST(ActionMacroTest, WorksWhenNotReferencingArguments) {
  Action<double()> a1 = Return5();
  EXPECT_DOUBLE_EQ(5, a1.Perform(std::make_tuple()));

  Action<int(double, bool)> a2 = Return5();
  EXPECT_EQ(5, a2.Perform(std::make_tuple(1, true)));
}

ACTION(IncrementArg1) { (*arg1)++; }

TEST(ActionMacroTest, WorksWhenReturningVoid) {
  Action<void(int, int*)> a1 = IncrementArg1();
  int n = 0;
  a1.Perform(std::make_tuple(5, &n));
  EXPECT_EQ(1, n);
}

ACTION(IncrementArg2) {
  StaticAssertTypeEq<int*, arg2_type>();
  arg2_type temp = arg2;
  (*temp)++;
}

TEST(ActionMacroTest, CanReferenceArgumentType) {
  Action<void(int, bool, int*)> a1 = IncrementArg2();
  int n = 0;
  a1.Perform(std::make_tuple(5, false, &n));
  EXPECT_EQ(1, n);
}

ACTION(Sum2) {
  StaticAssertTypeEq<std::tuple<int, char, int*>, args_type>();
  args_type args_copy = args;
  return std::get<0>(args_copy) + std::get<1>(args_copy);
}

TEST(ActionMacroTest, CanReferenceArgumentTuple) {
  Action<int(int, char, int*)> a1 = Sum2();
  int dummy = 0;
  EXPECT_EQ(11, a1.Perform(std::make_tuple(5, Char(6), &dummy)));
}

namespace {

int Dummy(bool flag) { return flag ? 1 : 0; }

}

ACTION(InvokeDummy) {
  StaticAssertTypeEq<int(bool), function_type>();
  function_type* fp = &Dummy;
  return (*fp)(true);
}

TEST(ActionMacroTest, CanReferenceMockFunctionType) {
  Action<int(bool)> a1 = InvokeDummy();
  EXPECT_EQ(1, a1.Perform(std::make_tuple(true)));
  EXPECT_EQ(1, a1.Perform(std::make_tuple(false)));
}

ACTION(InvokeDummy2) {
  StaticAssertTypeEq<int, return_type>();
  return_type result = Dummy(true);
  return result;
}

TEST(ActionMacroTest, CanReferenceMockFunctionReturnType) {
  Action<int(bool)> a1 = InvokeDummy2();
  EXPECT_EQ(1, a1.Perform(std::make_tuple(true)));
  EXPECT_EQ(1, a1.Perform(std::make_tuple(false)));
}

ACTION(ReturnAddrOfConstBoolReferenceArg) {
  StaticAssertTypeEq<const bool&, arg1_type>();
  return &arg1;
}

TEST(ActionMacroTest, WorksForConstReferenceArg) {
  Action<const bool*(int, const bool&)> a = ReturnAddrOfConstBoolReferenceArg();
  const bool b = false;
  EXPECT_EQ(&b, a.Perform(std::tuple<int, const bool&>(0, b)));
}

ACTION(ReturnAddrOfIntReferenceArg) {
  StaticAssertTypeEq<int&, arg0_type>();
  return &arg0;
}

TEST(ActionMacroTest, WorksForNonConstReferenceArg) {
  Action<int*(int&, bool, int)> a = ReturnAddrOfIntReferenceArg();
  int n = 0;
  EXPECT_EQ(&n, a.Perform(std::tuple<int&, bool, int>(n, true, 1)));
}

namespace action_test {
ACTION(Sum) { return arg0 + arg1; }
}

TEST(ActionMacroTest, WorksInNamespace) {
  Action<int(int, int)> a1 = action_test::Sum();
  EXPECT_EQ(3, a1.Perform(std::make_tuple(1, 2)));
}

ACTION(PlusTwo) { return arg0 + 2; }

TEST(ActionMacroTest, WorksForDifferentArgumentNumbers) {
  Action<int(int)> a1 = PlusTwo();
  EXPECT_EQ(4, a1.Perform(std::make_tuple(2)));

  Action<double(float, void*)> a2 = PlusTwo();
  int dummy;
  EXPECT_DOUBLE_EQ(6, a2.Perform(std::make_tuple(4.0f, &dummy)));
}

ACTION_P(Plus, n) { return arg0 + n; }

TEST(ActionPMacroTest, DefinesParameterizedAction) {
  Action<int(int m, bool t)> a1 = Plus(9);
  EXPECT_EQ(10, a1.Perform(std::make_tuple(1, true)));
}

ACTION_P(TypedPlus, n) {
  arg0_type t1 = arg0;
  n_type t2 = n;
  return t1 + t2;
}

TEST(ActionPMacroTest, CanReferenceArgumentAndParameterTypes) {
  Action<int(char m, bool t)> a1 = TypedPlus(9);
  EXPECT_EQ(10, a1.Perform(std::make_tuple(Char(1), true)));
}

TEST(ActionPMacroTest, WorksInCompatibleMockFunction) {
  Action<std::string(const std::string& s)> a1 = Plus("tail");
  const std::string re = "re";
  std::tuple<const std::string> dummy = std::make_tuple(re);
  EXPECT_EQ("retail", a1.Perform(dummy));
}

ACTION(OverloadedAction) { return arg0 ? arg1 : "hello"; }

ACTION_P(OverloadedAction, default_value) {
  return arg0 ? arg1 : default_value;
}

ACTION_P2(OverloadedAction, true_value, false_value) {
  return arg0 ? true_value : false_value;
}

TEST(ActionMacroTest, CanDefineOverloadedActions) {
  using MyAction = Action<const char*(bool, const char*)>;

  const MyAction a1 = OverloadedAction();
  EXPECT_STREQ("hello", a1.Perform(std::make_tuple(false, CharPtr("world"))));
  EXPECT_STREQ("world", a1.Perform(std::make_tuple(true, CharPtr("world"))));

  const MyAction a2 = OverloadedAction("hi");
  EXPECT_STREQ("hi", a2.Perform(std::make_tuple(false, CharPtr("world"))));
  EXPECT_STREQ("world", a2.Perform(std::make_tuple(true, CharPtr("world"))));

  const MyAction a3 = OverloadedAction("hi", "you");
  EXPECT_STREQ("hi", a3.Perform(std::make_tuple(true, CharPtr("world"))));
  EXPECT_STREQ("you", a3.Perform(std::make_tuple(false, CharPtr("world"))));
}

ACTION_P3(Plus, m, n, k) { return arg0 + m + n + k; }

TEST(ActionPnMacroTest, WorksFor3Parameters) {
  Action<double(int m, bool t)> a1 = Plus(100, 20, 3.4);
  EXPECT_DOUBLE_EQ(3123.4, a1.Perform(std::make_tuple(3000, true)));

  Action<std::string(const std::string& s)> a2 = Plus("tail", "-", ">");
  const std::string re = "re";
  std::tuple<const std::string> dummy = std::make_tuple(re);
  EXPECT_EQ("retail->", a2.Perform(dummy));
}

ACTION_P4(Plus, p0, p1, p2, p3) { return arg0 + p0 + p1 + p2 + p3; }

TEST(ActionPnMacroTest, WorksFor4Parameters) {
  Action<int(int)> a1 = Plus(1, 2, 3, 4);
  EXPECT_EQ(10 + 1 + 2 + 3 + 4, a1.Perform(std::make_tuple(10)));
}

ACTION_P5(Plus, p0, p1, p2, p3, p4) { return arg0 + p0 + p1 + p2 + p3 + p4; }

TEST(ActionPnMacroTest, WorksFor5Parameters) {
  Action<int(int)> a1 = Plus(1, 2, 3, 4, 5);
  EXPECT_EQ(10 + 1 + 2 + 3 + 4 + 5, a1.Perform(std::make_tuple(10)));
}

ACTION_P6(Plus, p0, p1, p2, p3, p4, p5) {
  return arg0 + p0 + p1 + p2 + p3 + p4 + p5;
}

TEST(ActionPnMacroTest, WorksFor6Parameters) {
  Action<int(int)> a1 = Plus(1, 2, 3, 4, 5, 6);
  EXPECT_EQ(10 + 1 + 2 + 3 + 4 + 5 + 6, a1.Perform(std::make_tuple(10)));
}

ACTION_P7(Plus, p0, p1, p2, p3, p4, p5, p6) {
  return arg0 + p0 + p1 + p2 + p3 + p4 + p5 + p6;
}

TEST(ActionPnMacroTest, WorksFor7Parameters) {
  Action<int(int)> a1 = Plus(1, 2, 3, 4, 5, 6, 7);
  EXPECT_EQ(10 + 1 + 2 + 3 + 4 + 5 + 6 + 7, a1.Perform(std::make_tuple(10)));
}

ACTION_P8(Plus, p0, p1, p2, p3, p4, p5, p6, p7) {
  return arg0 + p0 + p1 + p2 + p3 + p4 + p5 + p6 + p7;
}

TEST(ActionPnMacroTest, WorksFor8Parameters) {
  Action<int(int)> a1 = Plus(1, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_EQ(10 + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8,
            a1.Perform(std::make_tuple(10)));
}

ACTION_P9(Plus, p0, p1, p2, p3, p4, p5, p6, p7, p8) {
  return arg0 + p0 + p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8;
}

TEST(ActionPnMacroTest, WorksFor9Parameters) {
  Action<int(int)> a1 = Plus(1, 2, 3, 4, 5, 6, 7, 8, 9);
  EXPECT_EQ(10 + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9,
            a1.Perform(std::make_tuple(10)));
}

ACTION_P10(Plus, p0, p1, p2, p3, p4, p5, p6, p7, p8, last_param) {
  arg0_type t0 = arg0;
  last_param_type t9 = last_param;
  return t0 + p0 + p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8 + t9;
}

TEST(ActionPnMacroTest, WorksFor10Parameters) {
  Action<int(int)> a1 = Plus(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  EXPECT_EQ(10 + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10,
            a1.Perform(std::make_tuple(10)));
}

ACTION_P2(PadArgument, prefix, suffix) {

  std::string prefix_str(prefix);
  char suffix_char = static_cast<char>(suffix);
  return prefix_str + arg0 + suffix_char;
}

TEST(ActionPnMacroTest, SimpleTypePromotion) {
  Action<std::string(const char*)> no_promo =
      PadArgument(std::string("foo"), 'r');
  Action<std::string(const char*)> promo =
      PadArgument("foo", static_cast<int>('r'));
  EXPECT_EQ("foobar", no_promo.Perform(std::make_tuple(CharPtr("ba"))));
  EXPECT_EQ("foobar", promo.Perform(std::make_tuple(CharPtr("ba"))));
}

ACTION_P3(ConcatImpl, a, b, c) {
  std::stringstream ss;
  ss << a << b << c;
  return ss.str();
}

template <typename T1, typename T2>

ConcatImplActionP3<std::string, T1, T2> Concat(const std::string& a, T1 b,
                                               T2 c) {
  GTEST_INTENTIONAL_CONST_COND_PUSH_()
  if (true) {
    GTEST_INTENTIONAL_CONST_COND_POP_()

    return ConcatImpl(a, b, c);
  } else {

    return ConcatImpl<std::string, T1, T2>(a, b, c);
  }
}

template <typename T1, typename T2>
ConcatImplActionP3<T1, int, T2> Concat(T1 a, int b, T2 c) {
  return ConcatImpl(a, b, c);
}

TEST(ActionPnMacroTest, CanPartiallyRestrictParameterTypes) {
  Action<const std::string()> a1 = Concat("Hello", "1", 2);
  EXPECT_EQ("Hello12", a1.Perform(std::make_tuple()));

  a1 = Concat(1, 2, 3);
  EXPECT_EQ("123", a1.Perform(std::make_tuple()));
}

ACTION(DoFoo) {}
ACTION_P(DoFoo, p) {}
ACTION_P2(DoFoo, p0, p1) {}

TEST(ActionPnMacroTest, TypesAreCorrect) {

  DoFooAction a0 = DoFoo();

  DoFooActionP<int> a1 = DoFoo(1);

  DoFooActionP2<int, char> a2 = DoFoo(1, '2');
  PlusActionP3<int, int, char> a3 = Plus(1, 2, '3');
  PlusActionP4<int, int, int, char> a4 = Plus(1, 2, 3, '4');
  PlusActionP5<int, int, int, int, char> a5 = Plus(1, 2, 3, 4, '5');
  PlusActionP6<int, int, int, int, int, char> a6 = Plus(1, 2, 3, 4, 5, '6');
  PlusActionP7<int, int, int, int, int, int, char> a7 =
      Plus(1, 2, 3, 4, 5, 6, '7');
  PlusActionP8<int, int, int, int, int, int, int, char> a8 =
      Plus(1, 2, 3, 4, 5, 6, 7, '8');
  PlusActionP9<int, int, int, int, int, int, int, int, char> a9 =
      Plus(1, 2, 3, 4, 5, 6, 7, 8, '9');
  PlusActionP10<int, int, int, int, int, int, int, int, int, char> a10 =
      Plus(1, 2, 3, 4, 5, 6, 7, 8, 9, '0');

  (void)a0;
  (void)a1;
  (void)a2;
  (void)a3;
  (void)a4;
  (void)a5;
  (void)a6;
  (void)a7;
  (void)a8;
  (void)a9;
  (void)a10;
}

ACTION_P(Plus1, x) { return x; }
ACTION_P2(Plus2, x, y) { return x + y; }
ACTION_P3(Plus3, x, y, z) { return x + y + z; }
ACTION_P10(Plus10, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9) {
  return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9;
}

TEST(ActionPnMacroTest, CanExplicitlyInstantiateWithReferenceTypes) {
  int x = 1, y = 2, z = 3;
  const std::tuple<> empty = std::make_tuple();

  Action<int()> a = Plus1<int&>(x);
  EXPECT_EQ(1, a.Perform(empty));

  a = Plus2<const int&, int&>(x, y);
  EXPECT_EQ(3, a.Perform(empty));

  a = Plus3<int&, const int&, int&>(x, y, z);
  EXPECT_EQ(6, a.Perform(empty));

  int n[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  a = Plus10<const int&, int&, const int&, int&, const int&, int&, const int&,
             int&, const int&, int&>(n[0], n[1], n[2], n[3], n[4], n[5], n[6],
                                     n[7], n[8], n[9]);
  EXPECT_EQ(55, a.Perform(empty));
}

class TenArgConstructorClass {
 public:
  TenArgConstructorClass(int a1, int a2, int a3, int a4, int a5, int a6, int a7,
                         int a8, int a9, int a10)
      : value_(a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10) {}
  int value_;
};

ACTION_TEMPLATE(CreateNew, HAS_1_TEMPLATE_PARAMS(typename, T),
                AND_0_VALUE_PARAMS()) {
  return new T;
}

TEST(ActionTemplateTest, WorksWithoutValueParam) {
  const Action<int*()> a = CreateNew<int>();
  int* p = a.Perform(std::make_tuple());
  delete p;
}

ACTION_TEMPLATE(CreateNew, HAS_1_TEMPLATE_PARAMS(typename, T),
                AND_1_VALUE_PARAMS(a0)) {
  return new T(a0);
}

TEST(ActionTemplateTest, WorksWithValueParams) {
  const Action<int*()> a = CreateNew<int>(42);
  int* p = a.Perform(std::make_tuple());
  EXPECT_EQ(42, *p);
  delete p;
}

ACTION_TEMPLATE(MyDeleteArg, HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  delete std::get<k>(args);
}

class BoolResetter {
 public:
  explicit BoolResetter(bool* value) : value_(value) {}
  ~BoolResetter() { *value_ = false; }

 private:
  bool* value_;
};

TEST(ActionTemplateTest, WorksForIntegralTemplateParams) {
  const Action<void(int*, BoolResetter*)> a = MyDeleteArg<1>();
  int n = 0;
  bool b = true;
  auto* resetter = new BoolResetter(&b);
  a.Perform(std::make_tuple(&n, resetter));
  EXPECT_FALSE(b);
}

ACTION_TEMPLATE(ReturnSmartPointer,
                HAS_1_TEMPLATE_PARAMS(template <typename Pointee> class,
                                      Pointer),
                AND_1_VALUE_PARAMS(pointee)) {
  return Pointer<pointee_type>(new pointee_type(pointee));
}

TEST(ActionTemplateTest, WorksForTemplateTemplateParameters) {
  const Action<std::shared_ptr<int>()> a =
      ReturnSmartPointer<std::shared_ptr>(42);
  std::shared_ptr<int> p = a.Perform(std::make_tuple());
  EXPECT_EQ(42, *p);
}

template <typename T1, typename T2, typename T3, int k4, bool k5,
          unsigned int k6, typename T7, typename T8, typename T9>
struct GiantTemplate {
 public:
  explicit GiantTemplate(int a_value) : value(a_value) {}
  int value;
};

ACTION_TEMPLATE(ReturnGiant,
                HAS_10_TEMPLATE_PARAMS(typename, T1, typename, T2, typename, T3,
                                       int, k4, bool, k5, unsigned int, k6,
                                       class, T7, class, T8, class, T9,
                                       template <typename T> class, T10),
                AND_1_VALUE_PARAMS(value)) {
  return GiantTemplate<T10<T1>, T2, T3, k4, k5, k6, T7, T8, T9>(value);
}

TEST(ActionTemplateTest, WorksFor10TemplateParameters) {
  using Giant = GiantTemplate<std::shared_ptr<int>, bool, double, 5, true, 6,
                              char, unsigned, int>;
  const Action<Giant()> a = ReturnGiant<int, bool, double, 5, true, 6, char,
                                        unsigned, int, std::shared_ptr>(42);
  Giant giant = a.Perform(std::make_tuple());
  EXPECT_EQ(42, giant.value);
}

ACTION_TEMPLATE(ReturnSum, HAS_1_TEMPLATE_PARAMS(typename, Number),
                AND_10_VALUE_PARAMS(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10)) {
  return static_cast<Number>(v1) + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10;
}

TEST(ActionTemplateTest, WorksFor10ValueParameters) {
  const Action<int()> a = ReturnSum<int>(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  EXPECT_EQ(55, a.Perform(std::make_tuple()));
}

ACTION(ReturnSum) { return 0; }

ACTION_P(ReturnSum, x) { return x; }

ACTION_TEMPLATE(ReturnSum, HAS_1_TEMPLATE_PARAMS(typename, Number),
                AND_2_VALUE_PARAMS(v1, v2)) {
  return static_cast<Number>(v1) + v2;
}

ACTION_TEMPLATE(ReturnSum, HAS_1_TEMPLATE_PARAMS(typename, Number),
                AND_3_VALUE_PARAMS(v1, v2, v3)) {
  return static_cast<Number>(v1) + v2 + v3;
}

ACTION_TEMPLATE(ReturnSum, HAS_2_TEMPLATE_PARAMS(typename, Number, int, k),
                AND_4_VALUE_PARAMS(v1, v2, v3, v4)) {
  return static_cast<Number>(v1) + v2 + v3 + v4 + k;
}

TEST(ActionTemplateTest, CanBeOverloadedOnNumberOfValueParameters) {
  const Action<int()> a0 = ReturnSum();
  const Action<int()> a1 = ReturnSum(1);
  const Action<int()> a2 = ReturnSum<int>(1, 2);
  const Action<int()> a3 = ReturnSum<int>(1, 2, 3);
  const Action<int()> a4 = ReturnSum<int, 10000>(2000, 300, 40, 5);
  EXPECT_EQ(0, a0.Perform(std::make_tuple()));
  EXPECT_EQ(1, a1.Perform(std::make_tuple()));
  EXPECT_EQ(3, a2.Perform(std::make_tuple()));
  EXPECT_EQ(6, a3.Perform(std::make_tuple()));
  EXPECT_EQ(12345, a4.Perform(std::make_tuple()));
}

}
}

GTEST_DISABLE_MSC_WARNINGS_POP_()
GTEST_DISABLE_MSC_WARNINGS_POP_()
