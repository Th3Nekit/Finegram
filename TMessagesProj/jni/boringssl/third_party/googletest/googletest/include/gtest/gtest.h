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
// This header file defines the public API for Google Test.  It should be
// included by any test program that uses Google Test.
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
//
// Acknowledgment: Google Test borrowed the idea of automatic test
// registration from Barthelemy Dagenais' (barthelemy@prologique.com)
// easyUnit framework.

#ifndef GOOGLETEST_INCLUDE_GTEST_GTEST_H_
#define GOOGLETEST_INCLUDE_GTEST_GTEST_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "gtest/gtest-assertion-result.h"
#include "gtest/gtest-death-test.h"
#include "gtest/gtest-matchers.h"
#include "gtest/gtest-message.h"
#include "gtest/gtest-param-test.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest-test-part.h"
#include "gtest/gtest-typed-test.h"
#include "gtest/gtest_pred_impl.h"
#include "gtest/gtest_prod.h"
#include "gtest/internal/gtest-internal.h"
#include "gtest/internal/gtest-string.h"

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

GTEST_DECLARE_bool_(also_run_disabled_tests);

GTEST_DECLARE_bool_(break_on_failure);

GTEST_DECLARE_bool_(catch_exceptions);

GTEST_DECLARE_string_(color);

GTEST_DECLARE_bool_(fail_fast);

GTEST_DECLARE_string_(filter);

GTEST_DECLARE_bool_(install_failure_signal_handler);

GTEST_DECLARE_bool_(list_tests);

GTEST_DECLARE_string_(output);

GTEST_DECLARE_bool_(brief);

GTEST_DECLARE_bool_(print_time);

GTEST_DECLARE_bool_(print_utf8);

GTEST_DECLARE_int32_(random_seed);

GTEST_DECLARE_int32_(repeat);

GTEST_DECLARE_bool_(recreate_environments_when_repeating);

GTEST_DECLARE_bool_(show_internal_stack_frames);

GTEST_DECLARE_bool_(shuffle);

GTEST_DECLARE_int32_(stack_trace_depth);

GTEST_DECLARE_bool_(throw_on_failure);

GTEST_DECLARE_string_(stream_result_to);

#if GTEST_USE_OWN_FLAGFILE_FLAG_
GTEST_DECLARE_string_(flagfile);
#endif

namespace testing {

GTEST_DISABLE_MSC_WARNINGS_PUSH_(4805 4100)

const int kMaxStackTraceDepth = 100;

namespace internal {

class AssertHelper;
class DefaultGlobalTestPartResultReporter;
class ExecDeathTest;
class NoExecDeathTest;
class FinalSuccessChecker;
class GTestFlagSaver;
class StreamingListenerTest;
class TestResultAccessor;
class TestEventListenersAccessor;
class TestEventRepeater;
class UnitTestRecordPropertyTestHelper;
class WindowsDeathTest;
class FuchsiaDeathTest;
class UnitTestImpl* GetUnitTestImpl();
void ReportFailureInUnknownLocation(TestPartResult::Type result_type,
                                    const std::string& message);
std::set<std::string>* GetIgnoredParameterizedTestSuites();

class GTestNonCopyable {
 public:
  GTestNonCopyable() = default;
  GTestNonCopyable(const GTestNonCopyable&) = delete;
  GTestNonCopyable& operator=(const GTestNonCopyable&) = delete;
  ~GTestNonCopyable() = default;
};

}

class Test;
class TestSuite;

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
using TestCase = TestSuite;
#endif
class TestInfo;
class UnitTest;

class GTEST_API_ Test {
 public:
  friend class TestInfo;

  virtual ~Test();

  static void SetUpTestSuite() {}

  static void TearDownTestSuite() {}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  static void TearDownTestCase() {}
  static void SetUpTestCase() {}
#endif

  static bool HasFatalFailure();

  static bool HasNonfatalFailure();

  static bool IsSkipped();

  static bool HasFailure() { return HasFatalFailure() || HasNonfatalFailure(); }

  static void RecordProperty(const std::string& key, const std::string& value);

  template <typename T, std::enable_if_t<std::is_convertible<T, int64_t>::value,
                                         bool> = true>
  static void RecordProperty(const std::string& key, const T& value) {
    RecordProperty(key, (Message() << value).GetString());
  }

 protected:

  Test();

  virtual void SetUp();

  virtual void TearDown();

 private:

  static bool HasSameFixtureClass();

  virtual void TestBody() = 0;

  void Run();

  void DeleteSelf_() { delete this; }

  const std::unique_ptr<GTEST_FLAG_SAVER_> gtest_flag_saver_;

  struct Setup_should_be_spelled_SetUp {};
  virtual Setup_should_be_spelled_SetUp* Setup() { return nullptr; }

  Test(const Test&) = delete;
  Test& operator=(const Test&) = delete;
};

typedef internal::TimeInMillis TimeInMillis;

class TestProperty {
 public:

  TestProperty(const std::string& a_key, const std::string& a_value)
      : key_(a_key), value_(a_value) {}

  const char* key() const { return key_.c_str(); }

  const char* value() const { return value_.c_str(); }

  void SetValue(const std::string& new_value) { value_ = new_value; }

 private:

  std::string key_;

  std::string value_;
};

class GTEST_API_ TestResult {
 public:

  TestResult();

  ~TestResult();

  int total_part_count() const;

  int test_property_count() const;

  bool Passed() const { return !Skipped() && !Failed(); }

  bool Skipped() const;

  bool Failed() const;

  bool HasFatalFailure() const;

  bool HasNonfatalFailure() const;

  TimeInMillis elapsed_time() const { return elapsed_time_; }

  TimeInMillis start_timestamp() const { return start_timestamp_; }

  const TestPartResult& GetTestPartResult(int i) const;

  const TestProperty& GetTestProperty(int i) const;

 private:
  friend class TestInfo;
  friend class TestSuite;
  friend class UnitTest;
  friend class internal::DefaultGlobalTestPartResultReporter;
  friend class internal::ExecDeathTest;
  friend class internal::TestResultAccessor;
  friend class internal::UnitTestImpl;
  friend class internal::WindowsDeathTest;
  friend class internal::FuchsiaDeathTest;

  const std::vector<TestPartResult>& test_part_results() const {
    return test_part_results_;
  }

  const std::vector<TestProperty>& test_properties() const {
    return test_properties_;
  }

  void set_start_timestamp(TimeInMillis start) { start_timestamp_ = start; }

  void set_elapsed_time(TimeInMillis elapsed) { elapsed_time_ = elapsed; }

  void RecordProperty(const std::string& xml_element,
                      const TestProperty& test_property);

  static bool ValidateTestProperty(const std::string& xml_element,
                                   const TestProperty& test_property);

  void AddTestPartResult(const TestPartResult& test_part_result);

  int death_test_count() const { return death_test_count_; }

  int increment_death_test_count() { return ++death_test_count_; }

  void ClearTestPartResults();

  void Clear();

  internal::Mutex test_properties_mutex_;

  std::vector<TestPartResult> test_part_results_;

  std::vector<TestProperty> test_properties_;

  int death_test_count_;

  TimeInMillis start_timestamp_;

  TimeInMillis elapsed_time_;

  TestResult(const TestResult&) = delete;
  TestResult& operator=(const TestResult&) = delete;
};

class GTEST_API_ TestInfo {
 public:

  ~TestInfo();

  const char* test_suite_name() const { return test_suite_name_.c_str(); }

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  const char* test_case_name() const { return test_suite_name(); }
#endif

  const char* name() const { return name_.c_str(); }

  const char* type_param() const {
    if (type_param_ != nullptr) return type_param_->c_str();
    return nullptr;
  }

  const char* value_param() const {
    if (value_param_ != nullptr) return value_param_->c_str();
    return nullptr;
  }

  const char* file() const { return location_.file.c_str(); }

  int line() const { return location_.line; }

  bool is_in_another_shard() const { return is_in_another_shard_; }

  bool should_run() const { return should_run_; }

  bool is_reportable() const {

    return matches_filter_ && !is_in_another_shard_;
  }

  const TestResult* result() const { return &result_; }

 private:
#ifdef GTEST_HAS_DEATH_TEST
  friend class internal::DefaultDeathTestFactory;
#endif
  friend class Test;
  friend class TestSuite;
  friend class internal::UnitTestImpl;
  friend class internal::StreamingListenerTest;
  friend TestInfo* internal::MakeAndRegisterTestInfo(
      std::string test_suite_name, const char* name, const char* type_param,
      const char* value_param, internal::CodeLocation code_location,
      internal::TypeId fixture_class_id, internal::SetUpTestSuiteFunc set_up_tc,
      internal::TearDownTestSuiteFunc tear_down_tc,
      internal::TestFactoryBase* factory);

  TestInfo(std::string test_suite_name, std::string name,
           const char* a_type_param,
           const char* a_value_param,
           internal::CodeLocation a_code_location,
           internal::TypeId fixture_class_id,
           internal::TestFactoryBase* factory);

  int increment_death_test_count() {
    return result_.increment_death_test_count();
  }

  void Run();

  void Skip();

  static void ClearTestResult(TestInfo* test_info) {
    test_info->result_.Clear();
  }

  const std::string test_suite_name_;
  const std::string name_;

  const std::unique_ptr<const ::std::string> type_param_;

  const std::unique_ptr<const ::std::string> value_param_;
  internal::CodeLocation location_;
  const internal::TypeId fixture_class_id_;
  bool should_run_;
  bool is_disabled_;
  bool matches_filter_;

  bool is_in_another_shard_;
  internal::TestFactoryBase* const factory_;

  TestResult result_;

  TestInfo(const TestInfo&) = delete;
  TestInfo& operator=(const TestInfo&) = delete;
};

class GTEST_API_ TestSuite {
 public:

  TestSuite(const std::string& name, const char* a_type_param,
            internal::SetUpTestSuiteFunc set_up_tc,
            internal::TearDownTestSuiteFunc tear_down_tc);

  virtual ~TestSuite();

  const char* name() const { return name_.c_str(); }

  const char* type_param() const {
    if (type_param_ != nullptr) return type_param_->c_str();
    return nullptr;
  }

  bool should_run() const { return should_run_; }

  int successful_test_count() const;

  int skipped_test_count() const;

  int failed_test_count() const;

  int reportable_disabled_test_count() const;

  int disabled_test_count() const;

  int reportable_test_count() const;

  int test_to_run_count() const;

  int total_test_count() const;

  bool Passed() const { return !Failed(); }

  bool Failed() const {
    return failed_test_count() > 0 || ad_hoc_test_result().Failed();
  }

  TimeInMillis elapsed_time() const { return elapsed_time_; }

  TimeInMillis start_timestamp() const { return start_timestamp_; }

  const TestInfo* GetTestInfo(int i) const;

  const TestResult& ad_hoc_test_result() const { return ad_hoc_test_result_; }

 private:
  friend class Test;
  friend class internal::UnitTestImpl;

  std::vector<TestInfo*>& test_info_list() { return test_info_list_; }

  const std::vector<TestInfo*>& test_info_list() const {
    return test_info_list_;
  }

  TestInfo* GetMutableTestInfo(int i);

  void set_should_run(bool should) { should_run_ = should; }

  void AddTestInfo(TestInfo* test_info);

  void ClearResult();

  static void ClearTestSuiteResult(TestSuite* test_suite) {
    test_suite->ClearResult();
  }

  void Run();

  void Skip();

  void RunSetUpTestSuite() {
    if (set_up_tc_ != nullptr) {
      (*set_up_tc_)();
    }
  }

  void RunTearDownTestSuite() {
    if (tear_down_tc_ != nullptr) {
      (*tear_down_tc_)();
    }
  }

  static bool TestPassed(const TestInfo* test_info) {
    return test_info->should_run() && test_info->result()->Passed();
  }

  static bool TestSkipped(const TestInfo* test_info) {
    return test_info->should_run() && test_info->result()->Skipped();
  }

  static bool TestFailed(const TestInfo* test_info) {
    return test_info->should_run() && test_info->result()->Failed();
  }

  static bool TestReportableDisabled(const TestInfo* test_info) {
    return test_info->is_reportable() && test_info->is_disabled_;
  }

  static bool TestDisabled(const TestInfo* test_info) {
    return test_info->is_disabled_;
  }

  static bool TestReportable(const TestInfo* test_info) {
    return test_info->is_reportable();
  }

  static bool ShouldRunTest(const TestInfo* test_info) {
    return test_info->should_run();
  }

  void ShuffleTests(internal::Random* random);

  void UnshuffleTests();

  std::string name_;

  const std::unique_ptr<const ::std::string> type_param_;

  std::vector<TestInfo*> test_info_list_;

  std::vector<int> test_indices_;

  internal::SetUpTestSuiteFunc set_up_tc_;

  internal::TearDownTestSuiteFunc tear_down_tc_;

  bool should_run_;

  TimeInMillis start_timestamp_;

  TimeInMillis elapsed_time_;

  TestResult ad_hoc_test_result_;

  TestSuite(const TestSuite&) = delete;
  TestSuite& operator=(const TestSuite&) = delete;
};

class Environment {
 public:

  virtual ~Environment() = default;

  virtual void SetUp() {}

  virtual void TearDown() {}

 private:

  struct Setup_should_be_spelled_SetUp {};
  virtual Setup_should_be_spelled_SetUp* Setup() { return nullptr; }
};

#if GTEST_HAS_EXCEPTIONS

class GTEST_API_ AssertionException
    : public internal::GoogleTestFailureException {
 public:
  explicit AssertionException(const TestPartResult& result)
      : GoogleTestFailureException(result) {}
};

#endif

class TestEventListener {
 public:
  virtual ~TestEventListener() = default;

  virtual void OnTestProgramStart(const UnitTest& unit_test) = 0;

  virtual void OnTestIterationStart(const UnitTest& unit_test,
                                    int iteration) = 0;

  virtual void OnEnvironmentsSetUpStart(const UnitTest& unit_test) = 0;

  virtual void OnEnvironmentsSetUpEnd(const UnitTest& unit_test) = 0;

  virtual void OnTestSuiteStart(const TestSuite&               ) {}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  virtual void OnTestCaseStart(const TestCase&              ) {}
#endif

  virtual void OnTestStart(const TestInfo& test_info) = 0;

  virtual void OnTestDisabled(const TestInfo&              ) {}

  virtual void OnTestPartResult(const TestPartResult& test_part_result) = 0;

  virtual void OnTestEnd(const TestInfo& test_info) = 0;

  virtual void OnTestSuiteEnd(const TestSuite&               ) {}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  virtual void OnTestCaseEnd(const TestCase&              ) {}
#endif

  virtual void OnEnvironmentsTearDownStart(const UnitTest& unit_test) = 0;

  virtual void OnEnvironmentsTearDownEnd(const UnitTest& unit_test) = 0;

  virtual void OnTestIterationEnd(const UnitTest& unit_test, int iteration) = 0;

  virtual void OnTestProgramEnd(const UnitTest& unit_test) = 0;
};

class EmptyTestEventListener : public TestEventListener {
 public:
  void OnTestProgramStart(const UnitTest&              ) override {}
  void OnTestIterationStart(const UnitTest&              ,
                            int              ) override {}
  void OnEnvironmentsSetUpStart(const UnitTest&              ) override {}
  void OnEnvironmentsSetUpEnd(const UnitTest&              ) override {}
  void OnTestSuiteStart(const TestSuite&               ) override {}

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseStart(const TestCase&              ) override {}
#endif

  void OnTestStart(const TestInfo&              ) override {}
  void OnTestDisabled(const TestInfo&              ) override {}
  void OnTestPartResult(const TestPartResult&                     ) override {}
  void OnTestEnd(const TestInfo&              ) override {}
  void OnTestSuiteEnd(const TestSuite&               ) override {}
#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  void OnTestCaseEnd(const TestCase&              ) override {}
#endif

  void OnEnvironmentsTearDownStart(const UnitTest&              ) override {}
  void OnEnvironmentsTearDownEnd(const UnitTest&              ) override {}
  void OnTestIterationEnd(const UnitTest&              ,
                          int              ) override {}
  void OnTestProgramEnd(const UnitTest&              ) override {}
};

class GTEST_API_ TestEventListeners {
 public:
  TestEventListeners();
  ~TestEventListeners();

  void Append(TestEventListener* listener);

  TestEventListener* Release(TestEventListener* listener);

  TestEventListener* default_result_printer() const {
    return default_result_printer_;
  }

  TestEventListener* default_xml_generator() const {
    return default_xml_generator_;
  }

  void SuppressEventForwarding(bool);

 private:
  friend class TestSuite;
  friend class TestInfo;
  friend class internal::DefaultGlobalTestPartResultReporter;
  friend class internal::NoExecDeathTest;
  friend class internal::TestEventListenersAccessor;
  friend class internal::UnitTestImpl;

  TestEventListener* repeater();

  void SetDefaultResultPrinter(TestEventListener* listener);

  void SetDefaultXmlGenerator(TestEventListener* listener);

  bool EventForwardingEnabled() const;

  internal::TestEventRepeater* repeater_;

  TestEventListener* default_result_printer_;

  TestEventListener* default_xml_generator_;

  TestEventListeners(const TestEventListeners&) = delete;
  TestEventListeners& operator=(const TestEventListeners&) = delete;
};

class GTEST_API_ UnitTest {
 public:

  static UnitTest* GetInstance();

  [[nodiscard]] int Run();

  const char* original_working_dir() const;

  const TestSuite* current_test_suite() const GTEST_LOCK_EXCLUDED_(mutex_);

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  const TestCase* current_test_case() const GTEST_LOCK_EXCLUDED_(mutex_);
#endif

  const TestInfo* current_test_info() const GTEST_LOCK_EXCLUDED_(mutex_);

  int random_seed() const;

  internal::ParameterizedTestSuiteRegistry& parameterized_test_registry()
      GTEST_LOCK_EXCLUDED_(mutex_);

  int successful_test_suite_count() const;

  int failed_test_suite_count() const;

  int total_test_suite_count() const;

  int test_suite_to_run_count() const;

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  int successful_test_case_count() const;
  int failed_test_case_count() const;
  int total_test_case_count() const;
  int test_case_to_run_count() const;
#endif

  int successful_test_count() const;

  int skipped_test_count() const;

  int failed_test_count() const;

  int reportable_disabled_test_count() const;

  int disabled_test_count() const;

  int reportable_test_count() const;

  int total_test_count() const;

  int test_to_run_count() const;

  TimeInMillis start_timestamp() const;

  TimeInMillis elapsed_time() const;

  bool Passed() const;

  bool Failed() const;

  const TestSuite* GetTestSuite(int i) const;

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
  const TestCase* GetTestCase(int i) const;
#endif

  const TestResult& ad_hoc_test_result() const;

  TestEventListeners& listeners();

 private:

  Environment* AddEnvironment(Environment* env);

  void AddTestPartResult(TestPartResult::Type result_type,
                         const char* file_name, int line_number,
                         const std::string& message,
                         const std::string& os_stack_trace)
      GTEST_LOCK_EXCLUDED_(mutex_);

  void RecordProperty(const std::string& key, const std::string& value);

  TestSuite* GetMutableTestSuite(int i);

  void UponLeavingGTest();

  void set_current_test_suite(TestSuite* a_current_test_suite)
      GTEST_LOCK_EXCLUDED_(mutex_);

  void set_current_test_info(TestInfo* a_current_test_info)
      GTEST_LOCK_EXCLUDED_(mutex_);

  internal::UnitTestImpl* impl() { return impl_; }
  const internal::UnitTestImpl* impl() const { return impl_; }

  friend class ScopedTrace;
  friend class Test;
  friend class TestInfo;
  friend class TestSuite;
  friend class internal::AssertHelper;
  friend class internal::StreamingListenerTest;
  friend class internal::UnitTestRecordPropertyTestHelper;
  friend Environment* AddGlobalTestEnvironment(Environment* env);
  friend std::set<std::string>* internal::GetIgnoredParameterizedTestSuites();
  friend internal::UnitTestImpl* internal::GetUnitTestImpl();
  friend void internal::ReportFailureInUnknownLocation(
      TestPartResult::Type result_type, const std::string& message);

  UnitTest();

  virtual ~UnitTest();

  void PushGTestTrace(const internal::TraceInfo& trace)
      GTEST_LOCK_EXCLUDED_(mutex_);

  void PopGTestTrace() GTEST_LOCK_EXCLUDED_(mutex_);

  mutable internal::Mutex mutex_;

  internal::UnitTestImpl* impl_;

  UnitTest(const UnitTest&) = delete;
  UnitTest& operator=(const UnitTest&) = delete;
};

inline Environment* AddGlobalTestEnvironment(Environment* env) {
  return UnitTest::GetInstance()->AddEnvironment(env);
}

GTEST_API_ void InitGoogleTest(int* argc, char** argv);

GTEST_API_ void InitGoogleTest(int* argc, wchar_t** argv);

GTEST_API_ void InitGoogleTest();

namespace internal {

template <typename T1, typename T2>
AssertionResult CmpHelperEQFailure(const char* lhs_expression,
                                   const char* rhs_expression, const T1& lhs,
                                   const T2& rhs) {
  return EqFailure(lhs_expression, rhs_expression,
                   FormatForComparisonFailureMessage(lhs, rhs),
                   FormatForComparisonFailureMessage(rhs, lhs), false);
}

struct faketype {};
inline bool operator==(faketype, faketype) { return true; }
inline bool operator!=(faketype, faketype) { return false; }

template <typename T1, typename T2>
AssertionResult CmpHelperEQ(const char* lhs_expression,
                            const char* rhs_expression, const T1& lhs,
                            const T2& rhs) {
  if (lhs == rhs) {
    return AssertionSuccess();
  }

  return CmpHelperEQFailure(lhs_expression, rhs_expression, lhs, rhs);
}

class EqHelper {
 public:

  template <
      typename T1, typename T2,

      typename std::enable_if<!std::is_integral<T1>::value ||
                              !std::is_pointer<T2>::value>::type* = nullptr>
  static AssertionResult Compare(const char* lhs_expression,
                                 const char* rhs_expression, const T1& lhs,
                                 const T2& rhs) {
    return CmpHelperEQ(lhs_expression, rhs_expression, lhs, rhs);
  }

  static AssertionResult Compare(const char* lhs_expression,
                                 const char* rhs_expression, BiggestInt lhs,
                                 BiggestInt rhs) {
    return CmpHelperEQ(lhs_expression, rhs_expression, lhs, rhs);
  }

  template <typename T>
  static AssertionResult Compare(
      const char* lhs_expression, const char* rhs_expression,

      std::nullptr_t          , T* rhs) {

    return CmpHelperEQ(lhs_expression, rhs_expression, static_cast<T*>(nullptr),
                       rhs);
  }
};

template <typename T1, typename T2>
AssertionResult CmpHelperOpFailure(const char* expr1, const char* expr2,
                                   const T1& val1, const T2& val2,
                                   const char* op) {
  return AssertionFailure()
         << "Expected: (" << expr1 << ") " << op << " (" << expr2
         << "), actual: " << FormatForComparisonFailureMessage(val1, val2)
         << " vs " << FormatForComparisonFailureMessage(val2, val1);
}

#define GTEST_IMPL_CMP_HELPER_(op_name, op)                                \
  template <typename T1, typename T2>                                      \
  AssertionResult CmpHelper##op_name(const char* expr1, const char* expr2, \
                                     const T1& val1, const T2& val2) {     \
    if (val1 op val2) {                                                    \
      return AssertionSuccess();                                           \
    } else {                                                               \
      return CmpHelperOpFailure(expr1, expr2, val1, val2, #op);            \
    }                                                                      \
  }

GTEST_IMPL_CMP_HELPER_(NE, !=)

GTEST_IMPL_CMP_HELPER_(LE, <=)

GTEST_IMPL_CMP_HELPER_(LT, <)

GTEST_IMPL_CMP_HELPER_(GE, >=)

GTEST_IMPL_CMP_HELPER_(GT, >)

#undef GTEST_IMPL_CMP_HELPER_

GTEST_API_ AssertionResult CmpHelperSTREQ(const char* s1_expression,
                                          const char* s2_expression,
                                          const char* s1, const char* s2);

GTEST_API_ AssertionResult CmpHelperSTRCASEEQ(const char* s1_expression,
                                              const char* s2_expression,
                                              const char* s1, const char* s2);

GTEST_API_ AssertionResult CmpHelperSTRNE(const char* s1_expression,
                                          const char* s2_expression,
                                          const char* s1, const char* s2);

GTEST_API_ AssertionResult CmpHelperSTRCASENE(const char* s1_expression,
                                              const char* s2_expression,
                                              const char* s1, const char* s2);

GTEST_API_ AssertionResult CmpHelperSTREQ(const char* s1_expression,
                                          const char* s2_expression,
                                          const wchar_t* s1, const wchar_t* s2);

GTEST_API_ AssertionResult CmpHelperSTRNE(const char* s1_expression,
                                          const char* s2_expression,
                                          const wchar_t* s1, const wchar_t* s2);

}

GTEST_API_ AssertionResult IsSubstring(const char* needle_expr,
                                       const char* haystack_expr,
                                       const char* needle,
                                       const char* haystack);
GTEST_API_ AssertionResult IsSubstring(const char* needle_expr,
                                       const char* haystack_expr,
                                       const wchar_t* needle,
                                       const wchar_t* haystack);
GTEST_API_ AssertionResult IsNotSubstring(const char* needle_expr,
                                          const char* haystack_expr,
                                          const char* needle,
                                          const char* haystack);
GTEST_API_ AssertionResult IsNotSubstring(const char* needle_expr,
                                          const char* haystack_expr,
                                          const wchar_t* needle,
                                          const wchar_t* haystack);
GTEST_API_ AssertionResult IsSubstring(const char* needle_expr,
                                       const char* haystack_expr,
                                       const ::std::string& needle,
                                       const ::std::string& haystack);
GTEST_API_ AssertionResult IsNotSubstring(const char* needle_expr,
                                          const char* haystack_expr,
                                          const ::std::string& needle,
                                          const ::std::string& haystack);

#if GTEST_HAS_STD_WSTRING
GTEST_API_ AssertionResult IsSubstring(const char* needle_expr,
                                       const char* haystack_expr,
                                       const ::std::wstring& needle,
                                       const ::std::wstring& haystack);
GTEST_API_ AssertionResult IsNotSubstring(const char* needle_expr,
                                          const char* haystack_expr,
                                          const ::std::wstring& needle,
                                          const ::std::wstring& haystack);
#endif

namespace internal {

template <typename RawType>
AssertionResult CmpHelperFloatingPointEQ(const char* lhs_expression,
                                         const char* rhs_expression,
                                         RawType lhs_value, RawType rhs_value) {
  const FloatingPoint<RawType> lhs(lhs_value), rhs(rhs_value);

  if (lhs.AlmostEquals(rhs)) {
    return AssertionSuccess();
  }

  ::std::stringstream lhs_ss;
  lhs_ss.precision(std::numeric_limits<RawType>::digits10 + 2);
  lhs_ss << lhs_value;

  ::std::stringstream rhs_ss;
  rhs_ss.precision(std::numeric_limits<RawType>::digits10 + 2);
  rhs_ss << rhs_value;

  return EqFailure(lhs_expression, rhs_expression,
                   StringStreamToString(&lhs_ss), StringStreamToString(&rhs_ss),
                   false);
}

GTEST_API_ AssertionResult DoubleNearPredFormat(const char* expr1,
                                                const char* expr2,
                                                const char* abs_error_expr,
                                                double val1, double val2,
                                                double abs_error);

class GTEST_API_ AssertHelper {
 public:

  AssertHelper(TestPartResult::Type type, const char* file, int line,
               const char* message);
  ~AssertHelper();

  void operator=(const Message& message) const;

 private:

  struct AssertHelperData {
    AssertHelperData(TestPartResult::Type t, const char* srcfile, int line_num,
                     const char* msg)
        : type(t), file(srcfile), line(line_num), message(msg) {}

    TestPartResult::Type const type;
    const char* const file;
    int const line;
    std::string const message;

   private:
    AssertHelperData(const AssertHelperData&) = delete;
    AssertHelperData& operator=(const AssertHelperData&) = delete;
  };

  AssertHelperData* const data_;

  AssertHelper(const AssertHelper&) = delete;
  AssertHelper& operator=(const AssertHelper&) = delete;
};

}

template <typename T>
class WithParamInterface {
 public:
  typedef T ParamType;
  virtual ~WithParamInterface() = default;

  static const ParamType& GetParam() {
    GTEST_CHECK_(parameter_ != nullptr)
        << "GetParam() can only be called inside a value-parameterized test "
        << "-- did you intend to write TEST_P instead of TEST_F?";
    return *parameter_;
  }

 private:

  static void SetParam(const ParamType* parameter) { parameter_ = parameter; }

  static const ParamType* parameter_;

  template <class TestClass>
  friend class internal::ParameterizedTestFactory;
};

template <typename T>
const T* WithParamInterface<T>::parameter_ = nullptr;

template <typename T>
class TestWithParam : public Test, public WithParamInterface<T> {};

#define GTEST_SKIP() GTEST_SKIP_("")

#define ADD_FAILURE() GTEST_NONFATAL_FAILURE_("Failed")

#define ADD_FAILURE_AT(file, line)        \
  GTEST_MESSAGE_AT_(file, line, "Failed", \
                    ::testing::TestPartResult::kNonFatalFailure)

#define GTEST_FAIL() GTEST_FATAL_FAILURE_("Failed")

#define GTEST_FAIL_AT(file, line)                \
  return GTEST_MESSAGE_AT_(file, line, "Failed", \
                           ::testing::TestPartResult::kFatalFailure)

#if !(defined(GTEST_DONT_DEFINE_FAIL) && GTEST_DONT_DEFINE_FAIL)
#define FAIL() GTEST_FAIL()
#define FAIL_AT(file, line) GTEST_FAIL_AT(file, line)
#endif

#define GTEST_SUCCEED() GTEST_SUCCESS_("Succeeded")

#if !(defined(GTEST_DONT_DEFINE_SUCCEED) && GTEST_DONT_DEFINE_SUCCEED)
#define SUCCEED() GTEST_SUCCEED()
#endif

#define EXPECT_THROW(statement, expected_exception) \
  GTEST_TEST_THROW_(statement, expected_exception, GTEST_NONFATAL_FAILURE_)
#define EXPECT_NO_THROW(statement) \
  GTEST_TEST_NO_THROW_(statement, GTEST_NONFATAL_FAILURE_)
#define EXPECT_ANY_THROW(statement) \
  GTEST_TEST_ANY_THROW_(statement, GTEST_NONFATAL_FAILURE_)
#define ASSERT_THROW(statement, expected_exception) \
  GTEST_TEST_THROW_(statement, expected_exception, GTEST_FATAL_FAILURE_)
#define ASSERT_NO_THROW(statement) \
  GTEST_TEST_NO_THROW_(statement, GTEST_FATAL_FAILURE_)
#define ASSERT_ANY_THROW(statement) \
  GTEST_TEST_ANY_THROW_(statement, GTEST_FATAL_FAILURE_)

#define GTEST_EXPECT_TRUE(condition)                      \
  GTEST_TEST_BOOLEAN_(condition, #condition, false, true, \
                      GTEST_NONFATAL_FAILURE_)
#define GTEST_EXPECT_FALSE(condition)                        \
  GTEST_TEST_BOOLEAN_(!(condition), #condition, true, false, \
                      GTEST_NONFATAL_FAILURE_)
#define GTEST_ASSERT_TRUE(condition) \
  GTEST_TEST_BOOLEAN_(condition, #condition, false, true, GTEST_FATAL_FAILURE_)
#define GTEST_ASSERT_FALSE(condition)                        \
  GTEST_TEST_BOOLEAN_(!(condition), #condition, true, false, \
                      GTEST_FATAL_FAILURE_)

#if !(defined(GTEST_DONT_DEFINE_EXPECT_TRUE) && GTEST_DONT_DEFINE_EXPECT_TRUE)
#define EXPECT_TRUE(condition) GTEST_EXPECT_TRUE(condition)
#endif

#if !(defined(GTEST_DONT_DEFINE_EXPECT_FALSE) && GTEST_DONT_DEFINE_EXPECT_FALSE)
#define EXPECT_FALSE(condition) GTEST_EXPECT_FALSE(condition)
#endif

#if !(defined(GTEST_DONT_DEFINE_ASSERT_TRUE) && GTEST_DONT_DEFINE_ASSERT_TRUE)
#define ASSERT_TRUE(condition) GTEST_ASSERT_TRUE(condition)
#endif

#if !(defined(GTEST_DONT_DEFINE_ASSERT_FALSE) && GTEST_DONT_DEFINE_ASSERT_FALSE)
#define ASSERT_FALSE(condition) GTEST_ASSERT_FALSE(condition)
#endif

#define EXPECT_EQ(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::EqHelper::Compare, val1, val2)
#define EXPECT_NE(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperNE, val1, val2)
#define EXPECT_LE(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperLE, val1, val2)
#define EXPECT_LT(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperLT, val1, val2)
#define EXPECT_GE(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperGE, val1, val2)
#define EXPECT_GT(val1, val2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperGT, val1, val2)

#define GTEST_ASSERT_EQ(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::EqHelper::Compare, val1, val2)
#define GTEST_ASSERT_NE(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperNE, val1, val2)
#define GTEST_ASSERT_LE(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperLE, val1, val2)
#define GTEST_ASSERT_LT(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperLT, val1, val2)
#define GTEST_ASSERT_GE(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperGE, val1, val2)
#define GTEST_ASSERT_GT(val1, val2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperGT, val1, val2)

#if !(defined(GTEST_DONT_DEFINE_ASSERT_EQ) && GTEST_DONT_DEFINE_ASSERT_EQ)
#define ASSERT_EQ(val1, val2) GTEST_ASSERT_EQ(val1, val2)
#endif

#if !(defined(GTEST_DONT_DEFINE_ASSERT_NE) && GTEST_DONT_DEFINE_ASSERT_NE)
#define ASSERT_NE(val1, val2) GTEST_ASSERT_NE(val1, val2)
#endif

#if !(defined(GTEST_DONT_DEFINE_ASSERT_LE) && GTEST_DONT_DEFINE_ASSERT_LE)
#define ASSERT_LE(val1, val2) GTEST_ASSERT_LE(val1, val2)
#endif

#if !(defined(GTEST_DONT_DEFINE_ASSERT_LT) && GTEST_DONT_DEFINE_ASSERT_LT)
#define ASSERT_LT(val1, val2) GTEST_ASSERT_LT(val1, val2)
#endif

#if !(defined(GTEST_DONT_DEFINE_ASSERT_GE) && GTEST_DONT_DEFINE_ASSERT_GE)
#define ASSERT_GE(val1, val2) GTEST_ASSERT_GE(val1, val2)
#endif

#if !(defined(GTEST_DONT_DEFINE_ASSERT_GT) && GTEST_DONT_DEFINE_ASSERT_GT)
#define ASSERT_GT(val1, val2) GTEST_ASSERT_GT(val1, val2)
#endif

#define EXPECT_STREQ(s1, s2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperSTREQ, s1, s2)
#define EXPECT_STRNE(s1, s2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperSTRNE, s1, s2)
#define EXPECT_STRCASEEQ(s1, s2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperSTRCASEEQ, s1, s2)
#define EXPECT_STRCASENE(s1, s2) \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperSTRCASENE, s1, s2)

#define ASSERT_STREQ(s1, s2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperSTREQ, s1, s2)
#define ASSERT_STRNE(s1, s2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperSTRNE, s1, s2)
#define ASSERT_STRCASEEQ(s1, s2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperSTRCASEEQ, s1, s2)
#define ASSERT_STRCASENE(s1, s2) \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperSTRCASENE, s1, s2)

#define EXPECT_FLOAT_EQ(val1, val2)                                         \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperFloatingPointEQ<float>, \
                      val1, val2)

#define EXPECT_DOUBLE_EQ(val1, val2)                                         \
  EXPECT_PRED_FORMAT2(::testing::internal::CmpHelperFloatingPointEQ<double>, \
                      val1, val2)

#define ASSERT_FLOAT_EQ(val1, val2)                                         \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperFloatingPointEQ<float>, \
                      val1, val2)

#define ASSERT_DOUBLE_EQ(val1, val2)                                         \
  ASSERT_PRED_FORMAT2(::testing::internal::CmpHelperFloatingPointEQ<double>, \
                      val1, val2)

#define EXPECT_NEAR(val1, val2, abs_error)                                   \
  EXPECT_PRED_FORMAT3(::testing::internal::DoubleNearPredFormat, val1, val2, \
                      abs_error)

#define ASSERT_NEAR(val1, val2, abs_error)                                   \
  ASSERT_PRED_FORMAT3(::testing::internal::DoubleNearPredFormat, val1, val2, \
                      abs_error)

GTEST_API_ AssertionResult FloatLE(const char* expr1, const char* expr2,
                                   float val1, float val2);
GTEST_API_ AssertionResult DoubleLE(const char* expr1, const char* expr2,
                                    double val1, double val2);

#ifdef GTEST_OS_WINDOWS

#define EXPECT_HRESULT_SUCCEEDED(expr) \
  EXPECT_PRED_FORMAT1(::testing::internal::IsHRESULTSuccess, (expr))

#define ASSERT_HRESULT_SUCCEEDED(expr) \
  ASSERT_PRED_FORMAT1(::testing::internal::IsHRESULTSuccess, (expr))

#define EXPECT_HRESULT_FAILED(expr) \
  EXPECT_PRED_FORMAT1(::testing::internal::IsHRESULTFailure, (expr))

#define ASSERT_HRESULT_FAILED(expr) \
  ASSERT_PRED_FORMAT1(::testing::internal::IsHRESULTFailure, (expr))

#endif

#define ASSERT_NO_FATAL_FAILURE(statement) \
  GTEST_TEST_NO_FATAL_FAILURE_(statement, GTEST_FATAL_FAILURE_)
#define EXPECT_NO_FATAL_FAILURE(statement) \
  GTEST_TEST_NO_FATAL_FAILURE_(statement, GTEST_NONFATAL_FAILURE_)

class GTEST_API_ ScopedTrace {
 public:

  template <typename T>
  ScopedTrace(const char* file, int line, const T& message) {
    PushTrace(file, line, (Message() << message).GetString());
  }

  ScopedTrace(const char* file, int line, const char* message) {
    PushTrace(file, line, message ? message : "(null)");
  }

  ScopedTrace(const char* file, int line, const std::string& message) {
    PushTrace(file, line, message);
  }

  ~ScopedTrace();

 private:
  void PushTrace(const char* file, int line, std::string message);

  ScopedTrace(const ScopedTrace&) = delete;
  ScopedTrace& operator=(const ScopedTrace&) = delete;
};

#define SCOPED_TRACE(message)                                               \
  const ::testing::ScopedTrace GTEST_CONCAT_TOKEN_(gtest_trace_, __LINE__)( \
      __FILE__, __LINE__, (message))

template <typename T1, typename T2>
constexpr bool StaticAssertTypeEq() noexcept {
  static_assert(std::is_same<T1, T2>::value, "T1 and T2 are not the same type");
  return true;
}

#define GTEST_TEST(test_suite_name, test_name)             \
  GTEST_TEST_(test_suite_name, test_name, ::testing::Test, \
              ::testing::internal::GetTestTypeId())

#if !(defined(GTEST_DONT_DEFINE_TEST) && GTEST_DONT_DEFINE_TEST)
#define TEST(test_suite_name, test_name) GTEST_TEST(test_suite_name, test_name)
#endif

#define GTEST_TEST_F(test_fixture, test_name)        \
  GTEST_TEST_(test_fixture, test_name, test_fixture, \
              ::testing::internal::GetTypeId<test_fixture>())
#if !(defined(GTEST_DONT_DEFINE_TEST_F) && GTEST_DONT_DEFINE_TEST_F)
#define TEST_F(test_fixture, test_name) GTEST_TEST_F(test_fixture, test_name)
#endif

GTEST_API_ std::string TempDir();

GTEST_API_ std::string SrcDir();

GTEST_DISABLE_MSC_WARNINGS_POP_()

template <int&... ExplicitParameterBarrier, typename Factory>
TestInfo* RegisterTest(const char* test_suite_name, const char* test_name,
                       const char* type_param, const char* value_param,
                       const char* file, int line, Factory factory) {
  using TestT = typename std::remove_pointer<decltype(factory())>::type;

  class FactoryImpl : public internal::TestFactoryBase {
   public:
    explicit FactoryImpl(Factory f) : factory_(std::move(f)) {}
    Test* CreateTest() override { return factory_(); }

   private:
    Factory factory_;
  };

  return internal::MakeAndRegisterTestInfo(
      test_suite_name, test_name, type_param, value_param,
      internal::CodeLocation(file, line), internal::GetTypeId<TestT>(),
      internal::SuiteApiResolver<TestT>::GetSetUpCaseOrSuite(file, line),
      internal::SuiteApiResolver<TestT>::GetTearDownCaseOrSuite(file, line),
      new FactoryImpl{std::move(factory)});
}

}

[[nodiscard]] int RUN_ALL_TESTS();

inline int RUN_ALL_TESTS() { return ::testing::UnitTest::GetInstance()->Run(); }

GTEST_DISABLE_MSC_WARNINGS_POP_()

#endif
