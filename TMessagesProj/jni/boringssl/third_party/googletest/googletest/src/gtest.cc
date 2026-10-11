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

//
// The Google C++ Testing and Mocking Framework (Google Test)

#include "gtest/gtest.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gtest/gtest-assertion-result.h"
#include "gtest/gtest-spi.h"
#include "gtest/internal/custom/gtest.h"
#include "gtest/internal/gtest-port.h"

#ifdef GTEST_OS_LINUX

#include <fcntl.h>
#include <limits.h>
#include <sched.h>

#include <strings.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include <string>

#elif defined(GTEST_OS_ZOS)
#include <sys/time.h>

#include <strings.h>

#elif defined(GTEST_OS_WINDOWS_MOBILE)

#include <windows.h>
#undef min

#elif defined(GTEST_OS_WINDOWS)

#include <windows.h>
#undef min

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include <io.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/types.h>

#ifdef GTEST_OS_WINDOWS_MINGW
#include <sys/time.h>
#endif

#else

#include <sys/time.h>
#include <unistd.h>

#endif

#if GTEST_HAS_EXCEPTIONS
#include <stdexcept>
#endif

#if GTEST_CAN_STREAM_RESULTS_
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include "src/gtest-internal-inl.h"

#ifdef GTEST_OS_WINDOWS
#define vsnprintf _vsnprintf
#endif

#ifdef GTEST_OS_MAC
#ifndef GTEST_OS_IOS
#include <crt_externs.h>
#endif
#endif

#ifdef GTEST_HAS_ABSL
#include "absl/container/flat_hash_set.h"
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#endif

#if defined(__has_builtin)
#define GTEST_HAS_BUILTIN(x) __has_builtin(x)
#else
#define GTEST_HAS_BUILTIN(x) 0
#endif

#if defined(GTEST_HAS_ABSL) && !defined(GTEST_NO_ABSL_FLAGS)
#define GTEST_HAS_ABSL_FLAGS
#endif

namespace testing {

using internal::CountIf;
using internal::ForEach;
using internal::GetElementOr;
using internal::Shuffle;

static const char kDisableTestFilter[] = "DISABLED_*:*/DISABLED_*";

static const char kDeathTestSuiteFilter[] = "*DeathTest:*DeathTest/*";

static const char kUniversalFilter[] = "*";

static const char kDefaultOutputFormat[] = "xml";

static const char kDefaultOutputFile[] = "test_detail";

static const char kTestShardIndex[] = "GTEST_SHARD_INDEX";

static const char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";

static const char kTestShardStatusFile[] = "GTEST_SHARD_STATUS_FILE";

namespace internal {

const char kStackTraceMarker[] = "\nStack trace:\n";

bool g_help_flag = false;

#if GTEST_HAS_FILE_SYSTEM

static FILE* OpenFileForWriting(const std::string& output_file) {
  FILE* fileout = nullptr;
  FilePath output_file_path(output_file);
  FilePath output_dir(output_file_path.RemoveFileName());

  if (output_dir.CreateDirectoriesRecursively()) {
    fileout = posix::FOpen(output_file.c_str(), "w");
  }
  if (fileout == nullptr) {
    GTEST_LOG_(FATAL) << "Unable to open file \"" << output_file << "\"";
  }
  return fileout;
}
#endif

}

static const char* GetDefaultFilter() {
  const char* const testbridge_test_only =
      internal::posix::GetEnv("TESTBRIDGE_TEST_ONLY");
  if (testbridge_test_only != nullptr) {
    return testbridge_test_only;
  }
  return kUniversalFilter;
}

static bool GetDefaultFailFast() {
  const char* const testbridge_test_runner_fail_fast =
      internal::posix::GetEnv("TESTBRIDGE_TEST_RUNNER_FAIL_FAST");
  if (testbridge_test_runner_fail_fast != nullptr) {
    return strcmp(testbridge_test_runner_fail_fast, "1") == 0;
  }
  return false;
}

}

GTEST_DEFINE_bool_(
    fail_fast,
    testing::internal::BoolFromGTestEnv("fail_fast",
                                        testing::GetDefaultFailFast()),
    "True if and only if a test failure should stop further test execution.");

GTEST_DEFINE_bool_(
    fail_if_no_test_linked,
    testing::internal::BoolFromGTestEnv("fail_if_no_test_linked", false),
    "True if and only if the test should fail if no test case (including "
    "disabled test cases) is linked.");

GTEST_DEFINE_bool_(
    also_run_disabled_tests,
    testing::internal::BoolFromGTestEnv("also_run_disabled_tests", false),
    "Run disabled tests too, in addition to the tests normally being run.");

GTEST_DEFINE_bool_(
    break_on_failure,
    testing::internal::BoolFromGTestEnv("break_on_failure", false),
    "True if and only if a failed assertion should be a debugger "
    "break-point.");

GTEST_DEFINE_bool_(catch_exceptions,
                   testing::internal::BoolFromGTestEnv("catch_exceptions",
                                                       true),
                   "True if and only if " GTEST_NAME_
                   " should catch exceptions and treat them as test failures.");

GTEST_DEFINE_string_(
    color, testing::internal::StringFromGTestEnv("color", "auto"),
    "Whether to use colors in the output.  Valid values: yes, no, "
    "and auto.  'auto' means to use colors if the output is "
    "being sent to a terminal and the TERM environment variable "
    "is set to a terminal type that supports colors.");

GTEST_DEFINE_string_(
    filter,
    testing::internal::StringFromGTestEnv("filter",
                                          testing::GetDefaultFilter()),
    "A colon-separated list of glob (not regex) patterns "
    "for filtering the tests to run, optionally followed by a "
    "'-' and a : separated list of negative patterns (tests to "
    "exclude).  A test is run if it matches one of the positive "
    "patterns and does not match any of the negative patterns.");

GTEST_DEFINE_bool_(
    install_failure_signal_handler,
    testing::internal::BoolFromGTestEnv("install_failure_signal_handler",
                                        false),
    "If true and supported on the current platform, " GTEST_NAME_
    " should "
    "install a signal handler that dumps debugging information when fatal "
    "signals are raised.");

GTEST_DEFINE_bool_(list_tests, false, "List all tests without running them.");

GTEST_DEFINE_string_(
    output,
    testing::internal::StringFromGTestEnv(
        "output", testing::internal::OutputFlagAlsoCheckEnvVar().c_str()),
    "A format (defaults to \"xml\" but can be specified to be \"json\"), "
    "optionally followed by a colon and an output file name or directory. "
    "A directory is indicated by a trailing pathname separator. "
    "Examples: \"xml:filename.xml\", \"xml::directoryname/\". "
    "If a directory is specified, output files will be created "
    "within that directory, with file-names based on the test "
    "executable's name and, if necessary, made unique by adding "
    "digits.");

GTEST_DEFINE_bool_(
    brief, testing::internal::BoolFromGTestEnv("brief", false),
    "True if only test failures should be displayed in text output.");

GTEST_DEFINE_bool_(print_time,
                   testing::internal::BoolFromGTestEnv("print_time", true),
                   "True if and only if " GTEST_NAME_
                   " should display elapsed time in text output.");

GTEST_DEFINE_bool_(print_utf8,
                   testing::internal::BoolFromGTestEnv("print_utf8", true),
                   "True if and only if " GTEST_NAME_
                   " prints UTF8 characters as text.");

GTEST_DEFINE_int32_(
    random_seed, testing::internal::Int32FromGTestEnv("random_seed", 0),
    "Random number seed to use when shuffling test orders.  Must be in range "
    "[1, 99999], or 0 to use a seed based on the current time.");

GTEST_DEFINE_int32_(
    repeat, testing::internal::Int32FromGTestEnv("repeat", 1),
    "How many times to repeat each test.  Specify a negative number "
    "for repeating forever.  Useful for shaking out flaky tests.");

GTEST_DEFINE_bool_(
    recreate_environments_when_repeating,
    testing::internal::BoolFromGTestEnv("recreate_environments_when_repeating",
                                        false),
    "Controls whether global test environments are recreated for each repeat "
    "of the tests. If set to false the global test environments are only set "
    "up once, for the first iteration, and only torn down once, for the last. "
    "Useful for shaking out flaky tests with stable, expensive test "
    "environments. If --gtest_repeat is set to a negative number, meaning "
    "there is no last run, the environments will always be recreated to avoid "
    "leaks.");

GTEST_DEFINE_bool_(show_internal_stack_frames, false,
                   "True if and only if " GTEST_NAME_
                   " should include internal stack frames when "
                   "printing test failure stack traces.");

GTEST_DEFINE_bool_(shuffle,
                   testing::internal::BoolFromGTestEnv("shuffle", false),
                   "True if and only if " GTEST_NAME_
                   " should randomize tests' order on every run.");

GTEST_DEFINE_int32_(
    stack_trace_depth,
    testing::internal::Int32FromGTestEnv("stack_trace_depth",
                                         testing::kMaxStackTraceDepth),
    "The maximum number of stack frames to print when an "
    "assertion fails.  The valid range is 0 through 100, inclusive.");

GTEST_DEFINE_string_(
    stream_result_to,
    testing::internal::StringFromGTestEnv("stream_result_to", ""),
    "This flag specifies the host name and the port number on which to stream "
    "test results. Example: \"localhost:555\". The flag is effective only on "
    "Linux and macOS.");

GTEST_DEFINE_bool_(
    throw_on_failure,
    testing::internal::BoolFromGTestEnv("throw_on_failure", false),
    "When this flag is specified, a failed assertion will throw an exception "
    "if exceptions are enabled or exit the program with a non-zero code "
    "otherwise. For use with an external test framework.");

#if GTEST_USE_OWN_FLAGFILE_FLAG_
GTEST_DEFINE_string_(
    flagfile, testing::internal::StringFromGTestEnv("flagfile", ""),
    "This flag specifies the flagfile to read command-line flags from.");
#endif

namespace testing {
namespace internal {

const uint32_t Random::kMaxRange;

uint32_t Random::Generate(uint32_t range) {

  state_ = static_cast<uint32_t>(1103515245ULL * state_ + 12345U) % kMaxRange;

  GTEST_CHECK_(range > 0) << "Cannot generate a number in the range [0, 0).";
  GTEST_CHECK_(range <= kMaxRange)
      << "Generation of a number in [0, " << range << ") was requested, "
      << "but this can only generate numbers in [0, " << kMaxRange << ").";

  return state_ % range;
}

static bool GTestIsInitialized() { return !GetArgvs().empty(); }

static int SumOverTestSuiteList(const std::vector<TestSuite*>& case_list,
                                int (TestSuite::*method)() const) {
  int sum = 0;
  for (size_t i = 0; i < case_list.size(); i++) {
    sum += (case_list[i]->*method)();
  }
  return sum;
}

static bool TestSuitePassed(const TestSuite* test_suite) {
  return test_suite->should_run() && test_suite->Passed();
}

static bool TestSuiteFailed(const TestSuite* test_suite) {
  return test_suite->should_run() && test_suite->Failed();
}

static bool ShouldRunTestSuite(const TestSuite* test_suite) {
  return test_suite->should_run();
}

namespace {

bool ShouldEmitStackTraceForResultType(TestPartResult::Type type) {

  return type != TestPartResult::kSuccess && type != TestPartResult::kSkip;
}

}

AssertHelper::AssertHelper(TestPartResult::Type type, const char* file,
                           int line, const char* message)
    : data_(new AssertHelperData(type, file, line, message)) {}

AssertHelper::~AssertHelper() { delete data_; }

void AssertHelper::operator=(const Message& message) const {
  UnitTest::GetInstance()->AddTestPartResult(
      data_->type, data_->file, data_->line,
      AppendUserMessage(data_->message, message),
      ShouldEmitStackTraceForResultType(data_->type)
          ? UnitTest::GetInstance()->impl()->CurrentOsStackTraceExceptTop(1)
          : ""

  );
}

namespace {

constexpr bool kErrorOnUninstantiatedParameterizedTest = true;
constexpr bool kErrorOnUninstantiatedTypeParameterizedTest = true;

class FailureTest : public Test {
 public:
  explicit FailureTest(const CodeLocation& loc, std::string error_message,
                       bool as_error)
      : loc_(loc),
        error_message_(std::move(error_message)),
        as_error_(as_error) {}

  void TestBody() override {
    if (as_error_) {
      AssertHelper(TestPartResult::kNonFatalFailure, loc_.file.c_str(),
                   loc_.line, "") = Message() << error_message_;
    } else {
      std::cout << error_message_ << std::endl;
    }
  }

 private:
  const CodeLocation loc_;
  const std::string error_message_;
  const bool as_error_;
};

}

std::set<std::string>* GetIgnoredParameterizedTestSuites() {
  return UnitTest::GetInstance()->impl()->ignored_parameterized_test_suites();
}

MarkAsIgnored::MarkAsIgnored(const char* test_suite) {
  GetIgnoredParameterizedTestSuites()->insert(test_suite);
}

void InsertSyntheticTestCase(const std::string& name, CodeLocation location,
                             bool has_test_p) {
  const auto& ignored = *GetIgnoredParameterizedTestSuites();
  if (ignored.find(name) != ignored.end()) return;

  const char kMissingInstantiation[] =
      " is defined via TEST_P, but never instantiated. None of the test "
      "cases "
      "will run. Either no INSTANTIATE_TEST_SUITE_P is provided or the only "
      "ones provided expand to nothing."
      "\n\n"
      "Ideally, TEST_P definitions should only ever be included as part of "
      "binaries that intend to use them. (As opposed to, for example, being "
      "placed in a library that may be linked in to get other utilities.)";

  const char kMissingTestCase[] =
      " is instantiated via INSTANTIATE_TEST_SUITE_P, but no tests are "
      "defined via TEST_P . No test cases will run."
      "\n\n"
      "Ideally, INSTANTIATE_TEST_SUITE_P should only ever be invoked from "
      "code that always depend on code that provides TEST_P. Failing to do "
      "so is often an indication of dead code, e.g. the last TEST_P was "
      "removed but the rest got left behind.";

  std::string message =
      "Parameterized test suite " + name +
      (has_test_p ? kMissingInstantiation : kMissingTestCase) +
      "\n\n"
      "To suppress this error for this test suite, insert the following line "
      "(in a non-header) in the namespace it is defined in:"
      "\n\n"
      "GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(" +
      name + ");";

  std::string full_name = "UninstantiatedParameterizedTestSuite<" + name + ">";
  RegisterTest(
      "GoogleTestVerification", full_name.c_str(),
      nullptr,
      nullptr,
      location.file.c_str(), location.line, [message, location] {
        return new FailureTest(location, message,
                               kErrorOnUninstantiatedParameterizedTest);
      });
}

void RegisterTypeParameterizedTestSuite(const char* test_suite_name,
                                        CodeLocation code_location) {
  GetUnitTestImpl()->type_parameterized_test_registry().RegisterTestSuite(
      test_suite_name, std::move(code_location));
}

void RegisterTypeParameterizedTestSuiteInstantiation(const char* case_name) {
  GetUnitTestImpl()->type_parameterized_test_registry().RegisterInstantiation(
      case_name);
}

void TypeParameterizedTestSuiteRegistry::RegisterTestSuite(
    const char* test_suite_name, CodeLocation code_location) {
  suites_.emplace(std::string(test_suite_name),
                  TypeParameterizedTestSuiteInfo(std::move(code_location)));
}

void TypeParameterizedTestSuiteRegistry::RegisterInstantiation(
    const char* test_suite_name) {
  auto it = suites_.find(std::string(test_suite_name));
  if (it != suites_.end()) {
    it->second.instantiated = true;
  } else {
    GTEST_LOG_(ERROR) << "Unknown type parameterized test suit '"
                      << test_suite_name << "'";
  }
}

void TypeParameterizedTestSuiteRegistry::CheckForInstantiations() {
  const auto& ignored = *GetIgnoredParameterizedTestSuites();
  for (const auto& testcase : suites_) {
    if (testcase.second.instantiated) continue;
    if (ignored.find(testcase.first) != ignored.end()) continue;

    std::string message =
        "Type parameterized test suite " + testcase.first +
        " is defined via REGISTER_TYPED_TEST_SUITE_P, but never instantiated "
        "via INSTANTIATE_TYPED_TEST_SUITE_P. None of the test cases will run."
        "\n\n"
        "Ideally, TYPED_TEST_P definitions should only ever be included as "
        "part of binaries that intend to use them. (As opposed to, for "
        "example, being placed in a library that may be linked in to get "
        "other "
        "utilities.)"
        "\n\n"
        "To suppress this error for this test suite, insert the following "
        "line "
        "(in a non-header) in the namespace it is defined in:"
        "\n\n"
        "GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(" +
        testcase.first + ");";

    std::string full_name =
        "UninstantiatedTypeParameterizedTestSuite<" + testcase.first + ">";
    RegisterTest(
        "GoogleTestVerification", full_name.c_str(),
        nullptr,
        nullptr,
        testcase.second.code_location.file.c_str(),
        testcase.second.code_location.line, [message, testcase] {
          return new FailureTest(testcase.second.code_location, message,
                                 kErrorOnUninstantiatedTypeParameterizedTest);
        });
  }
}

static ::std::vector<std::string> g_argvs;

::std::vector<std::string> GetArgvs() {
#if defined(GTEST_CUSTOM_GET_ARGVS_)

  const auto& custom = GTEST_CUSTOM_GET_ARGVS_();
  return ::std::vector<std::string>(custom.begin(), custom.end());
#else
  return g_argvs;
#endif
}

#if GTEST_HAS_FILE_SYSTEM

FilePath GetCurrentExecutableName() {
  FilePath result;

  auto args = GetArgvs();
  if (!args.empty()) {
#if defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_OS2)
    result.Set(FilePath(args[0]).RemoveExtension("exe"));
#else
    result.Set(FilePath(args[0]));
#endif
  }

  return result.RemoveDirectoryName();
}
#endif

std::string UnitTestOptions::GetOutputFormat() {
  std::string s = GTEST_FLAG_GET(output);
  const char* const gtest_output_flag = s.c_str();
  const char* const colon = strchr(gtest_output_flag, ':');
  return (colon == nullptr)
             ? std::string(gtest_output_flag)
             : std::string(gtest_output_flag,
                           static_cast<size_t>(colon - gtest_output_flag));
}

#if GTEST_HAS_FILE_SYSTEM

std::string UnitTestOptions::GetAbsolutePathToOutputFile() {
  std::string s = GTEST_FLAG_GET(output);
  const char* const gtest_output_flag = s.c_str();

  std::string format = GetOutputFormat();
  if (format.empty()) format = std::string(kDefaultOutputFormat);

  const char* const colon = strchr(gtest_output_flag, ':');
  if (colon == nullptr)
    return internal::FilePath::MakeFileName(
               internal::FilePath(
                   UnitTest::GetInstance()->original_working_dir()),
               internal::FilePath(kDefaultOutputFile), 0, format.c_str())
        .string();

  internal::FilePath output_name(colon + 1);
  if (!output_name.IsAbsolutePath())
    output_name = internal::FilePath::ConcatPaths(
        internal::FilePath(UnitTest::GetInstance()->original_working_dir()),
        internal::FilePath(colon + 1));

  if (!output_name.IsDirectory()) return output_name.string();

  internal::FilePath result(internal::FilePath::GenerateUniqueFileName(
      output_name, internal::GetCurrentExecutableName(),
      GetOutputFormat().c_str()));
  return result.string();
}
#endif

static bool PatternMatchesString(const std::string& name_str,
                                 const char* pattern, const char* pattern_end) {
  const char* name = name_str.c_str();
  const char* const name_begin = name;
  const char* const name_end = name + name_str.size();

  const char* pattern_next = pattern;
  const char* name_next = name;

  while (pattern < pattern_end || name < name_end) {
    if (pattern < pattern_end) {
      switch (*pattern) {
        default:
          if (name < name_end && *name == *pattern) {
            ++pattern;
            ++name;
            continue;
          }
          break;
        case '?':
          if (name < name_end) {
            ++pattern;
            ++name;
            continue;
          }
          break;
        case '*':

          pattern_next = pattern;
          name_next = name + 1;
          ++pattern;
          continue;
      }
    }

    if (name_begin < name_next && name_next <= name_end) {
      pattern = pattern_next;
      name = name_next;
      continue;
    }
    return false;
  }
  return true;
}

namespace {

bool IsGlobPattern(const std::string& pattern) {
  return std::any_of(pattern.begin(), pattern.end(),
                     [](const char c) { return c == '?' || c == '*'; });
}

class UnitTestFilter {
 public:
  UnitTestFilter() = default;

  explicit UnitTestFilter(const std::string& filter) {

    std::vector<std::string> all_patterns;
    SplitString(filter, ':', &all_patterns);
    const auto exact_match_patterns_begin = std::partition(
        all_patterns.begin(), all_patterns.end(), &IsGlobPattern);

    glob_patterns_.reserve(static_cast<size_t>(
        std::distance(all_patterns.begin(), exact_match_patterns_begin)));
    std::move(all_patterns.begin(), exact_match_patterns_begin,
              std::inserter(glob_patterns_, glob_patterns_.begin()));
    std::move(
        exact_match_patterns_begin, all_patterns.end(),
        std::inserter(exact_match_patterns_, exact_match_patterns_.begin()));
  }

  bool MatchesName(const std::string& name) const {
    return exact_match_patterns_.find(name) != exact_match_patterns_.end() ||
           std::any_of(glob_patterns_.begin(), glob_patterns_.end(),
                       [&name](const std::string& pattern) {
                         return PatternMatchesString(
                             name, pattern.c_str(),
                             pattern.c_str() + pattern.size());
                       });
  }

 private:
  std::vector<std::string> glob_patterns_;
  std::unordered_set<std::string> exact_match_patterns_;
};

class PositiveAndNegativeUnitTestFilter {
 public:

  explicit PositiveAndNegativeUnitTestFilter(const std::string& filter) {
    std::vector<std::string> positive_and_negative_filters;

    SplitString(filter, '-', &positive_and_negative_filters);
    const auto& positive_filter = positive_and_negative_filters.front();

    if (positive_and_negative_filters.size() > 1) {
      positive_filter_ = UnitTestFilter(
          positive_filter.empty() ? kUniversalFilter : positive_filter);

      auto negative_filter_string = positive_and_negative_filters[1];
      for (std::size_t i = 2; i < positive_and_negative_filters.size(); i++)
        negative_filter_string =
            negative_filter_string + '-' + positive_and_negative_filters[i];
      negative_filter_ = UnitTestFilter(negative_filter_string);
    } else {

      positive_filter_ = UnitTestFilter(positive_filter);
    }
  }

  bool MatchesTest(const std::string& test_suite_name,
                   const std::string& test_name) const {
    return MatchesName(test_suite_name + "." + test_name);
  }

  bool MatchesName(const std::string& name) const {
    return positive_filter_.MatchesName(name) &&
           !negative_filter_.MatchesName(name);
  }

 private:
  UnitTestFilter positive_filter_;
  UnitTestFilter negative_filter_;
};
}

bool UnitTestOptions::MatchesFilter(const std::string& name_str,
                                    const char* filter) {
  return UnitTestFilter(filter).MatchesName(name_str);
}

bool UnitTestOptions::FilterMatchesTest(const std::string& test_suite_name,
                                        const std::string& test_name) {

  return PositiveAndNegativeUnitTestFilter(GTEST_FLAG_GET(filter))
      .MatchesTest(test_suite_name, test_name);
}

#if GTEST_HAS_SEH
static std::string FormatSehExceptionMessage(DWORD exception_code,
                                             const char* location) {
  Message message;
  message << "SEH exception with code 0x" << std::setbase(16) << exception_code
          << std::setbase(10) << " thrown in " << location << ".";
  return message.GetString();
}

int UnitTestOptions::GTestProcessSEH(DWORD seh_code, const char* location) {

  const DWORD kCxxExceptionCode = 0xe06d7363;

  if (!GTEST_FLAG_GET(catch_exceptions) || seh_code == kCxxExceptionCode ||
      seh_code == EXCEPTION_BREAKPOINT ||
      seh_code == EXCEPTION_STACK_OVERFLOW) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  internal::ReportFailureInUnknownLocation(
      TestPartResult::kFatalFailure,
      FormatSehExceptionMessage(seh_code, location) +
          "\n"
          "Stack trace:\n" +
          ::testing::internal::GetCurrentOsStackTraceExceptTop(1));

  return EXCEPTION_EXECUTE_HANDLER;
}
#endif

}

ScopedFakeTestPartResultReporter::ScopedFakeTestPartResultReporter(
    TestPartResultArray* result)
    : intercept_mode_(INTERCEPT_ONLY_CURRENT_THREAD), result_(result) {
  Init();
}

ScopedFakeTestPartResultReporter::ScopedFakeTestPartResultReporter(
    InterceptMode intercept_mode, TestPartResultArray* result)
    : intercept_mode_(intercept_mode), result_(result) {
  Init();
}

void ScopedFakeTestPartResultReporter::Init() {
  internal::UnitTestImpl* const impl = internal::GetUnitTestImpl();
  if (intercept_mode_ == INTERCEPT_ALL_THREADS) {
    old_reporter_ = impl->GetGlobalTestPartResultReporter();
    impl->SetGlobalTestPartResultReporter(this);
  } else {
    old_reporter_ = impl->GetTestPartResultReporterForCurrentThread();
    impl->SetTestPartResultReporterForCurrentThread(this);
  }
}

ScopedFakeTestPartResultReporter::~ScopedFakeTestPartResultReporter() {
  internal::UnitTestImpl* const impl = internal::GetUnitTestImpl();
  if (intercept_mode_ == INTERCEPT_ALL_THREADS) {
    impl->SetGlobalTestPartResultReporter(old_reporter_);
  } else {
    impl->SetTestPartResultReporterForCurrentThread(old_reporter_);
  }
}

void ScopedFakeTestPartResultReporter::ReportTestPartResult(
    const TestPartResult& result) {
  result_->Append(result);
}

namespace internal {

TypeId GetTestTypeId() { return GetTypeId<Test>(); }

extern const TypeId kTestTypeIdInGoogleTest = GetTestTypeId();

static AssertionResult HasOneFailure(const char*                   ,
                                     const char*                ,
                                     const char*                  ,
                                     const TestPartResultArray& results,
                                     TestPartResult::Type type,
                                     const std::string& substr) {
  const std::string expected(type == TestPartResult::kFatalFailure
                                 ? "1 fatal failure"
                                 : "1 non-fatal failure");
  Message msg;
  if (results.size() != 1) {
    msg << "Expected: " << expected << "\n"
        << "  Actual: " << results.size() << " failures";
    for (int i = 0; i < results.size(); i++) {
      msg << "\n" << results.GetTestPartResult(i);
    }
    return AssertionFailure() << msg;
  }

  const TestPartResult& r = results.GetTestPartResult(0);
  if (r.type() != type) {
    return AssertionFailure() << "Expected: " << expected << "\n"
                              << "  Actual:\n"
                              << r;
  }

  if (strstr(r.message(), substr.c_str()) == nullptr) {
    return AssertionFailure()
           << "Expected: " << expected << " containing \"" << substr << "\"\n"
           << "  Actual:\n"
           << r;
  }

  return AssertionSuccess();
}

SingleFailureChecker::SingleFailureChecker(const TestPartResultArray* results,
                                           TestPartResult::Type type,
                                           const std::string& substr)
    : results_(results), type_(type), substr_(substr) {}

SingleFailureChecker::~SingleFailureChecker() {
  EXPECT_PRED_FORMAT3(HasOneFailure, *results_, type_, substr_);
}

DefaultGlobalTestPartResultReporter::DefaultGlobalTestPartResultReporter(
    UnitTestImpl* unit_test)
    : unit_test_(unit_test) {}

void DefaultGlobalTestPartResultReporter::ReportTestPartResult(
    const TestPartResult& result) {
  unit_test_->current_test_result()->AddTestPartResult(result);
  unit_test_->listeners()->repeater()->OnTestPartResult(result);
}

DefaultPerThreadTestPartResultReporter::DefaultPerThreadTestPartResultReporter(
    UnitTestImpl* unit_test)
    : unit_test_(unit_test) {}

void DefaultPerThreadTestPartResultReporter::ReportTestPartResult(
    const TestPartResult& result) {
  unit_test_->GetGlobalTestPartResultReporter()->ReportTestPartResult(result);
}

TestPartResultReporterInterface*
UnitTestImpl::GetGlobalTestPartResultReporter() {
  internal::MutexLock lock(&global_test_part_result_reporter_mutex_);
  return global_test_part_result_reporter_;
}

void UnitTestImpl::SetGlobalTestPartResultReporter(
    TestPartResultReporterInterface* reporter) {
  internal::MutexLock lock(&global_test_part_result_reporter_mutex_);
  global_test_part_result_reporter_ = reporter;
}

TestPartResultReporterInterface*
UnitTestImpl::GetTestPartResultReporterForCurrentThread() {
  return per_thread_test_part_result_reporter_.get();
}

void UnitTestImpl::SetTestPartResultReporterForCurrentThread(
    TestPartResultReporterInterface* reporter) {
  per_thread_test_part_result_reporter_.set(reporter);
}

int UnitTestImpl::successful_test_suite_count() const {
  return CountIf(test_suites_, TestSuitePassed);
}

int UnitTestImpl::failed_test_suite_count() const {
  return CountIf(test_suites_, TestSuiteFailed);
}

int UnitTestImpl::total_test_suite_count() const {
  return static_cast<int>(test_suites_.size());
}

int UnitTestImpl::test_suite_to_run_count() const {
  return CountIf(test_suites_, ShouldRunTestSuite);
}

int UnitTestImpl::successful_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::successful_test_count);
}

int UnitTestImpl::skipped_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::skipped_test_count);
}

int UnitTestImpl::failed_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::failed_test_count);
}

int UnitTestImpl::reportable_disabled_test_count() const {
  return SumOverTestSuiteList(test_suites_,
                              &TestSuite::reportable_disabled_test_count);
}

int UnitTestImpl::disabled_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::disabled_test_count);
}

int UnitTestImpl::reportable_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::reportable_test_count);
}

int UnitTestImpl::total_test_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::total_test_count);
}

int UnitTestImpl::test_to_run_count() const {
  return SumOverTestSuiteList(test_suites_, &TestSuite::test_to_run_count);
}

std::string UnitTestImpl::CurrentOsStackTraceExceptTop(int skip_count) {
  return os_stack_trace_getter()->CurrentStackTrace(
      static_cast<int>(GTEST_FLAG_GET(stack_trace_depth)), skip_count + 1

  );
}

class Timer {
 public:
  Timer() : start_(clock::now()) {}

  TimeInMillis Elapsed() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
                                                                 start_)
        .count();
  }

 private:

#if defined(_NEWLIB_VERSION) && !defined(CLOCK_MONOTONIC)
  using clock = std::chrono::system_clock;
#else
  using clock = std::chrono::steady_clock;
#endif
  clock::time_point start_;
};

TimeInMillis GetTimeInMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now() -
             std::chrono::system_clock::from_time_t(0))
      .count();
}

#ifdef GTEST_OS_WINDOWS_MOBILE

LPCWSTR String::AnsiToUtf16(const char* ansi) {
  if (!ansi) return nullptr;
  const int length = strlen(ansi);
  const int unicode_length =
      MultiByteToWideChar(CP_ACP, 0, ansi, length, nullptr, 0);
  WCHAR* unicode = new WCHAR[unicode_length + 1];
  MultiByteToWideChar(CP_ACP, 0, ansi, length, unicode, unicode_length);
  unicode[unicode_length] = 0;
  return unicode;
}

const char* String::Utf16ToAnsi(LPCWSTR utf16_str) {
  if (!utf16_str) return nullptr;
  const int ansi_length = WideCharToMultiByte(CP_ACP, 0, utf16_str, -1, nullptr,
                                              0, nullptr, nullptr);
  char* ansi = new char[ansi_length + 1];
  WideCharToMultiByte(CP_ACP, 0, utf16_str, -1, ansi, ansi_length, nullptr,
                      nullptr);
  ansi[ansi_length] = 0;
  return ansi;
}

#endif

bool String::CStringEquals(const char* lhs, const char* rhs) {
  if (lhs == nullptr) return rhs == nullptr;

  if (rhs == nullptr) return false;

  return strcmp(lhs, rhs) == 0;
}

#if GTEST_HAS_STD_WSTRING

static void StreamWideCharsToMessage(const wchar_t* wstr, size_t length,
                                     Message* msg) {
  for (size_t i = 0; i != length;) {
    if (wstr[i] != L'\0') {
      *msg << WideStringToUtf8(wstr + i, static_cast<int>(length - i));
      while (i != length && wstr[i] != L'\0') i++;
    } else {
      *msg << '\0';
      i++;
    }
  }
}

#endif

void SplitString(const ::std::string& str, char delimiter,
                 ::std::vector< ::std::string>* dest) {
  ::std::vector< ::std::string> parsed;
  ::std::string::size_type pos = 0;
  while (::testing::internal::AlwaysTrue()) {
    const ::std::string::size_type colon = str.find(delimiter, pos);
    if (colon == ::std::string::npos) {
      parsed.push_back(str.substr(pos));
      break;
    } else {
      parsed.push_back(str.substr(pos, colon - pos));
      pos = colon + 1;
    }
  }
  dest->swap(parsed);
}

}

Message::Message() : ss_(new ::std::stringstream) {

  *ss_ << std::setprecision(std::numeric_limits<double>::digits10 + 2);
}

Message& Message::operator<<(const wchar_t* wide_c_str) {
  return *this << internal::String::ShowWideCString(wide_c_str);
}
Message& Message::operator<<(wchar_t* wide_c_str) {
  return *this << internal::String::ShowWideCString(wide_c_str);
}

#if GTEST_HAS_STD_WSTRING

Message& Message::operator<<(const ::std::wstring& wstr) {
  internal::StreamWideCharsToMessage(wstr.c_str(), wstr.length(), this);
  return *this;
}
#endif

std::string Message::GetString() const {
  return internal::StringStreamToString(ss_.get());
}

namespace internal {

namespace edit_distance {
std::vector<EditType> CalculateOptimalEdits(const std::vector<size_t>& left,
                                            const std::vector<size_t>& right) {
  std::vector<std::vector<double> > costs(
      left.size() + 1, std::vector<double>(right.size() + 1));
  std::vector<std::vector<EditType> > best_move(
      left.size() + 1, std::vector<EditType>(right.size() + 1));

  for (size_t l_i = 0; l_i < costs.size(); ++l_i) {
    costs[l_i][0] = static_cast<double>(l_i);
    best_move[l_i][0] = kRemove;
  }

  for (size_t r_i = 1; r_i < costs[0].size(); ++r_i) {
    costs[0][r_i] = static_cast<double>(r_i);
    best_move[0][r_i] = kAdd;
  }

  for (size_t l_i = 0; l_i < left.size(); ++l_i) {
    for (size_t r_i = 0; r_i < right.size(); ++r_i) {
      if (left[l_i] == right[r_i]) {

        costs[l_i + 1][r_i + 1] = costs[l_i][r_i];
        best_move[l_i + 1][r_i + 1] = kMatch;
        continue;
      }

      const double add = costs[l_i + 1][r_i];
      const double remove = costs[l_i][r_i + 1];
      const double replace = costs[l_i][r_i];
      if (add < remove && add < replace) {
        costs[l_i + 1][r_i + 1] = add + 1;
        best_move[l_i + 1][r_i + 1] = kAdd;
      } else if (remove < add && remove < replace) {
        costs[l_i + 1][r_i + 1] = remove + 1;
        best_move[l_i + 1][r_i + 1] = kRemove;
      } else {

        costs[l_i + 1][r_i + 1] = replace + 1.00001;
        best_move[l_i + 1][r_i + 1] = kReplace;
      }
    }
  }

  std::vector<EditType> best_path;
  for (size_t l_i = left.size(), r_i = right.size(); l_i > 0 || r_i > 0;) {
    EditType move = best_move[l_i][r_i];
    best_path.push_back(move);
    l_i -= move != kAdd;
    r_i -= move != kRemove;
  }
  std::reverse(best_path.begin(), best_path.end());
  return best_path;
}

namespace {

class InternalStrings {
 public:
  size_t GetId(const std::string& str) {
    IdMap::iterator it = ids_.find(str);
    if (it != ids_.end()) return it->second;
    size_t id = ids_.size();
    return ids_[str] = id;
  }

 private:
  typedef std::map<std::string, size_t> IdMap;
  IdMap ids_;
};

}

std::vector<EditType> CalculateOptimalEdits(
    const std::vector<std::string>& left,
    const std::vector<std::string>& right) {
  std::vector<size_t> left_ids, right_ids;
  {
    InternalStrings intern_table;
    for (size_t i = 0; i < left.size(); ++i) {
      left_ids.push_back(intern_table.GetId(left[i]));
    }
    for (size_t i = 0; i < right.size(); ++i) {
      right_ids.push_back(intern_table.GetId(right[i]));
    }
  }
  return CalculateOptimalEdits(left_ids, right_ids);
}

namespace {

class Hunk {
 public:
  Hunk(size_t left_start, size_t right_start)
      : left_start_(left_start),
        right_start_(right_start),
        adds_(),
        removes_(),
        common_() {}

  void PushLine(char edit, const char* line) {
    switch (edit) {
      case ' ':
        ++common_;
        FlushEdits();
        hunk_.push_back(std::make_pair(' ', line));
        break;
      case '-':
        ++removes_;
        hunk_removes_.push_back(std::make_pair('-', line));
        break;
      case '+':
        ++adds_;
        hunk_adds_.push_back(std::make_pair('+', line));
        break;
    }
  }

  void PrintTo(std::ostream* os) {
    PrintHeader(os);
    FlushEdits();
    for (std::list<std::pair<char, const char*> >::const_iterator it =
             hunk_.begin();
         it != hunk_.end(); ++it) {
      *os << it->first << it->second << "\n";
    }
  }

  bool has_edits() const { return adds_ || removes_; }

 private:
  void FlushEdits() {
    hunk_.splice(hunk_.end(), hunk_removes_);
    hunk_.splice(hunk_.end(), hunk_adds_);
  }

  void PrintHeader(std::ostream* ss) const {
    *ss << "@@ ";
    if (removes_) {
      *ss << "-" << left_start_ << "," << (removes_ + common_);
    }
    if (removes_ && adds_) {
      *ss << " ";
    }
    if (adds_) {
      *ss << "+" << right_start_ << "," << (adds_ + common_);
    }
    *ss << " @@\n";
  }

  size_t left_start_, right_start_;
  size_t adds_, removes_, common_;
  std::list<std::pair<char, const char*> > hunk_, hunk_adds_, hunk_removes_;
};

}

std::string CreateUnifiedDiff(const std::vector<std::string>& left,
                              const std::vector<std::string>& right,
                              size_t context) {
  const std::vector<EditType> edits = CalculateOptimalEdits(left, right);

  size_t l_i = 0, r_i = 0, edit_i = 0;
  std::stringstream ss;
  while (edit_i < edits.size()) {

    while (edit_i < edits.size() && edits[edit_i] == kMatch) {
      ++l_i;
      ++r_i;
      ++edit_i;
    }

    const size_t prefix_context = std::min(l_i, context);
    Hunk hunk(l_i - prefix_context + 1, r_i - prefix_context + 1);
    for (size_t i = prefix_context; i > 0; --i) {
      hunk.PushLine(' ', left[l_i - i].c_str());
    }

    size_t n_suffix = 0;
    for (; edit_i < edits.size(); ++edit_i) {
      if (n_suffix >= context) {

        auto it = edits.begin() + static_cast<int>(edit_i);
        while (it != edits.end() && *it == kMatch) ++it;
        if (it == edits.end() ||
            static_cast<size_t>(it - edits.begin()) - edit_i >= context) {

          break;
        }
      }

      EditType edit = edits[edit_i];

      n_suffix = edit == kMatch ? n_suffix + 1 : 0;

      if (edit == kMatch || edit == kRemove || edit == kReplace) {
        hunk.PushLine(edit == kMatch ? ' ' : '-', left[l_i].c_str());
      }
      if (edit == kAdd || edit == kReplace) {
        hunk.PushLine('+', right[r_i].c_str());
      }

      l_i += edit != kAdd;
      r_i += edit != kRemove;
    }

    if (!hunk.has_edits()) {

      break;
    }

    hunk.PrintTo(&ss);
  }
  return ss.str();
}

}

namespace {

std::vector<std::string> SplitEscapedString(const std::string& str) {
  std::vector<std::string> lines;
  size_t start = 0, end = str.size();
  if (end > 2 && str[0] == '"' && str[end - 1] == '"') {
    ++start;
    --end;
  }
  bool escaped = false;
  for (size_t i = start; i + 1 < end; ++i) {
    if (escaped) {
      escaped = false;
      if (str[i] == 'n') {
        lines.push_back(str.substr(start, i - start - 1));
        start = i + 1;
      }
    } else {
      escaped = str[i] == '\\';
    }
  }
  lines.push_back(str.substr(start, end - start));
  return lines;
}

}

AssertionResult EqFailure(const char* lhs_expression,
                          const char* rhs_expression,
                          const std::string& lhs_value,
                          const std::string& rhs_value, bool ignoring_case) {
  Message msg;
  msg << "Expected equality of these values:";
  msg << "\n  " << lhs_expression;
  if (lhs_value != lhs_expression) {
    msg << "\n    Which is: " << lhs_value;
  }
  msg << "\n  " << rhs_expression;
  if (rhs_value != rhs_expression) {
    msg << "\n    Which is: " << rhs_value;
  }

  if (ignoring_case) {
    msg << "\nIgnoring case";
  }

  if (!lhs_value.empty() && !rhs_value.empty()) {
    const std::vector<std::string> lhs_lines = SplitEscapedString(lhs_value);
    const std::vector<std::string> rhs_lines = SplitEscapedString(rhs_value);
    if (lhs_lines.size() > 1 || rhs_lines.size() > 1) {
      msg << "\nWith diff:\n"
          << edit_distance::CreateUnifiedDiff(lhs_lines, rhs_lines);
    }
  }

  return AssertionFailure() << msg;
}

std::string GetBoolAssertionFailureMessage(
    const AssertionResult& assertion_result, const char* expression_text,
    const char* actual_predicate_value, const char* expected_predicate_value) {
  const char* actual_message = assertion_result.message();
  Message msg;
  msg << "Value of: " << expression_text
      << "\n  Actual: " << actual_predicate_value;
  if (actual_message[0] != '\0') msg << " (" << actual_message << ")";
  msg << "\nExpected: " << expected_predicate_value;
  return msg.GetString();
}

AssertionResult DoubleNearPredFormat(const char* expr1, const char* expr2,
                                     const char* abs_error_expr, double val1,
                                     double val2, double abs_error) {

  if (std::isinf(val1) && std::isinf(val2) &&
      (std::signbit(val1) == std::signbit(val2) ||
       (abs_error > 0.0 && std::isinf(abs_error)))) {
    return AssertionSuccess();
  }

  const double diff = fabs(val1 - val2);
  if (diff <= abs_error) return AssertionSuccess();

  const double min_abs = std::min(fabs(val1), fabs(val2));

  const double epsilon =
      nextafter(min_abs, std::numeric_limits<double>::infinity()) - min_abs;

  if (!(std::isnan)(val1) && !(std::isnan)(val2) && abs_error > 0 &&
      abs_error < epsilon) {
    return AssertionFailure()
           << "The difference between " << expr1 << " and " << expr2 << " is "
           << diff << ", where\n"
           << expr1 << " evaluates to " << val1 << ",\n"
           << expr2 << " evaluates to " << val2 << ".\nThe abs_error parameter "
           << abs_error_expr << " evaluates to " << abs_error
           << " which is smaller than the minimum distance between doubles for "
              "numbers of this magnitude which is "
           << epsilon
           << ", thus making this EXPECT_NEAR check equivalent to "
              "EXPECT_EQUAL. Consider using EXPECT_DOUBLE_EQ instead.";
  }
  return AssertionFailure()
         << "The difference between " << expr1 << " and " << expr2 << " is "
         << diff << ", which exceeds " << abs_error_expr << ", where\n"
         << expr1 << " evaluates to " << val1 << ",\n"
         << expr2 << " evaluates to " << val2 << ", and\n"
         << abs_error_expr << " evaluates to " << abs_error << ".";
}

template <typename RawType>
AssertionResult FloatingPointLE(const char* expr1, const char* expr2,
                                RawType val1, RawType val2) {

  if (val1 < val2) {
    return AssertionSuccess();
  }

  const FloatingPoint<RawType> lhs(val1), rhs(val2);
  if (lhs.AlmostEquals(rhs)) {
    return AssertionSuccess();
  }

  ::std::stringstream val1_ss;
  val1_ss << std::setprecision(std::numeric_limits<RawType>::digits10 + 2)
          << val1;

  ::std::stringstream val2_ss;
  val2_ss << std::setprecision(std::numeric_limits<RawType>::digits10 + 2)
          << val2;

  return AssertionFailure()
         << "Expected: (" << expr1 << ") <= (" << expr2 << ")\n"
         << "  Actual: " << StringStreamToString(&val1_ss) << " vs "
         << StringStreamToString(&val2_ss);
}

}

AssertionResult FloatLE(const char* expr1, const char* expr2, float val1,
                        float val2) {
  return internal::FloatingPointLE<float>(expr1, expr2, val1, val2);
}

AssertionResult DoubleLE(const char* expr1, const char* expr2, double val1,
                         double val2) {
  return internal::FloatingPointLE<double>(expr1, expr2, val1, val2);
}

namespace internal {

AssertionResult CmpHelperSTREQ(const char* lhs_expression,
                               const char* rhs_expression, const char* lhs,
                               const char* rhs) {
  if (String::CStringEquals(lhs, rhs)) {
    return AssertionSuccess();
  }

  return EqFailure(lhs_expression, rhs_expression, PrintToString(lhs),
                   PrintToString(rhs), false);
}

AssertionResult CmpHelperSTRCASEEQ(const char* lhs_expression,
                                   const char* rhs_expression, const char* lhs,
                                   const char* rhs) {
  if (String::CaseInsensitiveCStringEquals(lhs, rhs)) {
    return AssertionSuccess();
  }

  return EqFailure(lhs_expression, rhs_expression, PrintToString(lhs),
                   PrintToString(rhs), true);
}

AssertionResult CmpHelperSTRNE(const char* s1_expression,
                               const char* s2_expression, const char* s1,
                               const char* s2) {
  if (!String::CStringEquals(s1, s2)) {
    return AssertionSuccess();
  } else {
    return AssertionFailure()
           << "Expected: (" << s1_expression << ") != (" << s2_expression
           << "), actual: \"" << s1 << "\" vs \"" << s2 << "\"";
  }
}

AssertionResult CmpHelperSTRCASENE(const char* s1_expression,
                                   const char* s2_expression, const char* s1,
                                   const char* s2) {
  if (!String::CaseInsensitiveCStringEquals(s1, s2)) {
    return AssertionSuccess();
  } else {
    return AssertionFailure()
           << "Expected: (" << s1_expression << ") != (" << s2_expression
           << ") (ignoring case), actual: \"" << s1 << "\" vs \"" << s2 << "\"";
  }
}

}

namespace {

bool IsSubstringPred(const char* needle, const char* haystack) {
  if (needle == nullptr || haystack == nullptr) return needle == haystack;

  return strstr(haystack, needle) != nullptr;
}

bool IsSubstringPred(const wchar_t* needle, const wchar_t* haystack) {
  if (needle == nullptr || haystack == nullptr) return needle == haystack;

  return wcsstr(haystack, needle) != nullptr;
}

template <typename StringType>
bool IsSubstringPred(const StringType& needle, const StringType& haystack) {
  return haystack.find(needle) != StringType::npos;
}

template <typename StringType>
AssertionResult IsSubstringImpl(bool expected_to_be_substring,
                                const char* needle_expr,
                                const char* haystack_expr,
                                const StringType& needle,
                                const StringType& haystack) {
  if (IsSubstringPred(needle, haystack) == expected_to_be_substring)
    return AssertionSuccess();

  const bool is_wide_string = sizeof(needle[0]) > 1;
  const char* const begin_string_quote = is_wide_string ? "L\"" : "\"";
  return AssertionFailure()
         << "Value of: " << needle_expr << "\n"
         << "  Actual: " << begin_string_quote << needle << "\"\n"
         << "Expected: " << (expected_to_be_substring ? "" : "not ")
         << "a substring of " << haystack_expr << "\n"
         << "Which is: " << begin_string_quote << haystack << "\"";
}

}

AssertionResult IsSubstring(const char* needle_expr, const char* haystack_expr,
                            const char* needle, const char* haystack) {
  return IsSubstringImpl(true, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsSubstring(const char* needle_expr, const char* haystack_expr,
                            const wchar_t* needle, const wchar_t* haystack) {
  return IsSubstringImpl(true, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsNotSubstring(const char* needle_expr,
                               const char* haystack_expr, const char* needle,
                               const char* haystack) {
  return IsSubstringImpl(false, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsNotSubstring(const char* needle_expr,
                               const char* haystack_expr, const wchar_t* needle,
                               const wchar_t* haystack) {
  return IsSubstringImpl(false, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsSubstring(const char* needle_expr, const char* haystack_expr,
                            const ::std::string& needle,
                            const ::std::string& haystack) {
  return IsSubstringImpl(true, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsNotSubstring(const char* needle_expr,
                               const char* haystack_expr,
                               const ::std::string& needle,
                               const ::std::string& haystack) {
  return IsSubstringImpl(false, needle_expr, haystack_expr, needle, haystack);
}

#if GTEST_HAS_STD_WSTRING
AssertionResult IsSubstring(const char* needle_expr, const char* haystack_expr,
                            const ::std::wstring& needle,
                            const ::std::wstring& haystack) {
  return IsSubstringImpl(true, needle_expr, haystack_expr, needle, haystack);
}

AssertionResult IsNotSubstring(const char* needle_expr,
                               const char* haystack_expr,
                               const ::std::wstring& needle,
                               const ::std::wstring& haystack) {
  return IsSubstringImpl(false, needle_expr, haystack_expr, needle, haystack);
}
#endif

namespace internal {

#ifdef GTEST_OS_WINDOWS

namespace {

AssertionResult HRESULTFailureHelper(const char* expr, const char* expected,
                                     long hr) {
#if defined(GTEST_OS_WINDOWS_MOBILE) || defined(GTEST_OS_WINDOWS_TV_TITLE)

  const char error_text[] = "";

#else

  const DWORD kFlags =
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD kBufSize = 4096;

  char error_text[kBufSize] = {'\0'};
  DWORD message_length = ::FormatMessageA(kFlags,
                                          0,
                                          static_cast<DWORD>(hr),
                                          0,
                                          error_text,
                                          kBufSize,
                                          nullptr);

  for (; message_length && IsSpace(error_text[message_length - 1]);
       --message_length) {
    error_text[message_length - 1] = '\0';
  }

#endif

  const std::string error_hex("0x" + String::FormatHexInt(hr));
  return ::testing::AssertionFailure()
         << "Expected: " << expr << " " << expected << ".\n"
         << "  Actual: " << error_hex << " " << error_text << "\n";
}

}

AssertionResult IsHRESULTSuccess(const char* expr, long hr) {
  if (SUCCEEDED(hr)) {
    return AssertionSuccess();
  }
  return HRESULTFailureHelper(expr, "succeeds", hr);
}

AssertionResult IsHRESULTFailure(const char* expr, long hr) {
  if (FAILED(hr)) {
    return AssertionSuccess();
  }
  return HRESULTFailureHelper(expr, "fails", hr);
}

#endif

constexpr uint32_t kMaxCodePoint1 = (static_cast<uint32_t>(1) << 7) - 1;

constexpr uint32_t kMaxCodePoint2 = (static_cast<uint32_t>(1) << (5 + 6)) - 1;

constexpr uint32_t kMaxCodePoint3 =
    (static_cast<uint32_t>(1) << (4 + 2 * 6)) - 1;

constexpr uint32_t kMaxCodePoint4 =
    (static_cast<uint32_t>(1) << (3 + 3 * 6)) - 1;

inline uint32_t ChopLowBits(uint32_t* bits, int n) {
  const uint32_t low_bits = *bits & ((static_cast<uint32_t>(1) << n) - 1);
  *bits >>= n;
  return low_bits;
}

std::string CodePointToUtf8(uint32_t code_point) {
  if (code_point > kMaxCodePoint4) {
    return "(Invalid Unicode 0x" + String::FormatHexUInt32(code_point) + ")";
  }

  char str[5];
  if (code_point <= kMaxCodePoint1) {
    str[1] = '\0';
    str[0] = static_cast<char>(code_point);
  } else if (code_point <= kMaxCodePoint2) {
    str[2] = '\0';
    str[1] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));
    str[0] = static_cast<char>(0xC0 | code_point);
  } else if (code_point <= kMaxCodePoint3) {
    str[3] = '\0';
    str[2] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));
    str[1] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));
    str[0] = static_cast<char>(0xE0 | code_point);
  } else {
    str[4] = '\0';
    str[3] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));
    str[2] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));
    str[1] = static_cast<char>(0x80 | ChopLowBits(&code_point, 6));
    str[0] = static_cast<char>(0xF0 | code_point);
  }
  return str;
}

inline bool IsUtf16SurrogatePair(wchar_t first, wchar_t second) {
  return sizeof(wchar_t) == 2 && (first & 0xFC00) == 0xD800 &&
         (second & 0xFC00) == 0xDC00;
}

inline uint32_t CreateCodePointFromUtf16SurrogatePair(wchar_t first,
                                                      wchar_t second) {
  const auto first_u = static_cast<uint32_t>(first);
  const auto second_u = static_cast<uint32_t>(second);
  const uint32_t mask = (1 << 10) - 1;
  return (sizeof(wchar_t) == 2)
             ? (((first_u & mask) << 10) | (second_u & mask)) + 0x10000
             :

             first_u;
}

std::string WideStringToUtf8(const wchar_t* str, int num_chars) {
  if (num_chars == -1) num_chars = static_cast<int>(wcslen(str));

  ::std::stringstream stream;
  for (int i = 0; i < num_chars; ++i) {
    uint32_t unicode_code_point;

    if (str[i] == L'\0') {
      break;
    } else if (i + 1 < num_chars && IsUtf16SurrogatePair(str[i], str[i + 1])) {
      unicode_code_point =
          CreateCodePointFromUtf16SurrogatePair(str[i], str[i + 1]);
      i++;
    } else {
      unicode_code_point = static_cast<uint32_t>(str[i]);
    }

    stream << CodePointToUtf8(unicode_code_point);
  }
  return StringStreamToString(&stream);
}

std::string String::ShowWideCString(const wchar_t* wide_c_str) {
  if (wide_c_str == nullptr) return "(null)";

  return internal::WideStringToUtf8(wide_c_str, -1);
}

bool String::WideCStringEquals(const wchar_t* lhs, const wchar_t* rhs) {
  if (lhs == nullptr) return rhs == nullptr;

  if (rhs == nullptr) return false;

  return wcscmp(lhs, rhs) == 0;
}

AssertionResult CmpHelperSTREQ(const char* lhs_expression,
                               const char* rhs_expression, const wchar_t* lhs,
                               const wchar_t* rhs) {
  if (String::WideCStringEquals(lhs, rhs)) {
    return AssertionSuccess();
  }

  return EqFailure(lhs_expression, rhs_expression, PrintToString(lhs),
                   PrintToString(rhs), false);
}

AssertionResult CmpHelperSTRNE(const char* s1_expression,
                               const char* s2_expression, const wchar_t* s1,
                               const wchar_t* s2) {
  if (!String::WideCStringEquals(s1, s2)) {
    return AssertionSuccess();
  }

  return AssertionFailure()
         << "Expected: (" << s1_expression << ") != (" << s2_expression
         << "), actual: " << PrintToString(s1) << " vs " << PrintToString(s2);
}

bool String::CaseInsensitiveCStringEquals(const char* lhs, const char* rhs) {
  if (lhs == nullptr) return rhs == nullptr;
  if (rhs == nullptr) return false;
  return posix::StrCaseCmp(lhs, rhs) == 0;
}

bool String::CaseInsensitiveWideCStringEquals(const wchar_t* lhs,
                                              const wchar_t* rhs) {
  if (lhs == nullptr) return rhs == nullptr;

  if (rhs == nullptr) return false;

#ifdef GTEST_OS_WINDOWS
  return _wcsicmp(lhs, rhs) == 0;
#elif defined(GTEST_OS_LINUX) && !defined(GTEST_OS_LINUX_ANDROID)
  return wcscasecmp(lhs, rhs) == 0;
#else

  wint_t left, right;
  do {
    left = towlower(static_cast<wint_t>(*lhs++));
    right = towlower(static_cast<wint_t>(*rhs++));
  } while (left && left == right);
  return left == right;
#endif
}

bool String::EndsWithCaseInsensitive(const std::string& str,
                                     const std::string& suffix) {
  const size_t str_len = str.length();
  const size_t suffix_len = suffix.length();
  return (str_len >= suffix_len) &&
         CaseInsensitiveCStringEquals(str.c_str() + str_len - suffix_len,
                                      suffix.c_str());
}

std::string String::FormatIntWidth2(int value) {
  return FormatIntWidthN(value, 2);
}

std::string String::FormatIntWidthN(int value, int width) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(width) << value;
  return ss.str();
}

std::string String::FormatHexUInt32(uint32_t value) {
  std::stringstream ss;
  ss << std::hex << std::uppercase << value;
  return ss.str();
}

std::string String::FormatHexInt(int value) {
  return FormatHexUInt32(static_cast<uint32_t>(value));
}

std::string String::FormatByte(unsigned char value) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << std::hex << std::uppercase
     << static_cast<unsigned int>(value);
  return ss.str();
}

std::string StringStreamToString(::std::stringstream* ss) {
  const ::std::string& str = ss->str();
  const char* const start = str.c_str();
  const char* const end = start + str.length();

  std::string result;
  result.reserve(static_cast<size_t>(2 * (end - start)));
  for (const char* ch = start; ch != end; ++ch) {
    if (*ch == '\0') {
      result += "\\0";
    } else {
      result += *ch;
    }
  }

  return result;
}

std::string AppendUserMessage(const std::string& gtest_msg,
                              const Message& user_msg) {

  const std::string user_msg_string = user_msg.GetString();
  if (user_msg_string.empty()) {
    return gtest_msg;
  }
  if (gtest_msg.empty()) {
    return user_msg_string;
  }
  return gtest_msg + "\n" + user_msg_string;
}

}

TestResult::TestResult()
    : death_test_count_(0), start_timestamp_(0), elapsed_time_(0) {}

TestResult::~TestResult() = default;

const TestPartResult& TestResult::GetTestPartResult(int i) const {
  if (i < 0 || i >= total_part_count()) internal::posix::Abort();
  return test_part_results_.at(static_cast<size_t>(i));
}

const TestProperty& TestResult::GetTestProperty(int i) const {
  if (i < 0 || i >= test_property_count()) internal::posix::Abort();
  return test_properties_.at(static_cast<size_t>(i));
}

void TestResult::ClearTestPartResults() { test_part_results_.clear(); }

void TestResult::AddTestPartResult(const TestPartResult& test_part_result) {
  test_part_results_.push_back(test_part_result);
}

void TestResult::RecordProperty(const std::string& xml_element,
                                const TestProperty& test_property) {
  if (!ValidateTestProperty(xml_element, test_property)) {
    return;
  }
  internal::MutexLock lock(&test_properties_mutex_);
  const std::vector<TestProperty>::iterator property_with_matching_key =
      std::find_if(test_properties_.begin(), test_properties_.end(),
                   internal::TestPropertyKeyIs(test_property.key()));
  if (property_with_matching_key == test_properties_.end()) {
    test_properties_.push_back(test_property);
    return;
  }
  property_with_matching_key->SetValue(test_property.value());
}

static const char* const kReservedTestSuitesAttributes[] = {
    "disabled",    "errors", "failures", "name",
    "random_seed", "tests",  "time",     "timestamp"};

static const char* const kReservedTestSuiteAttributes[] = {
    "disabled", "errors", "failures",  "name",
    "tests",    "time",   "timestamp", "skipped"};

static const char* const kReservedTestCaseAttributes[] = {
    "classname",  "name",        "status", "time",
    "type_param", "value_param", "file",   "line"};

static const char* const kReservedOutputTestCaseAttributes[] = {
    "classname",   "name", "status", "time",   "type_param",
    "value_param", "file", "line",   "result", "timestamp"};

template <size_t kSize>
std::vector<std::string> ArrayAsVector(const char* const (&array)[kSize]) {
  return std::vector<std::string>(array, array + kSize);
}

static std::vector<std::string> GetReservedAttributesForElement(
    const std::string& xml_element) {
  if (xml_element == "testsuites") {
    return ArrayAsVector(kReservedTestSuitesAttributes);
  } else if (xml_element == "testsuite") {
    return ArrayAsVector(kReservedTestSuiteAttributes);
  } else if (xml_element == "testcase") {
    return ArrayAsVector(kReservedTestCaseAttributes);
  } else {
    GTEST_CHECK_(false) << "Unrecognized xml_element provided: " << xml_element;
  }

  return std::vector<std::string>();
}

#if GTEST_HAS_FILE_SYSTEM

static std::vector<std::string> GetReservedOutputAttributesForElement(
    const std::string& xml_element) {
  if (xml_element == "testsuites") {
    return ArrayAsVector(kReservedTestSuitesAttributes);
  } else if (xml_element == "testsuite") {
    return ArrayAsVector(kReservedTestSuiteAttributes);
  } else if (xml_element == "testcase") {
    return ArrayAsVector(kReservedOutputTestCaseAttributes);
  } else {
    GTEST_CHECK_(false) << "Unrecognized xml_element provided: " << xml_element;
  }

  return std::vector<std::string>();
}
#endif

static std::string FormatWordList(const std::vector<std::string>& words) {
  Message word_list;
  for (size_t i = 0; i < words.size(); ++i) {
    if (i > 0 && words.size() > 2) {
      word_list << ", ";
    }
    if (i == words.size() - 1) {
      word_list << "and ";
    }
    word_list << "'" << words[i] << "'";
  }
  return word_list.GetString();
}

static bool ValidateTestPropertyName(
    const std::string& property_name,
    const std::vector<std::string>& reserved_names) {
  if (std::find(reserved_names.begin(), reserved_names.end(), property_name) !=
      reserved_names.end()) {
    ADD_FAILURE() << "Reserved key used in RecordProperty(): " << property_name
                  << " (" << FormatWordList(reserved_names)
                  << " are reserved by " << GTEST_NAME_ << ")";
    return false;
  }
  return true;
}

bool TestResult::ValidateTestProperty(const std::string& xml_element,
                                      const TestProperty& test_property) {
  return ValidateTestPropertyName(test_property.key(),
                                  GetReservedAttributesForElement(xml_element));
}

void TestResult::Clear() {
  test_part_results_.clear();
  test_properties_.clear();
  death_test_count_ = 0;
  elapsed_time_ = 0;
}

static bool TestPartSkipped(const TestPartResult& result) {
  return result.skipped();
}

bool TestResult::Skipped() const {
  return !Failed() && CountIf(test_part_results_, TestPartSkipped) > 0;
}

bool TestResult::Failed() const {
  for (int i = 0; i < total_part_count(); ++i) {
    if (GetTestPartResult(i).failed()) return true;
  }
  return false;
}

static bool TestPartFatallyFailed(const TestPartResult& result) {
  return result.fatally_failed();
}

bool TestResult::HasFatalFailure() const {
  return CountIf(test_part_results_, TestPartFatallyFailed) > 0;
}

static bool TestPartNonfatallyFailed(const TestPartResult& result) {
  return result.nonfatally_failed();
}

bool TestResult::HasNonfatalFailure() const {
  return CountIf(test_part_results_, TestPartNonfatallyFailed) > 0;
}

int TestResult::total_part_count() const {
  return static_cast<int>(test_part_results_.size());
}

int TestResult::test_property_count() const {
  return static_cast<int>(test_properties_.size());
}

Test::Test() : gtest_flag_saver_(new GTEST_FLAG_SAVER_) {}

Test::~Test() = default;

void Test::SetUp() {}

void Test::TearDown() {}

void Test::RecordProperty(const std::string& key, const std::string& value) {
  UnitTest::GetInstance()->RecordProperty(key, value);
}

namespace internal {

void ReportFailureInUnknownLocation(TestPartResult::Type result_type,
                                    const std::string& message) {

  UnitTest::GetInstance()->AddTestPartResult(
      result_type,
      nullptr,
      -1,
      message,
      "");
}

}

bool Test::HasSameFixtureClass() {
  internal::UnitTestImpl* const impl = internal::GetUnitTestImpl();
  const TestSuite* const test_suite = impl->current_test_suite();

  const TestInfo* const first_test_info = test_suite->test_info_list()[0];
  const internal::TypeId first_fixture_id = first_test_info->fixture_class_id_;
  const char* const first_test_name = first_test_info->name();

  const TestInfo* const this_test_info = impl->current_test_info();
  const internal::TypeId this_fixture_id = this_test_info->fixture_class_id_;
  const char* const this_test_name = this_test_info->name();

  if (this_fixture_id != first_fixture_id) {

    const bool first_is_TEST = first_fixture_id == internal::GetTestTypeId();

    const bool this_is_TEST = this_fixture_id == internal::GetTestTypeId();

    if (first_is_TEST || this_is_TEST) {

      const char* const TEST_name =
          first_is_TEST ? first_test_name : this_test_name;
      const char* const TEST_F_name =
          first_is_TEST ? this_test_name : first_test_name;

      ADD_FAILURE()
          << "All tests in the same test suite must use the same test fixture\n"
          << "class, so mixing TEST_F and TEST in the same test suite is\n"
          << "illegal.  In test suite " << this_test_info->test_suite_name()
          << ",\n"
          << "test " << TEST_F_name << " is defined using TEST_F but\n"
          << "test " << TEST_name << " is defined using TEST.  You probably\n"
          << "want to change the TEST to TEST_F or move it to another test\n"
          << "case.";
    } else {

      ADD_FAILURE()
          << "All tests in the same test suite must use the same test fixture\n"
          << "class.  However, in test suite "
          << this_test_info->test_suite_name() << ",\n"
          << "you defined test " << first_test_name << " and test "
          << this_test_name << "\n"
          << "using two different test fixture classes.  This can happen if\n"
          << "the two classes are from different namespaces or translation\n"
          << "units and have the same name.  You should probably rename one\n"
          << "of the classes to put the tests into different test suites.";
    }
    return false;
  }

  return true;
}

namespace internal {

#if GTEST_HAS_EXCEPTIONS

static std::string FormatCxxExceptionMessage(const char* description,
                                             const char* location) {
  Message message;
  if (description != nullptr) {
    message << "C++ exception with description \"" << description << "\"";
  } else {
    message << "Unknown C++ exception";
  }
  message << " thrown in " << location << ".";

  return message.GetString();
}

static std::string PrintTestPartResultToString(
    const TestPartResult& test_part_result);

GoogleTestFailureException::GoogleTestFailureException(
    const TestPartResult& failure)
    : ::std::runtime_error(PrintTestPartResultToString(failure).c_str()) {}

#endif

template <class T, typename Result>
Result HandleSehExceptionsInMethodIfSupported(T* object, Result (T::*method)(),
                                              const char* location) {
#if GTEST_HAS_SEH
  __try {
    return (object->*method)();
  } __except (internal::UnitTestOptions::GTestProcessSEH(
      GetExceptionCode(), location)) {
    return static_cast<Result>(0);
  }
#else
  (void)location;
  return (object->*method)();
#endif
}

template <class T, typename Result>
Result HandleExceptionsInMethodIfSupported(T* object, Result (T::*method)(),
                                           const char* location) {

  if (internal::GetUnitTestImpl()->catch_exceptions()) {
#if GTEST_HAS_EXCEPTIONS
    try {
      return HandleSehExceptionsInMethodIfSupported(object, method, location);
    } catch (const AssertionException&) {

    } catch (const internal::GoogleTestFailureException&) {

      throw;
    } catch (const std::exception& e) {
      internal::ReportFailureInUnknownLocation(
          TestPartResult::kFatalFailure,
          FormatCxxExceptionMessage(e.what(), location));
    } catch (...) {
      internal::ReportFailureInUnknownLocation(
          TestPartResult::kFatalFailure,
          FormatCxxExceptionMessage(nullptr, location));
    }
    return static_cast<Result>(0);
#else
    return HandleSehExceptionsInMethodIfSupported(object, method, location);
#endif
  } else {
    return (object->*method)();
  }
}

}

void Test::Run() {
  if (!HasSameFixtureClass()) return;

  internal::UnitTestImpl* const impl = internal::GetUnitTestImpl();
  impl->os_stack_trace_getter()->UponLeavingGTest();
  internal::HandleExceptionsInMethodIfSupported(this, &Test::SetUp, "SetUp()");

  if (!HasFatalFailure() && !IsSkipped()) {
    impl->os_stack_trace_getter()->UponLeavingGTest();
    internal::HandleExceptionsInMethodIfSupported(this, &Test::TestBody,
                                                  "the test body");
  }

  impl->os_stack_trace_getter()->UponLeavingGTest();
  internal::HandleExceptionsInMethodIfSupported(this, &Test::TearDown,
                                                "TearDown()");
}

bool Test::HasFatalFailure() {
  return internal::GetUnitTestImpl()->current_test_result()->HasFatalFailure();
}

bool Test::HasNonfatalFailure() {
  return internal::GetUnitTestImpl()
      ->current_test_result()
      ->HasNonfatalFailure();
}

bool Test::IsSkipped() {
  return internal::GetUnitTestImpl()->current_test_result()->Skipped();
}

TestInfo::TestInfo(std::string a_test_suite_name, std::string a_name,
                   const char* a_type_param, const char* a_value_param,
                   internal::CodeLocation a_code_location,
                   internal::TypeId fixture_class_id,
                   internal::TestFactoryBase* factory)
    : test_suite_name_(std::move(a_test_suite_name)),
      name_(std::move(a_name)),
      type_param_(a_type_param ? new std::string(a_type_param) : nullptr),
      value_param_(a_value_param ? new std::string(a_value_param) : nullptr),
      location_(std::move(a_code_location)),
      fixture_class_id_(fixture_class_id),
      should_run_(false),
      is_disabled_(false),
      matches_filter_(false),
      is_in_another_shard_(false),
      factory_(factory),
      result_() {}

TestInfo::~TestInfo() { delete factory_; }

namespace internal {

TestInfo* MakeAndRegisterTestInfo(
    std::string test_suite_name, const char* name, const char* type_param,
    const char* value_param, CodeLocation code_location,
    TypeId fixture_class_id, SetUpTestSuiteFunc set_up_tc,
    TearDownTestSuiteFunc tear_down_tc, TestFactoryBase* factory) {
  TestInfo* const test_info =
      new TestInfo(std::move(test_suite_name), name, type_param, value_param,
                   std::move(code_location), fixture_class_id, factory);
  GetUnitTestImpl()->AddTestInfo(set_up_tc, tear_down_tc, test_info);
  return test_info;
}

void ReportInvalidTestSuiteType(const char* test_suite_name,
                                const CodeLocation& code_location) {
  Message errors;
  errors
      << "Attempted redefinition of test suite " << test_suite_name << ".\n"
      << "All tests in the same test suite must use the same test fixture\n"
      << "class.  However, in test suite " << test_suite_name << ", you tried\n"
      << "to define a test using a fixture class different from the one\n"
      << "used earlier. This can happen if the two fixture classes are\n"
      << "from different namespaces and have the same name. You should\n"
      << "probably rename one of the classes to put the tests into different\n"
      << "test suites.";

  GTEST_LOG_(ERROR) << FormatFileLocation(code_location.file.c_str(),
                                          code_location.line)
                    << " " << errors.GetString();
}

void UnitTestImpl::RegisterParameterizedTests() {
  if (!parameterized_tests_registered_) {
    parameterized_test_registry_.RegisterTests();
    type_parameterized_test_registry_.CheckForInstantiations();
    parameterized_tests_registered_ = true;
  }
}

}

void TestInfo::Run() {
  TestEventListener* repeater = UnitTest::GetInstance()->listeners().repeater();
  if (!should_run_) {
    if (is_disabled_ && matches_filter_) repeater->OnTestDisabled(*this);
    return;
  }

  UnitTest::GetInstance()->set_current_test_info(this);

  repeater->OnTestStart(*this);
  result_.set_start_timestamp(internal::GetTimeInMillis());
  internal::Timer timer;
  UnitTest::GetInstance()->UponLeavingGTest();

  Test* const test = internal::HandleExceptionsInMethodIfSupported(
      factory_, &internal::TestFactoryBase::CreateTest,
      "the test fixture's constructor");

  if (!Test::HasFatalFailure() && !Test::IsSkipped()) {

    test->Run();
  }

  if (test != nullptr) {

    UnitTest::GetInstance()->UponLeavingGTest();
    internal::HandleExceptionsInMethodIfSupported(
        test, &Test::DeleteSelf_, "the test fixture's destructor");
  }

  result_.set_elapsed_time(timer.Elapsed());

  repeater->OnTestEnd(*this);

  UnitTest::GetInstance()->set_current_test_info(nullptr);
}

void TestInfo::Skip() {
  if (!should_run_) return;

  UnitTest::GetInstance()->set_current_test_info(this);

  TestEventListener* repeater = UnitTest::GetInstance()->listeners().repeater();

  repeater->OnTestStart(*this);

  const TestPartResult test_part_result =
      TestPartResult(TestPartResult::kSkip, this->file(), this->line(), "");
  internal::GetUnitTestImpl()
      ->GetTestPartResultReporterForCurrentThread()
      ->ReportTestPartResult(test_part_result);

  repeater->OnTestEnd(*this);
  UnitTest::GetInstance()->set_current_test_info(nullptr);
}

int TestSuite::successful_test_count() const {
  return CountIf(test_info_list_, TestPassed);
}

int TestSuite::skipped_test_count() const {
  return CountIf(test_info_list_, TestSkipped);
}

int TestSuite::failed_test_count() const {
  return CountIf(test_info_list_, TestFailed);
}

int TestSuite::reportable_disabled_test_count() const {
  return CountIf(test_info_list_, TestReportableDisabled);
}

int TestSuite::disabled_test_count() const {
  return CountIf(test_info_list_, TestDisabled);
}

int TestSuite::reportable_test_count() const {
  return CountIf(test_info_list_, TestReportable);
}

int TestSuite::test_to_run_count() const {
  return CountIf(test_info_list_, ShouldRunTest);
}

int TestSuite::total_test_count() const {
  return static_cast<int>(test_info_list_.size());
}

TestSuite::TestSuite(const std::string& a_name, const char* a_type_param,
                     internal::SetUpTestSuiteFunc set_up_tc,
                     internal::TearDownTestSuiteFunc tear_down_tc)
    : name_(a_name),
      type_param_(a_type_param ? new std::string(a_type_param) : nullptr),
      set_up_tc_(set_up_tc),
      tear_down_tc_(tear_down_tc),
      should_run_(false),
      start_timestamp_(0),
      elapsed_time_(0) {}

TestSuite::~TestSuite() {

  ForEach(test_info_list_, internal::Delete<TestInfo>);
}

const TestInfo* TestSuite::GetTestInfo(int i) const {
  const int index = GetElementOr(test_indices_, i, -1);
  return index < 0 ? nullptr : test_info_list_[static_cast<size_t>(index)];
}

TestInfo* TestSuite::GetMutableTestInfo(int i) {
  const int index = GetElementOr(test_indices_, i, -1);
  return index < 0 ? nullptr : test_info_list_[static_cast<size_t>(index)];
}

void TestSuite::AddTestInfo(TestInfo* test_info) {
  test_info_list_.push_back(test_info);
  test_indices_.push_back(static_cast<int>(test_indices_.size()));
}

void TestSuite::Run() {
  if (!should_run_) return;

  UnitTest::GetInstance()->set_current_test_suite(this);

  TestEventListener* repeater = UnitTest::GetInstance()->listeners().repeater();

  std::stable_sort(test_info_list_.begin(), test_info_list_.end(),
                   [](const TestInfo* const a, const TestInfo* const b) {
                     if (const int result = std::strcmp(a->file(), b->file())) {
                       return result < 0;
                     }

                     return a->line() < b->line();
                   });

  repeater->OnTestSuiteStart(*this);

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  repeater->OnTestCaseStart(*this);
#endif

  UnitTest::GetInstance()->UponLeavingGTest();
  internal::HandleExceptionsInMethodIfSupported(
      this, &TestSuite::RunSetUpTestSuite, "SetUpTestSuite()");

  const bool skip_all =
      ad_hoc_test_result().Failed() || ad_hoc_test_result().Skipped();

  start_timestamp_ = internal::GetTimeInMillis();
  internal::Timer timer;
  for (int i = 0; i < total_test_count(); i++) {
    if (skip_all) {
      GetMutableTestInfo(i)->Skip();
    } else {
      GetMutableTestInfo(i)->Run();
    }
    if (GTEST_FLAG_GET(fail_fast) &&
        GetMutableTestInfo(i)->result()->Failed()) {
      for (int j = i + 1; j < total_test_count(); j++) {
        GetMutableTestInfo(j)->Skip();
      }
      break;
    }
  }
  elapsed_time_ = timer.Elapsed();

  UnitTest::GetInstance()->UponLeavingGTest();
  internal::HandleExceptionsInMethodIfSupported(
      this, &TestSuite::RunTearDownTestSuite, "TearDownTestSuite()");

  repeater->OnTestSuiteEnd(*this);

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  repeater->OnTestCaseEnd(*this);
#endif

  UnitTest::GetInstance()->set_current_test_suite(nullptr);
}

void TestSuite::Skip() {
  if (!should_run_) return;

  UnitTest::GetInstance()->set_current_test_suite(this);

  TestEventListener* repeater = UnitTest::GetInstance()->listeners().repeater();

  repeater->OnTestSuiteStart(*this);

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  repeater->OnTestCaseStart(*this);
#endif

  for (int i = 0; i < total_test_count(); i++) {
    GetMutableTestInfo(i)->Skip();
  }

  repeater->OnTestSuiteEnd(*this);

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  repeater->OnTestCaseEnd(*this);
#endif

  UnitTest::GetInstance()->set_current_test_suite(nullptr);
}

void TestSuite::ClearResult() {
  ad_hoc_test_result_.Clear();
  ForEach(test_info_list_, TestInfo::ClearTestResult);
}

void TestSuite::ShuffleTests(internal::Random* random) {
  Shuffle(random, &test_indices_);
}

void TestSuite::UnshuffleTests() {
  for (size_t i = 0; i < test_indices_.size(); i++) {
    test_indices_[i] = static_cast<int>(i);
  }
}

static std::string FormatCountableNoun(int count, const char* singular_form,
                                       const char* plural_form) {
  return internal::StreamableToString(count) + " " +
         (count == 1 ? singular_form : plural_form);
}

static std::string FormatTestCount(int test_count) {
  return FormatCountableNoun(test_count, "test", "tests");
}

static std::string FormatTestSuiteCount(int test_suite_count) {
  return FormatCountableNoun(test_suite_count, "test suite", "test suites");
}

static const char* TestPartResultTypeToString(TestPartResult::Type type) {
  switch (type) {
    case TestPartResult::kSkip:
      return "Skipped\n";
    case TestPartResult::kSuccess:
      return "Success";

    case TestPartResult::kNonFatalFailure:
    case TestPartResult::kFatalFailure:
#ifdef _MSC_VER
      return "error: ";
#else
      return "Failure\n";
#endif
    default:
      return "Unknown result type";
  }
}

namespace internal {
namespace {
enum class GTestColor { kDefault, kRed, kGreen, kYellow };
}

static std::string PrintTestPartResultToString(
    const TestPartResult& test_part_result) {
  return (Message() << internal::FormatFileLocation(
                           test_part_result.file_name(),
                           test_part_result.line_number())
                    << " "
                    << TestPartResultTypeToString(test_part_result.type())
                    << test_part_result.message())
      .GetString();
}

static void PrintTestPartResult(const TestPartResult& test_part_result) {
  const std::string& result = PrintTestPartResultToString(test_part_result);
  printf("%s\n", result.c_str());
  fflush(stdout);

#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MOBILE)

  ::OutputDebugStringA(result.c_str());
  ::OutputDebugStringA("\n");
#endif
}

#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MOBILE) &&       \
    !defined(GTEST_OS_WINDOWS_GAMES) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT) && !defined(GTEST_OS_WINDOWS_MINGW)

static WORD GetColorAttribute(GTestColor color) {
  switch (color) {
    case GTestColor::kRed:
      return FOREGROUND_RED;
    case GTestColor::kGreen:
      return FOREGROUND_GREEN;
    case GTestColor::kYellow:
      return FOREGROUND_RED | FOREGROUND_GREEN;
    default:
      return 0;
  }
}

static int GetBitOffset(WORD color_mask) {
  if (color_mask == 0) return 0;

  int bitOffset = 0;
  while ((color_mask & 1) == 0) {
    color_mask >>= 1;
    ++bitOffset;
  }
  return bitOffset;
}

static WORD GetNewColor(GTestColor color, WORD old_color_attrs) {

  static const WORD background_mask = BACKGROUND_BLUE | BACKGROUND_GREEN |
                                      BACKGROUND_RED | BACKGROUND_INTENSITY;
  static const WORD foreground_mask = FOREGROUND_BLUE | FOREGROUND_GREEN |
                                      FOREGROUND_RED | FOREGROUND_INTENSITY;
  const WORD existing_bg = old_color_attrs & background_mask;

  WORD new_color =
      GetColorAttribute(color) | existing_bg | FOREGROUND_INTENSITY;
  static const int bg_bitOffset = GetBitOffset(background_mask);
  static const int fg_bitOffset = GetBitOffset(foreground_mask);

  if (((new_color & background_mask) >> bg_bitOffset) ==
      ((new_color & foreground_mask) >> fg_bitOffset)) {
    new_color ^= FOREGROUND_INTENSITY;
  }
  return new_color;
}

#else

static const char* GetAnsiColorCode(GTestColor color) {
  switch (color) {
    case GTestColor::kRed:
      return "1";
    case GTestColor::kGreen:
      return "2";
    case GTestColor::kYellow:
      return "3";
    default:
      assert(false);
      return "9";
  }
}

#endif

bool ShouldUseColor(bool stdout_is_tty) {
  std::string c = GTEST_FLAG_GET(color);
  const char* const gtest_color = c.c_str();

  if (String::CaseInsensitiveCStringEquals(gtest_color, "auto")) {
#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MINGW)

    return stdout_is_tty;
#else

    const char* const term = posix::GetEnv("TERM");
    const bool term_supports_color =
        term != nullptr && (String::CStringEquals(term, "xterm") ||
                            String::CStringEquals(term, "xterm-color") ||
                            String::CStringEquals(term, "xterm-kitty") ||
                            String::CStringEquals(term, "alacritty") ||
                            String::CStringEquals(term, "screen") ||
                            String::CStringEquals(term, "tmux") ||
                            String::CStringEquals(term, "rxvt-unicode") ||
                            String::CStringEquals(term, "linux") ||
                            String::CStringEquals(term, "cygwin") ||
                            String::EndsWithCaseInsensitive(term, "-256color"));
    return stdout_is_tty && term_supports_color;
#endif
  }

  return String::CaseInsensitiveCStringEquals(gtest_color, "yes") ||
         String::CaseInsensitiveCStringEquals(gtest_color, "true") ||
         String::CaseInsensitiveCStringEquals(gtest_color, "t") ||
         String::CStringEquals(gtest_color, "1");

}

GTEST_ATTRIBUTE_PRINTF_(2, 3)
static void ColoredPrintf(GTestColor color, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  static const bool in_color_mode =

      ShouldUseColor(posix::IsATTY(posix::FileNo(stdout)) != 0);

  const bool use_color = in_color_mode && (color != GTestColor::kDefault);

  if (!use_color) {
    vprintf(fmt, args);
    va_end(args);
    return;
  }

#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MOBILE) &&       \
    !defined(GTEST_OS_WINDOWS_GAMES) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT) && !defined(GTEST_OS_WINDOWS_MINGW)
  const HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

  CONSOLE_SCREEN_BUFFER_INFO buffer_info;
  GetConsoleScreenBufferInfo(stdout_handle, &buffer_info);
  const WORD old_color_attrs = buffer_info.wAttributes;
  const WORD new_color = GetNewColor(color, old_color_attrs);

  fflush(stdout);
  SetConsoleTextAttribute(stdout_handle, new_color);

  vprintf(fmt, args);

  fflush(stdout);

  SetConsoleTextAttribute(stdout_handle, old_color_attrs);
#else
  printf("\033[0;3%sm", GetAnsiColorCode(color));
  vprintf(fmt, args);
  printf("\033[m");
#endif
  va_end(args);
}

static const char kTypeParamLabel[] = "TypeParam";
static const char kValueParamLabel[] = "GetParam()";

static void PrintFullTestCommentIfPresent(const TestInfo& test_info) {
  const char* const type_param = test_info.type_param();
  const char* const value_param = test_info.value_param();

  if (type_param != nullptr || value_param != nullptr) {
    printf(", where ");
    if (type_param != nullptr) {
      printf("%s = %s", kTypeParamLabel, type_param);
      if (value_param != nullptr) printf(" and ");
    }
    if (value_param != nullptr) {
      printf("%s = %s", kValueParamLabel, value_param);
    }
  }
}

class PrettyUnitTestResultPrinter : public TestEventListener {
 public:
  PrettyUnitTestResultPrinter() = default;
  static void PrintTestName(const char* test_suite, const char* test) {
    printf("%s.%s", test_suite, test);
  }

  void OnTestProgramStart(const UnitTest&              ) override {}
  void OnTestIterationStart(const UnitTest& unit_test, int iteration) override;
  void OnEnvironmentsSetUpStart(const UnitTest& unit_test) override;
  void OnEnvironmentsSetUpEnd(const UnitTest&              ) override {}
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseStart(const TestCase& test_case) override;
#else
  void OnTestSuiteStart(const TestSuite& test_suite) override;
#endif

  void OnTestStart(const TestInfo& test_info) override;
  void OnTestDisabled(const TestInfo& test_info) override;

  void OnTestPartResult(const TestPartResult& result) override;
  void OnTestEnd(const TestInfo& test_info) override;
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseEnd(const TestCase& test_case) override;
#else
  void OnTestSuiteEnd(const TestSuite& test_suite) override;
#endif

  void OnEnvironmentsTearDownStart(const UnitTest& unit_test) override;
  void OnEnvironmentsTearDownEnd(const UnitTest&              ) override {}
  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;
  void OnTestProgramEnd(const UnitTest&              ) override {}

 private:
  static void PrintFailedTests(const UnitTest& unit_test);
  static void PrintFailedTestSuites(const UnitTest& unit_test);
  static void PrintSkippedTests(const UnitTest& unit_test);
};

void PrettyUnitTestResultPrinter::OnTestIterationStart(
    const UnitTest& unit_test, int iteration) {
  if (GTEST_FLAG_GET(repeat) != 1)
    printf("\nRepeating all tests (iteration %d) . . .\n\n", iteration + 1);

  std::string f = GTEST_FLAG_GET(filter);
  const char* const filter = f.c_str();

  if (!String::CStringEquals(filter, kUniversalFilter)) {
    ColoredPrintf(GTestColor::kYellow, "Note: %s filter = %s\n", GTEST_NAME_,
                  filter);
  }

  if (internal::ShouldShard(kTestTotalShards, kTestShardIndex, false)) {
    const int32_t shard_index = Int32FromEnvOrDie(kTestShardIndex, -1);
    ColoredPrintf(GTestColor::kYellow, "Note: This is test shard %d of %s.\n",
                  static_cast<int>(shard_index) + 1,
                  internal::posix::GetEnv(kTestTotalShards));
  }

  if (GTEST_FLAG_GET(shuffle)) {
    ColoredPrintf(GTestColor::kYellow,
                  "Note: Randomizing tests' orders with a seed of %d .\n",
                  unit_test.random_seed());
  }

  ColoredPrintf(GTestColor::kGreen, "[==========] ");
  printf("Running %s from %s.\n",
         FormatTestCount(unit_test.test_to_run_count()).c_str(),
         FormatTestSuiteCount(unit_test.test_suite_to_run_count()).c_str());
  fflush(stdout);
}

void PrettyUnitTestResultPrinter::OnEnvironmentsSetUpStart(
    const UnitTest&              ) {
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("Global test environment set-up.\n");
  fflush(stdout);
}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
void PrettyUnitTestResultPrinter::OnTestCaseStart(const TestCase& test_case) {
  const std::string counts =
      FormatCountableNoun(test_case.test_to_run_count(), "test", "tests");
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("%s from %s", counts.c_str(), test_case.name());
  if (test_case.type_param() == nullptr) {
    printf("\n");
  } else {
    printf(", where %s = %s\n", kTypeParamLabel, test_case.type_param());
  }
  fflush(stdout);
}
#else
void PrettyUnitTestResultPrinter::OnTestSuiteStart(
    const TestSuite& test_suite) {
  const std::string counts =
      FormatCountableNoun(test_suite.test_to_run_count(), "test", "tests");
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("%s from %s", counts.c_str(), test_suite.name());
  if (test_suite.type_param() == nullptr) {
    printf("\n");
  } else {
    printf(", where %s = %s\n", kTypeParamLabel, test_suite.type_param());
  }
  fflush(stdout);
}
#endif

void PrettyUnitTestResultPrinter::OnTestStart(const TestInfo& test_info) {
  ColoredPrintf(GTestColor::kGreen, "[ RUN      ] ");
  PrintTestName(test_info.test_suite_name(), test_info.name());
  printf("\n");
  fflush(stdout);
}

void PrettyUnitTestResultPrinter::OnTestDisabled(const TestInfo& test_info) {
  ColoredPrintf(GTestColor::kYellow, "[ DISABLED ] ");
  PrintTestName(test_info.test_suite_name(), test_info.name());
  printf("\n");
  fflush(stdout);
}

void PrettyUnitTestResultPrinter::OnTestPartResult(
    const TestPartResult& result) {
  switch (result.type()) {

    case TestPartResult::kSuccess:
      return;
    default:

      PrintTestPartResult(result);
      fflush(stdout);
  }
}

void PrettyUnitTestResultPrinter::OnTestEnd(const TestInfo& test_info) {
  if (test_info.result()->Passed()) {
    ColoredPrintf(GTestColor::kGreen, "[       OK ] ");
  } else if (test_info.result()->Skipped()) {
    ColoredPrintf(GTestColor::kGreen, "[  SKIPPED ] ");
  } else {
    ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
  }
  PrintTestName(test_info.test_suite_name(), test_info.name());
  if (test_info.result()->Failed()) PrintFullTestCommentIfPresent(test_info);

  if (GTEST_FLAG_GET(print_time)) {
    printf(" (%s ms)\n",
           internal::StreamableToString(test_info.result()->elapsed_time())
               .c_str());
  } else {
    printf("\n");
  }
  fflush(stdout);
}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
void PrettyUnitTestResultPrinter::OnTestCaseEnd(const TestCase& test_case) {
  if (!GTEST_FLAG_GET(print_time)) return;

  const std::string counts =
      FormatCountableNoun(test_case.test_to_run_count(), "test", "tests");
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("%s from %s (%s ms total)\n\n", counts.c_str(), test_case.name(),
         internal::StreamableToString(test_case.elapsed_time()).c_str());
  fflush(stdout);
}
#else
void PrettyUnitTestResultPrinter::OnTestSuiteEnd(const TestSuite& test_suite) {
  if (!GTEST_FLAG_GET(print_time)) return;

  const std::string counts =
      FormatCountableNoun(test_suite.test_to_run_count(), "test", "tests");
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("%s from %s (%s ms total)\n\n", counts.c_str(), test_suite.name(),
         internal::StreamableToString(test_suite.elapsed_time()).c_str());
  fflush(stdout);
}
#endif

void PrettyUnitTestResultPrinter::OnEnvironmentsTearDownStart(
    const UnitTest&              ) {
  ColoredPrintf(GTestColor::kGreen, "[----------] ");
  printf("Global test environment tear-down\n");
  fflush(stdout);
}

void PrettyUnitTestResultPrinter::PrintFailedTests(const UnitTest& unit_test) {
  const int failed_test_count = unit_test.failed_test_count();
  ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
  printf("%s, listed below:\n", FormatTestCount(failed_test_count).c_str());

  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    const TestSuite& test_suite = *unit_test.GetTestSuite(i);
    if (!test_suite.should_run() || (test_suite.failed_test_count() == 0)) {
      continue;
    }
    for (int j = 0; j < test_suite.total_test_count(); ++j) {
      const TestInfo& test_info = *test_suite.GetTestInfo(j);
      if (!test_info.should_run() || !test_info.result()->Failed()) {
        continue;
      }
      ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
      printf("%s.%s", test_suite.name(), test_info.name());
      PrintFullTestCommentIfPresent(test_info);
      printf("\n");
    }
  }
  printf("\n%2d FAILED %s\n", failed_test_count,
         failed_test_count == 1 ? "TEST" : "TESTS");
}

void PrettyUnitTestResultPrinter::PrintFailedTestSuites(
    const UnitTest& unit_test) {
  int suite_failure_count = 0;
  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    const TestSuite& test_suite = *unit_test.GetTestSuite(i);
    if (!test_suite.should_run()) {
      continue;
    }
    if (test_suite.ad_hoc_test_result().Failed()) {
      ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
      printf("%s: SetUpTestSuite or TearDownTestSuite\n", test_suite.name());
      ++suite_failure_count;
    }
  }
  if (suite_failure_count > 0) {
    printf("\n%2d FAILED TEST %s\n", suite_failure_count,
           suite_failure_count == 1 ? "SUITE" : "SUITES");
  }
}

void PrettyUnitTestResultPrinter::PrintSkippedTests(const UnitTest& unit_test) {
  const int skipped_test_count = unit_test.skipped_test_count();
  if (skipped_test_count == 0) {
    return;
  }

  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    const TestSuite& test_suite = *unit_test.GetTestSuite(i);
    if (!test_suite.should_run() || (test_suite.skipped_test_count() == 0)) {
      continue;
    }
    for (int j = 0; j < test_suite.total_test_count(); ++j) {
      const TestInfo& test_info = *test_suite.GetTestInfo(j);
      if (!test_info.should_run() || !test_info.result()->Skipped()) {
        continue;
      }
      ColoredPrintf(GTestColor::kGreen, "[  SKIPPED ] ");
      printf("%s.%s", test_suite.name(), test_info.name());
      printf("\n");
    }
  }
}

void PrettyUnitTestResultPrinter::OnTestIterationEnd(const UnitTest& unit_test,
                                                     int              ) {
  ColoredPrintf(GTestColor::kGreen, "[==========] ");
  printf("%s from %s ran.",
         FormatTestCount(unit_test.test_to_run_count()).c_str(),
         FormatTestSuiteCount(unit_test.test_suite_to_run_count()).c_str());
  if (GTEST_FLAG_GET(print_time)) {
    printf(" (%s ms total)",
           internal::StreamableToString(unit_test.elapsed_time()).c_str());
  }
  printf("\n");
  ColoredPrintf(GTestColor::kGreen, "[  PASSED  ] ");
  printf("%s.\n", FormatTestCount(unit_test.successful_test_count()).c_str());

  const int skipped_test_count = unit_test.skipped_test_count();
  if (skipped_test_count > 0) {
    ColoredPrintf(GTestColor::kGreen, "[  SKIPPED ] ");
    printf("%s, listed below:\n", FormatTestCount(skipped_test_count).c_str());
    PrintSkippedTests(unit_test);
  }

  if (!unit_test.Passed()) {
    PrintFailedTests(unit_test);
    PrintFailedTestSuites(unit_test);
  }

  int num_disabled = unit_test.reportable_disabled_test_count();
  if (num_disabled && !GTEST_FLAG_GET(also_run_disabled_tests)) {
    if (unit_test.Passed()) {
      printf("\n");
    }
    ColoredPrintf(GTestColor::kYellow, "  YOU HAVE %d DISABLED %s\n\n",
                  num_disabled, num_disabled == 1 ? "TEST" : "TESTS");
  }

  fflush(stdout);
}

class BriefUnitTestResultPrinter : public TestEventListener {
 public:
  BriefUnitTestResultPrinter() = default;
  static void PrintTestName(const char* test_suite, const char* test) {
    printf("%s.%s", test_suite, test);
  }

  void OnTestProgramStart(const UnitTest&              ) override {}
  void OnTestIterationStart(const UnitTest&              ,
                            int              ) override {}
  void OnEnvironmentsSetUpStart(const UnitTest&              ) override {}
  void OnEnvironmentsSetUpEnd(const UnitTest&              ) override {}
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseStart(const TestCase&              ) override {}
#else
  void OnTestSuiteStart(const TestSuite&               ) override {}
#endif

  void OnTestStart(const TestInfo&              ) override {}
  void OnTestDisabled(const TestInfo&              ) override {}

  void OnTestPartResult(const TestPartResult& result) override;
  void OnTestEnd(const TestInfo& test_info) override;
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseEnd(const TestCase&              ) override {}
#else
  void OnTestSuiteEnd(const TestSuite&               ) override {}
#endif

  void OnEnvironmentsTearDownStart(const UnitTest&              ) override {}
  void OnEnvironmentsTearDownEnd(const UnitTest&              ) override {}
  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;
  void OnTestProgramEnd(const UnitTest&              ) override {}
};

void BriefUnitTestResultPrinter::OnTestPartResult(
    const TestPartResult& result) {
  switch (result.type()) {

    case TestPartResult::kSuccess:
      return;
    default:

      PrintTestPartResult(result);
      fflush(stdout);
  }
}

void BriefUnitTestResultPrinter::OnTestEnd(const TestInfo& test_info) {
  if (test_info.result()->Failed()) {
    ColoredPrintf(GTestColor::kRed, "[  FAILED  ] ");
    PrintTestName(test_info.test_suite_name(), test_info.name());
    PrintFullTestCommentIfPresent(test_info);

    if (GTEST_FLAG_GET(print_time)) {
      printf(" (%s ms)\n",
             internal::StreamableToString(test_info.result()->elapsed_time())
                 .c_str());
    } else {
      printf("\n");
    }
    fflush(stdout);
  }
}

void BriefUnitTestResultPrinter::OnTestIterationEnd(const UnitTest& unit_test,
                                                    int              ) {
  ColoredPrintf(GTestColor::kGreen, "[==========] ");
  printf("%s from %s ran.",
         FormatTestCount(unit_test.test_to_run_count()).c_str(),
         FormatTestSuiteCount(unit_test.test_suite_to_run_count()).c_str());
  if (GTEST_FLAG_GET(print_time)) {
    printf(" (%s ms total)",
           internal::StreamableToString(unit_test.elapsed_time()).c_str());
  }
  printf("\n");
  ColoredPrintf(GTestColor::kGreen, "[  PASSED  ] ");
  printf("%s.\n", FormatTestCount(unit_test.successful_test_count()).c_str());

  const int skipped_test_count = unit_test.skipped_test_count();
  if (skipped_test_count > 0) {
    ColoredPrintf(GTestColor::kGreen, "[  SKIPPED ] ");
    printf("%s.\n", FormatTestCount(skipped_test_count).c_str());
  }

  int num_disabled = unit_test.reportable_disabled_test_count();
  if (num_disabled && !GTEST_FLAG_GET(also_run_disabled_tests)) {
    if (unit_test.Passed()) {
      printf("\n");
    }
    ColoredPrintf(GTestColor::kYellow, "  YOU HAVE %d DISABLED %s\n\n",
                  num_disabled, num_disabled == 1 ? "TEST" : "TESTS");
  }

  fflush(stdout);
}

class TestEventRepeater : public TestEventListener {
 public:
  TestEventRepeater() : forwarding_enabled_(true) {}
  ~TestEventRepeater() override;
  void Append(TestEventListener* listener);
  TestEventListener* Release(TestEventListener* listener);

  bool forwarding_enabled() const { return forwarding_enabled_; }
  void set_forwarding_enabled(bool enable) { forwarding_enabled_ = enable; }

  void OnTestProgramStart(const UnitTest& parameter) override;
  void OnTestIterationStart(const UnitTest& unit_test, int iteration) override;
  void OnEnvironmentsSetUpStart(const UnitTest& parameter) override;
  void OnEnvironmentsSetUpEnd(const UnitTest& parameter) override;

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseStart(const TestSuite& parameter) override;
#endif
  void OnTestSuiteStart(const TestSuite& parameter) override;
  void OnTestStart(const TestInfo& parameter) override;
  void OnTestDisabled(const TestInfo& parameter) override;
  void OnTestPartResult(const TestPartResult& parameter) override;
  void OnTestEnd(const TestInfo& parameter) override;

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseEnd(const TestCase& parameter) override;
#endif
  void OnTestSuiteEnd(const TestSuite& parameter) override;
  void OnEnvironmentsTearDownStart(const UnitTest& parameter) override;
  void OnEnvironmentsTearDownEnd(const UnitTest& parameter) override;
  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;
  void OnTestProgramEnd(const UnitTest& parameter) override;

 private:

  bool forwarding_enabled_;

  std::vector<TestEventListener*> listeners_;

  TestEventRepeater(const TestEventRepeater&) = delete;
  TestEventRepeater& operator=(const TestEventRepeater&) = delete;
};

TestEventRepeater::~TestEventRepeater() {
  ForEach(listeners_, Delete<TestEventListener>);
}

void TestEventRepeater::Append(TestEventListener* listener) {
  listeners_.push_back(listener);
}

TestEventListener* TestEventRepeater::Release(TestEventListener* listener) {
  for (size_t i = 0; i < listeners_.size(); ++i) {
    if (listeners_[i] == listener) {
      listeners_.erase(listeners_.begin() + static_cast<int>(i));
      return listener;
    }
  }

  return nullptr;
}

#define GTEST_REPEATER_METHOD_(Name, Type)              \
  void TestEventRepeater::Name(const Type& parameter) { \
    if (forwarding_enabled_) {                          \
      for (size_t i = 0; i < listeners_.size(); i++) {  \
        listeners_[i]->Name(parameter);                 \
      }                                                 \
    }                                                   \
  }

#define GTEST_REVERSE_REPEATER_METHOD_(Name, Type)      \
  void TestEventRepeater::Name(const Type& parameter) { \
    if (forwarding_enabled_) {                          \
      for (size_t i = listeners_.size(); i != 0; i--) { \
        listeners_[i - 1]->Name(parameter);             \
      }                                                 \
    }                                                   \
  }

GTEST_REPEATER_METHOD_(OnTestProgramStart, UnitTest)
GTEST_REPEATER_METHOD_(OnEnvironmentsSetUpStart, UnitTest)

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
GTEST_REPEATER_METHOD_(OnTestCaseStart, TestSuite)
#endif
GTEST_REPEATER_METHOD_(OnTestSuiteStart, TestSuite)
GTEST_REPEATER_METHOD_(OnTestStart, TestInfo)
GTEST_REPEATER_METHOD_(OnTestDisabled, TestInfo)
GTEST_REPEATER_METHOD_(OnTestPartResult, TestPartResult)
GTEST_REPEATER_METHOD_(OnEnvironmentsTearDownStart, UnitTest)
GTEST_REVERSE_REPEATER_METHOD_(OnEnvironmentsSetUpEnd, UnitTest)
GTEST_REVERSE_REPEATER_METHOD_(OnEnvironmentsTearDownEnd, UnitTest)
GTEST_REVERSE_REPEATER_METHOD_(OnTestEnd, TestInfo)

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
GTEST_REVERSE_REPEATER_METHOD_(OnTestCaseEnd, TestSuite)
#endif
GTEST_REVERSE_REPEATER_METHOD_(OnTestSuiteEnd, TestSuite)
GTEST_REVERSE_REPEATER_METHOD_(OnTestProgramEnd, UnitTest)

#undef GTEST_REPEATER_METHOD_
#undef GTEST_REVERSE_REPEATER_METHOD_

void TestEventRepeater::OnTestIterationStart(const UnitTest& unit_test,
                                             int iteration) {
  if (forwarding_enabled_) {
    for (size_t i = 0; i < listeners_.size(); i++) {
      listeners_[i]->OnTestIterationStart(unit_test, iteration);
    }
  }
}

void TestEventRepeater::OnTestIterationEnd(const UnitTest& unit_test,
                                           int iteration) {
  if (forwarding_enabled_) {
    for (size_t i = listeners_.size(); i > 0; i--) {
      listeners_[i - 1]->OnTestIterationEnd(unit_test, iteration);
    }
  }
}

#if GTEST_HAS_FILE_SYSTEM

class XmlUnitTestResultPrinter : public EmptyTestEventListener {
 public:
  explicit XmlUnitTestResultPrinter(const char* output_file);

  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;
  void ListTestsMatchingFilter(const std::vector<TestSuite*>& test_suites);

  static void PrintXmlTestsList(std::ostream* stream,
                                const std::vector<TestSuite*>& test_suites);

 private:

  static bool IsNormalizableWhitespace(unsigned char c) {
    return c == '\t' || c == '\n' || c == '\r';
  }

  static bool IsValidXmlCharacter(unsigned char c) {
    return IsNormalizableWhitespace(c) || c >= 0x20;
  }

  static std::string EscapeXml(const std::string& str, bool is_attribute);

  static std::string RemoveInvalidXmlCharacters(const std::string& str);

  static std::string EscapeXmlAttribute(const std::string& str) {
    return EscapeXml(str, true);
  }

  static std::string EscapeXmlText(const char* str) {
    return EscapeXml(str, false);
  }

  static void OutputXmlAttribute(std::ostream* stream,
                                 const std::string& element_name,
                                 const std::string& name,
                                 const std::string& value);

  static void OutputXmlCDataSection(::std::ostream* stream, const char* data);

  static void OutputXmlTestSuiteForTestResult(::std::ostream* stream,
                                              const TestResult& result);

  static void OutputXmlTestCaseForTestResult(::std::ostream* stream,
                                             const TestResult& result);

  static void OutputXmlTestResult(::std::ostream* stream,
                                  const TestResult& result);

  static void OutputXmlTestInfo(::std::ostream* stream,
                                const char* test_suite_name,
                                const TestInfo& test_info);

  static void PrintXmlTestSuite(::std::ostream* stream,
                                const TestSuite& test_suite);

  static void PrintXmlUnitTest(::std::ostream* stream,
                               const UnitTest& unit_test);

  static void OutputXmlTestProperties(std::ostream* stream,
                                      const TestResult& result,
                                      const std::string& indent);

  const std::string output_file_;

  XmlUnitTestResultPrinter(const XmlUnitTestResultPrinter&) = delete;
  XmlUnitTestResultPrinter& operator=(const XmlUnitTestResultPrinter&) = delete;
};

XmlUnitTestResultPrinter::XmlUnitTestResultPrinter(const char* output_file)
    : output_file_(output_file) {
  if (output_file_.empty()) {
    GTEST_LOG_(FATAL) << "XML output file may not be null";
  }
}

void XmlUnitTestResultPrinter::OnTestIterationEnd(const UnitTest& unit_test,
                                                  int              ) {
  FILE* xmlout = OpenFileForWriting(output_file_);
  std::stringstream stream;
  PrintXmlUnitTest(&stream, unit_test);
  fprintf(xmlout, "%s", StringStreamToString(&stream).c_str());
  fclose(xmlout);
}

void XmlUnitTestResultPrinter::ListTestsMatchingFilter(
    const std::vector<TestSuite*>& test_suites) {
  FILE* xmlout = OpenFileForWriting(output_file_);
  std::stringstream stream;
  PrintXmlTestsList(&stream, test_suites);
  fprintf(xmlout, "%s", StringStreamToString(&stream).c_str());
  fclose(xmlout);
}

std::string XmlUnitTestResultPrinter::EscapeXml(const std::string& str,
                                                bool is_attribute) {
  Message m;

  for (size_t i = 0; i < str.size(); ++i) {
    const char ch = str[i];
    switch (ch) {
      case '<':
        m << "&lt;";
        break;
      case '>':
        m << "&gt;";
        break;
      case '&':
        m << "&amp;";
        break;
      case '\'':
        if (is_attribute)
          m << "&apos;";
        else
          m << '\'';
        break;
      case '"':
        if (is_attribute)
          m << "&quot;";
        else
          m << '"';
        break;
      default:
        if (IsValidXmlCharacter(static_cast<unsigned char>(ch))) {
          if (is_attribute &&
              IsNormalizableWhitespace(static_cast<unsigned char>(ch)))
            m << "&#x" << String::FormatByte(static_cast<unsigned char>(ch))
              << ";";
          else
            m << ch;
        }
        break;
    }
  }

  return m.GetString();
}

std::string XmlUnitTestResultPrinter::RemoveInvalidXmlCharacters(
    const std::string& str) {
  std::string output;
  output.reserve(str.size());
  for (std::string::const_iterator it = str.begin(); it != str.end(); ++it)
    if (IsValidXmlCharacter(static_cast<unsigned char>(*it)))
      output.push_back(*it);

  return output;
}

std::string FormatTimeInMillisAsSeconds(TimeInMillis ms) {
  ::std::stringstream ss;

  ss << std::fixed
     << std::setprecision(
            ms % 1000 == 0 ? 0 : (ms % 100 == 0 ? 1 : (ms % 10 == 0 ? 2 : 3)))
     << std::showpoint;
  ss << (static_cast<double>(ms) * 1e-3);
  return ss.str();
}

static bool PortableLocaltime(time_t seconds, struct tm* out) {
#if defined(_MSC_VER)
  return localtime_s(out, &seconds) == 0;
#elif defined(__MINGW32__) || defined(__MINGW64__)

  struct tm* tm_ptr = localtime(&seconds);
  if (tm_ptr == nullptr) return false;
  *out = *tm_ptr;
  return true;
#elif defined(__STDC_LIB_EXT1__)

  return localtime_s(&seconds, out) != nullptr;
#else
  return localtime_r(&seconds, out) != nullptr;
#endif
}

std::string FormatEpochTimeInMillisAsIso8601(TimeInMillis ms) {
  struct tm time_struct;
  if (!PortableLocaltime(static_cast<time_t>(ms / 1000), &time_struct))
    return "";

  return StreamableToString(time_struct.tm_year + 1900) + "-" +
         String::FormatIntWidth2(time_struct.tm_mon + 1) + "-" +
         String::FormatIntWidth2(time_struct.tm_mday) + "T" +
         String::FormatIntWidth2(time_struct.tm_hour) + ":" +
         String::FormatIntWidth2(time_struct.tm_min) + ":" +
         String::FormatIntWidth2(time_struct.tm_sec) + "." +
         String::FormatIntWidthN(static_cast<int>(ms % 1000), 3);
}

void XmlUnitTestResultPrinter::OutputXmlCDataSection(::std::ostream* stream,
                                                     const char* data) {
  const char* segment = data;
  *stream << "<![CDATA[";
  for (;;) {
    const char* const next_segment = strstr(segment, "]]>");
    if (next_segment != nullptr) {
      stream->write(segment,
                    static_cast<std::streamsize>(next_segment - segment));
      *stream << "]]>]]&gt;<![CDATA[";
      segment = next_segment + strlen("]]>");
    } else {
      *stream << segment;
      break;
    }
  }
  *stream << "]]>";
}

void XmlUnitTestResultPrinter::OutputXmlAttribute(
    std::ostream* stream, const std::string& element_name,
    const std::string& name, const std::string& value) {
  const std::vector<std::string>& allowed_names =
      GetReservedOutputAttributesForElement(element_name);

  GTEST_CHECK_(std::find(allowed_names.begin(), allowed_names.end(), name) !=
               allowed_names.end())
      << "Attribute " << name << " is not allowed for element <" << element_name
      << ">.";

  *stream << " " << name << "=\"" << EscapeXmlAttribute(value) << "\"";
}

void XmlUnitTestResultPrinter::OutputXmlTestSuiteForTestResult(
    ::std::ostream* stream, const TestResult& result) {

  *stream << "  <testsuite";
  OutputXmlAttribute(stream, "testsuite", "name", "NonTestSuiteFailure");
  OutputXmlAttribute(stream, "testsuite", "tests", "1");
  OutputXmlAttribute(stream, "testsuite", "failures", "1");
  OutputXmlAttribute(stream, "testsuite", "disabled", "0");
  OutputXmlAttribute(stream, "testsuite", "skipped", "0");
  OutputXmlAttribute(stream, "testsuite", "errors", "0");
  OutputXmlAttribute(stream, "testsuite", "time",
                     FormatTimeInMillisAsSeconds(result.elapsed_time()));
  OutputXmlAttribute(
      stream, "testsuite", "timestamp",
      FormatEpochTimeInMillisAsIso8601(result.start_timestamp()));
  *stream << ">";

  OutputXmlTestCaseForTestResult(stream, result);

  *stream << "  </testsuite>\n";
}

void XmlUnitTestResultPrinter::OutputXmlTestCaseForTestResult(
    ::std::ostream* stream, const TestResult& result) {

  *stream << "    <testcase";
  OutputXmlAttribute(stream, "testcase", "name", "");
  OutputXmlAttribute(stream, "testcase", "status", "run");
  OutputXmlAttribute(stream, "testcase", "result", "completed");
  OutputXmlAttribute(stream, "testcase", "classname", "");
  OutputXmlAttribute(stream, "testcase", "time",
                     FormatTimeInMillisAsSeconds(result.elapsed_time()));
  OutputXmlAttribute(
      stream, "testcase", "timestamp",
      FormatEpochTimeInMillisAsIso8601(result.start_timestamp()));

  OutputXmlTestResult(stream, result);
}

void XmlUnitTestResultPrinter::OutputXmlTestInfo(::std::ostream* stream,
                                                 const char* test_suite_name,
                                                 const TestInfo& test_info) {
  const TestResult& result = *test_info.result();
  const std::string kTestsuite = "testcase";

  if (test_info.is_in_another_shard()) {
    return;
  }

  *stream << "    <testcase";
  OutputXmlAttribute(stream, kTestsuite, "name", test_info.name());

  if (test_info.value_param() != nullptr) {
    OutputXmlAttribute(stream, kTestsuite, "value_param",
                       test_info.value_param());
  }
  if (test_info.type_param() != nullptr) {
    OutputXmlAttribute(stream, kTestsuite, "type_param",
                       test_info.type_param());
  }

  OutputXmlAttribute(stream, kTestsuite, "file", test_info.file());
  OutputXmlAttribute(stream, kTestsuite, "line",
                     StreamableToString(test_info.line()));
  if (GTEST_FLAG_GET(list_tests)) {
    *stream << " />\n";
    return;
  }

  OutputXmlAttribute(stream, kTestsuite, "status",
                     test_info.should_run() ? "run" : "notrun");
  OutputXmlAttribute(stream, kTestsuite, "result",
                     test_info.should_run()
                         ? (result.Skipped() ? "skipped" : "completed")
                         : "suppressed");
  OutputXmlAttribute(stream, kTestsuite, "time",
                     FormatTimeInMillisAsSeconds(result.elapsed_time()));
  OutputXmlAttribute(
      stream, kTestsuite, "timestamp",
      FormatEpochTimeInMillisAsIso8601(result.start_timestamp()));
  OutputXmlAttribute(stream, kTestsuite, "classname", test_suite_name);

  OutputXmlTestResult(stream, result);
}

void XmlUnitTestResultPrinter::OutputXmlTestResult(::std::ostream* stream,
                                                   const TestResult& result) {
  int failures = 0;
  int skips = 0;
  for (int i = 0; i < result.total_part_count(); ++i) {
    const TestPartResult& part = result.GetTestPartResult(i);
    if (part.failed()) {
      if (++failures == 1 && skips == 0) {
        *stream << ">\n";
      }
      const std::string location =
          internal::FormatCompilerIndependentFileLocation(part.file_name(),
                                                          part.line_number());
      const std::string summary = location + "\n" + part.summary();
      *stream << "      <failure message=\"" << EscapeXmlAttribute(summary)
              << "\" type=\"\">";
      const std::string detail = location + "\n" + part.message();
      OutputXmlCDataSection(stream, RemoveInvalidXmlCharacters(detail).c_str());
      *stream << "</failure>\n";
    } else if (part.skipped()) {
      if (++skips == 1 && failures == 0) {
        *stream << ">\n";
      }
      const std::string location =
          internal::FormatCompilerIndependentFileLocation(part.file_name(),
                                                          part.line_number());
      const std::string summary = location + "\n" + part.summary();
      *stream << "      <skipped message=\""
              << EscapeXmlAttribute(summary.c_str()) << "\">";
      const std::string detail = location + "\n" + part.message();
      OutputXmlCDataSection(stream, RemoveInvalidXmlCharacters(detail).c_str());
      *stream << "</skipped>\n";
    }
  }

  if (failures == 0 && skips == 0 && result.test_property_count() == 0) {
    *stream << " />\n";
  } else {
    if (failures == 0 && skips == 0) {
      *stream << ">\n";
    }
    OutputXmlTestProperties(stream, result,            "      ");
    *stream << "    </testcase>\n";
  }
}

void XmlUnitTestResultPrinter::PrintXmlTestSuite(std::ostream* stream,
                                                 const TestSuite& test_suite) {
  const std::string kTestsuite = "testsuite";
  *stream << "  <" << kTestsuite;
  OutputXmlAttribute(stream, kTestsuite, "name", test_suite.name());
  OutputXmlAttribute(stream, kTestsuite, "tests",
                     StreamableToString(test_suite.reportable_test_count()));
  if (!GTEST_FLAG_GET(list_tests)) {
    OutputXmlAttribute(stream, kTestsuite, "failures",
                       StreamableToString(test_suite.failed_test_count()));
    OutputXmlAttribute(
        stream, kTestsuite, "disabled",
        StreamableToString(test_suite.reportable_disabled_test_count()));
    OutputXmlAttribute(stream, kTestsuite, "skipped",
                       StreamableToString(test_suite.skipped_test_count()));

    OutputXmlAttribute(stream, kTestsuite, "errors", "0");

    OutputXmlAttribute(stream, kTestsuite, "time",
                       FormatTimeInMillisAsSeconds(test_suite.elapsed_time()));
    OutputXmlAttribute(
        stream, kTestsuite, "timestamp",
        FormatEpochTimeInMillisAsIso8601(test_suite.start_timestamp()));
  }
  *stream << ">\n";
  OutputXmlTestProperties(stream, test_suite.ad_hoc_test_result(),
                                     "    ");
  for (int i = 0; i < test_suite.total_test_count(); ++i) {
    if (test_suite.GetTestInfo(i)->is_reportable())
      OutputXmlTestInfo(stream, test_suite.name(), *test_suite.GetTestInfo(i));
  }
  if (test_suite.ad_hoc_test_result().Failed()) {
    OutputXmlTestCaseForTestResult(stream, test_suite.ad_hoc_test_result());
  }

  *stream << "  </" << kTestsuite << ">\n";
}

void XmlUnitTestResultPrinter::PrintXmlUnitTest(std::ostream* stream,
                                                const UnitTest& unit_test) {
  const std::string kTestsuites = "testsuites";

  *stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  *stream << "<" << kTestsuites;

  OutputXmlAttribute(stream, kTestsuites, "tests",
                     StreamableToString(unit_test.reportable_test_count()));
  OutputXmlAttribute(stream, kTestsuites, "failures",
                     StreamableToString(unit_test.failed_test_count()));
  OutputXmlAttribute(
      stream, kTestsuites, "disabled",
      StreamableToString(unit_test.reportable_disabled_test_count()));
  OutputXmlAttribute(stream, kTestsuites, "errors", "0");
  OutputXmlAttribute(stream, kTestsuites, "time",
                     FormatTimeInMillisAsSeconds(unit_test.elapsed_time()));
  OutputXmlAttribute(
      stream, kTestsuites, "timestamp",
      FormatEpochTimeInMillisAsIso8601(unit_test.start_timestamp()));

  if (GTEST_FLAG_GET(shuffle)) {
    OutputXmlAttribute(stream, kTestsuites, "random_seed",
                       StreamableToString(unit_test.random_seed()));
  }

  OutputXmlAttribute(stream, kTestsuites, "name", "AllTests");
  *stream << ">\n";

  OutputXmlTestProperties(stream, unit_test.ad_hoc_test_result(),
                                     "  ");
  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    if (unit_test.GetTestSuite(i)->reportable_test_count() > 0)
      PrintXmlTestSuite(stream, *unit_test.GetTestSuite(i));
  }

  if (unit_test.ad_hoc_test_result().Failed()) {
    OutputXmlTestSuiteForTestResult(stream, unit_test.ad_hoc_test_result());
  }

  *stream << "</" << kTestsuites << ">\n";
}

void XmlUnitTestResultPrinter::PrintXmlTestsList(
    std::ostream* stream, const std::vector<TestSuite*>& test_suites) {
  const std::string kTestsuites = "testsuites";

  *stream << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  *stream << "<" << kTestsuites;

  int total_tests = 0;
  for (auto test_suite : test_suites) {
    total_tests += test_suite->total_test_count();
  }
  OutputXmlAttribute(stream, kTestsuites, "tests",
                     StreamableToString(total_tests));
  OutputXmlAttribute(stream, kTestsuites, "name", "AllTests");
  *stream << ">\n";

  for (auto test_suite : test_suites) {
    PrintXmlTestSuite(stream, *test_suite);
  }
  *stream << "</" << kTestsuites << ">\n";
}

void XmlUnitTestResultPrinter::OutputXmlTestProperties(
    std::ostream* stream, const TestResult& result, const std::string& indent) {
  const std::string kProperties = "properties";
  const std::string kProperty = "property";

  if (result.test_property_count() <= 0) {
    return;
  }

  *stream << indent << "<" << kProperties << ">\n";
  for (int i = 0; i < result.test_property_count(); ++i) {
    const TestProperty& property = result.GetTestProperty(i);
    *stream << indent << "  <" << kProperty;
    *stream << " name=\"" << EscapeXmlAttribute(property.key()) << "\"";
    *stream << " value=\"" << EscapeXmlAttribute(property.value()) << "\"";
    *stream << "/>\n";
  }
  *stream << indent << "</" << kProperties << ">\n";
}

#endif

#if GTEST_HAS_FILE_SYSTEM

class JsonUnitTestResultPrinter : public EmptyTestEventListener {
 public:
  explicit JsonUnitTestResultPrinter(const char* output_file);

  void OnTestIterationEnd(const UnitTest& unit_test, int iteration) override;

  static void PrintJsonTestList(::std::ostream* stream,
                                const std::vector<TestSuite*>& test_suites);

 private:

  static std::string EscapeJson(const std::string& str);

  static void OutputJsonKey(std::ostream* stream,
                            const std::string& element_name,
                            const std::string& name, const std::string& value,
                            const std::string& indent, bool comma = true);
  static void OutputJsonKey(std::ostream* stream,
                            const std::string& element_name,
                            const std::string& name, int value,
                            const std::string& indent, bool comma = true);

  static void OutputJsonTestSuiteForTestResult(::std::ostream* stream,
                                               const TestResult& result);

  static void OutputJsonTestCaseForTestResult(::std::ostream* stream,
                                              const TestResult& result);

  static void OutputJsonTestResult(::std::ostream* stream,
                                   const TestResult& result);

  static void OutputJsonTestInfo(::std::ostream* stream,
                                 const char* test_suite_name,
                                 const TestInfo& test_info);

  static void PrintJsonTestSuite(::std::ostream* stream,
                                 const TestSuite& test_suite);

  static void PrintJsonUnitTest(::std::ostream* stream,
                                const UnitTest& unit_test);

  static std::string TestPropertiesAsJson(const TestResult& result,
                                          const std::string& indent);

  const std::string output_file_;

  JsonUnitTestResultPrinter(const JsonUnitTestResultPrinter&) = delete;
  JsonUnitTestResultPrinter& operator=(const JsonUnitTestResultPrinter&) =
      delete;
};

JsonUnitTestResultPrinter::JsonUnitTestResultPrinter(const char* output_file)
    : output_file_(output_file) {
  if (output_file_.empty()) {
    GTEST_LOG_(FATAL) << "JSON output file may not be null";
  }
}

void JsonUnitTestResultPrinter::OnTestIterationEnd(const UnitTest& unit_test,
                                                   int              ) {
  FILE* jsonout = OpenFileForWriting(output_file_);
  std::stringstream stream;
  PrintJsonUnitTest(&stream, unit_test);
  fprintf(jsonout, "%s", StringStreamToString(&stream).c_str());
  fclose(jsonout);
}

std::string JsonUnitTestResultPrinter::EscapeJson(const std::string& str) {
  Message m;

  for (size_t i = 0; i < str.size(); ++i) {
    const char ch = str[i];
    switch (ch) {
      case '\\':
      case '"':
      case '/':
        m << '\\' << ch;
        break;
      case '\b':
        m << "\\b";
        break;
      case '\t':
        m << "\\t";
        break;
      case '\n':
        m << "\\n";
        break;
      case '\f':
        m << "\\f";
        break;
      case '\r':
        m << "\\r";
        break;
      default:
        if (ch < ' ') {
          m << "\\u00" << String::FormatByte(static_cast<unsigned char>(ch));
        } else {
          m << ch;
        }
        break;
    }
  }

  return m.GetString();
}

static std::string FormatTimeInMillisAsDuration(TimeInMillis ms) {
  ::std::stringstream ss;
  ss << (static_cast<double>(ms) * 1e-3) << "s";
  return ss.str();
}

static std::string FormatEpochTimeInMillisAsRFC3339(TimeInMillis ms) {
  struct tm time_struct;
  if (!PortableLocaltime(static_cast<time_t>(ms / 1000), &time_struct))
    return "";

  return StreamableToString(time_struct.tm_year + 1900) + "-" +
         String::FormatIntWidth2(time_struct.tm_mon + 1) + "-" +
         String::FormatIntWidth2(time_struct.tm_mday) + "T" +
         String::FormatIntWidth2(time_struct.tm_hour) + ":" +
         String::FormatIntWidth2(time_struct.tm_min) + ":" +
         String::FormatIntWidth2(time_struct.tm_sec) + "Z";
}

static inline std::string Indent(size_t width) {
  return std::string(width, ' ');
}

void JsonUnitTestResultPrinter::OutputJsonKey(std::ostream* stream,
                                              const std::string& element_name,
                                              const std::string& name,
                                              const std::string& value,
                                              const std::string& indent,
                                              bool comma) {
  const std::vector<std::string>& allowed_names =
      GetReservedOutputAttributesForElement(element_name);

  GTEST_CHECK_(std::find(allowed_names.begin(), allowed_names.end(), name) !=
               allowed_names.end())
      << "Key \"" << name << "\" is not allowed for value \"" << element_name
      << "\".";

  *stream << indent << "\"" << name << "\": \"" << EscapeJson(value) << "\"";
  if (comma) *stream << ",\n";
}

void JsonUnitTestResultPrinter::OutputJsonKey(
    std::ostream* stream, const std::string& element_name,
    const std::string& name, int value, const std::string& indent, bool comma) {
  const std::vector<std::string>& allowed_names =
      GetReservedOutputAttributesForElement(element_name);

  GTEST_CHECK_(std::find(allowed_names.begin(), allowed_names.end(), name) !=
               allowed_names.end())
      << "Key \"" << name << "\" is not allowed for value \"" << element_name
      << "\".";

  *stream << indent << "\"" << name << "\": " << StreamableToString(value);
  if (comma) *stream << ",\n";
}

void JsonUnitTestResultPrinter::OutputJsonTestSuiteForTestResult(
    ::std::ostream* stream, const TestResult& result) {

  *stream << Indent(4) << "{\n";
  OutputJsonKey(stream, "testsuite", "name", "NonTestSuiteFailure", Indent(6));
  OutputJsonKey(stream, "testsuite", "tests", 1, Indent(6));
  if (!GTEST_FLAG_GET(list_tests)) {
    OutputJsonKey(stream, "testsuite", "failures", 1, Indent(6));
    OutputJsonKey(stream, "testsuite", "disabled", 0, Indent(6));
    OutputJsonKey(stream, "testsuite", "skipped", 0, Indent(6));
    OutputJsonKey(stream, "testsuite", "errors", 0, Indent(6));
    OutputJsonKey(stream, "testsuite", "time",
                  FormatTimeInMillisAsDuration(result.elapsed_time()),
                  Indent(6));
    OutputJsonKey(stream, "testsuite", "timestamp",
                  FormatEpochTimeInMillisAsRFC3339(result.start_timestamp()),
                  Indent(6));
  }
  *stream << Indent(6) << "\"testsuite\": [\n";

  OutputJsonTestCaseForTestResult(stream, result);

  *stream << "\n" << Indent(6) << "]\n" << Indent(4) << "}";
}

void JsonUnitTestResultPrinter::OutputJsonTestCaseForTestResult(
    ::std::ostream* stream, const TestResult& result) {

  *stream << Indent(8) << "{\n";
  OutputJsonKey(stream, "testcase", "name", "", Indent(10));
  OutputJsonKey(stream, "testcase", "status", "RUN", Indent(10));
  OutputJsonKey(stream, "testcase", "result", "COMPLETED", Indent(10));
  OutputJsonKey(stream, "testcase", "timestamp",
                FormatEpochTimeInMillisAsRFC3339(result.start_timestamp()),
                Indent(10));
  OutputJsonKey(stream, "testcase", "time",
                FormatTimeInMillisAsDuration(result.elapsed_time()),
                Indent(10));
  OutputJsonKey(stream, "testcase", "classname", "", Indent(10), false);
  *stream << TestPropertiesAsJson(result, Indent(10));

  OutputJsonTestResult(stream, result);
}

void JsonUnitTestResultPrinter::OutputJsonTestInfo(::std::ostream* stream,
                                                   const char* test_suite_name,
                                                   const TestInfo& test_info) {
  const TestResult& result = *test_info.result();
  const std::string kTestsuite = "testcase";
  const std::string kIndent = Indent(10);

  *stream << Indent(8) << "{\n";
  OutputJsonKey(stream, kTestsuite, "name", test_info.name(), kIndent);

  if (test_info.value_param() != nullptr) {
    OutputJsonKey(stream, kTestsuite, "value_param", test_info.value_param(),
                  kIndent);
  }
  if (test_info.type_param() != nullptr) {
    OutputJsonKey(stream, kTestsuite, "type_param", test_info.type_param(),
                  kIndent);
  }

  OutputJsonKey(stream, kTestsuite, "file", test_info.file(), kIndent);
  OutputJsonKey(stream, kTestsuite, "line", test_info.line(), kIndent, false);
  if (GTEST_FLAG_GET(list_tests)) {
    *stream << "\n" << Indent(8) << "}";
    return;
  } else {
    *stream << ",\n";
  }

  OutputJsonKey(stream, kTestsuite, "status",
                test_info.should_run() ? "RUN" : "NOTRUN", kIndent);
  OutputJsonKey(stream, kTestsuite, "result",
                test_info.should_run()
                    ? (result.Skipped() ? "SKIPPED" : "COMPLETED")
                    : "SUPPRESSED",
                kIndent);
  OutputJsonKey(stream, kTestsuite, "timestamp",
                FormatEpochTimeInMillisAsRFC3339(result.start_timestamp()),
                kIndent);
  OutputJsonKey(stream, kTestsuite, "time",
                FormatTimeInMillisAsDuration(result.elapsed_time()), kIndent);
  OutputJsonKey(stream, kTestsuite, "classname", test_suite_name, kIndent,
                false);
  *stream << TestPropertiesAsJson(result, kIndent);

  OutputJsonTestResult(stream, result);
}

void JsonUnitTestResultPrinter::OutputJsonTestResult(::std::ostream* stream,
                                                     const TestResult& result) {
  const std::string kIndent = Indent(10);

  {
    int failures = 0;
    for (int i = 0; i < result.total_part_count(); ++i) {
      const TestPartResult& part = result.GetTestPartResult(i);
      if (part.failed()) {
        *stream << ",\n";
        if (++failures == 1) {
          *stream << kIndent << "\"" << "failures" << "\": [\n";
        }
        const std::string location =
            internal::FormatCompilerIndependentFileLocation(part.file_name(),
                                                            part.line_number());
        const std::string message =
            EscapeJson(location + "\n" + part.message());
        *stream << kIndent << "  {\n"
                << kIndent << "    \"failure\": \"" << message << "\",\n"
                << kIndent << "    \"type\": \"\"\n"
                << kIndent << "  }";
      }
    }

    if (failures > 0) *stream << "\n" << kIndent << "]";
  }

  {
    int skipped = 0;
    for (int i = 0; i < result.total_part_count(); ++i) {
      const TestPartResult& part = result.GetTestPartResult(i);
      if (part.skipped()) {
        *stream << ",\n";
        if (++skipped == 1) {
          *stream << kIndent << "\"" << "skipped" << "\": [\n";
        }
        const std::string location =
            internal::FormatCompilerIndependentFileLocation(part.file_name(),
                                                            part.line_number());
        const std::string message =
            EscapeJson(location + "\n" + part.message());
        *stream << kIndent << "  {\n"
                << kIndent << "    \"message\": \"" << message << "\"\n"
                << kIndent << "  }";
      }
    }

    if (skipped > 0) *stream << "\n" << kIndent << "]";
  }

  *stream << "\n" << Indent(8) << "}";
}

void JsonUnitTestResultPrinter::PrintJsonTestSuite(
    std::ostream* stream, const TestSuite& test_suite) {
  const std::string kTestsuite = "testsuite";
  const std::string kIndent = Indent(6);

  *stream << Indent(4) << "{\n";
  OutputJsonKey(stream, kTestsuite, "name", test_suite.name(), kIndent);
  OutputJsonKey(stream, kTestsuite, "tests", test_suite.reportable_test_count(),
                kIndent);
  if (!GTEST_FLAG_GET(list_tests)) {
    OutputJsonKey(stream, kTestsuite, "failures",
                  test_suite.failed_test_count(), kIndent);
    OutputJsonKey(stream, kTestsuite, "disabled",
                  test_suite.reportable_disabled_test_count(), kIndent);
    OutputJsonKey(stream, kTestsuite, "errors", 0, kIndent);
    OutputJsonKey(
        stream, kTestsuite, "timestamp",
        FormatEpochTimeInMillisAsRFC3339(test_suite.start_timestamp()),
        kIndent);
    OutputJsonKey(stream, kTestsuite, "time",
                  FormatTimeInMillisAsDuration(test_suite.elapsed_time()),
                  kIndent, false);
    *stream << TestPropertiesAsJson(test_suite.ad_hoc_test_result(), kIndent)
            << ",\n";
  }

  *stream << kIndent << "\"" << kTestsuite << "\": [\n";

  bool comma = false;
  for (int i = 0; i < test_suite.total_test_count(); ++i) {
    if (test_suite.GetTestInfo(i)->is_reportable()) {
      if (comma) {
        *stream << ",\n";
      } else {
        comma = true;
      }
      OutputJsonTestInfo(stream, test_suite.name(), *test_suite.GetTestInfo(i));
    }
  }

  if (test_suite.ad_hoc_test_result().Failed()) {
    if (comma) {
      *stream << ",\n";
    }
    OutputJsonTestCaseForTestResult(stream, test_suite.ad_hoc_test_result());
  }

  *stream << "\n" << kIndent << "]\n" << Indent(4) << "}";
}

void JsonUnitTestResultPrinter::PrintJsonUnitTest(std::ostream* stream,
                                                  const UnitTest& unit_test) {
  const std::string kTestsuites = "testsuites";
  const std::string kIndent = Indent(2);
  *stream << "{\n";

  OutputJsonKey(stream, kTestsuites, "tests", unit_test.reportable_test_count(),
                kIndent);
  OutputJsonKey(stream, kTestsuites, "failures", unit_test.failed_test_count(),
                kIndent);
  OutputJsonKey(stream, kTestsuites, "disabled",
                unit_test.reportable_disabled_test_count(), kIndent);
  OutputJsonKey(stream, kTestsuites, "errors", 0, kIndent);
  if (GTEST_FLAG_GET(shuffle)) {
    OutputJsonKey(stream, kTestsuites, "random_seed", unit_test.random_seed(),
                  kIndent);
  }
  OutputJsonKey(stream, kTestsuites, "timestamp",
                FormatEpochTimeInMillisAsRFC3339(unit_test.start_timestamp()),
                kIndent);
  OutputJsonKey(stream, kTestsuites, "time",
                FormatTimeInMillisAsDuration(unit_test.elapsed_time()), kIndent,
                false);

  *stream << TestPropertiesAsJson(unit_test.ad_hoc_test_result(), kIndent)
          << ",\n";

  OutputJsonKey(stream, kTestsuites, "name", "AllTests", kIndent);
  *stream << kIndent << "\"" << kTestsuites << "\": [\n";

  bool comma = false;
  for (int i = 0; i < unit_test.total_test_suite_count(); ++i) {
    if (unit_test.GetTestSuite(i)->reportable_test_count() > 0) {
      if (comma) {
        *stream << ",\n";
      } else {
        comma = true;
      }
      PrintJsonTestSuite(stream, *unit_test.GetTestSuite(i));
    }
  }

  if (unit_test.ad_hoc_test_result().Failed()) {
    if (comma) {
      *stream << ",\n";
    }
    OutputJsonTestSuiteForTestResult(stream, unit_test.ad_hoc_test_result());
  }

  *stream << "\n"
          << kIndent << "]\n"
          << "}\n";
}

void JsonUnitTestResultPrinter::PrintJsonTestList(
    std::ostream* stream, const std::vector<TestSuite*>& test_suites) {
  const std::string kTestsuites = "testsuites";
  const std::string kIndent = Indent(2);
  *stream << "{\n";
  int total_tests = 0;
  for (auto test_suite : test_suites) {
    total_tests += test_suite->total_test_count();
  }
  OutputJsonKey(stream, kTestsuites, "tests", total_tests, kIndent);

  OutputJsonKey(stream, kTestsuites, "name", "AllTests", kIndent);
  *stream << kIndent << "\"" << kTestsuites << "\": [\n";

  for (size_t i = 0; i < test_suites.size(); ++i) {
    if (i != 0) {
      *stream << ",\n";
    }
    PrintJsonTestSuite(stream, *test_suites[i]);
  }

  *stream << "\n"
          << kIndent << "]\n"
          << "}\n";
}

std::string JsonUnitTestResultPrinter::TestPropertiesAsJson(
    const TestResult& result, const std::string& indent) {
  Message attributes;
  for (int i = 0; i < result.test_property_count(); ++i) {
    const TestProperty& property = result.GetTestProperty(i);
    attributes << ",\n"
               << indent << "\"" << property.key() << "\": " << "\""
               << EscapeJson(property.value()) << "\"";
  }
  return attributes.GetString();
}

#endif

#if GTEST_CAN_STREAM_RESULTS_

std::string StreamingListener::UrlEncode(const char* str) {
  std::string result;
  result.reserve(strlen(str) + 1);
  for (char ch = *str; ch != '\0'; ch = *++str) {
    switch (ch) {
      case '%':
      case '=':
      case '&':
      case '\n':
        result.push_back('%');
        result.append(String::FormatByte(static_cast<unsigned char>(ch)));
        break;
      default:
        result.push_back(ch);
        break;
    }
  }
  return result;
}

void StreamingListener::SocketWriter::MakeConnection() {
  GTEST_CHECK_(sockfd_ == -1)
      << "MakeConnection() can't be called when there is already a connection.";

  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* servinfo = nullptr;

  const int error_num =
      getaddrinfo(host_name_.c_str(), port_num_.c_str(), &hints, &servinfo);
  if (error_num != 0) {
    GTEST_LOG_(WARNING) << "stream_result_to: getaddrinfo() failed: "
                        << gai_strerror(error_num);
  }

  for (addrinfo* cur_addr = servinfo; sockfd_ == -1 && cur_addr != nullptr;
       cur_addr = cur_addr->ai_next) {
    sockfd_ = socket(cur_addr->ai_family, cur_addr->ai_socktype,
                     cur_addr->ai_protocol);
    if (sockfd_ != -1) {

      if (connect(sockfd_, cur_addr->ai_addr, cur_addr->ai_addrlen) == -1) {
        close(sockfd_);
        sockfd_ = -1;
      }
    }
  }

  freeaddrinfo(servinfo);

  if (sockfd_ == -1) {
    GTEST_LOG_(WARNING) << "stream_result_to: failed to connect to "
                        << host_name_ << ":" << port_num_;
  }
}

#endif

const char* const OsStackTraceGetterInterface::kElidedFramesMarker =
    "... " GTEST_NAME_ " internal frames ...";

std::string OsStackTraceGetter::CurrentStackTrace(int max_depth, int skip_count)
    GTEST_LOCK_EXCLUDED_(mutex_) {
#ifdef GTEST_HAS_ABSL
  std::string result;

  if (max_depth <= 0) {
    return result;
  }

  max_depth = std::min(max_depth, kMaxStackTraceDepth);

  std::vector<void*> raw_stack(max_depth);

  const int raw_stack_size =
      absl::GetStackTrace(&raw_stack[0], max_depth, skip_count + 1);

  void* caller_frame = nullptr;
  {
    MutexLock lock(&mutex_);
    caller_frame = caller_frame_;
  }

  for (int i = 0; i < raw_stack_size; ++i) {
    if (raw_stack[i] == caller_frame &&
        !GTEST_FLAG_GET(show_internal_stack_frames)) {

      absl::StrAppend(&result, kElidedFramesMarker, "\n");
      break;
    }

    char tmp[1024];
    const char* symbol = "(unknown)";
    if (absl::Symbolize(raw_stack[i], tmp, sizeof(tmp))) {
      symbol = tmp;
    }

    char line[1024];
    snprintf(line, sizeof(line), "  %p: %s\n", raw_stack[i], symbol);
    result += line;
  }

  return result;

#else
  static_cast<void>(max_depth);
  static_cast<void>(skip_count);
  return "";
#endif
}

void OsStackTraceGetter::UponLeavingGTest() GTEST_LOCK_EXCLUDED_(mutex_) {
#ifdef GTEST_HAS_ABSL
  void* caller_frame = nullptr;
  if (absl::GetStackTrace(&caller_frame, 1, 3) <= 0) {
    caller_frame = nullptr;
  }

  MutexLock lock(&mutex_);
  caller_frame_ = caller_frame;
#endif
}

#ifdef GTEST_HAS_DEATH_TEST

class ScopedPrematureExitFile {
 public:
  explicit ScopedPrematureExitFile(const char* premature_exit_filepath)
      : premature_exit_filepath_(
            premature_exit_filepath ? premature_exit_filepath : "") {

    if (!premature_exit_filepath_.empty()) {

      FILE* pfile = posix::FOpen(premature_exit_filepath_.c_str(), "w");
      fwrite("0", 1, 1, pfile);
      fclose(pfile);
    }
  }

  ~ScopedPrematureExitFile() {
#ifndef GTEST_OS_ESP8266
    if (!premature_exit_filepath_.empty()) {
      int retval = remove(premature_exit_filepath_.c_str());
      if (retval) {
        GTEST_LOG_(ERROR) << "Failed to remove premature exit filepath \""
                          << premature_exit_filepath_ << "\" with error "
                          << retval;
      }
    }
#endif
  }

 private:
  const std::string premature_exit_filepath_;

  ScopedPrematureExitFile(const ScopedPrematureExitFile&) = delete;
  ScopedPrematureExitFile& operator=(const ScopedPrematureExitFile&) = delete;
};
#endif

}

TestEventListeners::TestEventListeners()
    : repeater_(new internal::TestEventRepeater()),
      default_result_printer_(nullptr),
      default_xml_generator_(nullptr) {}

TestEventListeners::~TestEventListeners() { delete repeater_; }

void TestEventListeners::Append(TestEventListener* listener) {
  repeater_->Append(listener);
}

TestEventListener* TestEventListeners::Release(TestEventListener* listener) {
  if (listener == default_result_printer_)
    default_result_printer_ = nullptr;
  else if (listener == default_xml_generator_)
    default_xml_generator_ = nullptr;
  return repeater_->Release(listener);
}

TestEventListener* TestEventListeners::repeater() { return repeater_; }

void TestEventListeners::SetDefaultResultPrinter(TestEventListener* listener) {
  if (default_result_printer_ != listener) {

    delete Release(default_result_printer_);
    default_result_printer_ = listener;
    if (listener != nullptr) Append(listener);
  }
}

void TestEventListeners::SetDefaultXmlGenerator(TestEventListener* listener) {
  if (default_xml_generator_ != listener) {

    delete Release(default_xml_generator_);
    default_xml_generator_ = listener;
    if (listener != nullptr) Append(listener);
  }
}

bool TestEventListeners::EventForwardingEnabled() const {
  return repeater_->forwarding_enabled();
}

void TestEventListeners::SuppressEventForwarding(bool suppress) {
  repeater_->set_forwarding_enabled(!suppress);
}

UnitTest* UnitTest::GetInstance() {

#if defined(__BORLANDC__)
  static UnitTest* const instance = new UnitTest;
  return instance;
#else
  static UnitTest instance;
  return &instance;
#endif
}

int UnitTest::successful_test_suite_count() const {
  return impl()->successful_test_suite_count();
}

int UnitTest::failed_test_suite_count() const {
  return impl()->failed_test_suite_count();
}

int UnitTest::total_test_suite_count() const {
  return impl()->total_test_suite_count();
}

int UnitTest::test_suite_to_run_count() const {
  return impl()->test_suite_to_run_count();
}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
int UnitTest::successful_test_case_count() const {
  return impl()->successful_test_suite_count();
}
int UnitTest::failed_test_case_count() const {
  return impl()->failed_test_suite_count();
}
int UnitTest::total_test_case_count() const {
  return impl()->total_test_suite_count();
}
int UnitTest::test_case_to_run_count() const {
  return impl()->test_suite_to_run_count();
}
#endif

int UnitTest::successful_test_count() const {
  return impl()->successful_test_count();
}

int UnitTest::skipped_test_count() const {
  return impl()->skipped_test_count();
}

int UnitTest::failed_test_count() const { return impl()->failed_test_count(); }

int UnitTest::reportable_disabled_test_count() const {
  return impl()->reportable_disabled_test_count();
}

int UnitTest::disabled_test_count() const {
  return impl()->disabled_test_count();
}

int UnitTest::reportable_test_count() const {
  return impl()->reportable_test_count();
}

int UnitTest::total_test_count() const { return impl()->total_test_count(); }

int UnitTest::test_to_run_count() const { return impl()->test_to_run_count(); }

internal::TimeInMillis UnitTest::start_timestamp() const {
  return impl()->start_timestamp();
}

internal::TimeInMillis UnitTest::elapsed_time() const {
  return impl()->elapsed_time();
}

bool UnitTest::Passed() const { return impl()->Passed(); }

bool UnitTest::Failed() const { return impl()->Failed(); }

const TestSuite* UnitTest::GetTestSuite(int i) const {
  return impl()->GetTestSuite(i);
}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
const TestCase* UnitTest::GetTestCase(int i) const {
  return impl()->GetTestCase(i);
}
#endif

const TestResult& UnitTest::ad_hoc_test_result() const {
  return *impl()->ad_hoc_test_result();
}

TestSuite* UnitTest::GetMutableTestSuite(int i) {
  return impl()->GetMutableSuiteCase(i);
}

void UnitTest::UponLeavingGTest() {
  impl()->os_stack_trace_getter()->UponLeavingGTest();
}

void UnitTest::set_current_test_suite(TestSuite* a_current_test_suite) {
  internal::MutexLock lock(&mutex_);
  impl_->set_current_test_suite(a_current_test_suite);
}

void UnitTest::set_current_test_info(TestInfo* a_current_test_info) {
  internal::MutexLock lock(&mutex_);
  impl_->set_current_test_info(a_current_test_info);
}

TestEventListeners& UnitTest::listeners() { return *impl()->listeners(); }

Environment* UnitTest::AddEnvironment(Environment* env) {
  if (env == nullptr) {
    return nullptr;
  }

  impl_->environments().push_back(env);
  return env;
}

void UnitTest::AddTestPartResult(TestPartResult::Type result_type,
                                 const char* file_name, int line_number,
                                 const std::string& message,
                                 const std::string& os_stack_trace)
    GTEST_LOCK_EXCLUDED_(mutex_) {
  Message msg;
  msg << message;

  internal::MutexLock lock(&mutex_);
  if (!impl_->gtest_trace_stack().empty()) {
    msg << "\n" << GTEST_NAME_ << " trace:";

    for (size_t i = impl_->gtest_trace_stack().size(); i > 0; --i) {
      const internal::TraceInfo& trace = impl_->gtest_trace_stack()[i - 1];
      msg << "\n"
          << internal::FormatFileLocation(trace.file, trace.line) << " "
          << trace.message;
    }
  }

  if (os_stack_trace.c_str() != nullptr && !os_stack_trace.empty()) {
    msg << internal::kStackTraceMarker << os_stack_trace;
  } else {
    msg << "\n";
  }

  const TestPartResult result = TestPartResult(
      result_type, file_name, line_number, msg.GetString().c_str());
  impl_->GetTestPartResultReporterForCurrentThread()->ReportTestPartResult(
      result);

  if (result_type != TestPartResult::kSuccess &&
      result_type != TestPartResult::kSkip) {

    if (GTEST_FLAG_GET(break_on_failure)) {
#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT)

      DebugBreak();
#elif (!defined(__native_client__)) &&            \
    ((defined(__clang__) || defined(__GNUC__)) && \
     (defined(__x86_64__) || defined(__i386__)))

      asm("int3");
#elif GTEST_HAS_BUILTIN(__builtin_trap)
      __builtin_trap();
#elif defined(SIGTRAP)
      raise(SIGTRAP);
#else

      *static_cast<volatile int*>(nullptr) = 1;
#endif
    } else if (GTEST_FLAG_GET(throw_on_failure)) {
#if GTEST_HAS_EXCEPTIONS
      throw internal::GoogleTestFailureException(result);
#else

      exit(1);
#endif
    }
  }
}

void UnitTest::RecordProperty(const std::string& key,
                              const std::string& value) {
  impl_->RecordProperty(TestProperty(key, value));
}

int UnitTest::Run() {
#ifdef GTEST_HAS_DEATH_TEST
  const bool in_death_test_child_process =
      !GTEST_FLAG_GET(internal_run_death_test).empty();

  const internal::ScopedPrematureExitFile premature_exit_file(
      in_death_test_child_process
          ? nullptr
          : internal::posix::GetEnv("TEST_PREMATURE_EXIT_FILE"));
#else
  const bool in_death_test_child_process = false;
#endif

  impl()->set_catch_exceptions(GTEST_FLAG_GET(catch_exceptions));

#ifdef GTEST_OS_WINDOWS

  if (impl()->catch_exceptions() || in_death_test_child_process) {
#if !defined(GTEST_OS_WINDOWS_MOBILE) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT) && !defined(GTEST_OS_WINDOWS_GAMES)

    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOALIGNMENTFAULTEXCEPT |
                 SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif

#if (defined(_MSC_VER) || defined(GTEST_OS_WINDOWS_MINGW)) && \
    !defined(GTEST_OS_WINDOWS_MOBILE)

    _set_error_mode(_OUT_TO_STDERR);
#endif

#if defined(_MSC_VER) && !defined(GTEST_OS_WINDOWS_MOBILE)

    if (!GTEST_FLAG_GET(break_on_failure))
      _set_abort_behavior(
          0x0,
          _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    if (!IsDebuggerPresent()) {
      (void)_CrtSetReportMode(_CRT_ASSERT,
                              _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
      (void)_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    }
#endif
  }
#else
  (void)in_death_test_child_process;
#endif

  return internal::HandleExceptionsInMethodIfSupported(
             impl(), &internal::UnitTestImpl::RunAllTests,
             "auxiliary test code (environments or event listeners)")
             ? 0
             : 1;
}

#if GTEST_HAS_FILE_SYSTEM

const char* UnitTest::original_working_dir() const {
  return impl_->original_working_dir_.c_str();
}
#endif

const TestSuite* UnitTest::current_test_suite() const
    GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  return impl_->current_test_suite();
}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
const TestCase* UnitTest::current_test_case() const
    GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  return impl_->current_test_suite();
}
#endif

const TestInfo* UnitTest::current_test_info() const
    GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  return impl_->current_test_info();
}

int UnitTest::random_seed() const { return impl_->random_seed(); }

internal::ParameterizedTestSuiteRegistry&
UnitTest::parameterized_test_registry() GTEST_LOCK_EXCLUDED_(mutex_) {
  return impl_->parameterized_test_registry();
}

UnitTest::UnitTest() { impl_ = new internal::UnitTestImpl(this); }

UnitTest::~UnitTest() { delete impl_; }

void UnitTest::PushGTestTrace(const internal::TraceInfo& trace)
    GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  impl_->gtest_trace_stack().push_back(trace);
}

void UnitTest::PopGTestTrace() GTEST_LOCK_EXCLUDED_(mutex_) {
  internal::MutexLock lock(&mutex_);
  impl_->gtest_trace_stack().pop_back();
}

namespace internal {

UnitTestImpl::UnitTestImpl(UnitTest* parent)
    : parent_(parent),
      GTEST_DISABLE_MSC_WARNINGS_PUSH_(4355                                )
          default_global_test_part_result_reporter_(this),
      default_per_thread_test_part_result_reporter_(this),
      GTEST_DISABLE_MSC_WARNINGS_POP_() global_test_part_result_reporter_(
          &default_global_test_part_result_reporter_),
      per_thread_test_part_result_reporter_(
          &default_per_thread_test_part_result_reporter_),
      parameterized_test_registry_(),
      parameterized_tests_registered_(false),
      last_death_test_suite_(-1),
      current_test_suite_(nullptr),
      current_test_info_(nullptr),
      ad_hoc_test_result_(),
      os_stack_trace_getter_(nullptr),
      post_flag_parse_init_performed_(false),
      random_seed_(0),
      random_(0),
      start_timestamp_(0),
      elapsed_time_(0),
#ifdef GTEST_HAS_DEATH_TEST
      death_test_factory_(new DefaultDeathTestFactory),
#endif

      catch_exceptions_(false) {
  listeners()->SetDefaultResultPrinter(new PrettyUnitTestResultPrinter);
}

UnitTestImpl::~UnitTestImpl() {

  ForEach(test_suites_, internal::Delete<TestSuite>);

  ForEach(environments_, internal::Delete<Environment>);

  delete os_stack_trace_getter_;
}

void UnitTestImpl::RecordProperty(const TestProperty& test_property) {
  std::string xml_element;
  TestResult* test_result;

  if (current_test_info_ != nullptr) {
    xml_element = "testcase";
    test_result = &(current_test_info_->result_);
  } else if (current_test_suite_ != nullptr) {
    xml_element = "testsuite";
    test_result = &(current_test_suite_->ad_hoc_test_result_);
  } else {
    xml_element = "testsuites";
    test_result = &ad_hoc_test_result_;
  }
  test_result->RecordProperty(xml_element, test_property);
}

#ifdef GTEST_HAS_DEATH_TEST

void UnitTestImpl::SuppressTestEventsIfInSubprocess() {
  if (internal_run_death_test_flag_ != nullptr)
    listeners()->SuppressEventForwarding(true);
}
#endif

void UnitTestImpl::ConfigureXmlOutput() {
  const std::string& output_format = UnitTestOptions::GetOutputFormat();
#if GTEST_HAS_FILE_SYSTEM
  if (output_format == "xml") {
    listeners()->SetDefaultXmlGenerator(new XmlUnitTestResultPrinter(
        UnitTestOptions::GetAbsolutePathToOutputFile().c_str()));
  } else if (output_format == "json") {
    listeners()->SetDefaultXmlGenerator(new JsonUnitTestResultPrinter(
        UnitTestOptions::GetAbsolutePathToOutputFile().c_str()));
  } else if (!output_format.empty()) {
    GTEST_LOG_(WARNING) << "WARNING: unrecognized output format \""
                        << output_format << "\" ignored.";
  }
#else
  if (!output_format.empty()) {
    GTEST_LOG_(ERROR) << "ERROR: alternative output formats require "
                      << "GTEST_HAS_FILE_SYSTEM to be enabled";
  }
#endif
}

#if GTEST_CAN_STREAM_RESULTS_

void UnitTestImpl::ConfigureStreamingOutput() {
  const std::string& target = GTEST_FLAG_GET(stream_result_to);
  if (!target.empty()) {
    const size_t pos = target.find(':');
    if (pos != std::string::npos) {
      listeners()->Append(
          new StreamingListener(target.substr(0, pos), target.substr(pos + 1)));
    } else {
      GTEST_LOG_(WARNING) << "unrecognized streaming target \"" << target
                          << "\" ignored.";
    }
  }
}
#endif

void UnitTestImpl::PostFlagParsingInit() {

  if (!post_flag_parse_init_performed_) {
    post_flag_parse_init_performed_ = true;

#if defined(GTEST_CUSTOM_TEST_EVENT_LISTENER_)

    listeners()->Append(new GTEST_CUSTOM_TEST_EVENT_LISTENER_());
#endif

#ifdef GTEST_HAS_DEATH_TEST
    InitDeathTestSubprocessControlInfo();
    SuppressTestEventsIfInSubprocess();
#endif

    RegisterParameterizedTests();

    ConfigureXmlOutput();

    if (GTEST_FLAG_GET(brief)) {
      listeners()->SetDefaultResultPrinter(new BriefUnitTestResultPrinter);
    }

#if GTEST_CAN_STREAM_RESULTS_

    ConfigureStreamingOutput();
#endif

#ifdef GTEST_HAS_ABSL
    if (GTEST_FLAG_GET(install_failure_signal_handler)) {
      absl::FailureSignalHandlerOptions options;
      absl::InstallFailureSignalHandler(options);
    }
#endif
  }
}

TestSuite* UnitTestImpl::GetTestSuite(
    const std::string& test_suite_name, const char* type_param,
    internal::SetUpTestSuiteFunc set_up_tc,
    internal::TearDownTestSuiteFunc tear_down_tc) {

  if (!test_suites_.empty() &&
      (*test_suites_.rbegin())->name_ == test_suite_name) {
    return *test_suites_.rbegin();
  }

  auto item_it = test_suites_by_name_.find(test_suite_name);
  if (item_it != test_suites_by_name_.end()) {
    return item_it->second;
  }

  auto* const new_test_suite =
      new TestSuite(test_suite_name, type_param, set_up_tc, tear_down_tc);
  test_suites_by_name_.emplace(test_suite_name, new_test_suite);

  const UnitTestFilter death_test_suite_filter(kDeathTestSuiteFilter);

  if (death_test_suite_filter.MatchesName(test_suite_name)) {

    ++last_death_test_suite_;
    test_suites_.insert(test_suites_.begin() + last_death_test_suite_,
                        new_test_suite);
  } else {

    test_suites_.push_back(new_test_suite);
  }

  test_suite_indices_.push_back(static_cast<int>(test_suite_indices_.size()));
  return new_test_suite;
}

static void SetUpEnvironment(Environment* env) { env->SetUp(); }
static void TearDownEnvironment(Environment* env) { env->TearDown(); }

bool UnitTestImpl::RunAllTests() {

  const bool gtest_is_initialized_before_run_all_tests = GTestIsInitialized();

  if (g_help_flag) return true;

  PostFlagParsingInit();

  if (GTEST_FLAG_GET(fail_if_no_test_linked) && total_test_count() == 0) {
    ColoredPrintf(
        GTestColor::kRed,
        "This test program does NOT link in any test case. This is INVALID. "
        "Please make sure to link in at least one test case.\n");
    return false;
  }

#if GTEST_HAS_FILE_SYSTEM

  internal::WriteToShardStatusFileIfNeeded();
#endif

  bool in_subprocess_for_death_test = false;

#ifdef GTEST_HAS_DEATH_TEST
  in_subprocess_for_death_test = (internal_run_death_test_flag_ != nullptr);
#if defined(GTEST_EXTRA_DEATH_TEST_CHILD_SETUP_)
  if (in_subprocess_for_death_test) {
    GTEST_EXTRA_DEATH_TEST_CHILD_SETUP_();
  }
#endif
#endif

  const bool should_shard = ShouldShard(kTestTotalShards, kTestShardIndex,
                                        in_subprocess_for_death_test);

  const bool has_tests_to_run =
      FilterTests(should_shard ? HONOR_SHARDING_PROTOCOL
                               : IGNORE_SHARDING_PROTOCOL) > 0;

  if (GTEST_FLAG_GET(list_tests)) {

    ListTestsMatchingFilter();
    return true;
  }

  random_seed_ = GetRandomSeedFromFlag(GTEST_FLAG_GET(random_seed));

  bool failed = false;

  TestEventListener* repeater = listeners()->repeater();

  start_timestamp_ = GetTimeInMillis();
  repeater->OnTestProgramStart(*parent_);

  const int repeat = in_subprocess_for_death_test ? 1 : GTEST_FLAG_GET(repeat);

  const bool gtest_repeat_forever = repeat < 0;

  const bool recreate_environments_when_repeating =
      GTEST_FLAG_GET(recreate_environments_when_repeating) ||
      gtest_repeat_forever;

  for (int i = 0; gtest_repeat_forever || i != repeat; i++) {

    ClearNonAdHocTestResult();

    Timer timer;

    if (has_tests_to_run && GTEST_FLAG_GET(shuffle)) {
      random()->Reseed(static_cast<uint32_t>(random_seed_));

      ShuffleTests();
    }

    repeater->OnTestIterationStart(*parent_, i);

    if (has_tests_to_run) {

      if (i == 0 || recreate_environments_when_repeating) {
        repeater->OnEnvironmentsSetUpStart(*parent_);
        ForEach(environments_, SetUpEnvironment);
        repeater->OnEnvironmentsSetUpEnd(*parent_);
      }

      if (Test::IsSkipped()) {

        TestResult& test_result =
            *internal::GetUnitTestImpl()->current_test_result();
        for (int j = 0; j < test_result.total_part_count(); ++j) {
          const TestPartResult& test_part_result =
              test_result.GetTestPartResult(j);
          if (test_part_result.type() == TestPartResult::kSkip) {
            const std::string& result = test_part_result.message();
            printf("%s\n", result.c_str());
          }
        }
        fflush(stdout);
      } else if (!Test::HasFatalFailure()) {
        for (int test_index = 0; test_index < total_test_suite_count();
             test_index++) {
          GetMutableSuiteCase(test_index)->Run();
          if (GTEST_FLAG_GET(fail_fast) &&
              GetMutableSuiteCase(test_index)->Failed()) {
            for (int j = test_index + 1; j < total_test_suite_count(); j++) {
              GetMutableSuiteCase(j)->Skip();
            }
            break;
          }
        }
      } else if (Test::HasFatalFailure()) {

        for (int test_index = 0; test_index < total_test_suite_count();
             test_index++) {
          GetMutableSuiteCase(test_index)->Skip();
        }
      }

      if (i == repeat - 1 || recreate_environments_when_repeating) {
        repeater->OnEnvironmentsTearDownStart(*parent_);
        std::for_each(environments_.rbegin(), environments_.rend(),
                      TearDownEnvironment);
        repeater->OnEnvironmentsTearDownEnd(*parent_);
      }
    }

    elapsed_time_ = timer.Elapsed();

    repeater->OnTestIterationEnd(*parent_, i);

    if (!Passed()) {
      failed = true;
    }

    UnshuffleTests();

    if (GTEST_FLAG_GET(shuffle)) {

      random_seed_ = GetNextRandomSeed(random_seed_);
    }
  }

  repeater->OnTestProgramEnd(*parent_);

  bool delete_environment_on_teardown = true;
  if (delete_environment_on_teardown) {
    ForEach(environments_, internal::Delete<Environment>);
    environments_.clear();
  }

  if (!gtest_is_initialized_before_run_all_tests) {
    ColoredPrintf(
        GTestColor::kRed,
        "\nIMPORTANT NOTICE - DO NOT IGNORE:\n"
        "This test program did NOT call " GTEST_INIT_GOOGLE_TEST_NAME_
        "() before calling RUN_ALL_TESTS(). This is INVALID. Soon " GTEST_NAME_
        " will start to enforce the valid usage. "
        "Please fix it ASAP, or IT WILL START TO FAIL.\n");
  }

  return !failed;
}

#if GTEST_HAS_FILE_SYSTEM

void WriteToShardStatusFileIfNeeded() {
  const char* const test_shard_file = posix::GetEnv(kTestShardStatusFile);
  if (test_shard_file != nullptr) {
    FILE* const file = posix::FOpen(test_shard_file, "w");
    if (file == nullptr) {
      ColoredPrintf(GTestColor::kRed,
                    "Could not write to the test shard status file \"%s\" "
                    "specified by the %s environment variable.\n",
                    test_shard_file, kTestShardStatusFile);
      fflush(stdout);
      exit(EXIT_FAILURE);
    }
    fclose(file);
  }
}
#endif

bool ShouldShard(const char* total_shards_env, const char* shard_index_env,
                 bool in_subprocess_for_death_test) {
  if (in_subprocess_for_death_test) {
    return false;
  }

  const int32_t total_shards = Int32FromEnvOrDie(total_shards_env, -1);
  const int32_t shard_index = Int32FromEnvOrDie(shard_index_env, -1);

  if (total_shards == -1 && shard_index == -1) {
    return false;
  } else if (total_shards == -1 && shard_index != -1) {
    const Message msg = Message() << "Invalid environment variables: you have "
                                  << kTestShardIndex << " = " << shard_index
                                  << ", but have left " << kTestTotalShards
                                  << " unset.\n";
    ColoredPrintf(GTestColor::kRed, "%s", msg.GetString().c_str());
    fflush(stdout);
    exit(EXIT_FAILURE);
  } else if (total_shards != -1 && shard_index == -1) {
    const Message msg = Message()
                        << "Invalid environment variables: you have "
                        << kTestTotalShards << " = " << total_shards
                        << ", but have left " << kTestShardIndex << " unset.\n";
    ColoredPrintf(GTestColor::kRed, "%s", msg.GetString().c_str());
    fflush(stdout);
    exit(EXIT_FAILURE);
  } else if (shard_index < 0 || shard_index >= total_shards) {
    const Message msg =
        Message() << "Invalid environment variables: we require 0 <= "
                  << kTestShardIndex << " < " << kTestTotalShards
                  << ", but you have " << kTestShardIndex << "=" << shard_index
                  << ", " << kTestTotalShards << "=" << total_shards << ".\n";
    ColoredPrintf(GTestColor::kRed, "%s", msg.GetString().c_str());
    fflush(stdout);
    exit(EXIT_FAILURE);
  }

  return total_shards > 1;
}

int32_t Int32FromEnvOrDie(const char* var, int32_t default_val) {
  const char* str_val = posix::GetEnv(var);
  if (str_val == nullptr) {
    return default_val;
  }

  int32_t result;
  if (!ParseInt32(Message() << "The value of environment variable " << var,
                  str_val, &result)) {
    exit(EXIT_FAILURE);
  }
  return result;
}

bool ShouldRunTestOnShard(int total_shards, int shard_index, int test_id) {
  return (test_id % total_shards) == shard_index;
}

int UnitTestImpl::FilterTests(ReactionToSharding shard_tests) {
  const int32_t total_shards = shard_tests == HONOR_SHARDING_PROTOCOL
                                   ? Int32FromEnvOrDie(kTestTotalShards, -1)
                                   : -1;
  const int32_t shard_index = shard_tests == HONOR_SHARDING_PROTOCOL
                                  ? Int32FromEnvOrDie(kTestShardIndex, -1)
                                  : -1;

  const PositiveAndNegativeUnitTestFilter gtest_flag_filter(
      GTEST_FLAG_GET(filter));
  const UnitTestFilter disable_test_filter(kDisableTestFilter);

  int num_runnable_tests = 0;
  int num_selected_tests = 0;
  for (auto* test_suite : test_suites_) {
    const std::string& test_suite_name = test_suite->name_;
    test_suite->set_should_run(false);

    for (TestInfo* test_info : test_suite->test_info_list()) {
      const std::string& test_name = test_info->name_;

      const bool is_disabled =
          disable_test_filter.MatchesName(test_suite_name) ||
          disable_test_filter.MatchesName(test_name);
      test_info->is_disabled_ = is_disabled;

      const bool matches_filter =
          gtest_flag_filter.MatchesTest(test_suite_name, test_name);
      test_info->matches_filter_ = matches_filter;

      const bool is_runnable =
          (GTEST_FLAG_GET(also_run_disabled_tests) || !is_disabled) &&
          matches_filter;

      const bool is_in_another_shard =
          shard_tests != IGNORE_SHARDING_PROTOCOL &&
          !ShouldRunTestOnShard(total_shards, shard_index, num_runnable_tests);
      test_info->is_in_another_shard_ = is_in_another_shard;
      const bool is_selected = is_runnable && !is_in_another_shard;

      num_runnable_tests += is_runnable;
      num_selected_tests += is_selected;

      test_info->should_run_ = is_selected;
      test_suite->set_should_run(test_suite->should_run() || is_selected);
    }
  }
  return num_selected_tests;
}

static void PrintOnOneLine(const char* str, int max_length) {
  if (str != nullptr) {
    for (int i = 0; *str != '\0'; ++str) {
      if (i >= max_length) {
        printf("...");
        break;
      }
      if (*str == '\n') {
        printf("\\n");
        i += 2;
      } else {
        printf("%c", *str);
        ++i;
      }
    }
  }
}

void UnitTestImpl::ListTestsMatchingFilter() {

  const int kMaxParamLength = 250;

  for (auto* test_suite : test_suites_) {
    bool printed_test_suite_name = false;

    for (size_t j = 0; j < test_suite->test_info_list().size(); j++) {
      const TestInfo* const test_info = test_suite->test_info_list()[j];
      if (test_info->matches_filter_) {
        if (!printed_test_suite_name) {
          printed_test_suite_name = true;
          printf("%s.", test_suite->name());
          if (test_suite->type_param() != nullptr) {
            printf("  # %s = ", kTypeParamLabel);

            PrintOnOneLine(test_suite->type_param(), kMaxParamLength);
          }
          printf("\n");
        }
        printf("  %s", test_info->name());
        if (test_info->value_param() != nullptr) {
          printf("  # %s = ", kValueParamLabel);

          PrintOnOneLine(test_info->value_param(), kMaxParamLength);
        }
        printf("\n");
      }
    }
  }
  fflush(stdout);
#if GTEST_HAS_FILE_SYSTEM
  const std::string& output_format = UnitTestOptions::GetOutputFormat();
  if (output_format == "xml" || output_format == "json") {
    FILE* fileout =
        OpenFileForWriting(UnitTestOptions::GetAbsolutePathToOutputFile());
    std::stringstream stream;
    if (output_format == "xml") {
      XmlUnitTestResultPrinter(
          UnitTestOptions::GetAbsolutePathToOutputFile().c_str())
          .PrintXmlTestsList(&stream, test_suites_);
    } else if (output_format == "json") {
      JsonUnitTestResultPrinter(
          UnitTestOptions::GetAbsolutePathToOutputFile().c_str())
          .PrintJsonTestList(&stream, test_suites_);
    }
    fprintf(fileout, "%s", StringStreamToString(&stream).c_str());
    fclose(fileout);
  }
#endif
}

void UnitTestImpl::set_os_stack_trace_getter(
    OsStackTraceGetterInterface* getter) {
  if (os_stack_trace_getter_ != getter) {
    delete os_stack_trace_getter_;
    os_stack_trace_getter_ = getter;
  }
}

OsStackTraceGetterInterface* UnitTestImpl::os_stack_trace_getter() {
  if (os_stack_trace_getter_ == nullptr) {
#ifdef GTEST_OS_STACK_TRACE_GETTER_
    os_stack_trace_getter_ = new GTEST_OS_STACK_TRACE_GETTER_;
#else
    os_stack_trace_getter_ = new OsStackTraceGetter;
#endif
  }

  return os_stack_trace_getter_;
}

TestResult* UnitTestImpl::current_test_result() {
  if (current_test_info_ != nullptr) {
    return &current_test_info_->result_;
  }
  if (current_test_suite_ != nullptr) {
    return &current_test_suite_->ad_hoc_test_result_;
  }
  return &ad_hoc_test_result_;
}

void UnitTestImpl::ShuffleTests() {

  ShuffleRange(random(), 0, last_death_test_suite_ + 1, &test_suite_indices_);

  ShuffleRange(random(), last_death_test_suite_ + 1,
               static_cast<int>(test_suites_.size()), &test_suite_indices_);

  for (auto& test_suite : test_suites_) {
    test_suite->ShuffleTests(random());
  }
}

void UnitTestImpl::UnshuffleTests() {
  for (size_t i = 0; i < test_suites_.size(); i++) {

    test_suites_[i]->UnshuffleTests();

    test_suite_indices_[i] = static_cast<int>(i);
  }
}

GTEST_NO_INLINE_ GTEST_NO_TAIL_CALL_ std::string
GetCurrentOsStackTraceExceptTop(int skip_count) {

  return GetUnitTestImpl()->CurrentOsStackTraceExceptTop(skip_count + 1);
}

namespace {
class ClassUniqueToAlwaysTrue {};
}

bool IsTrue(bool condition) { return condition; }

bool AlwaysTrue() {
#if GTEST_HAS_EXCEPTIONS

  if (IsTrue(false)) throw ClassUniqueToAlwaysTrue();
#endif
  return true;
}

bool SkipPrefix(const char* prefix, const char** pstr) {
  const size_t prefix_len = strlen(prefix);
  if (strncmp(*pstr, prefix, prefix_len) == 0) {
    *pstr += prefix_len;
    return true;
  }
  return false;
}

static const char* ParseFlagValue(const char* str, const char* flag_name,
                                  bool def_optional) {

  if (str == nullptr || flag_name == nullptr) return nullptr;

  const std::string flag_str =
      std::string("--") + GTEST_FLAG_PREFIX_ + flag_name;
  const size_t flag_len = flag_str.length();
  if (strncmp(str, flag_str.c_str(), flag_len) != 0) return nullptr;

  const char* flag_end = str + flag_len;

  if (def_optional && (flag_end[0] == '\0')) {
    return flag_end;
  }

  if (flag_end[0] != '=') return nullptr;

  return flag_end + 1;
}

static bool ParseFlag(const char* str, const char* flag_name, bool* value) {

  const char* const value_str = ParseFlagValue(str, flag_name, true);

  if (value_str == nullptr) return false;

  *value = !(*value_str == '0' || *value_str == 'f' || *value_str == 'F');
  return true;
}

bool ParseFlag(const char* str, const char* flag_name, int32_t* value) {

  const char* const value_str = ParseFlagValue(str, flag_name, false);

  if (value_str == nullptr) return false;

  return ParseInt32(Message() << "The value of flag --" << flag_name, value_str,
                    value);
}

template <typename String>
static bool ParseFlag(const char* str, const char* flag_name, String* value) {

  const char* const value_str = ParseFlagValue(str, flag_name, false);

  if (value_str == nullptr) return false;

  *value = value_str;
  return true;
}

static bool HasGoogleTestFlagPrefix(const char* str) {
  return (SkipPrefix("--", &str) || SkipPrefix("-", &str) ||
          SkipPrefix("/", &str)) &&
         !SkipPrefix(GTEST_FLAG_PREFIX_ "internal_", &str) &&
         (SkipPrefix(GTEST_FLAG_PREFIX_, &str) ||
          SkipPrefix(GTEST_FLAG_PREFIX_DASH_, &str));
}

static void PrintColorEncoded(const char* str) {
  GTestColor color = GTestColor::kDefault;

  for (;;) {
    const char* p = strchr(str, '@');
    if (p == nullptr) {
      ColoredPrintf(color, "%s", str);
      return;
    }

    ColoredPrintf(color, "%s", std::string(str, p).c_str());

    const char ch = p[1];
    str = p + 2;
    if (ch == '@') {
      ColoredPrintf(color, "@");
    } else if (ch == 'D') {
      color = GTestColor::kDefault;
    } else if (ch == 'R') {
      color = GTestColor::kRed;
    } else if (ch == 'G') {
      color = GTestColor::kGreen;
    } else if (ch == 'Y') {
      color = GTestColor::kYellow;
    } else {
      --str;
    }
  }
}

static const char kColorEncodedHelpMessage[] =
    "This program contains tests written using " GTEST_NAME_
    ". You can use the\n"
    "following command line flags to control its behavior:\n"
    "\n"
    "Test Selection:\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "list_tests@D\n"
    "      List the names of all tests instead of running them. The name of\n"
    "      TEST(Foo, Bar) is \"Foo.Bar\".\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "filter=@YPOSITIVE_PATTERNS"
    "[@G-@YNEGATIVE_PATTERNS]@D\n"
    "      Run only the tests whose name matches one of the positive patterns "
    "but\n"
    "      none of the negative patterns. '?' matches any single character; "
    "'*'\n"
    "      matches any substring; ':' separates two patterns.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "also_run_disabled_tests@D\n"
    "      Run all disabled tests too.\n"
    "\n"
    "Test Execution:\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "repeat=@Y[COUNT]@D\n"
    "      Run the tests repeatedly; use a negative count to repeat forever.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "shuffle@D\n"
    "      Randomize tests' orders on every iteration.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "random_seed=@Y[NUMBER]@D\n"
    "      Random number seed to use for shuffling test orders (between 1 and\n"
    "      99999, or 0 to use a seed based on the current time).\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "recreate_environments_when_repeating@D\n"
    "      Sets up and tears down the global test environment on each repeat\n"
    "      of the test.\n"
    "\n"
    "Test Output:\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "color=@Y(@Gyes@Y|@Gno@Y|@Gauto@Y)@D\n"
    "      Enable/disable colored output. The default is @Gauto@D.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "brief=1@D\n"
    "      Only print test failures.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "print_time=0@D\n"
    "      Don't print the elapsed time of each test.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "output=@Y(@Gjson@Y|@Gxml@Y)[@G:@YDIRECTORY_PATH@G" GTEST_PATH_SEP_
    "@Y|@G:@YFILE_PATH]@D\n"
    "      Generate a JSON or XML report in the given directory or with the "
    "given\n"
    "      file name. @YFILE_PATH@D defaults to @Gtest_detail.xml@D.\n"
#if GTEST_CAN_STREAM_RESULTS_
    "  @G--" GTEST_FLAG_PREFIX_
    "stream_result_to=@YHOST@G:@YPORT@D\n"
    "      Stream test results to the given server.\n"
#endif
    "\n"
    "Assertion Behavior:\n"
#if defined(GTEST_HAS_DEATH_TEST) && !defined(GTEST_OS_WINDOWS)
    "  @G--" GTEST_FLAG_PREFIX_
    "death_test_style=@Y(@Gfast@Y|@Gthreadsafe@Y)@D\n"
    "      Set the default death test style.\n"
#endif
    "  @G--" GTEST_FLAG_PREFIX_
    "break_on_failure@D\n"
    "      Turn assertion failures into debugger break-points.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "throw_on_failure@D\n"
    "      Turn assertion failures into C++ exceptions for use by an external\n"
    "      test framework.\n"
    "  @G--" GTEST_FLAG_PREFIX_
    "catch_exceptions=0@D\n"
    "      Do not report exceptions as test failures. Instead, allow them\n"
    "      to crash the program or throw a pop-up (on Windows).\n"
    "\n"
    "Except for @G--" GTEST_FLAG_PREFIX_
    "list_tests@D, you can alternatively set "
    "the corresponding\n"
    "environment variable of a flag (all letters in upper-case). For example, "
    "to\n"
    "disable colored text output, you can either specify "
    "@G--" GTEST_FLAG_PREFIX_
    "color=no@D or set\n"
    "the @G" GTEST_FLAG_PREFIX_UPPER_
    "COLOR@D environment variable to @Gno@D.\n"
    "\n"
    "For more information, please read the " GTEST_NAME_
    " documentation at\n"
    "@G" GTEST_PROJECT_URL_ "@D. If you find a bug in " GTEST_NAME_
    "\n"
    "(not one in your own code or tests), please report it to\n"
    "@G<" GTEST_DEV_EMAIL_ ">@D.\n";

static bool ParseGoogleTestFlag(const char* const arg) {
#define GTEST_INTERNAL_PARSE_FLAG(flag_name)  \
  do {                                        \
    auto value = GTEST_FLAG_GET(flag_name);   \
    if (ParseFlag(arg, #flag_name, &value)) { \
      GTEST_FLAG_SET(flag_name, value);       \
      return true;                            \
    }                                         \
  } while (false)

  GTEST_INTERNAL_PARSE_FLAG(also_run_disabled_tests);
  GTEST_INTERNAL_PARSE_FLAG(break_on_failure);
  GTEST_INTERNAL_PARSE_FLAG(catch_exceptions);
  GTEST_INTERNAL_PARSE_FLAG(color);
  GTEST_INTERNAL_PARSE_FLAG(death_test_style);
  GTEST_INTERNAL_PARSE_FLAG(death_test_use_fork);
  GTEST_INTERNAL_PARSE_FLAG(fail_fast);
  GTEST_INTERNAL_PARSE_FLAG(fail_if_no_test_linked);
  GTEST_INTERNAL_PARSE_FLAG(filter);
  GTEST_INTERNAL_PARSE_FLAG(internal_run_death_test);
  GTEST_INTERNAL_PARSE_FLAG(list_tests);
  GTEST_INTERNAL_PARSE_FLAG(output);
  GTEST_INTERNAL_PARSE_FLAG(brief);
  GTEST_INTERNAL_PARSE_FLAG(print_time);
  GTEST_INTERNAL_PARSE_FLAG(print_utf8);
  GTEST_INTERNAL_PARSE_FLAG(random_seed);
  GTEST_INTERNAL_PARSE_FLAG(repeat);
  GTEST_INTERNAL_PARSE_FLAG(recreate_environments_when_repeating);
  GTEST_INTERNAL_PARSE_FLAG(shuffle);
  GTEST_INTERNAL_PARSE_FLAG(stack_trace_depth);
  GTEST_INTERNAL_PARSE_FLAG(stream_result_to);
  GTEST_INTERNAL_PARSE_FLAG(throw_on_failure);
  return false;
}

#if GTEST_USE_OWN_FLAGFILE_FLAG_ && GTEST_HAS_FILE_SYSTEM
static void LoadFlagsFromFile(const std::string& path) {
  FILE* flagfile = posix::FOpen(path.c_str(), "r");
  if (!flagfile) {
    GTEST_LOG_(FATAL) << "Unable to open file \"" << GTEST_FLAG_GET(flagfile)
                      << "\"";
  }
  std::string contents(ReadEntireFile(flagfile));
  posix::FClose(flagfile);
  std::vector<std::string> lines;
  SplitString(contents, '\n', &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].empty()) continue;
    if (!ParseGoogleTestFlag(lines[i].c_str())) g_help_flag = true;
  }
}
#endif

template <typename CharType>
void ParseGoogleTestFlagsOnlyImpl(int* argc, CharType** argv) {
  std::string flagfile_value;
  for (int i = 1; i < *argc; i++) {
    const std::string arg_string = StreamableToString(argv[i]);
    const char* const arg = arg_string.c_str();

    using internal::ParseFlag;

    bool remove_flag = false;
    if (ParseGoogleTestFlag(arg)) {
      remove_flag = true;
#if GTEST_USE_OWN_FLAGFILE_FLAG_ && GTEST_HAS_FILE_SYSTEM
    } else if (ParseFlag(arg, "flagfile", &flagfile_value)) {
      GTEST_FLAG_SET(flagfile, flagfile_value);
      LoadFlagsFromFile(flagfile_value);
      remove_flag = true;
#endif
    } else if (arg_string == "--help" || HasGoogleTestFlagPrefix(arg)) {

      g_help_flag = true;
    }

    if (remove_flag) {

      for (int j = i + 1; j < *argc; ++j) {
        argv[j - 1] = argv[j];
      }

      (*argc)--;

      argv[*argc] = nullptr;

      i--;
    }
  }

  if (g_help_flag) {

    PrintColorEncoded(kColorEncodedHelpMessage);
  }
}

void ParseGoogleTestFlagsOnly(int* argc, char** argv) {
#ifdef GTEST_HAS_ABSL_FLAGS
  if (*argc <= 0) return;

  std::vector<char*> positional_args;
  std::vector<absl::UnrecognizedFlag> unrecognized_flags;
  absl::ParseAbseilFlagsOnly(*argc, argv, positional_args, unrecognized_flags);
  absl::flat_hash_set<absl::string_view> unrecognized;
  for (const auto& flag : unrecognized_flags) {
    unrecognized.insert(flag.flag_name);
  }
  absl::flat_hash_set<char*> positional;
  for (const auto& arg : positional_args) {
    positional.insert(arg);
  }

  int out_pos = 1;
  int in_pos = 1;
  for (; in_pos < *argc; ++in_pos) {
    char* arg = argv[in_pos];
    absl::string_view arg_str(arg);
    if (absl::ConsumePrefix(&arg_str, "--")) {

      if (unrecognized.contains(arg_str)) {
        argv[out_pos++] = argv[in_pos];
        continue;
      }
    }

    if (arg_str.empty()) {
      ++in_pos;
      break;
    }

    if (positional.contains(arg)) {
      argv[out_pos++] = arg;
    }
  }

  while (in_pos < *argc) {
    argv[out_pos++] = argv[in_pos++];
  }

  *argc = out_pos;
  argv[out_pos] = nullptr;
#else
  ParseGoogleTestFlagsOnlyImpl(argc, argv);
#endif

#ifdef GTEST_OS_MAC
#ifndef GTEST_OS_IOS
  if (*_NSGetArgv() == argv) {
    *_NSGetArgc() = *argc;
  }
#endif
#endif
}
void ParseGoogleTestFlagsOnly(int* argc, wchar_t** argv) {
  ParseGoogleTestFlagsOnlyImpl(argc, argv);
}

template <typename CharType>
void InitGoogleTestImpl(int* argc, CharType** argv) {

  if (GTestIsInitialized()) return;

  if (*argc <= 0) return;

  g_argvs.clear();
  for (int i = 0; i != *argc; i++) {
    g_argvs.push_back(StreamableToString(argv[i]));
  }

#ifdef GTEST_HAS_ABSL
  absl::InitializeSymbolizer(g_argvs[0].c_str());

#ifdef GTEST_HAS_ABSL_FLAGS

  absl::SetProgramUsageMessage(absl::StrReplaceAll(
      kColorEncodedHelpMessage,
      {{"@D", ""}, {"@R", ""}, {"@G", ""}, {"@Y", ""}, {"@@", "@"}}));
#endif
#endif

  ParseGoogleTestFlagsOnly(argc, argv);
  GetUnitTestImpl()->PostFlagParsingInit();
}

}

void InitGoogleTest(int* argc, char** argv) {
#if defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_(argc, argv);
#else
  internal::InitGoogleTestImpl(argc, argv);
#endif
}

void InitGoogleTest(int* argc, wchar_t** argv) {
#if defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_(argc, argv);
#else
  internal::InitGoogleTestImpl(argc, argv);
#endif
}

void InitGoogleTest() {

  int argc = 1;
  const auto arg0 = "dummy";
  char* argv0 = const_cast<char*>(arg0);
  char** argv = &argv0;

#if defined(GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_)
  GTEST_CUSTOM_INIT_GOOGLE_TEST_FUNCTION_(&argc, argv);
#else
  internal::InitGoogleTestImpl(&argc, argv);
#endif
}

#if !defined(GTEST_CUSTOM_TEMPDIR_FUNCTION_) || \
    !defined(GTEST_CUSTOM_SRCDIR_FUNCTION_)

static std::string GetDirFromEnv(
    std::initializer_list<const char*> environment_variables,
    const char* fallback, char separator) {
  for (const char* variable_name : environment_variables) {
    const char* value = internal::posix::GetEnv(variable_name);
    if (value != nullptr && value[0] != '\0') {
      if (value[strlen(value) - 1] != separator) {
        return std::string(value).append(1, separator);
      }
      return value;
    }
  }
  return fallback;
}
#endif

std::string TempDir() {
#if defined(GTEST_CUSTOM_TEMPDIR_FUNCTION_)
  return GTEST_CUSTOM_TEMPDIR_FUNCTION_();
#elif defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_WINDOWS_MOBILE)
  return GetDirFromEnv({"TEST_TMPDIR", "TEMP"}, "\\temp\\", '\\');
#elif defined(GTEST_OS_LINUX_ANDROID)
  return GetDirFromEnv({"TEST_TMPDIR", "TMPDIR"}, "/data/local/tmp/", '/');
#else
  return GetDirFromEnv({"TEST_TMPDIR", "TMPDIR"}, "/tmp/", '/');
#endif
}

#if GTEST_HAS_FILE_SYSTEM && !defined(GTEST_CUSTOM_SRCDIR_FUNCTION_)

static std::string GetCurrentExecutableDirectory() {
  internal::FilePath argv_0(internal::GetArgvs()[0]);
  return argv_0.RemoveFileName().string();
}
#endif

#if GTEST_HAS_FILE_SYSTEM
std::string SrcDir() {
#if defined(GTEST_CUSTOM_SRCDIR_FUNCTION_)
  return GTEST_CUSTOM_SRCDIR_FUNCTION_();
#elif defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_WINDOWS_MOBILE)
  return GetDirFromEnv({"TEST_SRCDIR"}, GetCurrentExecutableDirectory().c_str(),
                       '\\');
#elif defined(GTEST_OS_LINUX_ANDROID)
  return GetDirFromEnv({"TEST_SRCDIR"}, GetCurrentExecutableDirectory().c_str(),
                       '/');
#else
  return GetDirFromEnv({"TEST_SRCDIR"}, GetCurrentExecutableDirectory().c_str(),
                       '/');
#endif
}
#endif

void ScopedTrace::PushTrace(const char* file, int line, std::string message) {
  internal::TraceInfo trace;
  trace.file = file;
  trace.line = line;
  trace.message.swap(message);

  UnitTest::GetInstance()->PushGTestTrace(trace);
}

ScopedTrace::~ScopedTrace() GTEST_LOCK_EXCLUDED_(&UnitTest::mutex_) {
  UnitTest::GetInstance()->PopGTestTrace();
}

}
