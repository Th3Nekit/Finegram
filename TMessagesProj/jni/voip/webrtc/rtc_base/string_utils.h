/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_STRING_UTILS_H_
#define RTC_BASE_STRING_UTILS_H_

#include <stdio.h>
#include <string.h>

#include "absl/strings/string_view.h"

#if defined(WEBRTC_WIN)
#include <malloc.h>
#include <wchar.h>
#include <windows.h>

#endif

#if defined(WEBRTC_POSIX)
#include <stdlib.h>
#include <strings.h>
#endif

#include <string>

#include "absl/strings/string_view.h"

namespace rtc {

const size_t SIZE_UNKNOWN = static_cast<size_t>(-1);

struct AbslStringViewCmp {
  using is_transparent = void;
  bool operator()(absl::string_view a, absl::string_view b) const {
    return a < b;
  }
};

size_t strcpyn(char* buffer, size_t buflen, absl::string_view source);

#if defined(WEBRTC_WIN)

inline std::wstring ToUtf16(const char* utf8, size_t len) {
  if (len == 0)
    return std::wstring();
  int len16 = ::MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len),
                                    nullptr, 0);
  std::wstring ws(len16, 0);
  ::MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len), &*ws.begin(),
                        len16);
  return ws;
}

inline std::wstring ToUtf16(absl::string_view str) {
  return ToUtf16(str.data(), str.length());
}

inline std::string ToUtf8(const wchar_t* wide, size_t len) {
  if (len == 0)
    return std::string();
  int len8 = ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(len),
                                   nullptr, 0, nullptr, nullptr);
  std::string ns(len8, 0);
  ::WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(len), &*ns.begin(),
                        len8, nullptr, nullptr);
  return ns;
}

inline std::string ToUtf8(const wchar_t* wide) {
  return ToUtf8(wide, wcslen(wide));
}

inline std::string ToUtf8(const std::wstring& wstr) {
  return ToUtf8(wstr.data(), wstr.length());
}

#endif

std::string ToHex(int i);

namespace rtc_base_string_utils_internal {
template <int NPlus1>
struct CompileTimeString {
  char string[NPlus1] = {0};
  constexpr CompileTimeString() = default;
  template <int MPlus1>
  explicit constexpr CompileTimeString(const char (&chars)[MPlus1]) {
    char* chars_pointer = string;
    for (auto c : chars)
      *chars_pointer++ = c;
  }
  template <int MPlus1>
  constexpr auto Concat(CompileTimeString<MPlus1> b) {
    CompileTimeString<NPlus1 + MPlus1 - 1> result;
    char* chars_pointer = result.string;
    for (auto c : string)
      *chars_pointer++ = c;
    chars_pointer = result.string + NPlus1 - 1;
    for (auto c : b.string)
      *chars_pointer++ = c;
    result.string[NPlus1 + MPlus1 - 2] = 0;
    return result;
  }
  constexpr operator const char*() { return string; }
};
}

template <int N>
constexpr auto MakeCompileTimeString(const char (&a)[N]) {
  return rtc_base_string_utils_internal::CompileTimeString<N>(a);
}

}

#endif
