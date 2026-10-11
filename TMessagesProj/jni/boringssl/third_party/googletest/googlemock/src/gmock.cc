// Copyright 2008, Google Inc.
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

#include "gmock/gmock.h"

#include <string>

#include "gmock/internal/gmock-port.h"

GMOCK_DEFINE_bool_(catch_leaked_mocks, true,
                   "true if and only if Google Mock should report leaked "
                   "mock objects as failures.");

GMOCK_DEFINE_string_(verbose, testing::internal::kWarningVerbosity,
                     "Controls how verbose Google Mock's output is."
                     "  Valid values:\n"
                     "  info    - prints all messages.\n"
                     "  warning - prints warnings and errors.\n"
                     "  error   - prints errors only.");

GMOCK_DEFINE_int32_(default_mock_behavior, 1,
                    "Controls the default behavior of mocks."
                    "  Valid values:\n"
                    "  0 - by default, mocks act as NiceMocks.\n"
                    "  1 - by default, mocks act as NaggyMocks.\n"
                    "  2 - by default, mocks act as StrictMocks.");

namespace testing {
namespace internal {

static const char* ParseGoogleMockFlagValue(const char* str,
                                            const char* flag_name,
                                            bool def_optional) {

  if (str == nullptr || flag_name == nullptr) return nullptr;

  const std::string flag_name_str = std::string("--gmock_") + flag_name;
  const size_t flag_name_len = flag_name_str.length();
  if (strncmp(str, flag_name_str.c_str(), flag_name_len) != 0) return nullptr;

  const char* flag_end = str + flag_name_len;

  if (def_optional && (flag_end[0] == '\0')) {
    return flag_end;
  }

  if (flag_end[0] != '=') return nullptr;

  return flag_end + 1;
}

static bool ParseGoogleMockFlag(const char* str, const char* flag_name,
                                bool* value) {

  const char* const value_str = ParseGoogleMockFlagValue(str, flag_name, true);

  if (value_str == nullptr) return false;

  *value = !(*value_str == '0' || *value_str == 'f' || *value_str == 'F');
  return true;
}

template <typename String>
static bool ParseGoogleMockFlag(const char* str, const char* flag_name,
                                String* value) {

  const char* const value_str = ParseGoogleMockFlagValue(str, flag_name, false);

  if (value_str == nullptr) return false;

  *value = value_str;
  return true;
}

static bool ParseGoogleMockFlag(const char* str, const char* flag_name,
                                int32_t* value) {

  const char* const value_str = ParseGoogleMockFlagValue(str, flag_name, true);

  if (value_str == nullptr) return false;

  return ParseInt32(Message() << "The value of flag --" << flag_name, value_str,
                    value);
}

template <typename CharType>
void InitGoogleMockImpl(int* argc, CharType** argv) {

  InitGoogleTest(argc, argv);
  if (*argc <= 0) return;

  for (int i = 1; i != *argc; i++) {
    const std::string arg_string = StreamableToString(argv[i]);
    const char* const arg = arg_string.c_str();

    bool found_gmock_flag = false;

#define GMOCK_INTERNAL_PARSE_FLAG(flag_name)            \
  if (!found_gmock_flag) {                              \
    auto value = GMOCK_FLAG_GET(flag_name);             \
    if (ParseGoogleMockFlag(arg, #flag_name, &value)) { \
      GMOCK_FLAG_SET(flag_name, value);                 \
      found_gmock_flag = true;                          \
    }                                                   \
  }

    GMOCK_INTERNAL_PARSE_FLAG(catch_leaked_mocks)
    GMOCK_INTERNAL_PARSE_FLAG(verbose)
    GMOCK_INTERNAL_PARSE_FLAG(default_mock_behavior)

    if (found_gmock_flag) {

      for (int j = i; j != *argc; j++) {
        argv[j] = argv[j + 1];
      }

      (*argc)--;

      i--;
    }
  }
}

}

GTEST_API_ void InitGoogleMock(int* argc, char** argv) {
  internal::InitGoogleMockImpl(argc, argv);
}

GTEST_API_ void InitGoogleMock(int* argc, wchar_t** argv) {
  internal::InitGoogleMockImpl(argc, argv);
}

GTEST_API_ void InitGoogleMock() {

  int argc = 1;
  const auto arg0 = "dummy";
  char* argv0 = const_cast<char*>(arg0);
  char** argv = &argv0;

  internal::InitGoogleMockImpl(&argc, argv);
}

}
