// Copyright 2010, Google Inc.
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

//
// Tests that verify interaction of exceptions and death tests.

#include "gtest/gtest-death-test.h"
#include "gtest/gtest.h"

#ifdef GTEST_HAS_DEATH_TEST

#if GTEST_HAS_SEH
#include <windows.h>
#endif

#include "gtest/gtest-spi.h"

#if GTEST_HAS_EXCEPTIONS

#include <exception>

TEST(CxxExceptionDeathTest, ExceptionIsFailure) {
  try {
    EXPECT_NONFATAL_FAILURE(EXPECT_DEATH(throw 1, ""), "threw an exception");
  } catch (...) {
    FAIL() << "An exception escaped a death test macro invocation "
           << "with catch_exceptions "
           << (GTEST_FLAG_GET(catch_exceptions) ? "enabled" : "disabled");
  }
}

class TestException : public std::exception {
 public:
  const char* what() const noexcept override { return "exceptional message"; }
};

TEST(CxxExceptionDeathTest, PrintsMessageForStdExceptions) {

  EXPECT_NONFATAL_FAILURE(EXPECT_DEATH(throw TestException(), ""),
                          "exceptional message");

  EXPECT_NONFATAL_FAILURE(EXPECT_DEATH(throw TestException(), ""), __FILE__);
}
#endif

#if GTEST_HAS_SEH

TEST(SehExceptionDeasTest, CatchExceptionsDoesNotInterfere) {
  EXPECT_DEATH(RaiseException(42, 0x0, 0, NULL), "")
      << "with catch_exceptions "
      << (GTEST_FLAG_GET(catch_exceptions) ? "enabled" : "disabled");
}
#endif

#endif

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  GTEST_FLAG_SET(catch_exceptions, GTEST_ENABLE_CATCH_EXCEPTIONS_ != 0);
  return RUN_ALL_TESTS();
}
