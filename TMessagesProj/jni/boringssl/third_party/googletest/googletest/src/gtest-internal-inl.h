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

// Utility functions and classes used by the Google C++ testing framework.//
// This file contains purely Google Test's internal implementation.  Please
// DO NOT #INCLUDE IT IN A USER PROGRAM.

#ifndef GOOGLETEST_SRC_GTEST_INTERNAL_INL_H_
#define GOOGLETEST_SRC_GTEST_INTERNAL_INL_H_

#ifndef _WIN32_WCE
#include <errno.h>
#endif
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "gtest/internal/gtest-port.h"

#if GTEST_CAN_STREAM_RESULTS_
#include <arpa/inet.h>
#include <netdb.h>
#endif

#ifdef GTEST_OS_WINDOWS
#include <windows.h>
#endif

#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

GTEST_DECLARE_bool_(death_test_use_fork);

namespace testing {
namespace internal {

GTEST_API_ extern const TypeId kTestTypeIdInGoogleTest;

const int kMaxRandomSeed = 99999;

GTEST_API_ extern bool g_help_flag;

GTEST_API_ TimeInMillis GetTimeInMillis();

GTEST_API_ bool ShouldUseColor(bool stdout_is_tty);

GTEST_API_ std::string FormatTimeInMillisAsSeconds(TimeInMillis ms);

GTEST_API_ std::string FormatEpochTimeInMillisAsIso8601(TimeInMillis ms);

GTEST_API_ bool ParseFlag(const char* str, const char* flag, int32_t* value);

inline int GetRandomSeedFromFlag(int32_t random_seed_flag) {
  const unsigned int raw_seed =
      (random_seed_flag == 0) ? static_cast<unsigned int>(GetTimeInMillis())
                              : static_cast<unsigned int>(random_seed_flag);

  const int normalized_seed =
      static_cast<int>((raw_seed - 1U) %
                       static_cast<unsigned int>(kMaxRandomSeed)) +
      1;
  return normalized_seed;
}

inline int GetNextRandomSeed(int seed) {
  GTEST_CHECK_(1 <= seed && seed <= kMaxRandomSeed)
      << "Invalid random seed " << seed << " - must be in [1, "
      << kMaxRandomSeed << "].";
  const int next_seed = seed + 1;
  return (next_seed > kMaxRandomSeed) ? 1 : next_seed;
}

class GTestFlagSaver {
 public:

  GTestFlagSaver() {
    also_run_disabled_tests_ = GTEST_FLAG_GET(also_run_disabled_tests);
    break_on_failure_ = GTEST_FLAG_GET(break_on_failure);
    catch_exceptions_ = GTEST_FLAG_GET(catch_exceptions);
    color_ = GTEST_FLAG_GET(color);
    death_test_style_ = GTEST_FLAG_GET(death_test_style);
    death_test_use_fork_ = GTEST_FLAG_GET(death_test_use_fork);
    fail_fast_ = GTEST_FLAG_GET(fail_fast);
    filter_ = GTEST_FLAG_GET(filter);
    internal_run_death_test_ = GTEST_FLAG_GET(internal_run_death_test);
    list_tests_ = GTEST_FLAG_GET(list_tests);
    output_ = GTEST_FLAG_GET(output);
    brief_ = GTEST_FLAG_GET(brief);
    print_time_ = GTEST_FLAG_GET(print_time);
    print_utf8_ = GTEST_FLAG_GET(print_utf8);
    random_seed_ = GTEST_FLAG_GET(random_seed);
    repeat_ = GTEST_FLAG_GET(repeat);
    recreate_environments_when_repeating_ =
        GTEST_FLAG_GET(recreate_environments_when_repeating);
    shuffle_ = GTEST_FLAG_GET(shuffle);
    stack_trace_depth_ = GTEST_FLAG_GET(stack_trace_depth);
    stream_result_to_ = GTEST_FLAG_GET(stream_result_to);
    throw_on_failure_ = GTEST_FLAG_GET(throw_on_failure);
  }

  ~GTestFlagSaver() {
    GTEST_FLAG_SET(also_run_disabled_tests, also_run_disabled_tests_);
    GTEST_FLAG_SET(break_on_failure, break_on_failure_);
    GTEST_FLAG_SET(catch_exceptions, catch_exceptions_);
    GTEST_FLAG_SET(color, color_);
    GTEST_FLAG_SET(death_test_style, death_test_style_);
    GTEST_FLAG_SET(death_test_use_fork, death_test_use_fork_);
    GTEST_FLAG_SET(filter, filter_);
    GTEST_FLAG_SET(fail_fast, fail_fast_);
    GTEST_FLAG_SET(internal_run_death_test, internal_run_death_test_);
    GTEST_FLAG_SET(list_tests, list_tests_);
    GTEST_FLAG_SET(output, output_);
    GTEST_FLAG_SET(brief, brief_);
    GTEST_FLAG_SET(print_time, print_time_);
    GTEST_FLAG_SET(print_utf8, print_utf8_);
    GTEST_FLAG_SET(random_seed, random_seed_);
    GTEST_FLAG_SET(repeat, repeat_);
    GTEST_FLAG_SET(recreate_environments_when_repeating,
                   recreate_environments_when_repeating_);
    GTEST_FLAG_SET(shuffle, shuffle_);
    GTEST_FLAG_SET(stack_trace_depth, stack_trace_depth_);
    GTEST_FLAG_SET(stream_result_to, stream_result_to_);
    GTEST_FLAG_SET(throw_on_failure, throw_on_failure_);
  }

 private:

  bool also_run_disabled_tests_;
  bool break_on_failure_;
  bool catch_exceptions_;
  std::string color_;
  std::string death_test_style_;
  bool death_test_use_fork_;
  bool fail_fast_;
  std::string filter_;
  std::string internal_run_death_test_;
  bool list_tests_;
  std::string output_;
  bool brief_;
  bool print_time_;
  bool print_utf8_;
  int32_t random_seed_;
  int32_t repeat_;
  bool recreate_environments_when_repeating_;
  bool shuffle_;
  int32_t stack_trace_depth_;
  std::string stream_result_to_;
  bool throw_on_failure_;
};

GTEST_API_ std::string CodePointToUtf8(uint32_t code_point);

GTEST_API_ std::string WideStringToUtf8(const wchar_t* str, int num_chars);

void WriteToShardStatusFileIfNeeded();

GTEST_API_ bool ShouldShard(const char* total_shards_str,
                            const char* shard_index_str,
                            bool in_subprocess_for_death_test);

GTEST_API_ int32_t Int32FromEnvOrDie(const char* env_var, int32_t default_val);

GTEST_API_ bool ShouldRunTestOnShard(int total_shards, int shard_index,
                                     int test_id);

template <class Container, typename Predicate>
inline int CountIf(const Container& c, Predicate predicate) {

  int count = 0;
  for (auto it = c.begin(); it != c.end(); ++it) {
    if (predicate(*it)) ++count;
  }
  return count;
}

template <class Container, typename Functor>
void ForEach(const Container& c, Functor functor) {
  std::for_each(c.begin(), c.end(), functor);
}

template <typename E>
inline E GetElementOr(const std::vector<E>& v, int i, E default_value) {
  return (i < 0 || i >= static_cast<int>(v.size())) ? default_value
                                                    : v[static_cast<size_t>(i)];
}

template <typename E>
void ShuffleRange(internal::Random* random, int begin, int end,
                  std::vector<E>* v) {
  const int size = static_cast<int>(v->size());
  GTEST_CHECK_(0 <= begin && begin <= size)
      << "Invalid shuffle range start " << begin << ": must be in range [0, "
      << size << "].";
  GTEST_CHECK_(begin <= end && end <= size)
      << "Invalid shuffle range finish " << end << ": must be in range ["
      << begin << ", " << size << "].";

  for (int range_width = end - begin; range_width >= 2; range_width--) {
    const int last_in_range = begin + range_width - 1;
    const int selected =
        begin +
        static_cast<int>(random->Generate(static_cast<uint32_t>(range_width)));
    std::swap((*v)[static_cast<size_t>(selected)],
              (*v)[static_cast<size_t>(last_in_range)]);
  }
}

template <typename E>
inline void Shuffle(internal::Random* random, std::vector<E>* v) {
  ShuffleRange(random, 0, static_cast<int>(v->size()), v);
}

template <typename T>
static void Delete(T* x) {
  delete x;
}

class TestPropertyKeyIs {
 public:

  explicit TestPropertyKeyIs(const std::string& key) : key_(key) {}

  bool operator()(const TestProperty& test_property) const {
    return test_property.key() == key_;
  }

 private:
  std::string key_;
};

class GTEST_API_ UnitTestOptions {
 public:

  static std::string GetOutputFormat();

  static std::string GetAbsolutePathToOutputFile();

  static bool FilterMatchesTest(const std::string& test_suite_name,
                                const std::string& test_name);

#ifdef GTEST_OS_WINDOWS

  static int GTestProcessSEH(DWORD seh_code, const char* location);
#endif

  static bool MatchesFilter(const std::string& name, const char* filter);
};

#if GTEST_HAS_FILE_SYSTEM

GTEST_API_ FilePath GetCurrentExecutableName();
#endif

class OsStackTraceGetterInterface {
 public:
  OsStackTraceGetterInterface() = default;
  virtual ~OsStackTraceGetterInterface() = default;

  virtual std::string CurrentStackTrace(int max_depth, int skip_count) = 0;

  virtual void UponLeavingGTest() = 0;

  static const char* const kElidedFramesMarker;

 private:
  OsStackTraceGetterInterface(const OsStackTraceGetterInterface&) = delete;
  OsStackTraceGetterInterface& operator=(const OsStackTraceGetterInterface&) =
      delete;
};

class OsStackTraceGetter : public OsStackTraceGetterInterface {
 public:
  OsStackTraceGetter() = default;

  std::string CurrentStackTrace(int max_depth, int skip_count) override;
  void UponLeavingGTest() override;

 private:
#ifdef GTEST_HAS_ABSL
  Mutex mutex_;

  void* caller_frame_ = nullptr;
#endif

  OsStackTraceGetter(const OsStackTraceGetter&) = delete;
  OsStackTraceGetter& operator=(const OsStackTraceGetter&) = delete;
};

struct TraceInfo {
  const char* file;
  int line;
  std::string message;
};

class DefaultGlobalTestPartResultReporter
    : public TestPartResultReporterInterface {
 public:
  explicit DefaultGlobalTestPartResultReporter(UnitTestImpl* unit_test);

  void ReportTestPartResult(const TestPartResult& result) override;

 private:
  UnitTestImpl* const unit_test_;

  DefaultGlobalTestPartResultReporter(
      const DefaultGlobalTestPartResultReporter&) = delete;
  DefaultGlobalTestPartResultReporter& operator=(
      const DefaultGlobalTestPartResultReporter&) = delete;
};

class DefaultPerThreadTestPartResultReporter
    : public TestPartResultReporterInterface {
 public:
  explicit DefaultPerThreadTestPartResultReporter(UnitTestImpl* unit_test);

  void ReportTestPartResult(const TestPartResult& result) override;

 private:
  UnitTestImpl* const unit_test_;

  DefaultPerThreadTestPartResultReporter(
      const DefaultPerThreadTestPartResultReporter&) = delete;
  DefaultPerThreadTestPartResultReporter& operator=(
      const DefaultPerThreadTestPartResultReporter&) = delete;
};

class GTEST_API_ UnitTestImpl {
 public:
  explicit UnitTestImpl(UnitTest* parent);
  virtual ~UnitTestImpl();

  TestPartResultReporterInterface* GetGlobalTestPartResultReporter();

  void SetGlobalTestPartResultReporter(
      TestPartResultReporterInterface* reporter);

  TestPartResultReporterInterface* GetTestPartResultReporterForCurrentThread();

  void SetTestPartResultReporterForCurrentThread(
      TestPartResultReporterInterface* reporter);

  int successful_test_suite_count() const;

  int failed_test_suite_count() const;

  int total_test_suite_count() const;

  int test_suite_to_run_count() const;

  int successful_test_count() const;

  int skipped_test_count() const;

  int failed_test_count() const;

  int reportable_disabled_test_count() const;

  int disabled_test_count() const;

  int reportable_test_count() const;

  int total_test_count() const;

  int test_to_run_count() const;

  TimeInMillis start_timestamp() const { return start_timestamp_; }

  TimeInMillis elapsed_time() const { return elapsed_time_; }

  bool Passed() const { return !Failed(); }

  bool Failed() const {
    return failed_test_suite_count() > 0 || ad_hoc_test_result()->Failed();
  }

  const TestSuite* GetTestSuite(int i) const {
    const int index = GetElementOr(test_suite_indices_, i, -1);
    return index < 0 ? nullptr : test_suites_[static_cast<size_t>(i)];
  }

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  const TestCase* GetTestCase(int i) const { return GetTestSuite(i); }
#endif

  TestSuite* GetMutableSuiteCase(int i) {
    const int index = GetElementOr(test_suite_indices_, i, -1);
    return index < 0 ? nullptr : test_suites_[static_cast<size_t>(index)];
  }

  TestEventListeners* listeners() { return &listeners_; }

  TestResult* current_test_result();

  const TestResult* ad_hoc_test_result() const { return &ad_hoc_test_result_; }

  void set_os_stack_trace_getter(OsStackTraceGetterInterface* getter);

  OsStackTraceGetterInterface* os_stack_trace_getter();

  std::string CurrentOsStackTraceExceptTop(int skip_count)
      GTEST_NO_INLINE_ GTEST_NO_TAIL_CALL_;

  TestSuite* GetTestSuite(const std::string& test_suite_name,
                          const char* type_param,
                          internal::SetUpTestSuiteFunc set_up_tc,
                          internal::TearDownTestSuiteFunc tear_down_tc);

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  TestCase* GetTestCase(const std::string& test_case_name,
                        const char* type_param,
                        internal::SetUpTestSuiteFunc set_up_tc,
                        internal::TearDownTestSuiteFunc tear_down_tc) {
    return GetTestSuite(test_case_name, type_param, set_up_tc, tear_down_tc);
  }
#endif

  void AddTestInfo(internal::SetUpTestSuiteFunc set_up_tc,
                   internal::TearDownTestSuiteFunc tear_down_tc,
                   TestInfo* test_info) {
#if GTEST_HAS_FILE_SYSTEM

    if (original_working_dir_.IsEmpty()) {
      original_working_dir_ = FilePath::GetCurrentDir();
      GTEST_CHECK_(!original_working_dir_.IsEmpty())
          << "Failed to get the current working directory.";
    }
#endif

    GetTestSuite(test_info->test_suite_name_, test_info->type_param(),
                 set_up_tc, tear_down_tc)
        ->AddTestInfo(test_info);
  }

  internal::ParameterizedTestSuiteRegistry& parameterized_test_registry() {
    return parameterized_test_registry_;
  }

  std::set<std::string>* ignored_parameterized_test_suites() {
    return &ignored_parameterized_test_suites_;
  }

  internal::TypeParameterizedTestSuiteRegistry&
  type_parameterized_test_registry() {
    return type_parameterized_test_registry_;
  }

  void RegisterParameterizedTests();

  bool RunAllTests();

  void ClearNonAdHocTestResult() {
    ForEach(test_suites_, TestSuite::ClearTestSuiteResult);
  }

  void ClearAdHocTestResult() { ad_hoc_test_result_.Clear(); }

  void RecordProperty(const TestProperty& test_property);

  enum ReactionToSharding { HONOR_SHARDING_PROTOCOL, IGNORE_SHARDING_PROTOCOL };

  int FilterTests(ReactionToSharding shard_tests);

  void ListTestsMatchingFilter();

  const TestSuite* current_test_suite() const { return current_test_suite_; }
  TestInfo* current_test_info() { return current_test_info_; }
  const TestInfo* current_test_info() const { return current_test_info_; }

  std::vector<Environment*>& environments() { return environments_; }

  std::vector<TraceInfo>& gtest_trace_stack() {
    return *(gtest_trace_stack_.pointer());
  }
  const std::vector<TraceInfo>& gtest_trace_stack() const {
    return gtest_trace_stack_.get();
  }

#ifdef GTEST_HAS_DEATH_TEST
  void InitDeathTestSubprocessControlInfo() {
    internal_run_death_test_flag_.reset(ParseInternalRunDeathTestFlag());
  }

  const InternalRunDeathTestFlag* internal_run_death_test_flag() const {
    return internal_run_death_test_flag_.get();
  }

  internal::DeathTestFactory* death_test_factory() {
    return death_test_factory_.get();
  }

  void SuppressTestEventsIfInSubprocess();

  friend class ReplaceDeathTestFactory;
#endif

  void ConfigureXmlOutput();

#if GTEST_CAN_STREAM_RESULTS_

  void ConfigureStreamingOutput();
#endif

  void PostFlagParsingInit();

  int random_seed() const { return random_seed_; }

  internal::Random* random() { return &random_; }

  void ShuffleTests();

  void UnshuffleTests();

  bool catch_exceptions() const { return catch_exceptions_; }

 private:
  struct CompareTestSuitesByPointer {
    bool operator()(const TestSuite* lhs, const TestSuite* rhs) const {
      return lhs->name_ < rhs->name_;
    }
  };

  friend class ::testing::UnitTest;

  void set_catch_exceptions(bool value) { catch_exceptions_ = value; }

  void set_current_test_suite(TestSuite* a_current_test_suite) {
    current_test_suite_ = a_current_test_suite;
  }

  void set_current_test_info(TestInfo* a_current_test_info) {
    current_test_info_ = a_current_test_info;
  }

  UnitTest* const parent_;

#if GTEST_HAS_FILE_SYSTEM

  internal::FilePath original_working_dir_;
#endif

  DefaultGlobalTestPartResultReporter default_global_test_part_result_reporter_;
  DefaultPerThreadTestPartResultReporter
      default_per_thread_test_part_result_reporter_;

  TestPartResultReporterInterface* global_test_part_result_reporter_;

  internal::Mutex global_test_part_result_reporter_mutex_;

  internal::ThreadLocal<TestPartResultReporterInterface*>
      per_thread_test_part_result_reporter_;

  std::vector<Environment*> environments_;

  std::vector<TestSuite*> test_suites_;

  std::unordered_map<std::string, TestSuite*> test_suites_by_name_;

  std::vector<int> test_suite_indices_;

  internal::ParameterizedTestSuiteRegistry parameterized_test_registry_;
  internal::TypeParameterizedTestSuiteRegistry
      type_parameterized_test_registry_;

  std::set<std::string> ignored_parameterized_test_suites_;

  bool parameterized_tests_registered_;

  int last_death_test_suite_;

  TestSuite* current_test_suite_;

  TestInfo* current_test_info_;

  TestResult ad_hoc_test_result_;

  TestEventListeners listeners_;

  OsStackTraceGetterInterface* os_stack_trace_getter_;

  bool post_flag_parse_init_performed_;

  int random_seed_;

  internal::Random random_;

  TimeInMillis start_timestamp_;

  TimeInMillis elapsed_time_;

#ifdef GTEST_HAS_DEATH_TEST

  std::unique_ptr<InternalRunDeathTestFlag> internal_run_death_test_flag_;
  std::unique_ptr<internal::DeathTestFactory> death_test_factory_;
#endif

  internal::ThreadLocal<std::vector<TraceInfo> > gtest_trace_stack_;

  bool catch_exceptions_;

  UnitTestImpl(const UnitTestImpl&) = delete;
  UnitTestImpl& operator=(const UnitTestImpl&) = delete;
};

inline UnitTestImpl* GetUnitTestImpl() {
  return UnitTest::GetInstance()->impl();
}

#ifdef GTEST_USES_SIMPLE_RE

GTEST_API_ bool IsInSet(char ch, const char* str);
GTEST_API_ bool IsAsciiDigit(char ch);
GTEST_API_ bool IsAsciiPunct(char ch);
GTEST_API_ bool IsRepeat(char ch);
GTEST_API_ bool IsAsciiWhiteSpace(char ch);
GTEST_API_ bool IsAsciiWordChar(char ch);
GTEST_API_ bool IsValidEscape(char ch);
GTEST_API_ bool AtomMatchesChar(bool escaped, char pattern, char ch);
GTEST_API_ bool ValidateRegex(const char* regex);
GTEST_API_ bool MatchRegexAtHead(const char* regex, const char* str);
GTEST_API_ bool MatchRepetitionAndRegexAtHead(bool escaped, char ch,
                                              char repeat, const char* regex,
                                              const char* str);
GTEST_API_ bool MatchRegexAnywhere(const char* regex, const char* str);

#endif

GTEST_API_ void ParseGoogleTestFlagsOnly(int* argc, char** argv);
GTEST_API_ void ParseGoogleTestFlagsOnly(int* argc, wchar_t** argv);

#ifdef GTEST_HAS_DEATH_TEST

GTEST_API_ std::string GetLastErrnoDescription();

template <typename Integer>
bool ParseNaturalNumber(const ::std::string& str, Integer* number) {

  if (str.empty() || !IsDigit(str[0])) {
    return false;
  }
  errno = 0;

  char* end;

  using BiggestConvertible = unsigned long long;

  const BiggestConvertible parsed = strtoull(str.c_str(), &end, 10);
  const bool parse_success = *end == '\0' && errno == 0;

  GTEST_CHECK_(sizeof(Integer) <= sizeof(parsed));

  const Integer result = static_cast<Integer>(parsed);
  if (parse_success && static_cast<BiggestConvertible>(result) == parsed) {
    *number = result;
    return true;
  }
  return false;
}
#endif

class TestResultAccessor {
 public:
  static void RecordProperty(TestResult* test_result,
                             const std::string& xml_element,
                             const TestProperty& property) {
    test_result->RecordProperty(xml_element, property);
  }

  static void ClearTestPartResults(TestResult* test_result) {
    test_result->ClearTestPartResults();
  }

  static const std::vector<testing::TestPartResult>& test_part_results(
      const TestResult& test_result) {
    return test_result.test_part_results();
  }
};

#if GTEST_CAN_STREAM_RESULTS_

class StreamingListener : public EmptyTestEventListener {
 public:

  class AbstractSocketWriter {
   public:
    virtual ~AbstractSocketWriter() = default;

    virtual void Send(const std::string& message) = 0;

    virtual void CloseConnection() {}

    void SendLn(const std::string& message) { Send(message + "\n"); }
  };

  class SocketWriter : public AbstractSocketWriter {
   public:
    SocketWriter(const std::string& host, const std::string& port)
        : sockfd_(-1), host_name_(host), port_num_(port) {
      MakeConnection();
    }

    ~SocketWriter() override {
      if (sockfd_ != -1) CloseConnection();
    }

    void Send(const std::string& message) override {
      GTEST_CHECK_(sockfd_ != -1)
          << "Send() can be called only when there is a connection.";

      const auto len = static_cast<size_t>(message.length());
      if (write(sockfd_, message.c_str(), len) != static_cast<ssize_t>(len)) {
        GTEST_LOG_(WARNING) << "stream_result_to: failed to stream to "
                            << host_name_ << ":" << port_num_;
      }
    }

   private:

    void MakeConnection();

    void CloseConnection() override {
      GTEST_CHECK_(sockfd_ != -1)
          << "CloseConnection() can be called only when there is a connection.";

      close(sockfd_);
      sockfd_ = -1;
    }

    int sockfd_;
    const std::string host_name_;
    const std::string port_num_;

    SocketWriter(const SocketWriter&) = delete;
    SocketWriter& operator=(const SocketWriter&) = delete;
  };

  static std::string UrlEncode(const char* str);

  StreamingListener(const std::string& host, const std::string& port)
      : socket_writer_(new SocketWriter(host, port)) {
    Start();
  }

  explicit StreamingListener(AbstractSocketWriter* socket_writer)
      : socket_writer_(socket_writer) {
    Start();
  }

  void OnTestProgramStart(const UnitTest&                ) override {
    SendLn("event=TestProgramStart");
  }

  void OnTestProgramEnd(const UnitTest& unit_test) override {

    SendLn("event=TestProgramEnd&passed=" + FormatBool(unit_test.Passed()));

    socket_writer_->CloseConnection();
  }

  void OnTestIterationStart(const UnitTest&                ,
                            int iteration) override {
    SendLn("event=TestIterationStart&iteration=" +
           StreamableToString(iteration));
  }

  void OnTestIterationEnd(const UnitTest& unit_test,
                          int                ) override {
    SendLn("event=TestIterationEnd&passed=" + FormatBool(unit_test.Passed()) +
           "&elapsed_time=" + StreamableToString(unit_test.elapsed_time()) +
           "ms");
  }

  void OnTestSuiteStart(const TestSuite& test_suite) override {
    SendLn(std::string("event=TestCaseStart&name=") + test_suite.name());
  }

  void OnTestSuiteEnd(const TestSuite& test_suite) override {
    SendLn("event=TestCaseEnd&passed=" + FormatBool(test_suite.Passed()) +
           "&elapsed_time=" + StreamableToString(test_suite.elapsed_time()) +
           "ms");
  }

  void OnTestStart(const TestInfo& test_info) override {
    SendLn(std::string("event=TestStart&name=") + test_info.name());
  }

  void OnTestEnd(const TestInfo& test_info) override {
    SendLn("event=TestEnd&passed=" +
           FormatBool((test_info.result())->Passed()) + "&elapsed_time=" +
           StreamableToString((test_info.result())->elapsed_time()) + "ms");
  }

  void OnTestPartResult(const TestPartResult& test_part_result) override {
    const char* file_name = test_part_result.file_name();
    if (file_name == nullptr) file_name = "";
    SendLn("event=TestPartResult&file=" + UrlEncode(file_name) +
           "&line=" + StreamableToString(test_part_result.line_number()) +
           "&message=" + UrlEncode(test_part_result.message()));
  }

 private:

  void SendLn(const std::string& message) { socket_writer_->SendLn(message); }

  void Start() { SendLn("gtest_streaming_protocol_version=1.0"); }

  std::string FormatBool(bool value) { return value ? "1" : "0"; }

  const std::unique_ptr<AbstractSocketWriter> socket_writer_;

  StreamingListener(const StreamingListener&) = delete;
  StreamingListener& operator=(const StreamingListener&) = delete;
};

#endif

}
}

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif
