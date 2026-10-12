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
// This file implements the AssertionResult type.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_ASSERTION_RESULT_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_ASSERTION_RESULT_H_

#include <memory>
#include <ostream>
#include <string>
#include <type_traits>

#include "gtest/gtest-message.h"
#include "gtest/internal/gtest-port.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251                                   \
                                                                          )

namespace testing {

#if !defined(SWIG)
class [[nodiscard]] AssertionResult;
#endif

class GTEST_API_ AssertionResult {
 public:

  AssertionResult(const AssertionResult& other);

#if defined(_MSC_VER) && (_MSC_VER < 1910 || _MSC_VER >= 1920)
  GTEST_DISABLE_MSC_WARNINGS_PUSH_(4800                            )
#endif

  template <typename T>
  explicit AssertionResult(
      const T& success,
      typename std::enable_if<
          !std::is_convertible<T, AssertionResult>::value>::type*

      = nullptr)
      : success_(success) {}

#if defined(_MSC_VER) && (_MSC_VER < 1910 || _MSC_VER >= 1920)
  GTEST_DISABLE_MSC_WARNINGS_POP_()
#endif

  AssertionResult& operator=(AssertionResult other) {
    swap(other);
    return *this;
  }

  operator bool() const { return success_; }

  AssertionResult operator!() const;

  const char* message() const {
    return message_ != nullptr ? message_->c_str() : "";
  }

  const char* failure_message() const { return message(); }

  template <typename T>
  AssertionResult& operator<<(const T& value) {
    AppendMessage(Message() << value);
    return *this;
  }

  AssertionResult& operator<<(
      ::std::ostream& (*basic_manipulator)(::std::ostream& stream)) {
    AppendMessage(Message() << basic_manipulator);
    return *this;
  }

 private:

  void AppendMessage(const Message& a_message) {
    if (message_ == nullptr) message_ = ::std::make_unique<::std::string>();
    message_->append(a_message.GetString().c_str());
  }

  void swap(AssertionResult& other);

  bool success_;

  std::unique_ptr< ::std::string> message_;
};

GTEST_API_ AssertionResult AssertionSuccess();

GTEST_API_ AssertionResult AssertionFailure();

GTEST_API_ AssertionResult AssertionFailure(const Message& msg);

}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif
