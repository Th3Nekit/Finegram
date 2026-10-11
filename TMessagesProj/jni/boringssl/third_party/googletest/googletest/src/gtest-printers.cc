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
// It uses the << operator when possible, and prints the bytes in the
// object otherwise.  A user can override its behavior for a class
// type Foo by defining either operator<<(::std::ostream&, const Foo&)
// or void PrintTo(const Foo&, ::std::ostream*) in the namespace that
// defines Foo.

#include "gtest/gtest-printers.h"

#include <stdio.h>

#include <cctype>
#include <cstdint>
#include <cwchar>
#include <iomanip>
#include <ios>
#include <ostream>
#include <string>
#include <type_traits>

#include "gtest/internal/gtest-port.h"
#include "src/gtest-internal-inl.h"

namespace testing {

namespace {

using ::std::ostream;

GTEST_ATTRIBUTE_NO_SANITIZE_MEMORY_
GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
GTEST_ATTRIBUTE_NO_SANITIZE_THREAD_
void PrintByteSegmentInObjectTo(const unsigned char* obj_bytes, size_t start,
                                size_t count, ostream* os) {
  char text[5] = "";
  for (size_t i = 0; i != count; i++) {
    const size_t j = start + i;
    if (i != 0) {

      if ((j % 2) == 0)
        *os << ' ';
      else
        *os << '-';
    }
    GTEST_SNPRINTF_(text, sizeof(text), "%02X", obj_bytes[j]);
    *os << text;
  }
}

void PrintBytesInObjectToImpl(const unsigned char* obj_bytes, size_t count,
                              ostream* os) {

  *os << count << "-byte object <";

  const size_t kThreshold = 132;
  const size_t kChunkSize = 64;

  if (count < kThreshold) {
    PrintByteSegmentInObjectTo(obj_bytes, 0, count, os);
  } else {
    PrintByteSegmentInObjectTo(obj_bytes, 0, kChunkSize, os);
    *os << " ... ";

    const size_t resume_pos = (count - kChunkSize + 1) / 2 * 2;
    PrintByteSegmentInObjectTo(obj_bytes, resume_pos, count - resume_pos, os);
  }
  *os << ">";
}

template <typename CharType>
char32_t ToChar32(CharType in) {
  return static_cast<char32_t>(
      static_cast<typename std::make_unsigned<CharType>::type>(in));
}

}

namespace internal {

void PrintBytesInObjectTo(const unsigned char* obj_bytes, size_t count,
                          ostream* os) {
  PrintBytesInObjectToImpl(obj_bytes, count, os);
}

enum CharFormat { kAsIs, kHexEscape, kSpecialEscape };

inline bool IsPrintableAscii(char32_t c) { return 0x20 <= c && c <= 0x7E; }

template <typename Char>
static CharFormat PrintAsCharLiteralTo(Char c, ostream* os) {
  const char32_t u_c = ToChar32(c);
  switch (u_c) {
    case L'\0':
      *os << "\\0";
      break;
    case L'\'':
      *os << "\\'";
      break;
    case L'\\':
      *os << "\\\\";
      break;
    case L'\a':
      *os << "\\a";
      break;
    case L'\b':
      *os << "\\b";
      break;
    case L'\f':
      *os << "\\f";
      break;
    case L'\n':
      *os << "\\n";
      break;
    case L'\r':
      *os << "\\r";
      break;
    case L'\t':
      *os << "\\t";
      break;
    case L'\v':
      *os << "\\v";
      break;
    default:
      if (IsPrintableAscii(u_c)) {
        *os << static_cast<char>(c);
        return kAsIs;
      } else {
        ostream::fmtflags flags = os->flags();
        *os << "\\x" << std::hex << std::uppercase << static_cast<int>(u_c);
        os->flags(flags);
        return kHexEscape;
      }
  }
  return kSpecialEscape;
}

static CharFormat PrintAsStringLiteralTo(char32_t c, ostream* os) {
  switch (c) {
    case L'\'':
      *os << "'";
      return kAsIs;
    case L'"':
      *os << "\\\"";
      return kSpecialEscape;
    default:
      return PrintAsCharLiteralTo(c, os);
  }
}

static const char* GetCharWidthPrefix(char) { return ""; }

static const char* GetCharWidthPrefix(signed char) { return ""; }

static const char* GetCharWidthPrefix(unsigned char) { return ""; }

#ifdef __cpp_lib_char8_t
static const char* GetCharWidthPrefix(char8_t) { return "u8"; }
#endif

static const char* GetCharWidthPrefix(char16_t) { return "u"; }

static const char* GetCharWidthPrefix(char32_t) { return "U"; }

static const char* GetCharWidthPrefix(wchar_t) { return "L"; }

static CharFormat PrintAsStringLiteralTo(char c, ostream* os) {
  return PrintAsStringLiteralTo(ToChar32(c), os);
}

#ifdef __cpp_lib_char8_t
static CharFormat PrintAsStringLiteralTo(char8_t c, ostream* os) {
  return PrintAsStringLiteralTo(ToChar32(c), os);
}
#endif

static CharFormat PrintAsStringLiteralTo(char16_t c, ostream* os) {
  return PrintAsStringLiteralTo(ToChar32(c), os);
}

static CharFormat PrintAsStringLiteralTo(wchar_t c, ostream* os) {
  return PrintAsStringLiteralTo(ToChar32(c), os);
}

template <typename Char>
void PrintCharAndCodeTo(Char c, ostream* os) {

  *os << GetCharWidthPrefix(c) << "'";
  const CharFormat format = PrintAsCharLiteralTo(c, os);
  *os << "'";

  if (c == 0) return;
  *os << " (" << static_cast<int>(c);

  if (format == kHexEscape || (1 <= c && c <= 9)) {

  } else {
    *os << ", 0x" << String::FormatHexInt(static_cast<int>(c));
  }
  *os << ")";
}

void PrintTo(unsigned char c, ::std::ostream* os) { PrintCharAndCodeTo(c, os); }
void PrintTo(signed char c, ::std::ostream* os) { PrintCharAndCodeTo(c, os); }

void PrintTo(wchar_t wc, ostream* os) { PrintCharAndCodeTo(wc, os); }

void PrintTo(char32_t c, ::std::ostream* os) {
  *os << std::hex << "U+" << std::uppercase << std::setfill('0') << std::setw(4)
      << static_cast<uint32_t>(c);
}

#if defined(__SIZEOF_INT128__)
void PrintTo(__uint128_t v, ::std::ostream* os) {
  if (v == 0) {
    *os << "0";
    return;
  }

  char buf[40];
  char* p = buf + sizeof(buf);

  uint64_t high = static_cast<uint64_t>(v >> 64);
  uint64_t low = static_cast<uint64_t>(v);

  *--p = 0;
  while (high != 0 || low != 0) {
    uint64_t high_mod = high % 10;
    high = high / 10;

    const uint64_t carry = 6 * high_mod + low % 10;
    low = low / 10 + high_mod * 1844674407370955161 + carry / 10;

    char digit = static_cast<char>(carry % 10);
    *--p = static_cast<char>('0' + digit);
  }
  *os << p;
}
void PrintTo(__int128_t v, ::std::ostream* os) {
  __uint128_t uv = static_cast<__uint128_t>(v);
  if (v < 0) {
    *os << "-";
    uv = -uv;
  }
  PrintTo(uv, os);
}
#endif

template <typename CharType>
GTEST_ATTRIBUTE_NO_SANITIZE_MEMORY_ GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
    GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
        GTEST_ATTRIBUTE_NO_SANITIZE_THREAD_ static CharFormat
        PrintCharsAsStringTo(const CharType* begin, size_t len, ostream* os) {
  const char* const quote_prefix = GetCharWidthPrefix(*begin);
  *os << quote_prefix << "\"";
  bool is_previous_hex = false;
  CharFormat print_format = kAsIs;
  for (size_t index = 0; index < len; ++index) {
    const CharType cur = begin[index];
    if (is_previous_hex && IsXDigit(cur)) {

      *os << "\" " << quote_prefix << "\"";
    }
    is_previous_hex = PrintAsStringLiteralTo(cur, os) == kHexEscape;

    if (is_previous_hex) {
      print_format = kHexEscape;
    }
  }
  *os << "\"";
  return print_format;
}

template <typename CharType>
GTEST_ATTRIBUTE_NO_SANITIZE_MEMORY_ GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
    GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
        GTEST_ATTRIBUTE_NO_SANITIZE_THREAD_ static void
        UniversalPrintCharArray(const CharType* begin, size_t len,
                                ostream* os) {

  if (len > 0 && begin[len - 1] == '\0') {
    PrintCharsAsStringTo(begin, len - 1, os);
    return;
  }

  PrintCharsAsStringTo(begin, len, os);
  *os << " (no terminating NUL)";
}

void UniversalPrintArray(const char* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}

#ifdef __cpp_lib_char8_t

void UniversalPrintArray(const char8_t* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}
#endif

void UniversalPrintArray(const char16_t* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}

void UniversalPrintArray(const char32_t* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}

void UniversalPrintArray(const wchar_t* begin, size_t len, ostream* os) {
  UniversalPrintCharArray(begin, len, os);
}

namespace {

template <typename Char>
void PrintCStringTo(const Char* s, ostream* os) {
  if (s == nullptr) {
    *os << "NULL";
  } else {
    *os << ImplicitCast_<const void*>(s) << " pointing to ";
    PrintCharsAsStringTo(s, std::char_traits<Char>::length(s), os);
  }
}

}

void PrintTo(const char* s, ostream* os) { PrintCStringTo(s, os); }

#ifdef __cpp_lib_char8_t
void PrintTo(const char8_t* s, ostream* os) { PrintCStringTo(s, os); }
#endif

void PrintTo(const char16_t* s, ostream* os) { PrintCStringTo(s, os); }

void PrintTo(const char32_t* s, ostream* os) { PrintCStringTo(s, os); }

#if !defined(_MSC_VER) || defined(_NATIVE_WCHAR_T_DEFINED)

void PrintTo(const wchar_t* s, ostream* os) { PrintCStringTo(s, os); }
#endif

namespace {

bool ContainsUnprintableControlCodes(const char* str, size_t length) {
  const unsigned char* s = reinterpret_cast<const unsigned char*>(str);

  for (size_t i = 0; i < length; i++) {
    unsigned char ch = *s++;
    if (std::iscntrl(ch)) {
      switch (ch) {
        case '\t':
        case '\n':
        case '\r':
          break;
        default:
          return true;
      }
    }
  }
  return false;
}

bool IsUTF8TrailByte(unsigned char t) { return 0x80 <= t && t <= 0xbf; }

bool IsValidUTF8(const char* str, size_t length) {
  const unsigned char* s = reinterpret_cast<const unsigned char*>(str);

  for (size_t i = 0; i < length;) {
    unsigned char lead = s[i++];

    if (lead <= 0x7f) {
      continue;
    }
    if (lead < 0xc2) {
      return false;
    } else if (lead <= 0xdf && (i + 1) <= length && IsUTF8TrailByte(s[i])) {
      ++i;
    } else if (0xe0 <= lead && lead <= 0xef && (i + 2) <= length &&
               IsUTF8TrailByte(s[i]) && IsUTF8TrailByte(s[i + 1]) &&

               (lead != 0xe0 || s[i] >= 0xa0) &&
               (lead != 0xed || s[i] < 0xa0)) {
      i += 2;
    } else if (0xf0 <= lead && lead <= 0xf4 && (i + 3) <= length &&
               IsUTF8TrailByte(s[i]) && IsUTF8TrailByte(s[i + 1]) &&
               IsUTF8TrailByte(s[i + 2]) &&

               (lead != 0xf0 || s[i] >= 0x90) &&
               (lead != 0xf4 || s[i] < 0x90)) {
      i += 3;
    } else {
      return false;
    }
  }
  return true;
}

void ConditionalPrintAsText(const char* str, size_t length, ostream* os) {
  if (!ContainsUnprintableControlCodes(str, length) &&
      IsValidUTF8(str, length)) {
    *os << "\n    As Text: \"" << str << "\"";
  }
}

}

void PrintStringTo(const ::std::string& s, ostream* os) {
  if (PrintCharsAsStringTo(s.data(), s.size(), os) == kHexEscape) {
    if (GTEST_FLAG_GET(print_utf8)) {
      ConditionalPrintAsText(s.data(), s.size(), os);
    }
  }
}

#ifdef __cpp_lib_char8_t
void PrintU8StringTo(const ::std::u8string& s, ostream* os) {
  PrintCharsAsStringTo(s.data(), s.size(), os);
}
#endif

void PrintU16StringTo(const ::std::u16string& s, ostream* os) {
  PrintCharsAsStringTo(s.data(), s.size(), os);
}

void PrintU32StringTo(const ::std::u32string& s, ostream* os) {
  PrintCharsAsStringTo(s.data(), s.size(), os);
}

#if GTEST_HAS_STD_WSTRING
void PrintWideStringTo(const ::std::wstring& s, ostream* os) {
  PrintCharsAsStringTo(s.data(), s.size(), os);
}
#endif

}

}
