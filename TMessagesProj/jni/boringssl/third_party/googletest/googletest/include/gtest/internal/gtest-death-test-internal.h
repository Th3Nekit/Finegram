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
// This header file defines internal utilities needed for implementing
// death tests.  They are subject to change without notice.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_DEATH_TEST_INTERNAL_H_
#define GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_DEATH_TEST_INTERNAL_H_

#include <stdio.h>

#include <memory>
#include <string>

#include "gtest/gtest-matchers.h"
#include "gtest/internal/gtest-internal.h"
#include "gtest/internal/gtest-port.h"

GTEST_DECLARE_string_(internal_run_death_test);

namespace testing {
namespace internal {

const char kInternalRunDeathTestFlag[] = "internal_run_death_test";

inline Matcher<const ::std::string&> MakeDeathTestMatcher(
    ::testing::internal::RE regex) {
  return ContainsRegex(regex.pattern());
}
inline Matcher<const ::std::string&> MakeDeathTestMatcher(const char* regex) {
  return ContainsRegex(regex);
}
inline Matcher<const ::std::string&> MakeDeathTestMatcher(
    const ::std::string& regex) {
  return ContainsRegex(regex);
}

inline Matcher<const ::std::string&> MakeDeathTestMatcher(
    Matcher<const ::std::string&> matcher) {
  return matcher;
}

#ifdef GTEST_HAS_DEATH_TEST

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

class GTEST_API_ DeathTest {
 public:

  static bool Create(const char* statement, Matcher<const std::string&> matcher,
                     const char* file, int line, DeathTest** test);
  DeathTest();
  virtual ~DeathTest() = default;

  class ReturnSentinel {
   public:
    explicit ReturnSentinel(DeathTest* test) : test_(test) {}
    ~ReturnSentinel() { test_->Abort(TEST_ENCOUNTERED_RETURN_STATEMENT); }

   private:
    DeathTest* const test_;
    ReturnSentinel(const ReturnSentinel&) = delete;
    ReturnSentinel& operator=(const ReturnSentinel&) = delete;
  };

  enum TestRole { OVERSEE_TEST, EXECUTE_TEST };

  enum AbortReason {
    TEST_ENCOUNTERED_RETURN_STATEMENT,
    TEST_THREW_EXCEPTION,
    TEST_DID_NOT_DIE
  };

  virtual TestRole AssumeRole() = 0;

  virtual int Wait() = 0;

  virtual bool Passed(bool exit_status_ok) = 0;

  virtual void Abort(AbortReason reason) = 0;

  static const char* LastMessage();

  static void set_last_death_test_message(const std::string& message);

 private:

  static std::string last_death_test_message_;

  DeathTest(const DeathTest&) = delete;
  DeathTest& operator=(const DeathTest&) = delete;
};

GTEST_DISABLE_MSC_WARNINGS_POP_()

class DeathTestFactory {
 public:
  virtual ~DeathTestFactory() = default;
  virtual bool Create(const char* statement,
                      Matcher<const std::string&> matcher, const char* file,
                      int line, DeathTest** test) = 0;
};

class DefaultDeathTestFactory : public DeathTestFactory {
 public:
  bool Create(const char* statement, Matcher<const std::string&> matcher,
              const char* file, int line, DeathTest** test) override;
};

GTEST_API_ bool ExitedUnsuccessfully(int exit_status);

#if GTEST_HAS_EXCEPTIONS
#define GTEST_EXECUTE_DEATH_TEST_STATEMENT_(statement, death_test)           \
  try {                                                                      \
    GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);               \
  } catch (const ::std::exception& gtest_exception) {                        \
    fprintf(                                                                 \
        stderr,                                                              \
        "\n%s: Caught std::exception-derived exception escaping the "        \
        "death test statement. Exception message: %s\n",                     \
        ::testing::internal::FormatFileLocation(__FILE__, __LINE__).c_str(), \
        gtest_exception.what());                                             \
    fflush(stderr);                                                          \
    death_test->Abort(::testing::internal::DeathTest::TEST_THREW_EXCEPTION); \
  } catch (...) {                                                            \
    death_test->Abort(::testing::internal::DeathTest::TEST_THREW_EXCEPTION); \
  }

#else
#define GTEST_EXECUTE_DEATH_TEST_STATEMENT_(statement, death_test) \
  GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement)

#endif

#define GTEST_DEATH_TEST_(statement, predicate, regex_or_matcher, fail)        \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                                \
  if (::testing::internal::AlwaysTrue()) {                                     \
    ::testing::internal::DeathTest* gtest_dt;                                  \
    if (!::testing::internal::DeathTest::Create(                               \
            #statement,                                                        \
            ::testing::internal::MakeDeathTestMatcher(regex_or_matcher),       \
            __FILE__, __LINE__, &gtest_dt)) {                                  \
      goto GTEST_CONCAT_TOKEN_(gtest_label_, __LINE__);                        \
    }                                                                          \
    if (gtest_dt != nullptr) {                                                 \
      std::unique_ptr< ::testing::internal::DeathTest> gtest_dt_ptr(gtest_dt); \
      switch (gtest_dt->AssumeRole()) {                                        \
        case ::testing::internal::DeathTest::OVERSEE_TEST:                     \
          if (!gtest_dt->Passed(predicate(gtest_dt->Wait()))) {                \
            goto GTEST_CONCAT_TOKEN_(gtest_label_, __LINE__);                  \
          }                                                                    \
          break;                                                               \
        case ::testing::internal::DeathTest::EXECUTE_TEST: {                   \
          const ::testing::internal::DeathTest::ReturnSentinel gtest_sentinel( \
              gtest_dt);                                                       \
          GTEST_EXECUTE_DEATH_TEST_STATEMENT_(statement, gtest_dt);            \
          gtest_dt->Abort(::testing::internal::DeathTest::TEST_DID_NOT_DIE);   \
          break;                                                               \
        }                                                                      \
      }                                                                        \
    }                                                                          \
  } else                                                                       \
    GTEST_CONCAT_TOKEN_(gtest_label_, __LINE__)                                \
        : fail(::testing::internal::DeathTest::LastMessage())

#define GTEST_EXECUTE_STATEMENT_(statement, regex_or_matcher)    \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_                                  \
  if (::testing::internal::AlwaysTrue()) {                       \
    GTEST_SUPPRESS_UNREACHABLE_CODE_WARNING_BELOW_(statement);   \
  } else if (!::testing::internal::AlwaysTrue()) {               \
    ::testing::internal::MakeDeathTestMatcher(regex_or_matcher); \
  } else                                                         \
    ::testing::Message()

class InternalRunDeathTestFlag {
 public:
  InternalRunDeathTestFlag(const std::string& a_file, int a_line, int an_index,
                           int a_write_fd)
      : file_(a_file), line_(a_line), index_(an_index), write_fd_(a_write_fd) {}

  ~InternalRunDeathTestFlag() {
    if (write_fd_ >= 0) posix::Close(write_fd_);
  }

  const std::string& file() const { return file_; }
  int line() const { return line_; }
  int index() const { return index_; }
  int write_fd() const { return write_fd_; }

 private:
  std::string file_;
  int line_;
  int index_;
  int write_fd_;

  InternalRunDeathTestFlag(const InternalRunDeathTestFlag&) = delete;
  InternalRunDeathTestFlag& operator=(const InternalRunDeathTestFlag&) = delete;
};

InternalRunDeathTestFlag* ParseInternalRunDeathTestFlag();

#endif

}
}

#endif
