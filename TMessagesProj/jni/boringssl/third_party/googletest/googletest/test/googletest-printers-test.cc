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

// Google Test - The Google C++ Testing and Mocking Framework
//
// This file tests the universal value printer.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <deque>
#include <forward_list>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"
#include "gtest/internal/gtest-port.h"

#ifdef GTEST_HAS_ABSL
#include "absl/strings/str_format.h"
#endif

#if GTEST_INTERNAL_HAS_STD_SPAN
#include <span>
#endif

#if GTEST_INTERNAL_HAS_COMPARE_LIB
#include <compare>
#endif

enum AnonymousEnum { kAE1 = -1, kAE2 = 1 };

enum EnumWithoutPrinter { kEWP1 = -2, kEWP2 = 42 };

enum EnumWithStreaming { kEWS1 = 10 };

std::ostream& operator<<(std::ostream& os, EnumWithStreaming e) {
  return os << (e == kEWS1 ? "kEWS1" : "invalid");
}

enum EnumWithPrintTo { kEWPT1 = 1 };

void PrintTo(EnumWithPrintTo e, std::ostream* os) {
  *os << (e == kEWPT1 ? "kEWPT1" : "invalid");
}

class BiggestIntConvertible {
 public:
  operator ::testing::internal::BiggestInt() const { return 42; }
};

class ParentClass {};
class ChildClassWithStreamOperator : public ParentClass {};
class ChildClassWithoutStreamOperator : public ParentClass {};
static void operator<<(std::ostream& os, const ParentClass&) {
  os << "ParentClass";
}
static void operator<<(std::ostream& os, const ChildClassWithStreamOperator&) {
  os << "ChildClassWithStreamOperator";
}

template <typename T>
class UnprintableTemplateInGlobal {
 public:
  UnprintableTemplateInGlobal() : value_() {}

 private:
  T value_;
};

class StreamableInGlobal {
 public:
  StreamableInGlobal() = default;
  StreamableInGlobal(const StreamableInGlobal&) = default;
  StreamableInGlobal& operator=(const StreamableInGlobal&) = default;
  virtual ~StreamableInGlobal() = default;
};

inline void operator<<(::std::ostream& os, const StreamableInGlobal&        ) {
  os << "StreamableInGlobal";
}

void operator<<(::std::ostream& os, const StreamableInGlobal*        ) {
  os << "StreamableInGlobal*";
}

#ifdef GTEST_HAS_ABSL

struct Point {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const Point& p) {
    absl::Format(&sink, "(%d, %d)", p.x, p.y);
  }

  int x = 10;
  int y = 20;
};
#endif

namespace foo {

class UnprintableInFoo {
 public:
  UnprintableInFoo() : z_(0) { memcpy(xy_, "\xEF\x12\x0\x0\x34\xAB\x0\x0", 8); }
  double z() const { return z_; }

 private:
  char xy_[8];
  double z_;
};

struct PrintableViaPrintTo {
  PrintableViaPrintTo() : value() {}
  int value;
};

void PrintTo(const PrintableViaPrintTo& x, ::std::ostream* os) {
  *os << "PrintableViaPrintTo: " << x.value;
}

struct PointerPrintable {};

::std::ostream& operator<<(::std::ostream& os,
                           const PointerPrintable*        ) {
  return os << "PointerPrintable*";
}

template <typename T>
class PrintableViaPrintToTemplate {
 public:
  explicit PrintableViaPrintToTemplate(const T& a_value) : value_(a_value) {}

  const T& value() const { return value_; }

 private:
  T value_;
};

template <typename T>
void PrintTo(const PrintableViaPrintToTemplate<T>& x, ::std::ostream* os) {
  *os << "PrintableViaPrintToTemplate: " << x.value();
}

template <typename T>
class StreamableTemplateInFoo {
 public:
  StreamableTemplateInFoo() : value_() {}

  const T& value() const { return value_; }

 private:
  T value_;
};

template <typename T>
inline ::std::ostream& operator<<(::std::ostream& os,
                                  const StreamableTemplateInFoo<T>& x) {
  return os << "StreamableTemplateInFoo: " << x.value();
}

struct TemplatedStreamableInFoo {};

template <typename OutputStream>
OutputStream& operator<<(OutputStream& os,
                         const TemplatedStreamableInFoo&       ) {
  os << "TemplatedStreamableInFoo";
  return os;
}

struct StreamableInLocal {};
void operator<<(::std::ostream& os, const StreamableInLocal&        ) {
  os << "StreamableInLocal";
}

class PathLike {
 public:
  struct iterator {
    typedef PathLike value_type;

    iterator& operator++();
    PathLike& operator*();
  };

  using value_type = char;
  using const_iterator = iterator;

  PathLike() = default;

  iterator begin() const { return iterator(); }
  iterator end() const { return iterator(); }

  friend ::std::ostream& operator<<(::std::ostream& os, const PathLike&) {
    return os << "Streamable-PathLike";
  }
};

}

namespace testing {
namespace {
template <typename T>
class Wrapper {
 public:
  explicit Wrapper(T&& value) : value_(std::forward<T>(value)) {}

  const T& value() const { return value_; }

 private:
  T value_;
};

}

namespace internal {
template <typename T>
class UniversalPrinter<Wrapper<T>> {
 public:
  static void Print(const Wrapper<T>& w, ::std::ostream* os) {
    *os << "Wrapper(";
    UniversalPrint(w.value(), os);
    *os << ')';
  }
};
}

namespace gtest_printers_test {

using ::std::deque;
using ::std::list;
using ::std::make_pair;
using ::std::map;
using ::std::multimap;
using ::std::multiset;
using ::std::pair;
using ::std::set;
using ::std::vector;
using ::testing::PrintToString;
using ::testing::internal::FormatForComparisonFailureMessage;
using ::testing::internal::NativeArray;
using ::testing::internal::RelationToSourceReference;
using ::testing::internal::Strings;
using ::testing::internal::UniversalPrint;
using ::testing::internal::UniversalPrinter;
using ::testing::internal::UniversalTersePrint;
using ::testing::internal::UniversalTersePrintTupleFieldsToStrings;

template <typename T>
std::string Print(const T& value) {
  ::std::stringstream ss;
  UniversalPrinter<T>::Print(value, &ss);
  return ss.str();
}

template <typename T>
std::string PrintByRef(const T& value) {
  ::std::stringstream ss;
  UniversalPrinter<T&>::Print(value, &ss);
  return ss.str();
}

TEST(PrintEnumTest, AnonymousEnum) {
  EXPECT_EQ("-1", Print(kAE1));
  EXPECT_EQ("1", Print(kAE2));
}

TEST(PrintEnumTest, EnumWithoutPrinter) {
  EXPECT_EQ("-2", Print(kEWP1));
  EXPECT_EQ("42", Print(kEWP2));
}

TEST(PrintEnumTest, EnumWithStreaming) {
  EXPECT_EQ("kEWS1", Print(kEWS1));
  EXPECT_EQ("invalid", Print(static_cast<EnumWithStreaming>(0)));
}

TEST(PrintEnumTest, EnumWithPrintTo) {
  EXPECT_EQ("kEWPT1", Print(kEWPT1));
  EXPECT_EQ("invalid", Print(static_cast<EnumWithPrintTo>(0)));
}

#ifdef GTEST_HAS_ABSL

TEST(PrintClassTest, AbslStringify) { EXPECT_EQ("(10, 20)", Print(Point())); }
#endif

TEST(PrintClassTest, BiggestIntConvertible) {
  EXPECT_EQ("42", Print(BiggestIntConvertible()));
}

TEST(PrintCharTest, PlainChar) {
  EXPECT_EQ("'\\0'", Print('\0'));
  EXPECT_EQ("'\\'' (39, 0x27)", Print('\''));
  EXPECT_EQ("'\"' (34, 0x22)", Print('"'));
  EXPECT_EQ("'?' (63, 0x3F)", Print('?'));
  EXPECT_EQ("'\\\\' (92, 0x5C)", Print('\\'));
  EXPECT_EQ("'\\a' (7)", Print('\a'));
  EXPECT_EQ("'\\b' (8)", Print('\b'));
  EXPECT_EQ("'\\f' (12, 0xC)", Print('\f'));
  EXPECT_EQ("'\\n' (10, 0xA)", Print('\n'));
  EXPECT_EQ("'\\r' (13, 0xD)", Print('\r'));
  EXPECT_EQ("'\\t' (9)", Print('\t'));
  EXPECT_EQ("'\\v' (11, 0xB)", Print('\v'));
  EXPECT_EQ("'\\x7F' (127)", Print('\x7F'));
  EXPECT_EQ("'\\xFF' (255)", Print('\xFF'));
  EXPECT_EQ("' ' (32, 0x20)", Print(' '));
  EXPECT_EQ("'a' (97, 0x61)", Print('a'));
}

TEST(PrintCharTest, SignedChar) {
  EXPECT_EQ("'\\0'", Print(static_cast<signed char>('\0')));
  EXPECT_EQ("'\\xCE' (-50)", Print(static_cast<signed char>(-50)));
}

TEST(PrintCharTest, UnsignedChar) {
  EXPECT_EQ("'\\0'", Print(static_cast<unsigned char>('\0')));
  EXPECT_EQ("'b' (98, 0x62)", Print(static_cast<unsigned char>('b')));
}

TEST(PrintCharTest, Char16) { EXPECT_EQ("U+0041", Print(u'A')); }

TEST(PrintCharTest, Char32) { EXPECT_EQ("U+0041", Print(U'A')); }

#ifdef __cpp_lib_char8_t
TEST(PrintCharTest, Char8) { EXPECT_EQ("U+0041", Print(u8'A')); }
#endif

TEST(PrintBuiltInTypeTest, Bool) {
  EXPECT_EQ("false", Print(false));
  EXPECT_EQ("true", Print(true));
}

TEST(PrintBuiltInTypeTest, Wchar_t) {
  EXPECT_EQ("L'\\0'", Print(L'\0'));
  EXPECT_EQ("L'\\'' (39, 0x27)", Print(L'\''));
  EXPECT_EQ("L'\"' (34, 0x22)", Print(L'"'));
  EXPECT_EQ("L'?' (63, 0x3F)", Print(L'?'));
  EXPECT_EQ("L'\\\\' (92, 0x5C)", Print(L'\\'));
  EXPECT_EQ("L'\\a' (7)", Print(L'\a'));
  EXPECT_EQ("L'\\b' (8)", Print(L'\b'));
  EXPECT_EQ("L'\\f' (12, 0xC)", Print(L'\f'));
  EXPECT_EQ("L'\\n' (10, 0xA)", Print(L'\n'));
  EXPECT_EQ("L'\\r' (13, 0xD)", Print(L'\r'));
  EXPECT_EQ("L'\\t' (9)", Print(L'\t'));
  EXPECT_EQ("L'\\v' (11, 0xB)", Print(L'\v'));
  EXPECT_EQ("L'\\x7F' (127)", Print(L'\x7F'));
  EXPECT_EQ("L'\\xFF' (255)", Print(L'\xFF'));
  EXPECT_EQ("L' ' (32, 0x20)", Print(L' '));
  EXPECT_EQ("L'a' (97, 0x61)", Print(L'a'));
  EXPECT_EQ("L'\\x576' (1398)", Print(static_cast<wchar_t>(0x576)));
  EXPECT_EQ("L'\\xC74D' (51021)", Print(static_cast<wchar_t>(0xC74D)));
}

TEST(PrintTypeSizeTest, Wchar_t) {
  EXPECT_LT(sizeof(wchar_t), sizeof(int64_t));
}

TEST(PrintBuiltInTypeTest, Integer) {
  EXPECT_EQ("'\\xFF' (255)", Print(static_cast<unsigned char>(255)));
  EXPECT_EQ("'\\x80' (-128)", Print(static_cast<signed char>(-128)));
  EXPECT_EQ("65535", Print(std::numeric_limits<uint16_t>::max()));
  EXPECT_EQ("-32768", Print(std::numeric_limits<int16_t>::min()));
  EXPECT_EQ("4294967295",
            Print(std::numeric_limits<uint32_t>::max()));
  EXPECT_EQ("-2147483648",
            Print(std::numeric_limits<int32_t>::min()));
  EXPECT_EQ("18446744073709551615",
            Print(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ("-9223372036854775808",
            Print(std::numeric_limits<int64_t>::min()));
#ifdef __cpp_lib_char8_t
  EXPECT_EQ("U+0000",
            Print(std::numeric_limits<char8_t>::min()));
  EXPECT_EQ("U+00FF",
            Print(std::numeric_limits<char8_t>::max()));
#endif
  EXPECT_EQ("U+0000",
            Print(std::numeric_limits<char16_t>::min()));
  EXPECT_EQ("U+FFFF",
            Print(std::numeric_limits<char16_t>::max()));
  EXPECT_EQ("U+0000",
            Print(std::numeric_limits<char32_t>::min()));
  EXPECT_EQ("U+FFFFFFFF",
            Print(std::numeric_limits<char32_t>::max()));
}

TEST(PrintBuiltInTypeTest, Size_t) {
  EXPECT_EQ("1", Print(sizeof('a')));
#ifndef GTEST_OS_WINDOWS

  EXPECT_EQ("-2", Print(static_cast<ssize_t>(-2)));
#endif
}

#if defined(__SIZEOF_INT128__)
TEST(PrintBuiltInTypeTest, Int128) {

  EXPECT_EQ("0", Print(__int128_t{0}));
  EXPECT_EQ("0", Print(__uint128_t{0}));
  EXPECT_EQ("12345", Print(__int128_t{12345}));
  EXPECT_EQ("12345", Print(__uint128_t{12345}));
  EXPECT_EQ("-12345", Print(__int128_t{-12345}));

  EXPECT_EQ("340282366920938463463374607431768211455", Print(~__uint128_t{}));
  __int128_t max_128 = static_cast<__int128_t>(~__uint128_t{} / 2);
  EXPECT_EQ("-170141183460469231731687303715884105728", Print(~max_128));
  EXPECT_EQ("170141183460469231731687303715884105727", Print(max_128));
}
#endif

TEST(PrintBuiltInTypeTest, FloatingPoints) {

  EXPECT_EQ("1.5", Print(1.5f));

  EXPECT_EQ("1.0999999", Print(1.09999990f));
  EXPECT_EQ("1.1", Print(1.10000002f));
  EXPECT_EQ("1.10000014", Print(1.10000014f));
  EXPECT_EQ("9e+09", Print(9e9f));

  EXPECT_EQ("-2.5", Print(-2.5));
}

#if GTEST_HAS_RTTI
TEST(PrintBuiltInTypeTest, TypeInfo) {
  struct MyStruct {};
  auto res = Print(typeid(MyStruct{}));

  EXPECT_NE(res.find("MyStruct"), res.npos) << res;
}
#endif

static std::string PrintPointer(const void* p) {
  ::std::stringstream expected_result_stream;
  expected_result_stream << p;
  return expected_result_stream.str();
}

TEST(PrintCStringTest, Const) {
  const char* p = "World";
  EXPECT_EQ(PrintPointer(p) + " pointing to \"World\"", Print(p));
}

TEST(PrintCStringTest, NonConst) {
  char p[] = "Hi";
  EXPECT_EQ(PrintPointer(p) + " pointing to \"Hi\"",
            Print(static_cast<char*>(p)));
}

TEST(PrintCStringTest, Null) {
  const char* p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintCStringTest, EscapesProperly) {
  const char* p = "'\"?\\\a\b\f\n\r\t\v\x7F\xFF a";
  EXPECT_EQ(PrintPointer(p) +
                " pointing to \"'\\\"?\\\\\\a\\b\\f"
                "\\n\\r\\t\\v\\x7F\\xFF a\"",
            Print(p));
}

#ifdef __cpp_lib_char8_t

TEST(PrintU8StringTest, Const) {
  const char8_t* p = u8"界";
  EXPECT_EQ(PrintPointer(p) + " pointing to u8\"\\xE7\\x95\\x8C\"", Print(p));
}

TEST(PrintU8StringTest, NonConst) {
  char8_t p[] = u8"世";
  EXPECT_EQ(PrintPointer(p) + " pointing to u8\"\\xE4\\xB8\\x96\"",
            Print(static_cast<char8_t*>(p)));
}

TEST(PrintU8StringTest, Null) {
  const char8_t* p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

#ifndef _MSC_VER
TEST(PrintU8StringTest, EscapesProperly) {
  const char8_t* p = u8"'\"?\\\a\b\f\n\r\t\v\x7F\xFF hello 世界";
  EXPECT_EQ(PrintPointer(p) +
                " pointing to u8\"'\\\"?\\\\\\a\\b\\f\\n\\r\\t\\v\\x7F\\xFF "
                "hello \\xE4\\xB8\\x96\\xE7\\x95\\x8C\"",
            Print(p));
}
#endif
#endif

TEST(PrintU16StringTest, Const) {
  const char16_t* p = u"界";
  EXPECT_EQ(PrintPointer(p) + " pointing to u\"\\x754C\"", Print(p));
}

TEST(PrintU16StringTest, NonConst) {
  char16_t p[] = u"世";
  EXPECT_EQ(PrintPointer(p) + " pointing to u\"\\x4E16\"",
            Print(static_cast<char16_t*>(p)));
}

TEST(PrintU16StringTest, Null) {
  const char16_t* p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintU16StringTest, EscapesProperly) {
  const char16_t* p = u"'\"?\\\a\b\f\n\r\t\v\x7F\xFF hello 世界";
  EXPECT_EQ(PrintPointer(p) +
                " pointing to u\"'\\\"?\\\\\\a\\b\\f\\n\\r\\t\\v\\x7F\\xFF "
                "hello \\x4E16\\x754C\"",
            Print(p));
}

TEST(PrintU32StringTest, Const) {
  const char32_t* p = U"🗺️";
  EXPECT_EQ(PrintPointer(p) + " pointing to U\"\\x1F5FA\\xFE0F\"", Print(p));
}

TEST(PrintU32StringTest, NonConst) {
  char32_t p[] = U"🌌";
  EXPECT_EQ(PrintPointer(p) + " pointing to U\"\\x1F30C\"",
            Print(static_cast<char32_t*>(p)));
}

TEST(PrintU32StringTest, Null) {
  const char32_t* p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintU32StringTest, EscapesProperly) {
  const char32_t* p = U"'\"?\\\a\b\f\n\r\t\v\x7F\xFF hello 🗺️";
  EXPECT_EQ(PrintPointer(p) +
                " pointing to U\"'\\\"?\\\\\\a\\b\\f\\n\\r\\t\\v\\x7F\\xFF "
                "hello \\x1F5FA\\xFE0F\"",
            Print(p));
}

#if !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED)

TEST(PrintWideCStringTest, Const) {
  const wchar_t* p = L"World";
  EXPECT_EQ(PrintPointer(p) + " pointing to L\"World\"", Print(p));
}

TEST(PrintWideCStringTest, NonConst) {
  wchar_t p[] = L"Hi";
  EXPECT_EQ(PrintPointer(p) + " pointing to L\"Hi\"",
            Print(static_cast<wchar_t*>(p)));
}

TEST(PrintWideCStringTest, Null) {
  const wchar_t* p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintWideCStringTest, EscapesProperly) {
  const wchar_t s[] = {'\'',  '"',   '?',    '\\', '\a', '\b',
                       '\f',  '\n',  '\r',   '\t', '\v', 0xD3,
                       0x576, 0x8D3, 0xC74D, ' ',  'a',  '\0'};
  EXPECT_EQ(PrintPointer(s) +
                " pointing to L\"'\\\"?\\\\\\a\\b\\f"
                "\\n\\r\\t\\v\\xD3\\x576\\x8D3\\xC74D a\"",
            Print(static_cast<const wchar_t*>(s)));
}
#endif

TEST(PrintCharPointerTest, SignedChar) {
  signed char* p = reinterpret_cast<signed char*>(0x1234);
  EXPECT_EQ(PrintPointer(p), Print(p));
  p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintCharPointerTest, ConstSignedChar) {
  signed char* p = reinterpret_cast<signed char*>(0x1234);
  EXPECT_EQ(PrintPointer(p), Print(p));
  p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintCharPointerTest, UnsignedChar) {
  unsigned char* p = reinterpret_cast<unsigned char*>(0x1234);
  EXPECT_EQ(PrintPointer(p), Print(p));
  p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintCharPointerTest, ConstUnsignedChar) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(0x1234);
  EXPECT_EQ(PrintPointer(p), Print(p));
  p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintPointerToBuiltInTypeTest, Bool) {
  bool* p = reinterpret_cast<bool*>(0xABCD);
  EXPECT_EQ(PrintPointer(p), Print(p));
  p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintPointerToBuiltInTypeTest, Void) {
  void* p = reinterpret_cast<void*>(0xABCD);
  EXPECT_EQ(PrintPointer(p), Print(p));
  p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintPointerToBuiltInTypeTest, ConstVoid) {
  const void* p = reinterpret_cast<const void*>(0xABCD);
  EXPECT_EQ(PrintPointer(p), Print(p));
  p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

TEST(PrintPointerToPointerTest, IntPointerPointer) {
  int** p = reinterpret_cast<int**>(0xABCD);
  EXPECT_EQ(PrintPointer(p), Print(p));
  p = nullptr;
  EXPECT_EQ("NULL", Print(p));
}

void MyFunction(int        ) {}

TEST(PrintPointerTest, NonMemberFunctionPointer) {

  EXPECT_EQ(PrintPointer(reinterpret_cast<const void*>(
                reinterpret_cast<internal::BiggestInt>(&MyFunction))),
            Print(&MyFunction));
  int (*p)(bool) = NULL;
  EXPECT_EQ("NULL", Print(p));
}

template <typename StringType>
AssertionResult HasPrefix(const StringType& str, const StringType& prefix) {
  if (str.find(prefix, 0) == 0) return AssertionSuccess();

  const bool is_wide_string = sizeof(prefix[0]) > 1;
  const char* const begin_string_quote = is_wide_string ? "L\"" : "\"";
  return AssertionFailure()
         << begin_string_quote << prefix << "\" is not a prefix of "
         << begin_string_quote << str << "\"\n";
}

struct Foo {
 public:
  virtual ~Foo() = default;
  int MyMethod(char x) { return x + 1; }
  virtual char MyVirtualMethod(int        ) { return 'a'; }

  int value;
};

TEST(PrintPointerTest, MemberVariablePointer) {
  EXPECT_TRUE(HasPrefix(Print(&Foo::value),
                        Print(sizeof(&Foo::value)) + "-byte object "));
  int Foo::*p = NULL;
  EXPECT_TRUE(HasPrefix(Print(p), Print(sizeof(p)) + "-byte object "));
}

TEST(PrintPointerTest, MemberFunctionPointer) {
  EXPECT_TRUE(HasPrefix(Print(&Foo::MyMethod),
                        Print(sizeof(&Foo::MyMethod)) + "-byte object "));
  EXPECT_TRUE(
      HasPrefix(Print(&Foo::MyVirtualMethod),
                Print(sizeof((&Foo::MyVirtualMethod))) + "-byte object "));
  int (Foo::*p)(char) = NULL;
  EXPECT_TRUE(HasPrefix(Print(p), Print(sizeof(p)) + "-byte object "));
}

template <typename T, size_t N>
std::string PrintArrayHelper(T (&a)[N]) {
  return Print(a);
}

TEST(PrintArrayTest, OneDimensionalArray) {
  int a[5] = {1, 2, 3, 4, 5};
  EXPECT_EQ("{ 1, 2, 3, 4, 5 }", PrintArrayHelper(a));
}

TEST(PrintArrayTest, TwoDimensionalArray) {
  int a[2][5] = {{1, 2, 3, 4, 5}, {6, 7, 8, 9, 0}};
  EXPECT_EQ("{ { 1, 2, 3, 4, 5 }, { 6, 7, 8, 9, 0 } }", PrintArrayHelper(a));
}

TEST(PrintArrayTest, ConstArray) {
  const bool a[1] = {false};
  EXPECT_EQ("{ false }", PrintArrayHelper(a));
}

TEST(PrintArrayTest, CharArrayWithNoTerminatingNul) {

  char a[] = {'H', '\0', 'i'};
  EXPECT_EQ("\"H\\0i\" (no terminating NUL)", PrintArrayHelper(a));
}

TEST(PrintArrayTest, CharArrayWithTerminatingNul) {
  const char a[] = "\0Hi";
  EXPECT_EQ("\"\\0Hi\"", PrintArrayHelper(a));
}

#ifdef __cpp_lib_char8_t

TEST(PrintArrayTest, Char8ArrayWithNoTerminatingNul) {

  const char8_t a[] = {u8'H', u8'\0', u8'i'};
  EXPECT_EQ("u8\"H\\0i\" (no terminating NUL)", PrintArrayHelper(a));
}

TEST(PrintArrayTest, Char8ArrayWithTerminatingNul) {
  const char8_t a[] = u8"\0世界";
  EXPECT_EQ("u8\"\\0\\xE4\\xB8\\x96\\xE7\\x95\\x8C\"", PrintArrayHelper(a));
}
#endif

TEST(PrintArrayTest, Char16ArrayWithNoTerminatingNul) {

  const char16_t a[] = {u'こ', u'\0', u'ん', u'に', u'ち', u'は'};
  EXPECT_EQ("u\"\\x3053\\0\\x3093\\x306B\\x3061\\x306F\" (no terminating NUL)",
            PrintArrayHelper(a));
}

TEST(PrintArrayTest, Char16ArrayWithTerminatingNul) {
  const char16_t a[] = u"\0こんにちは";
  EXPECT_EQ("u\"\\0\\x3053\\x3093\\x306B\\x3061\\x306F\"", PrintArrayHelper(a));
}

TEST(PrintArrayTest, Char32ArrayWithNoTerminatingNul) {

  const char32_t a[] = {U'👋', U'\0', U'🌌'};
  EXPECT_EQ("U\"\\x1F44B\\0\\x1F30C\" (no terminating NUL)",
            PrintArrayHelper(a));
}

TEST(PrintArrayTest, Char32ArrayWithTerminatingNul) {
  const char32_t a[] = U"\0👋🌌";
  EXPECT_EQ("U\"\\0\\x1F44B\\x1F30C\"", PrintArrayHelper(a));
}

TEST(PrintArrayTest, WCharArrayWithNoTerminatingNul) {

  const wchar_t a[] = {L'H', L'\0', L'i'};
  EXPECT_EQ("L\"H\\0i\" (no terminating NUL)", PrintArrayHelper(a));
}

TEST(PrintArrayTest, WCharArrayWithTerminatingNul) {
  const wchar_t a[] = L"\0Hi";
  EXPECT_EQ("L\"\\0Hi\"", PrintArrayHelper(a));
}

TEST(PrintArrayTest, ObjectArray) {
  std::string a[3] = {"Hi", "Hello", "Ni hao"};
  EXPECT_EQ("{ \"Hi\", \"Hello\", \"Ni hao\" }", PrintArrayHelper(a));
}

TEST(PrintArrayTest, BigArray) {
  int a[100] = {1, 2, 3};
  EXPECT_EQ("{ 1, 2, 3, 0, 0, 0, 0, 0, ..., 0, 0, 0, 0, 0, 0, 0, 0 }",
            PrintArrayHelper(a));
}

TEST(PrintStringTest, StringInStdNamespace) {
  const char s[] = "'\"?\\\a\b\f\n\0\r\t\v\x7F\xFF a";
  const ::std::string str(s, sizeof(s));
  EXPECT_EQ("\"'\\\"?\\\\\\a\\b\\f\\n\\0\\r\\t\\v\\x7F\\xFF a\\0\"",
            Print(str));
}

TEST(PrintStringTest, StringAmbiguousHex) {

  EXPECT_EQ("\"0\\x12\" \"3\"", Print(::std::string("0\x12"
                                                    "3")));

  EXPECT_EQ("\"mm\\x6\" \"bananas\"", Print(::std::string("mm\x6"
                                                          "bananas")));

  EXPECT_EQ("\"NOM\\x6\" \"BANANA\"", Print(::std::string("NOM\x6"
                                                          "BANANA")));

  EXPECT_EQ("\"!\\x5-!\"", Print(::std::string("!\x5-!")));
}

#if GTEST_HAS_STD_WSTRING

TEST(PrintWideStringTest, StringInStdNamespace) {
  const wchar_t s[] = L"'\"?\\\a\b\f\n\0\r\t\v\xD3\x576\x8D3\xC74D a";
  const ::std::wstring str(s, sizeof(s) / sizeof(wchar_t));
  EXPECT_EQ(
      "L\"'\\\"?\\\\\\a\\b\\f\\n\\0\\r\\t\\v"
      "\\xD3\\x576\\x8D3\\xC74D a\\0\"",
      Print(str));
}

TEST(PrintWideStringTest, StringAmbiguousHex) {

  EXPECT_EQ("L\"0\\x12\" L\"3\"", Print(::std::wstring(L"0\x12"
                                                       L"3")));
  EXPECT_EQ("L\"mm\\x6\" L\"bananas\"", Print(::std::wstring(L"mm\x6"
                                                             L"bananas")));
  EXPECT_EQ("L\"NOM\\x6\" L\"BANANA\"", Print(::std::wstring(L"NOM\x6"
                                                             L"BANANA")));
  EXPECT_EQ("L\"!\\x5-!\"", Print(::std::wstring(L"!\x5-!")));
}
#endif

#ifdef __cpp_lib_char8_t
TEST(PrintStringTest, U8String) {
  std::u8string str = u8"Hello, 世界";
  EXPECT_EQ(str, str);
  EXPECT_EQ("u8\"Hello, \\xE4\\xB8\\x96\\xE7\\x95\\x8C\"", Print(str));
}
#endif

TEST(PrintStringTest, U16String) {
  std::u16string str = u"Hello, 世界";
  EXPECT_EQ(str, str);
  EXPECT_EQ("u\"Hello, \\x4E16\\x754C\"", Print(str));
}

TEST(PrintStringTest, U32String) {
  std::u32string str = U"Hello, 🗺️";
  EXPECT_EQ(str, str);
  EXPECT_EQ("U\"Hello, \\x1F5FA\\xFE0F\"", Print(str));
}

class AllowsGenericStreaming {};

template <typename Char, typename CharTraits>
std::basic_ostream<Char, CharTraits>& operator<<(
    std::basic_ostream<Char, CharTraits>& os,
    const AllowsGenericStreaming&        ) {
  return os << "AllowsGenericStreaming";
}

TEST(PrintTypeWithGenericStreamingTest, NonTemplateType) {
  AllowsGenericStreaming a;
  EXPECT_EQ("AllowsGenericStreaming", Print(a));
}

template <typename T>
class AllowsGenericStreamingTemplate {};

template <typename Char, typename CharTraits, typename T>
std::basic_ostream<Char, CharTraits>& operator<<(
    std::basic_ostream<Char, CharTraits>& os,
    const AllowsGenericStreamingTemplate<T>&        ) {
  return os << "AllowsGenericStreamingTemplate";
}

TEST(PrintTypeWithGenericStreamingTest, TemplateType) {
  AllowsGenericStreamingTemplate<int> a;
  EXPECT_EQ("AllowsGenericStreamingTemplate", Print(a));
}

template <typename T>
class AllowsGenericStreamingAndImplicitConversionTemplate {
 public:
  operator bool() const { return false; }
};

template <typename Char, typename CharTraits, typename T>
std::basic_ostream<Char, CharTraits>& operator<<(
    std::basic_ostream<Char, CharTraits>& os,
    const AllowsGenericStreamingAndImplicitConversionTemplate<T>&        ) {
  return os << "AllowsGenericStreamingAndImplicitConversionTemplate";
}

TEST(PrintTypeWithGenericStreamingTest, TypeImplicitlyConvertible) {
  AllowsGenericStreamingAndImplicitConversionTemplate<int> a;
  EXPECT_EQ("AllowsGenericStreamingAndImplicitConversionTemplate", Print(a));
}

#if GTEST_INTERNAL_HAS_STRING_VIEW

TEST(PrintStringViewTest, SimpleStringView) {
  const internal::StringView sp = "Hello";
  EXPECT_EQ("\"Hello\"", Print(sp));
}

TEST(PrintStringViewTest, UnprintableCharacters) {
  const char str[] = "NUL (\0) and \r\t";
  const internal::StringView sp(str, sizeof(str) - 1);
  EXPECT_EQ("\"NUL (\\0) and \\r\\t\"", Print(sp));
}

#endif

TEST(PrintStlContainerTest, EmptyDeque) {
  deque<char> empty;
  EXPECT_EQ("{}", Print(empty));
}

TEST(PrintStlContainerTest, NonEmptyDeque) {
  deque<int> non_empty;
  non_empty.push_back(1);
  non_empty.push_back(3);
  EXPECT_EQ("{ 1, 3 }", Print(non_empty));
}

TEST(PrintStlContainerTest, OneElementHashMap) {
  ::std::unordered_map<int, char> map1;
  map1[1] = 'a';
  EXPECT_EQ("{ (1, 'a' (97, 0x61)) }", Print(map1));
}

TEST(PrintStlContainerTest, HashMultiMap) {
  ::std::unordered_multimap<int, bool> map1;
  map1.insert(make_pair(5, true));
  map1.insert(make_pair(5, false));

  const std::string result = Print(map1);
  EXPECT_TRUE(result == "{ (5, true), (5, false) }" ||
              result == "{ (5, false), (5, true) }")
      << " where Print(map1) returns \"" << result << "\".";
}

TEST(PrintStlContainerTest, HashSet) {
  ::std::unordered_set<int> set1;
  set1.insert(1);
  EXPECT_EQ("{ 1 }", Print(set1));
}

TEST(PrintStlContainerTest, HashMultiSet) {
  const int kSize = 5;
  int a[kSize] = {1, 1, 2, 5, 1};
  ::std::unordered_multiset<int> set1(a, a + kSize);

  const std::string result = Print(set1);
  const std::string expected_pattern = "{ d, d, d, d, d }";

  ASSERT_EQ(expected_pattern.length(), result.length());
  std::vector<int> numbers;
  for (size_t i = 0; i != result.length(); i++) {
    if (expected_pattern[i] == 'd') {
      ASSERT_NE(isdigit(static_cast<unsigned char>(result[i])), 0);
      numbers.push_back(result[i] - '0');
    } else {
      EXPECT_EQ(expected_pattern[i], result[i])
          << " where result is " << result;
    }
  }

  std::sort(numbers.begin(), numbers.end());
  std::sort(a, a + kSize);
  EXPECT_TRUE(std::equal(a, a + kSize, numbers.begin()));
}

TEST(PrintStlContainerTest, List) {
  const std::string a[] = {"hello", "world"};
  const list<std::string> strings(a, a + 2);
  EXPECT_EQ("{ \"hello\", \"world\" }", Print(strings));
}

TEST(PrintStlContainerTest, Map) {
  map<int, bool> map1;
  map1[1] = true;
  map1[5] = false;
  map1[3] = true;
  EXPECT_EQ("{ (1, true), (3, true), (5, false) }", Print(map1));
}

TEST(PrintStlContainerTest, MultiMap) {
  multimap<bool, int> map1;

  map1.insert(pair<const bool, int>(true, 0));
  map1.insert(pair<const bool, int>(true, 1));
  map1.insert(pair<const bool, int>(false, 2));
  EXPECT_EQ("{ (false, 2), (true, 0), (true, 1) }", Print(map1));
}

TEST(PrintStlContainerTest, Set) {
  const unsigned int a[] = {3, 0, 5};
  set<unsigned int> set1(a, a + 3);
  EXPECT_EQ("{ 0, 3, 5 }", Print(set1));
}

TEST(PrintStlContainerTest, MultiSet) {
  const int a[] = {1, 1, 2, 5, 1};
  multiset<int> set1(a, a + 5);
  EXPECT_EQ("{ 1, 1, 1, 2, 5 }", Print(set1));
}

TEST(PrintStlContainerTest, SinglyLinkedList) {
  int a[] = {9, 2, 8};
  const std::forward_list<int> ints(a, a + 3);
  EXPECT_EQ("{ 9, 2, 8 }", Print(ints));
}

TEST(PrintStlContainerTest, Pair) {
  pair<const bool, int> p(true, 5);
  EXPECT_EQ("(true, 5)", Print(p));
}

TEST(PrintStlContainerTest, Vector) {
  vector<int> v;
  v.push_back(1);
  v.push_back(2);
  EXPECT_EQ("{ 1, 2 }", Print(v));
}

TEST(PrintStlContainerTest, StdSpan) {
#if GTEST_INTERNAL_HAS_STD_SPAN
  int a[] = {3, 6, 5};
  std::span<int> s = a;

  EXPECT_EQ("{ 3, 6, 5 }", Print(s));
#else
  GTEST_SKIP() << "Does not have std::span.";
#endif
}

TEST(PrintStlContainerTest, LongSequence) {
  const int a[100] = {1, 2, 3};
  const vector<int> v(a, a + 100);
  EXPECT_EQ(
      "{ 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "
      "0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ... }",
      Print(v));
}

TEST(PrintStlContainerTest, NestedContainer) {
  const int a1[] = {1, 2};
  const int a2[] = {3, 4, 5};
  const list<int> l1(a1, a1 + 2);
  const list<int> l2(a2, a2 + 3);

  vector<list<int>> v;
  v.push_back(l1);
  v.push_back(l2);
  EXPECT_EQ("{ { 1, 2 }, { 3, 4, 5 } }", Print(v));
}

TEST(PrintStlContainerTest, OneDimensionalNativeArray) {
  const int a[3] = {1, 2, 3};
  NativeArray<int> b(a, 3, RelationToSourceReference());
  EXPECT_EQ("{ 1, 2, 3 }", Print(b));
}

TEST(PrintStlContainerTest, TwoDimensionalNativeArray) {
  const int a[2][3] = {{1, 2, 3}, {4, 5, 6}};
  NativeArray<int[3]> b(a, 2, RelationToSourceReference());
  EXPECT_EQ("{ { 1, 2, 3 }, { 4, 5, 6 } }", Print(b));
}

struct iterator {
  char x;
};

TEST(PrintStlContainerTest, Iterator) {
  iterator it = {};
  EXPECT_EQ("1-byte object <00>", Print(it));
}

struct const_iterator {
  char x;
};

TEST(PrintStlContainerTest, ConstIterator) {
  const_iterator it = {};
  EXPECT_EQ("1-byte object <00>", Print(it));
}

TEST(PrintStdTupleTest, VariousSizes) {
  ::std::tuple<> t0;
  EXPECT_EQ("()", Print(t0));

  ::std::tuple<int> t1(5);
  EXPECT_EQ("(5)", Print(t1));

  ::std::tuple<char, bool> t2('a', true);
  EXPECT_EQ("('a' (97, 0x61), true)", Print(t2));

  ::std::tuple<bool, int, int> t3(false, 2, 3);
  EXPECT_EQ("(false, 2, 3)", Print(t3));

  ::std::tuple<bool, int, int, int> t4(false, 2, 3, 4);
  EXPECT_EQ("(false, 2, 3, 4)", Print(t4));

  const char* const str = "8";
  ::std::tuple<bool, char, short, int32_t, int64_t, float, double,
               const char*, void*, std::string>
      t10(false, 'a', static_cast<short>(3), 4, 5, 1.5F, -2.5, str,
          nullptr, "10");
  EXPECT_EQ("(false, 'a' (97, 0x61), 3, 4, 5, 1.5, -2.5, " + PrintPointer(str) +
                " pointing to \"8\", NULL, \"10\")",
            Print(t10));
}

TEST(PrintStdTupleTest, NestedTuple) {
  ::std::tuple<::std::tuple<int, bool>, char> nested(::std::make_tuple(5, true),
                                                     'a');
  EXPECT_EQ("((5, true), 'a' (97, 0x61))", Print(nested));
}

TEST(PrintNullptrT, Basic) { EXPECT_EQ("(nullptr)", Print(nullptr)); }

TEST(PrintReferenceWrapper, Printable) {
  int x = 5;
  EXPECT_EQ("@" + PrintPointer(&x) + " 5", Print(std::ref(x)));
  EXPECT_EQ("@" + PrintPointer(&x) + " 5", Print(std::cref(x)));
}

TEST(PrintReferenceWrapper, Unprintable) {
  ::foo::UnprintableInFoo up;
  EXPECT_EQ(
      "@" + PrintPointer(&up) +
          " 16-byte object <EF-12 00-00 34-AB 00-00 00-00 00-00 00-00 00-00>",
      Print(std::ref(up)));
  EXPECT_EQ(
      "@" + PrintPointer(&up) +
          " 16-byte object <EF-12 00-00 34-AB 00-00 00-00 00-00 00-00 00-00>",
      Print(std::cref(up)));
}

TEST(PrintUnprintableTypeTest, InGlobalNamespace) {
  EXPECT_EQ("1-byte object <00>", Print(UnprintableTemplateInGlobal<char>()));
}

TEST(PrintUnprintableTypeTest, InUserNamespace) {
  EXPECT_EQ("16-byte object <EF-12 00-00 34-AB 00-00 00-00 00-00 00-00 00-00>",
            Print(::foo::UnprintableInFoo()));
}

struct Big {
  Big() { memset(array, 0, sizeof(array)); }
  char array[257];
};

TEST(PrintUnpritableTypeTest, BigObject) {
  EXPECT_EQ(
      "257-byte object <00-00 00-00 00-00 00-00 00-00 00-00 "
      "00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 "
      "00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 "
      "00-00 00-00 00-00 00-00 00-00 00-00 ... 00-00 00-00 00-00 "
      "00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 "
      "00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 "
      "00-00 00-00 00-00 00-00 00-00 00-00 00-00 00-00 00>",
      Print(Big()));
}

TEST(PrintStreamableTypeTest, InGlobalNamespace) {
  StreamableInGlobal x;
  EXPECT_EQ("StreamableInGlobal", Print(x));
  EXPECT_EQ("StreamableInGlobal*", Print(&x));
}

TEST(PrintStreamableTypeTest, TemplateTypeInUserNamespace) {
  EXPECT_EQ("StreamableTemplateInFoo: 0",
            Print(::foo::StreamableTemplateInFoo<int>()));
}

TEST(PrintStreamableTypeTest, TypeInUserNamespaceWithTemplatedStreamOperator) {
  EXPECT_EQ("TemplatedStreamableInFoo",
            Print(::foo::TemplatedStreamableInFoo()));
}

TEST(PrintStreamableTypeTest, SubclassUsesSuperclassStreamOperator) {
  ParentClass parent;
  ChildClassWithStreamOperator child_stream;
  ChildClassWithoutStreamOperator child_no_stream;
  EXPECT_EQ("ParentClass", Print(parent));
  EXPECT_EQ("ChildClassWithStreamOperator", Print(child_stream));
  EXPECT_EQ("ParentClass", Print(child_no_stream));
}

TEST(PrintStreamableTypeTest, PathLikeInUserNamespace) {
  ::foo::PathLike x;
  EXPECT_EQ("Streamable-PathLike", Print(x));
  const ::foo::PathLike cx;
  EXPECT_EQ("Streamable-PathLike", Print(cx));
}

TEST(PrintPrintableTypeTest, InUserNamespace) {
  EXPECT_EQ("PrintableViaPrintTo: 0", Print(::foo::PrintableViaPrintTo()));
}

TEST(PrintPrintableTypeTest, PointerInUserNamespace) {
  ::foo::PointerPrintable x;
  EXPECT_EQ("PointerPrintable*", Print(&x));
}

TEST(PrintPrintableTypeTest, TemplateInUserNamespace) {
  EXPECT_EQ("PrintableViaPrintToTemplate: 5",
            Print(::foo::PrintableViaPrintToTemplate<int>(5)));
}

TEST(PrintReferenceTest, PrintsAddressAndValue) {
  int n = 5;
  EXPECT_EQ("@" + PrintPointer(&n) + " 5", PrintByRef(n));

  int a[2][3] = {{0, 1, 2}, {3, 4, 5}};
  EXPECT_EQ("@" + PrintPointer(a) + " { { 0, 1, 2 }, { 3, 4, 5 } }",
            PrintByRef(a));

  const ::foo::UnprintableInFoo x;
  EXPECT_EQ("@" + PrintPointer(&x) +
                " 16-byte object "
                "<EF-12 00-00 34-AB 00-00 00-00 00-00 00-00 00-00>",
            PrintByRef(x));
}

TEST(PrintReferenceTest, HandlesFunctionPointer) {
  void (*fp)(int n) = &MyFunction;
  const std::string fp_pointer_string =
      PrintPointer(reinterpret_cast<const void*>(&fp));

  const std::string fp_string = PrintPointer(reinterpret_cast<const void*>(
      reinterpret_cast<internal::BiggestInt>(fp)));
  EXPECT_EQ("@" + fp_pointer_string + " " + fp_string, PrintByRef(fp));
}

TEST(PrintReferenceTest, HandlesMemberFunctionPointer) {
  int (Foo::*p)(char ch) = &Foo::MyMethod;
  EXPECT_TRUE(HasPrefix(PrintByRef(p),
                        "@" + PrintPointer(reinterpret_cast<const void*>(&p)) +
                            " " + Print(sizeof(p)) + "-byte object "));

  char (Foo::*p2)(int n) = &Foo::MyVirtualMethod;
  EXPECT_TRUE(HasPrefix(PrintByRef(p2),
                        "@" + PrintPointer(reinterpret_cast<const void*>(&p2)) +
                            " " + Print(sizeof(p2)) + "-byte object "));
}

TEST(PrintReferenceTest, HandlesMemberVariablePointer) {
  int Foo::*p = &Foo::value;
  EXPECT_TRUE(HasPrefix(PrintByRef(p), "@" + PrintPointer(&p) + " " +
                                           Print(sizeof(p)) + "-byte object "));
}

TEST(FormatForComparisonFailureMessageTest, WorksForScalar) {
  EXPECT_STREQ("123", FormatForComparisonFailureMessage(123, 124).c_str());
}

TEST(FormatForComparisonFailureMessageTest, WorksForNonCharPointer) {
  int n = 0;
  EXPECT_EQ(PrintPointer(&n),
            FormatForComparisonFailureMessage(&n, &n).c_str());
}

TEST(FormatForComparisonFailureMessageTest, FormatsNonCharArrayAsPointer) {

  int n[] = {1, 2, 3};
  EXPECT_EQ(PrintPointer(n), FormatForComparisonFailureMessage(n, n).c_str());
}

TEST(FormatForComparisonFailureMessageTest, WorksForCharPointerVsPointer) {

  const char* s = "hello";
  EXPECT_EQ(PrintPointer(s), FormatForComparisonFailureMessage(s, s).c_str());

  char ch = 'a';
  EXPECT_EQ(PrintPointer(&ch),
            FormatForComparisonFailureMessage(&ch, &ch).c_str());
}

TEST(FormatForComparisonFailureMessageTest, WorksForWCharPointerVsPointer) {

  const wchar_t* s = L"hello";
  EXPECT_EQ(PrintPointer(s), FormatForComparisonFailureMessage(s, s).c_str());

  wchar_t ch = L'a';
  EXPECT_EQ(PrintPointer(&ch),
            FormatForComparisonFailureMessage(&ch, &ch).c_str());
}

TEST(FormatForComparisonFailureMessageTest, WorksForCharPointerVsStdString) {
  const char* s = "hello \"world";
  EXPECT_STREQ("\"hello \\\"world\"",
               FormatForComparisonFailureMessage(s, ::std::string()).c_str());

  char str[] = "hi\1";
  char* p = str;
  EXPECT_STREQ("\"hi\\x1\"",
               FormatForComparisonFailureMessage(p, ::std::string()).c_str());
}

#if GTEST_HAS_STD_WSTRING

TEST(FormatForComparisonFailureMessageTest, WorksForWCharPointerVsStdWString) {
  const wchar_t* s = L"hi \"world";
  EXPECT_STREQ("L\"hi \\\"world\"",
               FormatForComparisonFailureMessage(s, ::std::wstring()).c_str());

  wchar_t str[] = L"hi\1";
  wchar_t* p = str;
  EXPECT_STREQ("L\"hi\\x1\"",
               FormatForComparisonFailureMessage(p, ::std::wstring()).c_str());
}
#endif

TEST(FormatForComparisonFailureMessageTest, WorksForCharArrayVsPointer) {
  char str[] = "hi \"world\"";
  char* p = nullptr;
  EXPECT_EQ(PrintPointer(str),
            FormatForComparisonFailureMessage(str, p).c_str());
}

TEST(FormatForComparisonFailureMessageTest, WorksForCharArrayVsCharArray) {
  const char str[] = "hi \"world\"";
  EXPECT_EQ(PrintPointer(str),
            FormatForComparisonFailureMessage(str, str).c_str());
}

TEST(FormatForComparisonFailureMessageTest, WorksForWCharArrayVsPointer) {
  wchar_t str[] = L"hi \"world\"";
  wchar_t* p = nullptr;
  EXPECT_EQ(PrintPointer(str),
            FormatForComparisonFailureMessage(str, p).c_str());
}

TEST(FormatForComparisonFailureMessageTest, WorksForWCharArrayVsWCharArray) {
  const wchar_t str[] = L"hi \"world\"";
  EXPECT_EQ(PrintPointer(str),
            FormatForComparisonFailureMessage(str, str).c_str());
}

TEST(FormatForComparisonFailureMessageTest, WorksForCharArrayVsStdString) {
  const char str[] = "hi \"world\"";
  EXPECT_STREQ("\"hi \\\"world\\\"\"",
               FormatForComparisonFailureMessage(str, ::std::string()).c_str());
}

#if GTEST_HAS_STD_WSTRING

TEST(FormatForComparisonFailureMessageTest, WorksForWCharArrayVsStdWString) {
  const wchar_t str[] = L"hi \"w\0rld\"";
  EXPECT_STREQ(
      "L\"hi \\\"w\"",

      FormatForComparisonFailureMessage(str, ::std::wstring()).c_str());
}
#endif

#define EXPECT_PRINT_TO_STRING_(value, expected_string)  \
  EXPECT_TRUE(PrintToString(value) == (expected_string)) \
      << " where " #value " prints as " << (PrintToString(value))

TEST(PrintToStringTest, WorksForScalar) { EXPECT_PRINT_TO_STRING_(123, "123"); }

TEST(PrintToStringTest, WorksForPointerToConstChar) {
  const char* p = "hello";
  EXPECT_PRINT_TO_STRING_(p, "\"hello\"");
}

TEST(PrintToStringTest, WorksForPointerToNonConstChar) {
  char s[] = "hello";
  char* p = s;
  EXPECT_PRINT_TO_STRING_(p, "\"hello\"");
}

TEST(PrintToStringTest, EscapesForPointerToConstChar) {
  const char* p = "hello\n";
  EXPECT_PRINT_TO_STRING_(p, "\"hello\\n\"");
}

TEST(PrintToStringTest, EscapesForPointerToNonConstChar) {
  char s[] = "hello\1";
  char* p = s;
  EXPECT_PRINT_TO_STRING_(p, "\"hello\\x1\"");
}

TEST(PrintToStringTest, WorksForArray) {
  int n[3] = {1, 2, 3};
  EXPECT_PRINT_TO_STRING_(n, "{ 1, 2, 3 }");
}

TEST(PrintToStringTest, WorksForCharArray) {
  char s[] = "hello";
  EXPECT_PRINT_TO_STRING_(s, "\"hello\"");
}

TEST(PrintToStringTest, WorksForCharArrayWithEmbeddedNul) {
  const char str_with_nul[] = "hello\0 world";
  EXPECT_PRINT_TO_STRING_(str_with_nul, "\"hello\\0 world\"");

  char mutable_str_with_nul[] = "hello\0 world";
  EXPECT_PRINT_TO_STRING_(mutable_str_with_nul, "\"hello\\0 world\"");
}

TEST(PrintToStringTest, ContainsNonLatin) {

  std::string non_ascii_str = ::std::string("오전 4:30");
  EXPECT_PRINT_TO_STRING_(non_ascii_str,
                          "\"\\xEC\\x98\\xA4\\xEC\\xA0\\x84 4:30\"\n"
                          "    As Text: \"오전 4:30\"");
  non_ascii_str = ::std::string("From ä — ẑ");
  EXPECT_PRINT_TO_STRING_(non_ascii_str,
                          "\"From \\xC3\\xA4 \\xE2\\x80\\x94 \\xE1\\xBA\\x91\""
                          "\n    As Text: \"From ä — ẑ\"");
}

TEST(PrintToStringTest, PrintStreamableInLocal) {
  EXPECT_STREQ("StreamableInLocal",
               PrintToString(foo::StreamableInLocal()).c_str());
}

TEST(PrintToStringTest, PrintReferenceToStreamableInLocal) {
  foo::StreamableInLocal s;
  std::reference_wrapper<foo::StreamableInLocal> r(s);
  EXPECT_STREQ("StreamableInLocal", PrintToString(r).c_str());
}

TEST(PrintToStringTest, PrintReferenceToStreamableInGlobal) {
  StreamableInGlobal s;
  std::reference_wrapper<StreamableInGlobal> r(s);
  EXPECT_STREQ("StreamableInGlobal", PrintToString(r).c_str());
}

#ifdef GTEST_HAS_ABSL
TEST(PrintToStringTest, AbslStringify) {
  EXPECT_PRINT_TO_STRING_(Point(), "(10, 20)");
}
#endif

TEST(IsValidUTF8Test, IllFormedUTF8) {

  static const char* const kTestdata[][2] = {

      {"\xC3\x74", "\"\\xC3t\""},

      {"\xC3\x84\xA4", "\"\\xC3\\x84\\xA4\""},

      {"abc\xC3", "\"abc\\xC3\""},

      {"x\xE2\x70\x94", "\"x\\xE2p\\x94\""},

      {"\xE2\x80", "\"\\xE2\\x80\""},

      {"\xE2\x80\xC3\x84", "\"\\xE2\\x80\\xC3\\x84\""},

      {"\xE2\x80\x7A", "\"\\xE2\\x80z\""},

      {"\xE2\xE2\x80\x94", "\"\\xE2\\xE2\\x80\\x94\""},

      {"\xF0\xE2\x80\x94", "\"\\xF0\\xE2\\x80\\x94\""},

      {"\xF0\xE2\x80", "\"\\xF0\\xE2\\x80\""},

      {"abc\xE2\x80\x94\xC3\x74xyc", "\"abc\\xE2\\x80\\x94\\xC3txyc\""},
      {"abc\xC3\x84\xE2\x80\xC3\x84xyz",
       "\"abc\\xC3\\x84\\xE2\\x80\\xC3\\x84xyz\""},

      {"\xC0\x80", "\"\\xC0\\x80\""},
      {"\xC1\x81", "\"\\xC1\\x81\""},

      {"\xE0\x80\x80", "\"\\xE0\\x80\\x80\""},
      {"\xf0\x80\x80\x80", "\"\\xF0\\x80\\x80\\x80\""},

      {"\xED\x9F\xBF", "\"\\xED\\x9F\\xBF\"\n    As Text: \"퟿\""},

      {"\xED\xA0\x80", "\"\\xED\\xA0\\x80\""},

      {"\xED\xAD\xBF", "\"\\xED\\xAD\\xBF\""},

      {"\xED\xAE\x80", "\"\\xED\\xAE\\x80\""},

      {"\xED\xAF\xBF", "\"\\xED\\xAF\\xBF\""},

      {"\xED\xB3\xBF", "\"\\xED\\xB3\\xBF\""},

      {"\xEE\x80\x80", "\"\\xEE\\x80\\x80\"\n    As Text: \"\""}};

  for (int i = 0; i < int(sizeof(kTestdata) / sizeof(kTestdata[0])); ++i) {
    EXPECT_PRINT_TO_STRING_(kTestdata[i][0], kTestdata[i][1]);
  }
}

#undef EXPECT_PRINT_TO_STRING_

TEST(UniversalTersePrintTest, WorksForNonReference) {
  ::std::stringstream ss;
  UniversalTersePrint(123, &ss);
  EXPECT_EQ("123", ss.str());
}

TEST(UniversalTersePrintTest, WorksForReference) {
  const int& n = 123;
  ::std::stringstream ss;
  UniversalTersePrint(n, &ss);
  EXPECT_EQ("123", ss.str());
}

TEST(UniversalTersePrintTest, WorksForCString) {
  const char* s1 = "abc";
  ::std::stringstream ss1;
  UniversalTersePrint(s1, &ss1);
  EXPECT_EQ("\"abc\"", ss1.str());

  char* s2 = const_cast<char*>(s1);
  ::std::stringstream ss2;
  UniversalTersePrint(s2, &ss2);
  EXPECT_EQ("\"abc\"", ss2.str());

  const char* s3 = nullptr;
  ::std::stringstream ss3;
  UniversalTersePrint(s3, &ss3);
  EXPECT_EQ("NULL", ss3.str());
}

TEST(UniversalPrintTest, WorksForNonReference) {
  ::std::stringstream ss;
  UniversalPrint(123, &ss);
  EXPECT_EQ("123", ss.str());
}

TEST(UniversalPrintTest, WorksForReference) {
  const int& n = 123;
  ::std::stringstream ss;
  UniversalPrint(n, &ss);
  EXPECT_EQ("123", ss.str());
}

TEST(UniversalPrintTest, WorksForPairWithConst) {
  std::pair<const Wrapper<std::string>, int> p(Wrapper<std::string>("abc"), 1);
  ::std::stringstream ss;
  UniversalPrint(p, &ss);
  EXPECT_EQ("(Wrapper(\"abc\"), 1)", ss.str());
}

TEST(UniversalPrintTest, WorksForCString) {
  const char* s1 = "abc";
  ::std::stringstream ss1;
  UniversalPrint(s1, &ss1);
  EXPECT_EQ(PrintPointer(s1) + " pointing to \"abc\"", std::string(ss1.str()));

  char* s2 = const_cast<char*>(s1);
  ::std::stringstream ss2;
  UniversalPrint(s2, &ss2);
  EXPECT_EQ(PrintPointer(s2) + " pointing to \"abc\"", std::string(ss2.str()));

  const char* s3 = nullptr;
  ::std::stringstream ss3;
  UniversalPrint(s3, &ss3);
  EXPECT_EQ("NULL", ss3.str());
}

TEST(UniversalPrintTest, WorksForCharArray) {
  const char str[] = "\"Line\0 1\"\nLine 2";
  ::std::stringstream ss1;
  UniversalPrint(str, &ss1);
  EXPECT_EQ("\"\\\"Line\\0 1\\\"\\nLine 2\"", ss1.str());

  const char mutable_str[] = "\"Line\0 1\"\nLine 2";
  ::std::stringstream ss2;
  UniversalPrint(mutable_str, &ss2);
  EXPECT_EQ("\"\\\"Line\\0 1\\\"\\nLine 2\"", ss2.str());
}

TEST(UniversalPrintTest, IncompleteType) {
  struct Incomplete;
  char some_object = 0;
  EXPECT_EQ("(incomplete type)",
            PrintToString(reinterpret_cast<Incomplete&>(some_object)));
}

TEST(UniversalPrintTest, SmartPointers) {
  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<int>()));
  std::unique_ptr<int> p(new int(17));
  EXPECT_EQ("(ptr = " + PrintPointer(p.get()) + ", value = 17)",
            PrintToString(p));
  std::unique_ptr<int[]> p2(new int[2]);
  EXPECT_EQ("(" + PrintPointer(p2.get()) + ")", PrintToString(p2));

  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<int>()));
  std::shared_ptr<int> p3(new int(1979));
  EXPECT_EQ("(ptr = " + PrintPointer(p3.get()) + ", value = 1979)",
            PrintToString(p3));
#if defined(__cpp_lib_shared_ptr_arrays) && \
    (__cpp_lib_shared_ptr_arrays >= 201611L)
  std::shared_ptr<int[]> p4(new int[2]);
  EXPECT_EQ("(" + PrintPointer(p4.get()) + ")", PrintToString(p4));
#endif

  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<int>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<const int>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<volatile int>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<volatile const int>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<int[]>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<const int[]>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<volatile int[]>()));
  EXPECT_EQ("(nullptr)",
            PrintToString(std::unique_ptr<volatile const int[]>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<int>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<const int>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<volatile int>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<volatile const int>()));
#if defined(__cpp_lib_shared_ptr_arrays) && \
    (__cpp_lib_shared_ptr_arrays >= 201611L)
  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<int[]>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<const int[]>()));
  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<volatile int[]>()));
  EXPECT_EQ("(nullptr)",
            PrintToString(std::shared_ptr<volatile const int[]>()));
#endif

  EXPECT_EQ("(nullptr)", PrintToString(std::unique_ptr<void, void (*)(void*)>(
                             nullptr, nullptr)));
  EXPECT_EQ("(" + PrintPointer(p.get()) + ")",
            PrintToString(
                std::unique_ptr<void, void (*)(void*)>(p.get(), [](void*) {})));
  EXPECT_EQ("(nullptr)", PrintToString(std::shared_ptr<void>()));
  EXPECT_EQ("(" + PrintPointer(p.get()) + ")",
            PrintToString(std::shared_ptr<void>(p.get(), [](void*) {})));
}

TEST(UniversalTersePrintTupleFieldsToStringsTestWithStd, PrintsEmptyTuple) {
  Strings result = UniversalTersePrintTupleFieldsToStrings(::std::make_tuple());
  EXPECT_EQ(0u, result.size());
}

TEST(UniversalTersePrintTupleFieldsToStringsTestWithStd, PrintsOneTuple) {
  Strings result =
      UniversalTersePrintTupleFieldsToStrings(::std::make_tuple(1));
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("1", result[0]);
}

TEST(UniversalTersePrintTupleFieldsToStringsTestWithStd, PrintsTwoTuple) {
  Strings result =
      UniversalTersePrintTupleFieldsToStrings(::std::make_tuple(1, 'a'));
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("1", result[0]);
  EXPECT_EQ("'a' (97, 0x61)", result[1]);
}

TEST(UniversalTersePrintTupleFieldsToStringsTestWithStd, PrintsTersely) {
  const int n = 1;
  Strings result = UniversalTersePrintTupleFieldsToStrings(
      ::std::tuple<const int&, const char*>(n, "a"));
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("1", result[0]);
  EXPECT_EQ("\"a\"", result[1]);
}

#if GTEST_INTERNAL_HAS_ANY
class PrintAnyTest : public ::testing::Test {
 protected:
  template <typename T>
  static std::string ExpectedTypeName() {
#if GTEST_HAS_RTTI
    return internal::GetTypeName<T>();
#else
    return "<unknown_type>";
#endif
  }
};

TEST_F(PrintAnyTest, Empty) {
  internal::Any any;
  EXPECT_EQ("no value", PrintToString(any));
}

TEST_F(PrintAnyTest, NonEmpty) {
  internal::Any any;
  constexpr int val1 = 10;
  const std::string val2 = "content";

  any = val1;
  EXPECT_EQ("value of type " + ExpectedTypeName<int>(), PrintToString(any));

  any = val2;
  EXPECT_EQ("value of type " + ExpectedTypeName<std::string>(),
            PrintToString(any));
}
#endif

#if GTEST_INTERNAL_HAS_OPTIONAL
TEST(PrintOptionalTest, Basic) {
  EXPECT_EQ("(nullopt)", PrintToString(internal::Nullopt()));
  internal::Optional<int> value;
  EXPECT_EQ("(nullopt)", PrintToString(value));
  value = {7};
  EXPECT_EQ("(7)", PrintToString(value));
  EXPECT_EQ("(1.1)", PrintToString(internal::Optional<double>{1.1}));
  EXPECT_EQ("(\"A\")", PrintToString(internal::Optional<std::string>{"A"}));
}
#endif

#if GTEST_INTERNAL_HAS_VARIANT
struct NonPrintable {
  unsigned char contents = 17;
};

TEST(PrintOneofTest, Basic) {
  using Type = internal::Variant<int, StreamableInGlobal, NonPrintable>;
  EXPECT_EQ("('int(index = 0)' with value 7)", PrintToString(Type(7)));
  EXPECT_EQ("('StreamableInGlobal(index = 1)' with value StreamableInGlobal)",
            PrintToString(Type(StreamableInGlobal{})));
  EXPECT_EQ(
      "('testing::gtest_printers_test::NonPrintable(index = 2)' with value "
      "1-byte object <11>)",
      PrintToString(Type(NonPrintable{})));
}
#endif

#if GTEST_INTERNAL_HAS_COMPARE_LIB
TEST(PrintOrderingTest, Basic) {
  EXPECT_EQ("(less)", PrintToString(std::strong_ordering::less));
  EXPECT_EQ("(greater)", PrintToString(std::strong_ordering::greater));

  EXPECT_EQ("(equal)", PrintToString(std::strong_ordering::equivalent));
  EXPECT_EQ("(equal)", PrintToString(std::strong_ordering::equal));

  EXPECT_EQ("(less)", PrintToString(std::weak_ordering::less));
  EXPECT_EQ("(greater)", PrintToString(std::weak_ordering::greater));
  EXPECT_EQ("(equivalent)", PrintToString(std::weak_ordering::equivalent));

  EXPECT_EQ("(less)", PrintToString(std::partial_ordering::less));
  EXPECT_EQ("(greater)", PrintToString(std::partial_ordering::greater));
  EXPECT_EQ("(equivalent)", PrintToString(std::partial_ordering::equivalent));
  EXPECT_EQ("(unordered)", PrintToString(std::partial_ordering::unordered));
}
#endif

namespace {
class string_ref;

class string_ptr {
 public:
  string_ptr(const char* data, size_t size) : data_(data), size_(size) {}

  string_ptr& operator++() noexcept {
    data_ += size_;
    return *this;
  }

  string_ref operator*() const noexcept;

 private:
  const char* data_;
  size_t size_;
};

class string_ref {
 public:
  string_ref(const char* data, size_t size) : data_(data), size_(size) {}

  string_ptr operator&() const noexcept { return {data_, size_}; }

  bool operator==(const char* s) const noexcept {
    if (size_ > 0 && data_[size_ - 1] != 0) {
      return std::string(data_, size_) == std::string(s);
    } else {
      return std::string(data_) == std::string(s);
    }
  }

 private:
  const char* data_;
  size_t size_;
};

string_ref string_ptr::operator*() const noexcept { return {data_, size_}; }

TEST(string_ref, compare) {
  const char* s = "alex\0davidjohn\0";
  string_ptr ptr(s, 5);
  EXPECT_EQ(*ptr, "alex");
  EXPECT_TRUE(*ptr == "alex");
  ++ptr;
  EXPECT_EQ(*ptr, "david");
  EXPECT_TRUE(*ptr == "david");
  ++ptr;
  EXPECT_EQ(*ptr, "john");
}

}

}
}
