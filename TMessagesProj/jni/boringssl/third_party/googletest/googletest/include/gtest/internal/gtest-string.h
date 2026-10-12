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
// This header file declares the String class and functions used internally by
// Google Test.  They are subject to change without notice. They should not used
// by code external to Google Test.
//
// This header file is #included by gtest-internal.h.
// It should not be #included by other files.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_STRING_H_
#define GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_STRING_H_

#ifdef __BORLANDC__

#include <mem.h>
#endif

#include <string.h>

#include <cstdint>
#include <sstream>
#include <string>

#include "gtest/internal/gtest-port.h"

namespace testing {
namespace internal {

class GTEST_API_ String {
 public:

  static const char* CloneCString(const char* c_str);

#ifdef GTEST_OS_WINDOWS_MOBILE

  static LPCWSTR AnsiToUtf16(const char* c_str);

  static const char* Utf16ToAnsi(LPCWSTR utf16_str);
#endif

  static bool CStringEquals(const char* lhs, const char* rhs);

  static std::string ShowWideCString(const wchar_t* wide_c_str);

  static bool WideCStringEquals(const wchar_t* lhs, const wchar_t* rhs);

  static bool CaseInsensitiveCStringEquals(const char* lhs, const char* rhs);

  static bool CaseInsensitiveWideCStringEquals(const wchar_t* lhs,
                                               const wchar_t* rhs);

  static bool EndsWithCaseInsensitive(const std::string& str,
                                      const std::string& suffix);

  static std::string FormatIntWidth2(int value);

  static std::string FormatIntWidthN(int value, int width);

  static std::string FormatHexInt(int value);

  static std::string FormatHexUInt32(uint32_t value);

  static std::string FormatByte(unsigned char value);

 private:
  String();
};

GTEST_API_ std::string StringStreamToString(::std::stringstream* stream);

}
}

#endif
