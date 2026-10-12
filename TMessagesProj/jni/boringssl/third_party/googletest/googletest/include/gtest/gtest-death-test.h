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
// This header file defines the public API for death tests.  It is
// #included by gtest.h so a user doesn't need to include this
// directly.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_DEATH_TEST_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_DEATH_TEST_H_

#include "gtest/internal/gtest-death-test-internal.h"

GTEST_DECLARE_string_(death_test_style);

namespace testing {

#ifdef GTEST_HAS_DEATH_TEST

namespace internal {

GTEST_API_ bool InDeathTestChild();

}

#define ASSERT_EXIT(statement, predicate, matcher) \
  GTEST_DEATH_TEST_(statement, predicate, matcher, GTEST_FATAL_FAILURE_)

#define EXPECT_EXIT(statement, predicate, matcher) \
  GTEST_DEATH_TEST_(statement, predicate, matcher, GTEST_NONFATAL_FAILURE_)

#define ASSERT_DEATH(statement, matcher) \
  ASSERT_EXIT(statement, ::testing::internal::ExitedUnsuccessfully, matcher)

#define EXPECT_DEATH(statement, matcher) \
  EXPECT_EXIT(statement, ::testing::internal::ExitedUnsuccessfully, matcher)

class GTEST_API_ ExitedWithCode {
 public:
  explicit ExitedWithCode(int exit_code);
  ExitedWithCode(const ExitedWithCode&) = default;
  void operator=(const ExitedWithCode& other) = delete;
  bool operator()(int exit_status) const;

 private:
  const int exit_code_;
};

#if !defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_FUCHSIA)

class GTEST_API_ KilledBySignal {
 public:
  explicit KilledBySignal(int signum);
  bool operator()(int exit_status) const;

 private:
  const int signum_;
};
#endif

#ifdef NDEBUG

#define EXPECT_DEBUG_DEATH(statement, regex) \
  GTEST_EXECUTE_STATEMENT_(statement, regex)

#define ASSERT_DEBUG_DEATH(statement, regex) \
  GTEST_EXECUTE_STATEMENT_(statement, regex)

#else

#define EXPECT_DEBUG_DEATH(statement, regex) EXPECT_DEATH(statement, regex)

#define ASSERT_DEBUG_DEATH(statement, regex) ASSERT_DEATH(statement, regex)

#endif
#endif

#define GTEST_UNSUPPORTED_DEATH_TEST(statement, regex_or_matcher, terminator)  \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                                \
  if (::testing::internal::AlwaysTrue()) {                                     \
    GTEST_LOG_(WARNING) << "Death tests are not supported on this platform.\n" \
                        << "Statement '" #statement "' cannot be verified.";   \
  } else if (::testing::internal::AlwaysFalse()) {                             \
    ::testing::internal::MakeDeathTestMatcher(regex_or_matcher);               \
    GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);                 \
    terminator;                                                                \
  } else                                                                       \
    ::testing::Message()

#ifdef GTEST_HAS_DEATH_TEST
#define EXPECT_DEATH_IF_SUPPORTED(statement, regex) \
  EXPECT_DEATH(statement, regex)
#define ASSERT_DEATH_IF_SUPPORTED(statement, regex) \
  ASSERT_DEATH(statement, regex)
#else
#define EXPECT_DEATH_IF_SUPPORTED(statement, regex) \
  GTEST_UNSUPPORTED_DEATH_TEST(statement, regex, )
#define ASSERT_DEATH_IF_SUPPORTED(statement, regex) \
  GTEST_UNSUPPORTED_DEATH_TEST(statement, regex, return)
#endif

}

#endif
