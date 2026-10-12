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
// This file implements a universal value printer that can print a
// value of any type T:
//
//   void ::testing::internal::UniversalPrinter<T>::Print(value, ostream_ptr);
//
// A user can teach this function how to print a class type T by
// defining either operator<<() or PrintTo() in the namespace that
// defines T.  More specifically, the FIRST defined function in the
// following list will be used (assuming T is defined in namespace
// foo):
//
//   1. foo::PrintTo(const T&, ostream*)
//   2. operator<<(ostream&, const T&) defined in either foo or the
//      global namespace.
// * Prefer AbslStringify(..) to operator<<(..), per https://abseil.io/tips/215.
// * Define foo::PrintTo(..) if the type already has AbslStringify(..), but an
//   alternative presentation in test results is of interest.
//
// However if T is an STL-style container then it is printed element-wise
// unless foo::PrintTo(const T&, ostream*) is defined. Note that
// operator<<() is ignored for container types.
//
// If none of the above is defined, it will print the debug string of
// the value if it is a protocol buffer, or print the raw bytes in the
// value otherwise.
//
// To aid debugging: when T is a reference type, the address of the
// value is also printed; when T is a (const) char pointer, both the
// pointer value and the NUL-terminated string it points to are
// printed.
//
// We also provide some convenient wrappers:
//
//   // Prints a value to a string.  For a (const or not) char
//   // pointer, the NUL-terminated string (but not the pointer) is
//   // printed.
//   std::string ::testing::PrintToString(const T& value);
//
//   // Prints a value tersely: for a reference type, the referenced
//   // value (but not the address) is printed; for a (const or not) char
//   // pointer, the NUL-terminated string (but not the pointer) is
//   // printed.
//   void ::testing::internal::UniversalTersePrint(const T& value, ostream*);
//
//   // Prints value using the type inferred by the compiler.  The difference
//   // from UniversalTersePrint() is that this function prints both the
//   // pointer and the NUL-terminated string for a (const or not) char pointer.
//   void ::testing::internal::UniversalPrint(const T& value, ostream*);
//
//   // Prints the fields of a tuple tersely to a string vector, one
//   // element for each field. Tuple support must be enabled in
//   // gtest-port.h.
//   std::vector<string> UniversalTersePrintTupleFieldsToStrings(
//       const Tuple& value);
//
// Known limitation:
//
// The print primitives print the elements of an STL-style container
// using the compiler-inferred type of *iter where iter is a
// const_iterator of the container.  When const_iterator is an input
// iterator but not a forward iterator, this inferred type may not
// match value_type, and the print output may be incorrect.  In
// practice, this is rarely a problem as for most containers
// const_iterator is a forward iterator.  We'll fix this if there's an
// actual need for it.  Note that this fix cannot rely on value_type
// being defined as many user-defined container types don't have
// value_type.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_PRINTERS_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_PRINTERS_H_

#include <functional>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#ifdef GTEST_HAS_ABSL
#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/str_cat.h"
#endif
#include "gtest/internal/gtest-internal.h"
#include "gtest/internal/gtest-port.h"

#if GTEST_INTERNAL_HAS_STD_SPAN
#include <span>
#endif

#if GTEST_INTERNAL_HAS_COMPARE_LIB
#include <compare>
#endif

namespace testing {

namespace internal {

template <typename T>
void UniversalPrint(const T& value, ::std::ostream* os);

template <typename T>
struct IsStdSpan {
  static constexpr bool value = false;
};

#if GTEST_INTERNAL_HAS_STD_SPAN
template <typename E>
struct IsStdSpan<std::span<E>> {
  static constexpr bool value = true;
};
#endif

struct ContainerPrinter {
  template <typename T,
            typename = typename std::enable_if<
                ((sizeof(IsContainerTest<T>(0)) == sizeof(IsContainer)) &&
                 !IsRecursiveContainer<T>::value) ||
                IsStdSpan<T>::value>::type>
  static void PrintValue(const T& container, std::ostream* os) {
    const size_t kMaxCount = 32;
    *os << '{';
    size_t count = 0;
    for (auto&& elem : container) {
      if (count > 0) {
        *os << ',';
        if (count == kMaxCount) {
          *os << " ...";
          break;
        }
      }
      *os << ' ';

      internal::UniversalPrint(elem, os);
      ++count;
    }

    if (count > 0) {
      *os << ' ';
    }
    *os << '}';
  }
};

struct FunctionPointerPrinter {
  template <typename T, typename = typename std::enable_if<
                            std::is_function<T>::value>::type>
  static void PrintValue(T* p, ::std::ostream* os) {
    if (p == nullptr) {
      *os << "NULL";
    } else {

      *os << reinterpret_cast<const void*>(p);
    }
  }
};

struct PointerPrinter {
  template <typename T>
  static void PrintValue(T* p, ::std::ostream* os) {
    if (p == nullptr) {
      *os << "NULL";
    } else {

      *os << p;
    }
  }
};

namespace internal_stream_operator_without_lexical_name_lookup {

struct LookupBlocker {};
void operator<<(LookupBlocker, LookupBlocker);

struct StreamPrinter {
  template <typename T,

            typename = typename std::enable_if<
                !std::is_member_pointer<T>::value>::type>

  static auto PrintValue(const T& value,
                         ::std::ostream* os) -> decltype((void)(*os << value)) {

    *os << value;
  }
};

}

struct ProtobufPrinter {

  static const size_t kProtobufOneLinerMaxLength = 50;

  template <typename T,
            typename = typename std::enable_if<
                internal::HasDebugStringAndShortDebugString<T>::value>::type>
  static void PrintValue(const T& value, ::std::ostream* os) {
    std::string pretty_str = value.ShortDebugString();
    if (pretty_str.length() > kProtobufOneLinerMaxLength) {
      pretty_str = "\n" + value.DebugString();
    }
    *os << ("<" + pretty_str + ">");
  }
};

struct ConvertibleToIntegerPrinter {

  static void PrintValue(internal::BiggestInt value, ::std::ostream* os) {
    *os << value;
  }
};

struct ConvertibleToStringViewPrinter {
#if GTEST_INTERNAL_HAS_STRING_VIEW
  static void PrintValue(internal::StringView value, ::std::ostream* os) {
    internal::UniversalPrint(value, os);
  }
#endif
};

#ifdef GTEST_HAS_ABSL
struct ConvertibleToAbslStringifyPrinter {
  template <typename T,
            typename = typename std::enable_if<
                absl::HasAbslStringify<T>::value>::type>
  static void PrintValue(const T& value, ::std::ostream* os) {
    *os << absl::StrCat(value);
  }
};
#endif

GTEST_API_ void PrintBytesInObjectTo(const unsigned char* obj_bytes,
                                     size_t count, ::std::ostream* os);
struct RawBytesPrinter {

  template <typename T, size_t = sizeof(T)>
  static void PrintValue(const T& value, ::std::ostream* os) {
    PrintBytesInObjectTo(
        static_cast<const unsigned char*>(

            reinterpret_cast<const void*>(std::addressof(value))),
        sizeof(value), os);
  }
};

struct FallbackPrinter {
  template <typename T>
  static void PrintValue(const T&, ::std::ostream* os) {
    *os << "(incomplete type)";
  }
};

template <typename T, typename E, typename Printer, typename... Printers>
struct FindFirstPrinter : FindFirstPrinter<T, E, Printers...> {};

template <typename T, typename Printer, typename... Printers>
struct FindFirstPrinter<
    T, decltype(Printer::PrintValue(std::declval<const T&>(), nullptr)),
    Printer, Printers...> {
  using type = Printer;
};

template <typename T>
void PrintWithFallback(const T& value, ::std::ostream* os) {
  using Printer = typename FindFirstPrinter<
      T, void, ContainerPrinter, FunctionPointerPrinter, PointerPrinter,
      ProtobufPrinter,
#ifdef GTEST_HAS_ABSL
      ConvertibleToAbslStringifyPrinter,
#endif
      internal_stream_operator_without_lexical_name_lookup::StreamPrinter,
      ConvertibleToIntegerPrinter, ConvertibleToStringViewPrinter,
      RawBytesPrinter, FallbackPrinter>::type;
  Printer::PrintValue(value, os);
}

template <typename ToPrint, typename OtherOperand>
class FormatForComparison {
 public:
  static ::std::string Format(const ToPrint& value) {
    return ::testing::PrintToString(value);
  }
};

template <typename ToPrint, size_t N, typename OtherOperand>
class FormatForComparison<ToPrint[N], OtherOperand> {
 public:
  static ::std::string Format(const ToPrint* value) {
    return FormatForComparison<const ToPrint*, OtherOperand>::Format(value);
  }
};

#define GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(CharType)                \
  template <typename OtherOperand>                                      \
  class FormatForComparison<CharType*, OtherOperand> {                  \
   public:                                                              \
    static ::std::string Format(CharType* value) {                      \
      return ::testing::PrintToString(static_cast<const void*>(value)); \
    }                                                                   \
  }

GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(char);
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(const char);
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(wchar_t);
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(const wchar_t);
#ifdef __cpp_lib_char8_t
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(char8_t);
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(const char8_t);
#endif
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(char16_t);
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(const char16_t);
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(char32_t);
GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_(const char32_t);

#undef GTEST_IMPL_FORMAT_C_STRING_AS_POINTER_

#define GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(CharType, OtherStringType) \
  template <>                                                            \
  class FormatForComparison<CharType*, OtherStringType> {                \
   public:                                                               \
    static ::std::string Format(CharType* value) {                       \
      return ::testing::PrintToString(value);                            \
    }                                                                    \
  }

GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(char, ::std::string);
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(const char, ::std::string);
#ifdef __cpp_lib_char8_t
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(char8_t, ::std::u8string);
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(const char8_t, ::std::u8string);
#endif
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(char16_t, ::std::u16string);
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(const char16_t, ::std::u16string);
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(char32_t, ::std::u32string);
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(const char32_t, ::std::u32string);

#if GTEST_HAS_STD_WSTRING
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(wchar_t, ::std::wstring);
GTEST_IMPL_FORMAT_C_STRING_AS_STRING_(const wchar_t, ::std::wstring);
#endif

#undef GTEST_IMPL_FORMAT_C_STRING_AS_STRING_

template <typename T1, typename T2>
std::string FormatForComparisonFailureMessage(const T1& value,
                                              const T2&                    ) {
  return FormatForComparison<T1, T2>::Format(value);
}

template <typename T>
class UniversalPrinter;

template <typename T>
void PrintTo(const T& value, ::std::ostream* os) {
  internal::PrintWithFallback(value, os);
}

GTEST_API_ void PrintTo(unsigned char c, ::std::ostream* os);
GTEST_API_ void PrintTo(signed char c, ::std::ostream* os);
inline void PrintTo(char c, ::std::ostream* os) {

  PrintTo(static_cast<unsigned char>(c), os);
}

inline void PrintTo(bool x, ::std::ostream* os) {
  *os << (x ? "true" : "false");
}

GTEST_API_ void PrintTo(wchar_t wc, ::std::ostream* os);

GTEST_API_ void PrintTo(char32_t c, ::std::ostream* os);
inline void PrintTo(char16_t c, ::std::ostream* os) {
  PrintTo(ImplicitCast_<char32_t>(c), os);
}
#ifdef __cpp_lib_char8_t
inline void PrintTo(char8_t c, ::std::ostream* os) {
  PrintTo(ImplicitCast_<char32_t>(c), os);
}
#endif

#if defined(__SIZEOF_INT128__)
GTEST_API_ void PrintTo(__uint128_t v, ::std::ostream* os);
GTEST_API_ void PrintTo(__int128_t v, ::std::ostream* os);
#endif

template <typename FloatType>
int AppropriateResolution(FloatType val) {
  int full = std::numeric_limits<FloatType>::max_digits10;
  if (val < 0) val = -val;

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
  if (val < 1000000) {
    FloatType mulfor6 = 1e10;

    if (val >= static_cast<FloatType>(100000.0)) {
      mulfor6 = 1.0;
    } else if (val >= static_cast<FloatType>(10000.0)) {
      mulfor6 = 1e1;
    } else if (val >= static_cast<FloatType>(1000.0)) {
      mulfor6 = 1e2;
    } else if (val >= static_cast<FloatType>(100.0)) {
      mulfor6 = 1e3;
    } else if (val >= static_cast<FloatType>(10.0)) {
      mulfor6 = 1e4;
    } else if (val >= static_cast<FloatType>(1.0)) {
      mulfor6 = 1e5;
    } else if (val >= static_cast<FloatType>(0.1)) {
      mulfor6 = 1e6;
    } else if (val >= static_cast<FloatType>(0.01)) {
      mulfor6 = 1e7;
    } else if (val >= static_cast<FloatType>(0.001)) {
      mulfor6 = 1e8;
    } else if (val >= static_cast<FloatType>(0.0001)) {
      mulfor6 = 1e9;
    }
    if (static_cast<FloatType>(static_cast<int32_t>(
            val * mulfor6 + (static_cast<FloatType>(0.5)))) /
            mulfor6 ==
        val)
      return 6;
  } else if (val < static_cast<FloatType>(1e10)) {
    FloatType divfor6 = static_cast<FloatType>(1.0);
    if (val >= static_cast<FloatType>(1e9)) {
      divfor6 = 10000;
    } else if (val >=
               static_cast<FloatType>(1e8)) {
      divfor6 = 1000;
    } else if (val >=
               static_cast<FloatType>(1e7)) {
      divfor6 = 100;
    } else if (val >= static_cast<FloatType>(1e6)) {
      divfor6 = 10;
    }
    if (static_cast<FloatType>(static_cast<int32_t>(
            val / divfor6 + (static_cast<FloatType>(0.5)))) *
            divfor6 ==
        val)
      return 6;
  }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
  return full;
}

inline void PrintTo(float f, ::std::ostream* os) {
  auto old_precision = os->precision();
  os->precision(AppropriateResolution(f));
  *os << f;
  os->precision(old_precision);
}

inline void PrintTo(double d, ::std::ostream* os) {
  auto old_precision = os->precision();
  os->precision(AppropriateResolution(d));
  *os << d;
  os->precision(old_precision);
}

GTEST_API_ void PrintTo(const char* s, ::std::ostream* os);
inline void PrintTo(char* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const char*>(s), os);
}

inline void PrintTo(const signed char* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const void*>(s), os);
}
inline void PrintTo(signed char* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const void*>(s), os);
}
inline void PrintTo(const unsigned char* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const void*>(s), os);
}
inline void PrintTo(unsigned char* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const void*>(s), os);
}
#ifdef __cpp_lib_char8_t

GTEST_API_ void PrintTo(const char8_t* s, ::std::ostream* os);
inline void PrintTo(char8_t* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const char8_t*>(s), os);
}
#endif

GTEST_API_ void PrintTo(const char16_t* s, ::std::ostream* os);
inline void PrintTo(char16_t* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const char16_t*>(s), os);
}

GTEST_API_ void PrintTo(const char32_t* s, ::std::ostream* os);
inline void PrintTo(char32_t* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const char32_t*>(s), os);
}

#if !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED)

GTEST_API_ void PrintTo(const wchar_t* s, ::std::ostream* os);
inline void PrintTo(wchar_t* s, ::std::ostream* os) {
  PrintTo(ImplicitCast_<const wchar_t*>(s), os);
}
#endif

template <typename T>
void PrintRawArrayTo(const T a[], size_t count, ::std::ostream* os) {
  UniversalPrint(a[0], os);
  for (size_t i = 1; i != count; i++) {
    *os << ", ";
    UniversalPrint(a[i], os);
  }
}

GTEST_API_ void PrintStringTo(const ::std::string& s, ::std::ostream* os);
inline void PrintTo(const ::std::string& s, ::std::ostream* os) {
  PrintStringTo(s, os);
}

#ifdef __cpp_lib_char8_t
GTEST_API_ void PrintU8StringTo(const ::std::u8string& s, ::std::ostream* os);
inline void PrintTo(const ::std::u8string& s, ::std::ostream* os) {
  PrintU8StringTo(s, os);
}
#endif

GTEST_API_ void PrintU16StringTo(const ::std::u16string& s, ::std::ostream* os);
inline void PrintTo(const ::std::u16string& s, ::std::ostream* os) {
  PrintU16StringTo(s, os);
}

GTEST_API_ void PrintU32StringTo(const ::std::u32string& s, ::std::ostream* os);
inline void PrintTo(const ::std::u32string& s, ::std::ostream* os) {
  PrintU32StringTo(s, os);
}

#if GTEST_HAS_STD_WSTRING
GTEST_API_ void PrintWideStringTo(const ::std::wstring& s, ::std::ostream* os);
inline void PrintTo(const ::std::wstring& s, ::std::ostream* os) {
  PrintWideStringTo(s, os);
}
#endif

#if GTEST_INTERNAL_HAS_STRING_VIEW

inline void PrintTo(internal::StringView sp, ::std::ostream* os) {
  PrintTo(::std::string(sp), os);
}
#endif

inline void PrintTo(std::nullptr_t, ::std::ostream* os) { *os << "(nullptr)"; }

#if GTEST_HAS_RTTI
inline void PrintTo(const std::type_info& info, std::ostream* os) {
  *os << internal::GetTypeName(info);
}
#endif

template <typename T>
void PrintTo(std::reference_wrapper<T> ref, ::std::ostream* os) {
  UniversalPrinter<T&>::Print(ref.get(), os);
}

inline const void* VoidifyPointer(const void* p) { return p; }
inline const void* VoidifyPointer(volatile const void* p) {
  return const_cast<const void*>(p);
}

template <typename T, typename Ptr>
void PrintSmartPointer(const Ptr& ptr, std::ostream* os, char) {
  if (ptr == nullptr) {
    *os << "(nullptr)";
  } else {

    *os << "(" << (VoidifyPointer)(ptr.get()) << ")";
  }
}
template <typename T, typename Ptr,
          typename = typename std::enable_if<!std::is_void<T>::value &&
                                             !std::is_array<T>::value>::type>
void PrintSmartPointer(const Ptr& ptr, std::ostream* os, int) {
  if (ptr == nullptr) {
    *os << "(nullptr)";
  } else {
    *os << "(ptr = " << (VoidifyPointer)(ptr.get()) << ", value = ";
    UniversalPrinter<T>::Print(*ptr, os);
    *os << ")";
  }
}

template <typename T, typename D>
void PrintTo(const std::unique_ptr<T, D>& ptr, std::ostream* os) {
  (PrintSmartPointer<T>)(ptr, os, 0);
}

template <typename T>
void PrintTo(const std::shared_ptr<T>& ptr, std::ostream* os) {
  (PrintSmartPointer<T>)(ptr, os, 0);
}

#if GTEST_INTERNAL_HAS_COMPARE_LIB
template <typename T>
void PrintOrderingHelper(T ordering, std::ostream* os) {
  if (ordering == T::less) {
    *os << "(less)";
  } else if (ordering == T::greater) {
    *os << "(greater)";
  } else if (ordering == T::equivalent) {
    *os << "(equivalent)";
  } else {
    *os << "(unknown ordering)";
  }
}

inline void PrintTo(std::strong_ordering ordering, std::ostream* os) {
  if (ordering == std::strong_ordering::equal) {
    *os << "(equal)";
  } else {
    PrintOrderingHelper(ordering, os);
  }
}

inline void PrintTo(std::partial_ordering ordering, std::ostream* os) {
  if (ordering == std::partial_ordering::unordered) {
    *os << "(unordered)";
  } else {
    PrintOrderingHelper(ordering, os);
  }
}

inline void PrintTo(std::weak_ordering ordering, std::ostream* os) {
  PrintOrderingHelper(ordering, os);
}
#endif

template <typename T>
void PrintTupleTo(const T&, std::integral_constant<size_t, 0>,
                  ::std::ostream*) {}

template <typename T, size_t I>
void PrintTupleTo(const T& t, std::integral_constant<size_t, I>,
                  ::std::ostream* os) {
  PrintTupleTo(t, std::integral_constant<size_t, I - 1>(), os);
  GTEST_INTENTIONAL_CONST_COND_PUSH_()
  if (I > 1) {
    GTEST_INTENTIONAL_CONST_COND_POP_()
    *os << ", ";
  }
  UniversalPrinter<typename std::tuple_element<I - 1, T>::type>::Print(
      std::get<I - 1>(t), os);
}

template <typename... Types>
void PrintTo(const ::std::tuple<Types...>& t, ::std::ostream* os) {
  *os << "(";
  PrintTupleTo(t, std::integral_constant<size_t, sizeof...(Types)>(), os);
  *os << ")";
}

template <typename T1, typename T2>
void PrintTo(const ::std::pair<T1, T2>& value, ::std::ostream* os) {
  *os << '(';

  UniversalPrinter<T1>::Print(value.first, os);
  *os << ", ";
  UniversalPrinter<T2>::Print(value.second, os);
  *os << ')';
}

template <typename T>
class UniversalPrinter {
 public:

  GTEST_DISABLE_MSC_WARNINGS_PUSH_(4180)

  static void Print(const T& value, ::std::ostream* os) {

    PrintTo(value, os);
  }

  GTEST_DISABLE_MSC_WARNINGS_POP_()
};

template <typename T>
class UniversalPrinter<const T> : public UniversalPrinter<T> {};

#if GTEST_INTERNAL_HAS_ANY

template <>
class UniversalPrinter<Any> {
 public:
  static void Print(const Any& value, ::std::ostream* os) {
    if (value.has_value()) {
      *os << "value of type " << GetTypeName(value);
    } else {
      *os << "no value";
    }
  }

 private:
  static std::string GetTypeName(const Any& value) {
#if GTEST_HAS_RTTI
    return internal::GetTypeName(value.type());
#else
    static_cast<void>(value);
    return "<unknown_type>";
#endif
  }
};

#endif

#if GTEST_INTERNAL_HAS_OPTIONAL

template <typename T>
class UniversalPrinter<Optional<T>> {
 public:
  static void Print(const Optional<T>& value, ::std::ostream* os) {
    *os << '(';
    if (!value) {
      *os << "nullopt";
    } else {
      UniversalPrint(*value, os);
    }
    *os << ')';
  }
};

template <>
class UniversalPrinter<decltype(Nullopt())> {
 public:
  static void Print(decltype(Nullopt()), ::std::ostream* os) {
    *os << "(nullopt)";
  }
};

#endif

#if GTEST_INTERNAL_HAS_VARIANT

template <typename... T>
class UniversalPrinter<Variant<T...>> {
 public:
  static void Print(const Variant<T...>& value, ::std::ostream* os) {
    *os << '(';
#ifdef GTEST_HAS_ABSL
    absl::visit(Visitor{os, value.index()}, value);
#else
    std::visit(Visitor{os, value.index()}, value);
#endif
    *os << ')';
  }

 private:
  struct Visitor {
    template <typename U>
    void operator()(const U& u) const {
      *os << "'" << GetTypeName<U>() << "(index = " << index
          << ")' with value ";
      UniversalPrint(u, os);
    }
    ::std::ostream* os;
    std::size_t index;
  };
};

#endif

template <typename T>
void UniversalPrintArray(const T* begin, size_t len, ::std::ostream* os) {
  if (len == 0) {
    *os << "{}";
  } else {
    *os << "{ ";
    const size_t kThreshold = 18;
    const size_t kChunkSize = 8;

    if (len <= kThreshold) {
      PrintRawArrayTo(begin, len, os);
    } else {
      PrintRawArrayTo(begin, kChunkSize, os);
      *os << ", ..., ";
      PrintRawArrayTo(begin + len - kChunkSize, kChunkSize, os);
    }
    *os << " }";
  }
}

GTEST_API_ void UniversalPrintArray(const char* begin, size_t len,
                                    ::std::ostream* os);

#ifdef __cpp_lib_char8_t

GTEST_API_ void UniversalPrintArray(const char8_t* begin, size_t len,
                                    ::std::ostream* os);
#endif

GTEST_API_ void UniversalPrintArray(const char16_t* begin, size_t len,
                                    ::std::ostream* os);

GTEST_API_ void UniversalPrintArray(const char32_t* begin, size_t len,
                                    ::std::ostream* os);

GTEST_API_ void UniversalPrintArray(const wchar_t* begin, size_t len,
                                    ::std::ostream* os);

template <typename T, size_t N>
class UniversalPrinter<T[N]> {
 public:

  static void Print(const T (&a)[N], ::std::ostream* os) {
    UniversalPrintArray(a, N, os);
  }
};

template <typename T>
class UniversalPrinter<T&> {
 public:

  GTEST_DISABLE_MSC_WARNINGS_PUSH_(4180)

  static void Print(const T& value, ::std::ostream* os) {

    *os << "@" << reinterpret_cast<const void*>(&value) << " ";

    UniversalPrint(value, os);
  }

  GTEST_DISABLE_MSC_WARNINGS_POP_()
};

template <typename T>
class UniversalTersePrinter {
 public:
  static void Print(const T& value, ::std::ostream* os) {
    UniversalPrint(value, os);
  }
};
template <typename T>
class UniversalTersePrinter<T&> {
 public:
  static void Print(const T& value, ::std::ostream* os) {
    UniversalPrint(value, os);
  }
};
template <typename T>
class UniversalTersePrinter<std::reference_wrapper<T>> {
 public:
  static void Print(std::reference_wrapper<T> value, ::std::ostream* os) {
    UniversalTersePrinter<T>::Print(value.get(), os);
  }
};
template <typename T, size_t N>
class UniversalTersePrinter<T[N]> {
 public:
  static void Print(const T (&value)[N], ::std::ostream* os) {
    UniversalPrinter<T[N]>::Print(value, os);
  }
};
template <>
class UniversalTersePrinter<const char*> {
 public:
  static void Print(const char* str, ::std::ostream* os) {
    if (str == nullptr) {
      *os << "NULL";
    } else {
      UniversalPrint(std::string(str), os);
    }
  }
};
template <>
class UniversalTersePrinter<char*> : public UniversalTersePrinter<const char*> {
};

#ifdef __cpp_lib_char8_t
template <>
class UniversalTersePrinter<const char8_t*> {
 public:
  static void Print(const char8_t* str, ::std::ostream* os) {
    if (str == nullptr) {
      *os << "NULL";
    } else {
      UniversalPrint(::std::u8string(str), os);
    }
  }
};
template <>
class UniversalTersePrinter<char8_t*>
    : public UniversalTersePrinter<const char8_t*> {};
#endif

template <>
class UniversalTersePrinter<const char16_t*> {
 public:
  static void Print(const char16_t* str, ::std::ostream* os) {
    if (str == nullptr) {
      *os << "NULL";
    } else {
      UniversalPrint(::std::u16string(str), os);
    }
  }
};
template <>
class UniversalTersePrinter<char16_t*>
    : public UniversalTersePrinter<const char16_t*> {};

template <>
class UniversalTersePrinter<const char32_t*> {
 public:
  static void Print(const char32_t* str, ::std::ostream* os) {
    if (str == nullptr) {
      *os << "NULL";
    } else {
      UniversalPrint(::std::u32string(str), os);
    }
  }
};
template <>
class UniversalTersePrinter<char32_t*>
    : public UniversalTersePrinter<const char32_t*> {};

#if GTEST_HAS_STD_WSTRING
template <>
class UniversalTersePrinter<const wchar_t*> {
 public:
  static void Print(const wchar_t* str, ::std::ostream* os) {
    if (str == nullptr) {
      *os << "NULL";
    } else {
      UniversalPrint(::std::wstring(str), os);
    }
  }
};
#endif

template <>
class UniversalTersePrinter<wchar_t*> {
 public:
  static void Print(wchar_t* str, ::std::ostream* os) {
    UniversalTersePrinter<const wchar_t*>::Print(str, os);
  }
};

template <typename T>
void UniversalTersePrint(const T& value, ::std::ostream* os) {
  UniversalTersePrinter<T>::Print(value, os);
}

template <typename T>
void UniversalPrint(const T& value, ::std::ostream* os) {

  typedef T T1;
  UniversalPrinter<T1>::Print(value, os);
}

typedef ::std::vector<::std::string> Strings;

template <typename Tuple>
void TersePrintPrefixToStrings(const Tuple&, std::integral_constant<size_t, 0>,
                               Strings*) {}
template <typename Tuple, size_t I>
void TersePrintPrefixToStrings(const Tuple& t,
                               std::integral_constant<size_t, I>,
                               Strings* strings) {
  TersePrintPrefixToStrings(t, std::integral_constant<size_t, I - 1>(),
                            strings);
  ::std::stringstream ss;
  UniversalTersePrint(std::get<I - 1>(t), &ss);
  strings->push_back(ss.str());
}

template <typename Tuple>
Strings UniversalTersePrintTupleFieldsToStrings(const Tuple& value) {
  Strings result;
  TersePrintPrefixToStrings(
      value, std::integral_constant<size_t, std::tuple_size<Tuple>::value>(),
      &result);
  return result;
}

}

template <typename T>
::std::string PrintToString(const T& value) {
  ::std::stringstream ss;
  internal::UniversalTersePrinter<T>::Print(value, &ss);
  return ss.str();
}

}

#include "gtest/internal/custom/gtest-printers.h"

#endif
