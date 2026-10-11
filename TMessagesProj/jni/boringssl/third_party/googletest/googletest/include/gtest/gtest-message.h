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
// This header file defines the Message class.
//
// IMPORTANT NOTE: Due to limitation of the C++ language, we have to
// leave some internal implementation details in this header file.
// They are clearly marked by comments like this:
//
//   // INTERNAL IMPLEMENTATION - DO NOT USE IN A USER PROGRAM.
//
// Such code is NOT meant to be used by a user directly, and is subject
// to CHANGE WITHOUT NOTICE.  Therefore DO NOT DEPEND ON IT in a user
// program!

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_MESSAGE_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_MESSAGE_H_

#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

#include "gtest/internal/gtest-port.h"

#ifdef GTEST_HAS_ABSL
#include <type_traits>

#include "absl/strings/has_absl_stringify.h"
#include "absl/strings/str_cat.h"
#endif

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

void operator<<(const testing::internal::Secret&, int);

namespace testing {

class GTEST_API_ Message {
 private:

  typedef std::ostream& (*BasicNarrowIoManip)(std::ostream&);

 public:

  Message();

  Message(const Message& msg) : ss_(new ::std::stringstream) {
    *ss_ << msg.GetString();
  }

  explicit Message(const char* str) : ss_(new ::std::stringstream) {
    *ss_ << str;
  }

  template <
      typename T
#ifdef GTEST_HAS_ABSL
      ,
      typename std::enable_if<!absl::HasAbslStringify<T>::value,
                              int>::type = 0
#endif
      >
  inline Message& operator<<(const T& val) {

    using ::operator<<;
    *ss_ << val;
    return *this;
  }

#ifdef GTEST_HAS_ABSL

  template <typename T,
            typename std::enable_if<absl::HasAbslStringify<T>::value,
                                    int>::type = 0>
  inline Message& operator<<(const T& val) {

    using ::operator<<;
    *ss_ << absl::StrCat(val);
    return *this;
  }
#endif

  template <typename T>
  inline Message& operator<<(T* const& pointer) {
    if (pointer == nullptr) {
      *ss_ << "(null)";
    } else {
      *ss_ << pointer;
    }
    return *this;
  }

  Message& operator<<(BasicNarrowIoManip val) {
    *ss_ << val;
    return *this;
  }

  Message& operator<<(bool b) { return *this << (b ? "true" : "false"); }

  Message& operator<<(const wchar_t* wide_c_str);
  Message& operator<<(wchar_t* wide_c_str);

#if GTEST_HAS_STD_WSTRING

  Message& operator<<(const ::std::wstring& wstr);
#endif

  std::string GetString() const;

 private:

  const std::unique_ptr< ::std::stringstream> ss_;

  void operator=(const Message&);
};

inline std::ostream& operator<<(std::ostream& os, const Message& sb) {
  return os << sb.GetString();
}

namespace internal {

template <typename T>
std::string StreamableToString(const T& streamable) {
  return (Message() << streamable).GetString();
}

}
}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif
