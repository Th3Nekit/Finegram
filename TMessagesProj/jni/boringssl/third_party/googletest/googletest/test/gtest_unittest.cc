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
// Tests for Google Test itself.  This verifies that the basic constructs of
// Google Test work.

#include "gtest/gtest.h"

TEST(CommandLineFlagsTest, CanBeAccessedInCodeOnceGTestHIsIncluded) {
  bool dummy =
      GTEST_FLAG_GET(also_run_disabled_tests) ||
      GTEST_FLAG_GET(break_on_failure) || GTEST_FLAG_GET(catch_exceptions) ||
      GTEST_FLAG_GET(color) != "unknown" || GTEST_FLAG_GET(fail_fast) ||
      GTEST_FLAG_GET(filter) != "unknown" || GTEST_FLAG_GET(list_tests) ||
      GTEST_FLAG_GET(output) != "unknown" || GTEST_FLAG_GET(brief) ||
      GTEST_FLAG_GET(print_time) || GTEST_FLAG_GET(random_seed) ||
      GTEST_FLAG_GET(repeat) > 0 ||
      GTEST_FLAG_GET(recreate_environments_when_repeating) ||
      GTEST_FLAG_GET(show_internal_stack_frames) || GTEST_FLAG_GET(shuffle) ||
      GTEST_FLAG_GET(stack_trace_depth) > 0 ||
      GTEST_FLAG_GET(stream_result_to) != "unknown" ||
      GTEST_FLAG_GET(throw_on_failure);
  EXPECT_TRUE(dummy || !dummy);
}

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cstdint>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "gtest/gtest-spi.h"
#include "src/gtest-internal-inl.h"

struct ConvertibleGlobalType {

  template <
      class T,
      std::enable_if_t<
          false, std::enable_if_t<std::is_constructible<T>::value, int>> = 0>
  operator T() const;
};
void operator<<(ConvertibleGlobalType&, int);
static_assert(sizeof(decltype(std::declval<ConvertibleGlobalType&>()
                              << 1)(*)()) > 0,
              "error in operator<< overload resolution");

namespace testing {
namespace internal {

#if GTEST_CAN_STREAM_RESULTS_

class StreamingListenerTest : public Test {
 public:
  class FakeSocketWriter : public StreamingListener::AbstractSocketWriter {
   public:

    void Send(const std::string& message) override { output_ += message; }

    std::string output_;
  };

  StreamingListenerTest()
      : fake_sock_writer_(new FakeSocketWriter),
        streamer_(fake_sock_writer_),
        test_info_obj_("FooTest", "Bar", nullptr, nullptr,
                       CodeLocation(__FILE__, __LINE__), nullptr, nullptr) {}

 protected:
  std::string* output() { return &(fake_sock_writer_->output_); }

  FakeSocketWriter* const fake_sock_writer_;
  StreamingListener streamer_;
  UnitTest unit_test_;
  TestInfo test_info_obj_;
};

TEST_F(StreamingListenerTest, OnTestProgramEnd) {
  *output() = "";
  streamer_.OnTestProgramEnd(unit_test_);
  EXPECT_EQ("event=TestProgramEnd&passed=1\n", *output());
}

TEST_F(StreamingListenerTest, OnTestIterationEnd) {
  *output() = "";
  streamer_.OnTestIterationEnd(unit_test_, 42);
  EXPECT_EQ("event=TestIterationEnd&passed=1&elapsed_time=0ms\n", *output());
}

TEST_F(StreamingListenerTest, OnTestSuiteStart) {
  *output() = "";
  streamer_.OnTestSuiteStart(TestSuite("FooTest", "Bar", nullptr, nullptr));
  EXPECT_EQ("event=TestCaseStart&name=FooTest\n", *output());
}

TEST_F(StreamingListenerTest, OnTestSuiteEnd) {
  *output() = "";
  streamer_.OnTestSuiteEnd(TestSuite("FooTest", "Bar", nullptr, nullptr));
  EXPECT_EQ("event=TestCaseEnd&passed=1&elapsed_time=0ms\n", *output());
}

TEST_F(StreamingListenerTest, OnTestStart) {
  *output() = "";
  streamer_.OnTestStart(test_info_obj_);
  EXPECT_EQ("event=TestStart&name=Bar\n", *output());
}

TEST_F(StreamingListenerTest, OnTestEnd) {
  *output() = "";
  streamer_.OnTestEnd(test_info_obj_);
  EXPECT_EQ("event=TestEnd&passed=1&elapsed_time=0ms\n", *output());
}

TEST_F(StreamingListenerTest, OnTestPartResult) {
  *output() = "";
  streamer_.OnTestPartResult(TestPartResult(TestPartResult::kFatalFailure,
                                            "foo.cc", 42, "failed=\n&%"));

  EXPECT_EQ(
      "event=TestPartResult&file=foo.cc&line=42&message=failed%3D%0A%26%25\n",
      *output());
}

#endif

class TestEventListenersAccessor {
 public:
  static TestEventListener* GetRepeater(TestEventListeners* listeners) {
    return listeners->repeater();
  }

  static void SetDefaultResultPrinter(TestEventListeners* listeners,
                                      TestEventListener* listener) {
    listeners->SetDefaultResultPrinter(listener);
  }
  static void SetDefaultXmlGenerator(TestEventListeners* listeners,
                                     TestEventListener* listener) {
    listeners->SetDefaultXmlGenerator(listener);
  }

  static bool EventForwardingEnabled(const TestEventListeners& listeners) {
    return listeners.EventForwardingEnabled();
  }

  static void SuppressEventForwarding(TestEventListeners* listeners) {
    listeners->SuppressEventForwarding(true);
  }
};

class UnitTestRecordPropertyTestHelper : public Test {
 protected:
  UnitTestRecordPropertyTestHelper() {}

  void UnitTestRecordProperty(const char* key, const std::string& value) {
    unit_test_.RecordProperty(key, value);
  }

  UnitTest unit_test_;
};

}
}

using testing::AssertionFailure;
using testing::AssertionResult;
using testing::AssertionSuccess;
using testing::DoubleLE;
using testing::EmptyTestEventListener;
using testing::Environment;
using testing::FloatLE;
using testing::IsNotSubstring;
using testing::IsSubstring;
using testing::kMaxStackTraceDepth;
using testing::Message;
using testing::ScopedFakeTestPartResultReporter;
using testing::StaticAssertTypeEq;
using testing::Test;
using testing::TestEventListeners;
using testing::TestInfo;
using testing::TestPartResult;
using testing::TestPartResultArray;
using testing::TestProperty;
using testing::TestResult;
using testing::TimeInMillis;
using testing::UnitTest;
using testing::internal::AlwaysFalse;
using testing::internal::AlwaysTrue;
using testing::internal::AppendUserMessage;
using testing::internal::ArrayAwareFind;
using testing::internal::ArrayEq;
using testing::internal::CodePointToUtf8;
using testing::internal::CopyArray;
using testing::internal::CountIf;
using testing::internal::EqFailure;
using testing::internal::FloatingPoint;
using testing::internal::ForEach;
using testing::internal::FormatEpochTimeInMillisAsIso8601;
using testing::internal::FormatTimeInMillisAsSeconds;
using testing::internal::GetElementOr;
using testing::internal::GetNextRandomSeed;
using testing::internal::GetRandomSeedFromFlag;
using testing::internal::GetTestTypeId;
using testing::internal::GetTimeInMillis;
using testing::internal::GetTypeId;
using testing::internal::GetUnitTestImpl;
using testing::internal::GTestFlagSaver;
using testing::internal::HasDebugStringAndShortDebugString;
using testing::internal::Int32FromEnvOrDie;
using testing::internal::IsContainer;
using testing::internal::IsContainerTest;
using testing::internal::IsNotContainer;
using testing::internal::kMaxRandomSeed;
using testing::internal::kTestTypeIdInGoogleTest;
using testing::internal::NativeArray;
using testing::internal::ParseFlag;
using testing::internal::RelationToSourceCopy;
using testing::internal::RelationToSourceReference;
using testing::internal::ShouldRunTestOnShard;
using testing::internal::ShouldShard;
using testing::internal::ShouldUseColor;
using testing::internal::Shuffle;
using testing::internal::ShuffleRange;
using testing::internal::SkipPrefix;
using testing::internal::StreamableToString;
using testing::internal::String;
using testing::internal::TestEventListenersAccessor;
using testing::internal::TestResultAccessor;
using testing::internal::WideStringToUtf8;
using testing::internal::edit_distance::CalculateOptimalEdits;
using testing::internal::edit_distance::CreateUnifiedDiff;
using testing::internal::edit_distance::EditType;

#if GTEST_HAS_STREAM_REDIRECTION
using testing::internal::CaptureStdout;
using testing::internal::GetCapturedStdout;
#endif

#ifdef GTEST_IS_THREADSAFE
using testing::internal::ThreadWithParam;
#endif

class TestingVector : public std::vector<int> {};

::std::ostream& operator<<(::std::ostream& os, const TestingVector& vector) {
  os << "{ ";
  for (size_t i = 0; i < vector.size(); i++) {
    os << vector[i] << " ";
  }
  os << "}";
  return os;
}

namespace {

TEST(GetRandomSeedFromFlagTest, HandlesZero) {
  const int seed = GetRandomSeedFromFlag(0);
  EXPECT_LE(1, seed);
  EXPECT_LE(seed, static_cast<int>(kMaxRandomSeed));
}

TEST(GetRandomSeedFromFlagTest, PreservesValidSeed) {
  EXPECT_EQ(1, GetRandomSeedFromFlag(1));
  EXPECT_EQ(2, GetRandomSeedFromFlag(2));
  EXPECT_EQ(kMaxRandomSeed - 1, GetRandomSeedFromFlag(kMaxRandomSeed - 1));
  EXPECT_EQ(static_cast<int>(kMaxRandomSeed),
            GetRandomSeedFromFlag(kMaxRandomSeed));
}

TEST(GetRandomSeedFromFlagTest, NormalizesInvalidSeed) {
  const int seed1 = GetRandomSeedFromFlag(-1);
  EXPECT_LE(1, seed1);
  EXPECT_LE(seed1, static_cast<int>(kMaxRandomSeed));

  const int seed2 = GetRandomSeedFromFlag(kMaxRandomSeed + 1);
  EXPECT_LE(1, seed2);
  EXPECT_LE(seed2, static_cast<int>(kMaxRandomSeed));
}

TEST(GetNextRandomSeedTest, WorksForValidInput) {
  EXPECT_EQ(2, GetNextRandomSeed(1));
  EXPECT_EQ(3, GetNextRandomSeed(2));
  EXPECT_EQ(static_cast<int>(kMaxRandomSeed),
            GetNextRandomSeed(kMaxRandomSeed - 1));
  EXPECT_EQ(1, GetNextRandomSeed(kMaxRandomSeed));

}

static void ClearCurrentTestPartResults() {
  TestResultAccessor::ClearTestPartResults(
      GetUnitTestImpl()->current_test_result());
}

TEST(GetTypeIdTest, ReturnsSameValueForSameType) {
  EXPECT_EQ(GetTypeId<int>(), GetTypeId<int>());
  EXPECT_EQ(GetTypeId<Test>(), GetTypeId<Test>());
}

class SubClassOfTest : public Test {};
class AnotherSubClassOfTest : public Test {};

TEST(GetTypeIdTest, ReturnsDifferentValuesForDifferentTypes) {
  EXPECT_NE(GetTypeId<int>(), GetTypeId<const int>());
  EXPECT_NE(GetTypeId<int>(), GetTypeId<char>());
  EXPECT_NE(GetTypeId<int>(), GetTestTypeId());
  EXPECT_NE(GetTypeId<SubClassOfTest>(), GetTestTypeId());
  EXPECT_NE(GetTypeId<AnotherSubClassOfTest>(), GetTestTypeId());
  EXPECT_NE(GetTypeId<AnotherSubClassOfTest>(), GetTypeId<SubClassOfTest>());
}

TEST(GetTestTypeIdTest, ReturnsTheSameValueInsideOrOutsideOfGoogleTest) {
  EXPECT_EQ(kTestTypeIdInGoogleTest, GetTestTypeId());
}

using ::testing::internal::CanonicalizeForStdLibVersioning;

TEST(CanonicalizeForStdLibVersioning, LeavesUnversionedNamesUnchanged) {
  EXPECT_EQ("std::bind", CanonicalizeForStdLibVersioning("std::bind"));
  EXPECT_EQ("std::_", CanonicalizeForStdLibVersioning("std::_"));
  EXPECT_EQ("std::__foo", CanonicalizeForStdLibVersioning("std::__foo"));
  EXPECT_EQ("gtl::__1::x", CanonicalizeForStdLibVersioning("gtl::__1::x"));
  EXPECT_EQ("__1::x", CanonicalizeForStdLibVersioning("__1::x"));
  EXPECT_EQ("::__1::x", CanonicalizeForStdLibVersioning("::__1::x"));
}

TEST(CanonicalizeForStdLibVersioning, ElidesDoubleUnderNames) {
  EXPECT_EQ("std::bind", CanonicalizeForStdLibVersioning("std::__1::bind"));
  EXPECT_EQ("std::_", CanonicalizeForStdLibVersioning("std::__1::_"));

  EXPECT_EQ("std::bind", CanonicalizeForStdLibVersioning("std::__g::bind"));
  EXPECT_EQ("std::_", CanonicalizeForStdLibVersioning("std::__g::_"));

  EXPECT_EQ("std::bind",
            CanonicalizeForStdLibVersioning("std::__google::bind"));
  EXPECT_EQ("std::_", CanonicalizeForStdLibVersioning("std::__google::_"));
}

TEST(FormatTimeInMillisAsSecondsTest, FormatsZero) {
  EXPECT_EQ("0.", FormatTimeInMillisAsSeconds(0));
}

TEST(FormatTimeInMillisAsSecondsTest, FormatsPositiveNumber) {
  EXPECT_EQ("0.003", FormatTimeInMillisAsSeconds(3));
  EXPECT_EQ("0.01", FormatTimeInMillisAsSeconds(10));
  EXPECT_EQ("0.2", FormatTimeInMillisAsSeconds(200));
  EXPECT_EQ("1.2", FormatTimeInMillisAsSeconds(1200));
  EXPECT_EQ("3.", FormatTimeInMillisAsSeconds(3000));
  EXPECT_EQ("10.", FormatTimeInMillisAsSeconds(10000));
  EXPECT_EQ("100.", FormatTimeInMillisAsSeconds(100000));
  EXPECT_EQ("123.456", FormatTimeInMillisAsSeconds(123456));
  EXPECT_EQ("1234567.89", FormatTimeInMillisAsSeconds(1234567890));
}

TEST(FormatTimeInMillisAsSecondsTest, FormatsNegativeNumber) {
  EXPECT_EQ("-0.003", FormatTimeInMillisAsSeconds(-3));
  EXPECT_EQ("-0.01", FormatTimeInMillisAsSeconds(-10));
  EXPECT_EQ("-0.2", FormatTimeInMillisAsSeconds(-200));
  EXPECT_EQ("-1.2", FormatTimeInMillisAsSeconds(-1200));
  EXPECT_EQ("-3.", FormatTimeInMillisAsSeconds(-3000));
  EXPECT_EQ("-10.", FormatTimeInMillisAsSeconds(-10000));
  EXPECT_EQ("-100.", FormatTimeInMillisAsSeconds(-100000));
  EXPECT_EQ("-123.456", FormatTimeInMillisAsSeconds(-123456));
  EXPECT_EQ("-1234567.89", FormatTimeInMillisAsSeconds(-1234567890));
}

#if !defined(__EMSCRIPTEN__)

class FormatEpochTimeInMillisAsIso8601Test : public Test {
 public:

  static const TimeInMillis kMillisPerSec = 1000;

 private:
  void SetUp() override {
    saved_tz_.reset();

    GTEST_DISABLE_MSC_DEPRECATED_PUSH_(                        )
    if (const char* tz = getenv("TZ")) {
      saved_tz_ = std::make_unique<std::string>(tz);
    }
    GTEST_DISABLE_MSC_DEPRECATED_POP_()

    SetTimeZone("UTC+00");
  }

  void TearDown() override {
    SetTimeZone(saved_tz_ != nullptr ? saved_tz_->c_str() : nullptr);
    saved_tz_.reset();
  }

  static void SetTimeZone(const char* time_zone) {

#if defined(_MSC_VER) || defined(GTEST_OS_WINDOWS_MINGW)

    const std::string env_var =
        std::string("TZ=") + (time_zone ? time_zone : "");
    _putenv(env_var.c_str());
    GTEST_DISABLE_MSC_WARNINGS_PUSH_(4996                          )
    tzset();
    GTEST_DISABLE_MSC_WARNINGS_POP_()
#else
#if defined(GTEST_OS_LINUX_ANDROID) && __ANDROID_API__ < 21

    setenv("TZ", "UTC", 1);
    tzset();
#endif
    if (time_zone) {
      setenv(("TZ"), time_zone, 1);
    } else {
      unsetenv("TZ");
    }
    tzset();
#endif
  }

  std::unique_ptr<std::string> saved_tz_;
};

const TimeInMillis FormatEpochTimeInMillisAsIso8601Test::kMillisPerSec;

TEST_F(FormatEpochTimeInMillisAsIso8601Test, PrintsTwoDigitSegments) {
  EXPECT_EQ("2011-10-31T18:52:42.000",
            FormatEpochTimeInMillisAsIso8601(1320087162 * kMillisPerSec));
}

TEST_F(FormatEpochTimeInMillisAsIso8601Test, IncludesMillisecondsAfterDot) {
  EXPECT_EQ("2011-10-31T18:52:42.234",
            FormatEpochTimeInMillisAsIso8601(1320087162 * kMillisPerSec + 234));
}

TEST_F(FormatEpochTimeInMillisAsIso8601Test, PrintsLeadingZeroes) {
  EXPECT_EQ("2011-09-03T05:07:02.000",
            FormatEpochTimeInMillisAsIso8601(1315026422 * kMillisPerSec));
}

TEST_F(FormatEpochTimeInMillisAsIso8601Test, Prints24HourTime) {
  EXPECT_EQ("2011-09-28T17:08:22.000",
            FormatEpochTimeInMillisAsIso8601(1317229702 * kMillisPerSec));
}

TEST_F(FormatEpochTimeInMillisAsIso8601Test, PrintsEpochStart) {
  EXPECT_EQ("1970-01-01T00:00:00.000", FormatEpochTimeInMillisAsIso8601(0));
}

#endif

#ifdef __BORLANDC__

#pragma option push -w-ccc -w-rch
#endif

TEST(NullLiteralTest, LHSAllowsNullLiterals) {
  EXPECT_EQ(0, static_cast<void*>(nullptr));
  ASSERT_EQ(0, static_cast<void*>(nullptr));
  EXPECT_EQ(NULL, static_cast<void*>(nullptr));
  ASSERT_EQ(NULL, static_cast<void*>(nullptr));
  EXPECT_EQ(nullptr, static_cast<void*>(nullptr));
  ASSERT_EQ(nullptr, static_cast<void*>(nullptr));

  const int* const p = nullptr;
  EXPECT_EQ(0, p);
  ASSERT_EQ(0, p);
  EXPECT_EQ(NULL, p);
  ASSERT_EQ(NULL, p);
  EXPECT_EQ(nullptr, p);
  ASSERT_EQ(nullptr, p);
}

struct ConvertToAll {
  template <typename T>
  operator T() const {
    return T();
  }
};

struct ConvertToPointer {
  template <class T>
  operator T*() const {
    return nullptr;
  }
};

struct ConvertToAllButNoPointers {
  template <typename T,
            typename std::enable_if<!std::is_pointer<T>::value, int>::type = 0>
  operator T() const {
    return T();
  }
};

struct MyType {};
inline bool operator==(MyType const&, MyType const&) { return true; }

TEST(NullLiteralTest, ImplicitConversion) {
  EXPECT_EQ(ConvertToPointer{}, static_cast<void*>(nullptr));
#if !defined(__GNUC__) || defined(__clang__)

  EXPECT_EQ(ConvertToAll{}, static_cast<void*>(nullptr));
#endif
  EXPECT_EQ(ConvertToAll{}, MyType{});
  EXPECT_EQ(ConvertToAllButNoPointers{}, MyType{});
}

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wzero-as-null-pointer-constant")
#pragma clang diagnostic error "-Wzero-as-null-pointer-constant"
#endif
#endif

TEST(NullLiteralTest, NoConversionNoWarning) {

  EXPECT_EQ(0, 0);
  ASSERT_EQ(0, 0);
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef __BORLANDC__

#pragma option pop
#endif

TEST(CodePointToUtf8Test, CanEncodeNul) {
  EXPECT_EQ("", CodePointToUtf8(L'\0'));
}

TEST(CodePointToUtf8Test, CanEncodeAscii) {
  EXPECT_EQ("a", CodePointToUtf8(L'a'));
  EXPECT_EQ("Z", CodePointToUtf8(L'Z'));
  EXPECT_EQ("&", CodePointToUtf8(L'&'));
  EXPECT_EQ("\x7F", CodePointToUtf8(L'\x7F'));
}

TEST(CodePointToUtf8Test, CanEncode8To11Bits) {

  EXPECT_EQ("\xC3\x93", CodePointToUtf8(L'\xD3'));

  EXPECT_EQ("\xD5\xB6", CodePointToUtf8(static_cast<wchar_t>(0x576)));
}

TEST(CodePointToUtf8Test, CanEncode12To16Bits) {

  EXPECT_EQ("\xE0\xA3\x93", CodePointToUtf8(static_cast<wchar_t>(0x8D3)));

  EXPECT_EQ("\xEC\x9D\x8D", CodePointToUtf8(static_cast<wchar_t>(0xC74D)));
}

#if !GTEST_WIDE_STRING_USES_UTF16_

TEST(CodePointToUtf8Test, CanEncode17To21Bits) {

  EXPECT_EQ("\xF0\x90\xA3\x93", CodePointToUtf8(L'\x108D3'));

  EXPECT_EQ("\xF0\x90\x90\x80", CodePointToUtf8(L'\x10400'));

  EXPECT_EQ("\xF4\x88\x98\xB4", CodePointToUtf8(L'\x108634'));
}

TEST(CodePointToUtf8Test, CanEncodeInvalidCodePoint) {
  EXPECT_EQ("(Invalid Unicode 0x1234ABCD)", CodePointToUtf8(L'\x1234ABCD'));
}

#endif

TEST(WideStringToUtf8Test, CanEncodeNul) {
  EXPECT_STREQ("", WideStringToUtf8(L"", 0).c_str());
  EXPECT_STREQ("", WideStringToUtf8(L"", -1).c_str());
}

TEST(WideStringToUtf8Test, CanEncodeAscii) {
  EXPECT_STREQ("a", WideStringToUtf8(L"a", 1).c_str());
  EXPECT_STREQ("ab", WideStringToUtf8(L"ab", 2).c_str());
  EXPECT_STREQ("a", WideStringToUtf8(L"a", -1).c_str());
  EXPECT_STREQ("ab", WideStringToUtf8(L"ab", -1).c_str());
}

TEST(WideStringToUtf8Test, CanEncode8To11Bits) {

  EXPECT_STREQ("\xC3\x93", WideStringToUtf8(L"\xD3", 1).c_str());
  EXPECT_STREQ("\xC3\x93", WideStringToUtf8(L"\xD3", -1).c_str());

  const wchar_t s[] = {0x576, '\0'};
  EXPECT_STREQ("\xD5\xB6", WideStringToUtf8(s, 1).c_str());
  EXPECT_STREQ("\xD5\xB6", WideStringToUtf8(s, -1).c_str());
}

TEST(WideStringToUtf8Test, CanEncode12To16Bits) {

  const wchar_t s1[] = {0x8D3, '\0'};
  EXPECT_STREQ("\xE0\xA3\x93", WideStringToUtf8(s1, 1).c_str());
  EXPECT_STREQ("\xE0\xA3\x93", WideStringToUtf8(s1, -1).c_str());

  const wchar_t s2[] = {0xC74D, '\0'};
  EXPECT_STREQ("\xEC\x9D\x8D", WideStringToUtf8(s2, 1).c_str());
  EXPECT_STREQ("\xEC\x9D\x8D", WideStringToUtf8(s2, -1).c_str());
}

TEST(WideStringToUtf8Test, StopsOnNulCharacter) {
  EXPECT_STREQ("ABC", WideStringToUtf8(L"ABC\0XYZ", 100).c_str());
}

TEST(WideStringToUtf8Test, StopsWhenLengthLimitReached) {
  EXPECT_STREQ("ABC", WideStringToUtf8(L"ABCDEF", 3).c_str());
}

#if !GTEST_WIDE_STRING_USES_UTF16_

TEST(WideStringToUtf8Test, CanEncode17To21Bits) {

  EXPECT_STREQ("\xF0\x90\xA3\x93", WideStringToUtf8(L"\x108D3", 1).c_str());
  EXPECT_STREQ("\xF0\x90\xA3\x93", WideStringToUtf8(L"\x108D3", -1).c_str());

  EXPECT_STREQ("\xF4\x88\x98\xB4", WideStringToUtf8(L"\x108634", 1).c_str());
  EXPECT_STREQ("\xF4\x88\x98\xB4", WideStringToUtf8(L"\x108634", -1).c_str());
}

TEST(WideStringToUtf8Test, CanEncodeInvalidCodePoint) {
  EXPECT_STREQ("(Invalid Unicode 0xABCDFF)",
               WideStringToUtf8(L"\xABCDFF", -1).c_str());
}
#else

TEST(WideStringToUtf8Test, CanEncodeValidUtf16SUrrogatePairs) {
  const wchar_t s[] = {0xD801, 0xDC00, '\0'};
  EXPECT_STREQ("\xF0\x90\x90\x80", WideStringToUtf8(s, -1).c_str());
}

TEST(WideStringToUtf8Test, CanEncodeInvalidUtf16SurrogatePair) {

  const wchar_t s1[] = {0xD800, '\0'};
  EXPECT_STREQ("\xED\xA0\x80", WideStringToUtf8(s1, -1).c_str());

  const wchar_t s2[] = {0xD800, 'M', '\0'};
  EXPECT_STREQ("\xED\xA0\x80M", WideStringToUtf8(s2, -1).c_str());

  const wchar_t s3[] = {0xDC00, 'P', 'Q', 'R', '\0'};
  EXPECT_STREQ("\xED\xB0\x80PQR", WideStringToUtf8(s3, -1).c_str());
}
#endif

#if !GTEST_WIDE_STRING_USES_UTF16_
TEST(WideStringToUtf8Test, ConcatenatesCodepointsCorrectly) {
  const wchar_t s[] = {0x108634, 0xC74D, '\n', 0x576, 0x8D3, 0x108634, '\0'};
  EXPECT_STREQ(
      "\xF4\x88\x98\xB4"
      "\xEC\x9D\x8D"
      "\n"
      "\xD5\xB6"
      "\xE0\xA3\x93"
      "\xF4\x88\x98\xB4",
      WideStringToUtf8(s, -1).c_str());
}
#else
TEST(WideStringToUtf8Test, ConcatenatesCodepointsCorrectly) {
  const wchar_t s[] = {0xC74D, '\n', 0x576, 0x8D3, '\0'};
  EXPECT_STREQ(
      "\xEC\x9D\x8D"
      "\n"
      "\xD5\xB6"
      "\xE0\xA3\x93",
      WideStringToUtf8(s, -1).c_str());
}
#endif

TEST(RandomDeathTest, GeneratesCrashesOnInvalidRange) {
  testing::internal::Random random(42);
  EXPECT_DEATH_IF_SUPPORTED(random.Generate(0),
                            "Cannot generate a number in the range \\[0, 0\\)");
  EXPECT_DEATH_IF_SUPPORTED(
      random.Generate(testing::internal::Random::kMaxRange + 1),
      "Generation of a number in \\[0, 2147483649\\) was requested, "
      "but this can only generate numbers in \\[0, 2147483648\\)");
}

TEST(RandomTest, GeneratesNumbersWithinRange) {
  constexpr uint32_t kRange = 10000;
  testing::internal::Random random(12345);
  for (int i = 0; i < 10; i++) {
    EXPECT_LT(random.Generate(kRange), kRange) << " for iteration " << i;
  }

  testing::internal::Random random2(testing::internal::Random::kMaxRange);
  for (int i = 0; i < 10; i++) {
    EXPECT_LT(random2.Generate(kRange), kRange) << " for iteration " << i;
  }
}

TEST(RandomTest, RepeatsWhenReseeded) {
  constexpr int kSeed = 123;
  constexpr int kArraySize = 10;
  constexpr uint32_t kRange = 10000;
  uint32_t values[kArraySize];

  testing::internal::Random random(kSeed);
  for (int i = 0; i < kArraySize; i++) {
    values[i] = random.Generate(kRange);
  }

  random.Reseed(kSeed);
  for (int i = 0; i < kArraySize; i++) {
    EXPECT_EQ(values[i], random.Generate(kRange)) << " for iteration " << i;
  }
}

static bool IsPositive(int n) { return n > 0; }

TEST(ContainerUtilityTest, CountIf) {
  std::vector<int> v;
  EXPECT_EQ(0, CountIf(v, IsPositive));

  v.push_back(-1);
  v.push_back(0);
  EXPECT_EQ(0, CountIf(v, IsPositive));

  v.push_back(2);
  v.push_back(-10);
  v.push_back(10);
  EXPECT_EQ(2, CountIf(v, IsPositive));
}

static int g_sum = 0;
static void Accumulate(int n) { g_sum += n; }

TEST(ContainerUtilityTest, ForEach) {
  std::vector<int> v;
  g_sum = 0;
  ForEach(v, Accumulate);
  EXPECT_EQ(0, g_sum);

  g_sum = 0;
  v.push_back(1);
  ForEach(v, Accumulate);
  EXPECT_EQ(1, g_sum);

  g_sum = 0;
  v.push_back(20);
  v.push_back(300);
  ForEach(v, Accumulate);
  EXPECT_EQ(321, g_sum);
}

TEST(ContainerUtilityTest, GetElementOr) {
  std::vector<char> a;
  EXPECT_EQ('x', GetElementOr(a, 0, 'x'));

  a.push_back('a');
  a.push_back('b');
  EXPECT_EQ('a', GetElementOr(a, 0, 'x'));
  EXPECT_EQ('b', GetElementOr(a, 1, 'x'));
  EXPECT_EQ('x', GetElementOr(a, -2, 'x'));
  EXPECT_EQ('x', GetElementOr(a, 2, 'x'));
}

TEST(ContainerUtilityDeathTest, ShuffleRange) {
  std::vector<int> a;
  a.push_back(0);
  a.push_back(1);
  a.push_back(2);
  testing::internal::Random random(1);

  EXPECT_DEATH_IF_SUPPORTED(
      ShuffleRange(&random, -1, 1, &a),
      "Invalid shuffle range start -1: must be in range \\[0, 3\\]");
  EXPECT_DEATH_IF_SUPPORTED(
      ShuffleRange(&random, 4, 4, &a),
      "Invalid shuffle range start 4: must be in range \\[0, 3\\]");
  EXPECT_DEATH_IF_SUPPORTED(
      ShuffleRange(&random, 3, 2, &a),
      "Invalid shuffle range finish 2: must be in range \\[3, 3\\]");
  EXPECT_DEATH_IF_SUPPORTED(
      ShuffleRange(&random, 3, 4, &a),
      "Invalid shuffle range finish 4: must be in range \\[3, 3\\]");
}

class VectorShuffleTest : public Test {
 protected:
  static const size_t kVectorSize = 20;

  VectorShuffleTest() : random_(1) {
    for (int i = 0; i < static_cast<int>(kVectorSize); i++) {
      vector_.push_back(i);
    }
  }

  static bool VectorIsCorrupt(const TestingVector& vector) {
    if (kVectorSize != vector.size()) {
      return true;
    }

    bool found_in_vector[kVectorSize] = {false};
    for (size_t i = 0; i < vector.size(); i++) {
      const int e = vector[i];
      if (e < 0 || e >= static_cast<int>(kVectorSize) || found_in_vector[e]) {
        return true;
      }
      found_in_vector[e] = true;
    }

    return false;
  }

  static bool VectorIsNotCorrupt(const TestingVector& vector) {
    return !VectorIsCorrupt(vector);
  }

  static bool RangeIsShuffled(const TestingVector& vector, int begin, int end) {
    for (int i = begin; i < end; i++) {
      if (i != vector[static_cast<size_t>(i)]) {
        return true;
      }
    }
    return false;
  }

  static bool RangeIsUnshuffled(const TestingVector& vector, int begin,
                                int end) {
    return !RangeIsShuffled(vector, begin, end);
  }

  static bool VectorIsShuffled(const TestingVector& vector) {
    return RangeIsShuffled(vector, 0, static_cast<int>(vector.size()));
  }

  static bool VectorIsUnshuffled(const TestingVector& vector) {
    return !VectorIsShuffled(vector);
  }

  testing::internal::Random random_;
  TestingVector vector_;
};

const size_t VectorShuffleTest::kVectorSize;

TEST_F(VectorShuffleTest, HandlesEmptyRange) {

  ShuffleRange(&random_, 0, 0, &vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  ASSERT_PRED1(VectorIsUnshuffled, vector_);

  ShuffleRange(&random_, kVectorSize / 2, kVectorSize / 2, &vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  ASSERT_PRED1(VectorIsUnshuffled, vector_);

  ShuffleRange(&random_, kVectorSize - 1, kVectorSize - 1, &vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  ASSERT_PRED1(VectorIsUnshuffled, vector_);

  ShuffleRange(&random_, kVectorSize, kVectorSize, &vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  ASSERT_PRED1(VectorIsUnshuffled, vector_);
}

TEST_F(VectorShuffleTest, HandlesRangeOfSizeOne) {

  ShuffleRange(&random_, 0, 1, &vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  ASSERT_PRED1(VectorIsUnshuffled, vector_);

  ShuffleRange(&random_, kVectorSize / 2, kVectorSize / 2 + 1, &vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  ASSERT_PRED1(VectorIsUnshuffled, vector_);

  ShuffleRange(&random_, kVectorSize - 1, kVectorSize, &vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  ASSERT_PRED1(VectorIsUnshuffled, vector_);
}

TEST_F(VectorShuffleTest, ShufflesEntireVector) {
  Shuffle(&random_, &vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  EXPECT_FALSE(VectorIsUnshuffled(vector_)) << vector_;

  EXPECT_NE(0, vector_[0]);
  EXPECT_NE(static_cast<int>(kVectorSize - 1), vector_[kVectorSize - 1]);
}

TEST_F(VectorShuffleTest, ShufflesStartOfVector) {
  const int kRangeSize = kVectorSize / 2;

  ShuffleRange(&random_, 0, kRangeSize, &vector_);

  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  EXPECT_PRED3(RangeIsShuffled, vector_, 0, kRangeSize);
  EXPECT_PRED3(RangeIsUnshuffled, vector_, kRangeSize,
               static_cast<int>(kVectorSize));
}

TEST_F(VectorShuffleTest, ShufflesEndOfVector) {
  const int kRangeSize = kVectorSize / 2;
  ShuffleRange(&random_, kRangeSize, kVectorSize, &vector_);

  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  EXPECT_PRED3(RangeIsUnshuffled, vector_, 0, kRangeSize);
  EXPECT_PRED3(RangeIsShuffled, vector_, kRangeSize,
               static_cast<int>(kVectorSize));
}

TEST_F(VectorShuffleTest, ShufflesMiddleOfVector) {
  const int kRangeSize = static_cast<int>(kVectorSize) / 3;
  ShuffleRange(&random_, kRangeSize, 2 * kRangeSize, &vector_);

  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  EXPECT_PRED3(RangeIsUnshuffled, vector_, 0, kRangeSize);
  EXPECT_PRED3(RangeIsShuffled, vector_, kRangeSize, 2 * kRangeSize);
  EXPECT_PRED3(RangeIsUnshuffled, vector_, 2 * kRangeSize,
               static_cast<int>(kVectorSize));
}

TEST_F(VectorShuffleTest, ShufflesRepeatably) {
  TestingVector vector2;
  for (size_t i = 0; i < kVectorSize; i++) {
    vector2.push_back(static_cast<int>(i));
  }

  random_.Reseed(1234);
  Shuffle(&random_, &vector_);
  random_.Reseed(1234);
  Shuffle(&random_, &vector2);

  ASSERT_PRED1(VectorIsNotCorrupt, vector_);
  ASSERT_PRED1(VectorIsNotCorrupt, vector2);

  for (size_t i = 0; i < kVectorSize; i++) {
    EXPECT_EQ(vector_[i], vector2[i]) << " where i is " << i;
  }
}

TEST(AssertHelperTest, AssertHelperIsSmall) {

  EXPECT_LE(sizeof(testing::internal::AssertHelper), sizeof(void*));
}

TEST(StringTest, EndsWithCaseInsensitive) {
  EXPECT_TRUE(String::EndsWithCaseInsensitive("foobar", "BAR"));
  EXPECT_TRUE(String::EndsWithCaseInsensitive("foobaR", "bar"));
  EXPECT_TRUE(String::EndsWithCaseInsensitive("foobar", ""));
  EXPECT_TRUE(String::EndsWithCaseInsensitive("", ""));

  EXPECT_FALSE(String::EndsWithCaseInsensitive("Foobar", "foo"));
  EXPECT_FALSE(String::EndsWithCaseInsensitive("foobar", "Foo"));
  EXPECT_FALSE(String::EndsWithCaseInsensitive("", "foo"));
}

static const wchar_t* const kNull = nullptr;

TEST(StringTest, CaseInsensitiveWideCStringEquals) {
  EXPECT_TRUE(String::CaseInsensitiveWideCStringEquals(nullptr, nullptr));
  EXPECT_FALSE(String::CaseInsensitiveWideCStringEquals(kNull, L""));
  EXPECT_FALSE(String::CaseInsensitiveWideCStringEquals(L"", kNull));
  EXPECT_FALSE(String::CaseInsensitiveWideCStringEquals(kNull, L"foobar"));
  EXPECT_FALSE(String::CaseInsensitiveWideCStringEquals(L"foobar", kNull));
  EXPECT_TRUE(String::CaseInsensitiveWideCStringEquals(L"foobar", L"foobar"));
  EXPECT_TRUE(String::CaseInsensitiveWideCStringEquals(L"foobar", L"FOOBAR"));
  EXPECT_TRUE(String::CaseInsensitiveWideCStringEquals(L"FOOBAR", L"foobar"));
}

#ifdef GTEST_OS_WINDOWS

TEST(StringTest, ShowWideCString) {
  EXPECT_STREQ("(null)", String::ShowWideCString(NULL).c_str());
  EXPECT_STREQ("", String::ShowWideCString(L"").c_str());
  EXPECT_STREQ("foo", String::ShowWideCString(L"foo").c_str());
}

#ifdef GTEST_OS_WINDOWS_MOBILE
TEST(StringTest, AnsiAndUtf16Null) {
  EXPECT_EQ(NULL, String::AnsiToUtf16(NULL));
  EXPECT_EQ(NULL, String::Utf16ToAnsi(NULL));
}

TEST(StringTest, AnsiAndUtf16ConvertBasic) {
  const char* ansi = String::Utf16ToAnsi(L"str");
  EXPECT_STREQ("str", ansi);
  delete[] ansi;
  const WCHAR* utf16 = String::AnsiToUtf16("str");
  EXPECT_EQ(0, wcsncmp(L"str", utf16, 3));
  delete[] utf16;
}

TEST(StringTest, AnsiAndUtf16ConvertPathChars) {
  const char* ansi = String::Utf16ToAnsi(L".:\\ \"*?");
  EXPECT_STREQ(".:\\ \"*?", ansi);
  delete[] ansi;
  const WCHAR* utf16 = String::AnsiToUtf16(".:\\ \"*?");
  EXPECT_EQ(0, wcsncmp(L".:\\ \"*?", utf16, 3));
  delete[] utf16;
}
#endif

#endif

TEST(TestPropertyTest, StringValue) {
  TestProperty property("key", "1");
  EXPECT_STREQ("key", property.key());
  EXPECT_STREQ("1", property.value());
}

TEST(TestPropertyTest, ReplaceStringValue) {
  TestProperty property("key", "1");
  EXPECT_STREQ("1", property.value());
  property.SetValue("2");
  EXPECT_STREQ("2", property.value());
}

static void AddFatalFailure() { FAIL() << "Expected fatal failure."; }

static void AddNonfatalFailure() {
  ADD_FAILURE() << "Expected non-fatal failure.";
}

class ScopedFakeTestPartResultReporterTest : public Test {
 public:
  enum FailureMode { FATAL_FAILURE, NONFATAL_FAILURE };
  static void AddFailure(FailureMode failure) {
    if (failure == FATAL_FAILURE) {
      AddFatalFailure();
    } else {
      AddNonfatalFailure();
    }
  }
};

TEST_F(ScopedFakeTestPartResultReporterTest, InterceptsTestFailures) {
  TestPartResultArray results;
  {
    ScopedFakeTestPartResultReporter reporter(
        ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD,
        &results);
    AddFailure(NONFATAL_FAILURE);
    AddFailure(FATAL_FAILURE);
  }

  EXPECT_EQ(2, results.size());
  EXPECT_TRUE(results.GetTestPartResult(0).nonfatally_failed());
  EXPECT_TRUE(results.GetTestPartResult(1).fatally_failed());
}

TEST_F(ScopedFakeTestPartResultReporterTest, DeprecatedConstructor) {
  TestPartResultArray results;
  {

    ScopedFakeTestPartResultReporter reporter(&results);
    AddFailure(NONFATAL_FAILURE);
  }
  EXPECT_EQ(1, results.size());
}

#ifdef GTEST_IS_THREADSAFE

class ScopedFakeTestPartResultReporterWithThreadsTest
    : public ScopedFakeTestPartResultReporterTest {
 protected:
  static void AddFailureInOtherThread(FailureMode failure) {
    ThreadWithParam<FailureMode> thread(&AddFailure, failure, nullptr);
    thread.Join();
  }
};

TEST_F(ScopedFakeTestPartResultReporterWithThreadsTest,
       InterceptsTestFailuresInAllThreads) {
  TestPartResultArray results;
  {
    ScopedFakeTestPartResultReporter reporter(
        ScopedFakeTestPartResultReporter::INTERCEPT_ALL_THREADS, &results);
    AddFailure(NONFATAL_FAILURE);
    AddFailure(FATAL_FAILURE);
    AddFailureInOtherThread(NONFATAL_FAILURE);
    AddFailureInOtherThread(FATAL_FAILURE);
  }

  EXPECT_EQ(4, results.size());
  EXPECT_TRUE(results.GetTestPartResult(0).nonfatally_failed());
  EXPECT_TRUE(results.GetTestPartResult(1).fatally_failed());
  EXPECT_TRUE(results.GetTestPartResult(2).nonfatally_failed());
  EXPECT_TRUE(results.GetTestPartResult(3).fatally_failed());
}

#endif

typedef ScopedFakeTestPartResultReporterTest ExpectFatalFailureTest;

TEST_F(ExpectFatalFailureTest, CatchesFatalFaliure) {
  EXPECT_FATAL_FAILURE(AddFatalFailure(), "Expected fatal failure.");
}

TEST_F(ExpectFatalFailureTest, AcceptsStdStringObject) {
  EXPECT_FATAL_FAILURE(AddFatalFailure(),
                       ::std::string("Expected fatal failure."));
}

TEST_F(ExpectFatalFailureTest, CatchesFatalFailureOnAllThreads) {

  EXPECT_FATAL_FAILURE_ON_ALL_THREADS(AddFatalFailure(),
                                      "Expected fatal failure.");
}

#ifdef __BORLANDC__

#pragma option push -w-ccc
#endif

int NonVoidFunction() {
  EXPECT_FATAL_FAILURE(ASSERT_TRUE(false), "");
  EXPECT_FATAL_FAILURE_ON_ALL_THREADS(FAIL(), "");
  return 0;
}

TEST_F(ExpectFatalFailureTest, CanBeUsedInNonVoidFunction) {
  NonVoidFunction();
}

void DoesNotAbortHelper(bool* aborted) {
  EXPECT_FATAL_FAILURE(ASSERT_TRUE(false), "");
  EXPECT_FATAL_FAILURE_ON_ALL_THREADS(FAIL(), "");

  *aborted = false;
}

#ifdef __BORLANDC__

#pragma option pop
#endif

TEST_F(ExpectFatalFailureTest, DoesNotAbort) {
  bool aborted = true;
  DoesNotAbortHelper(&aborted);
  EXPECT_FALSE(aborted);
}

static int global_var = 0;
#define GTEST_USE_UNPROTECTED_COMMA_ global_var++, global_var++

TEST_F(ExpectFatalFailureTest, AcceptsMacroThatExpandsToUnprotectedComma) {
#ifndef __BORLANDC__

  EXPECT_FATAL_FAILURE(
      {
        GTEST_USE_UNPROTECTED_COMMA_;
        AddFatalFailure();
      },
      "");
#endif

  EXPECT_FATAL_FAILURE_ON_ALL_THREADS(
      {
        GTEST_USE_UNPROTECTED_COMMA_;
        AddFatalFailure();
      },
      "");
}

typedef ScopedFakeTestPartResultReporterTest ExpectNonfatalFailureTest;

TEST_F(ExpectNonfatalFailureTest, CatchesNonfatalFailure) {
  EXPECT_NONFATAL_FAILURE(AddNonfatalFailure(), "Expected non-fatal failure.");
}

TEST_F(ExpectNonfatalFailureTest, AcceptsStdStringObject) {
  EXPECT_NONFATAL_FAILURE(AddNonfatalFailure(),
                          ::std::string("Expected non-fatal failure."));
}

TEST_F(ExpectNonfatalFailureTest, CatchesNonfatalFailureOnAllThreads) {

  EXPECT_NONFATAL_FAILURE_ON_ALL_THREADS(AddNonfatalFailure(),
                                         "Expected non-fatal failure.");
}

TEST_F(ExpectNonfatalFailureTest, AcceptsMacroThatExpandsToUnprotectedComma) {
  EXPECT_NONFATAL_FAILURE(
      {
        GTEST_USE_UNPROTECTED_COMMA_;
        AddNonfatalFailure();
      },
      "");

  EXPECT_NONFATAL_FAILURE_ON_ALL_THREADS(
      {
        GTEST_USE_UNPROTECTED_COMMA_;
        AddNonfatalFailure();
      },
      "");
}

#ifdef GTEST_IS_THREADSAFE

typedef ScopedFakeTestPartResultReporterWithThreadsTest
    ExpectFailureWithThreadsTest;

TEST_F(ExpectFailureWithThreadsTest, ExpectFatalFailureOnAllThreads) {
  EXPECT_FATAL_FAILURE_ON_ALL_THREADS(AddFailureInOtherThread(FATAL_FAILURE),
                                      "Expected fatal failure.");
}

TEST_F(ExpectFailureWithThreadsTest, ExpectNonFatalFailureOnAllThreads) {
  EXPECT_NONFATAL_FAILURE_ON_ALL_THREADS(
      AddFailureInOtherThread(NONFATAL_FAILURE), "Expected non-fatal failure.");
}

#endif

TEST(TestPropertyTest, ConstructorWorks) {
  const TestProperty property("key", "value");
  EXPECT_STREQ("key", property.key());
  EXPECT_STREQ("value", property.value());
}

TEST(TestPropertyTest, SetValue) {
  TestProperty property("key", "value_1");
  EXPECT_STREQ("key", property.key());
  property.SetValue("value_2");
  EXPECT_STREQ("key", property.key());
  EXPECT_STREQ("value_2", property.value());
}

class TestResultTest : public Test {
 protected:
  typedef std::vector<TestPartResult> TPRVector;

  TestPartResult *pr1, *pr2;

  TestResult *r0, *r1, *r2;

  void SetUp() override {

    pr1 = new TestPartResult(TestPartResult::kSuccess, "foo/bar.cc", 10,
                             "Success!");

    pr2 = new TestPartResult(TestPartResult::kFatalFailure, "foo/bar.cc",
                             -1,
                             "Failure!");

    r0 = new TestResult();
    r1 = new TestResult();
    r2 = new TestResult();

    TPRVector* results1 =
        const_cast<TPRVector*>(&TestResultAccessor::test_part_results(*r1));
    TPRVector* results2 =
        const_cast<TPRVector*>(&TestResultAccessor::test_part_results(*r2));

    results1->push_back(*pr1);

    results2->push_back(*pr1);
    results2->push_back(*pr2);
  }

  void TearDown() override {
    delete pr1;
    delete pr2;

    delete r0;
    delete r1;
    delete r2;
  }

  static void CompareTestPartResult(const TestPartResult& expected,
                                    const TestPartResult& actual) {
    EXPECT_EQ(expected.type(), actual.type());
    EXPECT_STREQ(expected.file_name(), actual.file_name());
    EXPECT_EQ(expected.line_number(), actual.line_number());
    EXPECT_STREQ(expected.summary(), actual.summary());
    EXPECT_STREQ(expected.message(), actual.message());
    EXPECT_EQ(expected.passed(), actual.passed());
    EXPECT_EQ(expected.failed(), actual.failed());
    EXPECT_EQ(expected.nonfatally_failed(), actual.nonfatally_failed());
    EXPECT_EQ(expected.fatally_failed(), actual.fatally_failed());
  }
};

TEST_F(TestResultTest, total_part_count) {
  ASSERT_EQ(0, r0->total_part_count());
  ASSERT_EQ(1, r1->total_part_count());
  ASSERT_EQ(2, r2->total_part_count());
}

TEST_F(TestResultTest, Passed) {
  ASSERT_TRUE(r0->Passed());
  ASSERT_TRUE(r1->Passed());
  ASSERT_FALSE(r2->Passed());
}

TEST_F(TestResultTest, Failed) {
  ASSERT_FALSE(r0->Failed());
  ASSERT_FALSE(r1->Failed());
  ASSERT_TRUE(r2->Failed());
}

typedef TestResultTest TestResultDeathTest;

TEST_F(TestResultDeathTest, GetTestPartResult) {
  CompareTestPartResult(*pr1, r2->GetTestPartResult(0));
  CompareTestPartResult(*pr2, r2->GetTestPartResult(1));
  EXPECT_DEATH_IF_SUPPORTED(r2->GetTestPartResult(2), "");
  EXPECT_DEATH_IF_SUPPORTED(r2->GetTestPartResult(-1), "");
}

TEST(TestResultPropertyTest, NoPropertiesFoundWhenNoneAreAdded) {
  TestResult test_result;
  ASSERT_EQ(0, test_result.test_property_count());
}

TEST(TestResultPropertyTest, OnePropertyFoundWhenAdded) {
  TestResult test_result;
  TestProperty property("key_1", "1");
  TestResultAccessor::RecordProperty(&test_result, "testcase", property);
  ASSERT_EQ(1, test_result.test_property_count());
  const TestProperty& actual_property = test_result.GetTestProperty(0);
  EXPECT_STREQ("key_1", actual_property.key());
  EXPECT_STREQ("1", actual_property.value());
}

TEST(TestResultPropertyTest, MultiplePropertiesFoundWhenAdded) {
  TestResult test_result;
  TestProperty property_1("key_1", "1");
  TestProperty property_2("key_2", "2");
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_1);
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_2);
  ASSERT_EQ(2, test_result.test_property_count());
  const TestProperty& actual_property_1 = test_result.GetTestProperty(0);
  EXPECT_STREQ("key_1", actual_property_1.key());
  EXPECT_STREQ("1", actual_property_1.value());

  const TestProperty& actual_property_2 = test_result.GetTestProperty(1);
  EXPECT_STREQ("key_2", actual_property_2.key());
  EXPECT_STREQ("2", actual_property_2.value());
}

TEST(TestResultPropertyTest, OverridesValuesForDuplicateKeys) {
  TestResult test_result;
  TestProperty property_1_1("key_1", "1");
  TestProperty property_2_1("key_2", "2");
  TestProperty property_1_2("key_1", "12");
  TestProperty property_2_2("key_2", "22");
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_1_1);
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_2_1);
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_1_2);
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_2_2);

  ASSERT_EQ(2, test_result.test_property_count());
  const TestProperty& actual_property_1 = test_result.GetTestProperty(0);
  EXPECT_STREQ("key_1", actual_property_1.key());
  EXPECT_STREQ("12", actual_property_1.value());

  const TestProperty& actual_property_2 = test_result.GetTestProperty(1);
  EXPECT_STREQ("key_2", actual_property_2.key());
  EXPECT_STREQ("22", actual_property_2.value());
}

TEST(TestResultPropertyTest, GetTestProperty) {
  TestResult test_result;
  TestProperty property_1("key_1", "1");
  TestProperty property_2("key_2", "2");
  TestProperty property_3("key_3", "3");
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_1);
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_2);
  TestResultAccessor::RecordProperty(&test_result, "testcase", property_3);

  const TestProperty& fetched_property_1 = test_result.GetTestProperty(0);
  const TestProperty& fetched_property_2 = test_result.GetTestProperty(1);
  const TestProperty& fetched_property_3 = test_result.GetTestProperty(2);

  EXPECT_STREQ("key_1", fetched_property_1.key());
  EXPECT_STREQ("1", fetched_property_1.value());

  EXPECT_STREQ("key_2", fetched_property_2.key());
  EXPECT_STREQ("2", fetched_property_2.value());

  EXPECT_STREQ("key_3", fetched_property_3.key());
  EXPECT_STREQ("3", fetched_property_3.value());

  EXPECT_DEATH_IF_SUPPORTED(test_result.GetTestProperty(3), "");
  EXPECT_DEATH_IF_SUPPORTED(test_result.GetTestProperty(-1), "");
}

class GTestFlagSaverTest : public Test {
 protected:

  static void SetUpTestSuite() {
    saver_ = new GTestFlagSaver;

    GTEST_FLAG_SET(also_run_disabled_tests, false);
    GTEST_FLAG_SET(break_on_failure, false);
    GTEST_FLAG_SET(catch_exceptions, false);
    GTEST_FLAG_SET(death_test_use_fork, false);
    GTEST_FLAG_SET(color, "auto");
    GTEST_FLAG_SET(fail_fast, false);
    GTEST_FLAG_SET(filter, "");
    GTEST_FLAG_SET(list_tests, false);
    GTEST_FLAG_SET(output, "");
    GTEST_FLAG_SET(brief, false);
    GTEST_FLAG_SET(print_time, true);
    GTEST_FLAG_SET(random_seed, 0);
    GTEST_FLAG_SET(repeat, 1);
    GTEST_FLAG_SET(recreate_environments_when_repeating, true);
    GTEST_FLAG_SET(shuffle, false);
    GTEST_FLAG_SET(stack_trace_depth, kMaxStackTraceDepth);
    GTEST_FLAG_SET(stream_result_to, "");
    GTEST_FLAG_SET(throw_on_failure, false);
  }

  static void TearDownTestSuite() {
    delete saver_;
    saver_ = nullptr;
  }

  void VerifyAndModifyFlags() {
    EXPECT_FALSE(GTEST_FLAG_GET(also_run_disabled_tests));
    EXPECT_FALSE(GTEST_FLAG_GET(break_on_failure));
    EXPECT_FALSE(GTEST_FLAG_GET(catch_exceptions));
    EXPECT_STREQ("auto", GTEST_FLAG_GET(color).c_str());
    EXPECT_FALSE(GTEST_FLAG_GET(death_test_use_fork));
    EXPECT_FALSE(GTEST_FLAG_GET(fail_fast));
    EXPECT_STREQ("", GTEST_FLAG_GET(filter).c_str());
    EXPECT_FALSE(GTEST_FLAG_GET(list_tests));
    EXPECT_STREQ("", GTEST_FLAG_GET(output).c_str());
    EXPECT_FALSE(GTEST_FLAG_GET(brief));
    EXPECT_TRUE(GTEST_FLAG_GET(print_time));
    EXPECT_EQ(0, GTEST_FLAG_GET(random_seed));
    EXPECT_EQ(1, GTEST_FLAG_GET(repeat));
    EXPECT_TRUE(GTEST_FLAG_GET(recreate_environments_when_repeating));
    EXPECT_FALSE(GTEST_FLAG_GET(shuffle));
    EXPECT_EQ(kMaxStackTraceDepth, GTEST_FLAG_GET(stack_trace_depth));
    EXPECT_STREQ("", GTEST_FLAG_GET(stream_result_to).c_str());
    EXPECT_FALSE(GTEST_FLAG_GET(throw_on_failure));

    GTEST_FLAG_SET(also_run_disabled_tests, true);
    GTEST_FLAG_SET(break_on_failure, true);
    GTEST_FLAG_SET(catch_exceptions, true);
    GTEST_FLAG_SET(color, "no");
    GTEST_FLAG_SET(death_test_use_fork, true);
    GTEST_FLAG_SET(fail_fast, true);
    GTEST_FLAG_SET(filter, "abc");
    GTEST_FLAG_SET(list_tests, true);
    GTEST_FLAG_SET(output, "xml:foo.xml");
    GTEST_FLAG_SET(brief, true);
    GTEST_FLAG_SET(print_time, false);
    GTEST_FLAG_SET(random_seed, 1);
    GTEST_FLAG_SET(repeat, 100);
    GTEST_FLAG_SET(recreate_environments_when_repeating, false);
    GTEST_FLAG_SET(shuffle, true);
    GTEST_FLAG_SET(stack_trace_depth, 1);
    GTEST_FLAG_SET(stream_result_to, "localhost:1234");
    GTEST_FLAG_SET(throw_on_failure, true);
  }

 private:

  static GTestFlagSaver* saver_;
};

GTestFlagSaver* GTestFlagSaverTest::saver_ = nullptr;

TEST_F(GTestFlagSaverTest, ModifyGTestFlags) { VerifyAndModifyFlags(); }

TEST_F(GTestFlagSaverTest, VerifyGTestFlags) { VerifyAndModifyFlags(); }

static void SetEnv(const char* name, const char* value) {
#ifdef GTEST_OS_WINDOWS_MOBILE

  return;
#elif defined(__BORLANDC__) || defined(__SunOS_5_8) || defined(__SunOS_5_9)

  static std::map<std::string, std::string*> added_env;

  std::string* prev_env = NULL;
  if (added_env.find(name) != added_env.end()) {
    prev_env = added_env[name];
  }
  added_env[name] =
      new std::string((Message() << name << "=" << value).GetString());

  putenv(const_cast<char*>(added_env[name]->c_str()));
  delete prev_env;
#elif defined(GTEST_OS_WINDOWS)
  _putenv((Message() << name << "=" << value).GetString().c_str());
#else
  if (*value == '\0') {
    unsetenv(name);
  } else {
    setenv(name, value, 1);
  }
#endif
}

#ifndef GTEST_OS_WINDOWS_MOBILE

using testing::internal::Int32FromGTestEnv;

TEST(Int32FromGTestEnvTest, ReturnsDefaultWhenVariableIsNotSet) {
  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "TEMP", "");
  EXPECT_EQ(10, Int32FromGTestEnv("temp", 10));
}

#if !defined(GTEST_GET_INT32_FROM_ENV_)

TEST(Int32FromGTestEnvTest, ReturnsDefaultWhenValueOverflows) {
  printf("(expecting 2 warnings)\n");

  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "TEMP", "12345678987654321");
  EXPECT_EQ(20, Int32FromGTestEnv("temp", 20));

  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "TEMP", "-12345678987654321");
  EXPECT_EQ(30, Int32FromGTestEnv("temp", 30));
}

TEST(Int32FromGTestEnvTest, ReturnsDefaultWhenValueIsInvalid) {
  printf("(expecting 2 warnings)\n");

  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "TEMP", "A1");
  EXPECT_EQ(40, Int32FromGTestEnv("temp", 40));

  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "TEMP", "12X");
  EXPECT_EQ(50, Int32FromGTestEnv("temp", 50));
}

#endif

TEST(Int32FromGTestEnvTest, ParsesAndReturnsValidValue) {
  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "TEMP", "123");
  EXPECT_EQ(123, Int32FromGTestEnv("temp", 0));

  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "TEMP", "-321");
  EXPECT_EQ(-321, Int32FromGTestEnv("temp", 0));
}
#endif

TEST(ParseInt32FlagTest, ReturnsFalseForInvalidFlag) {
  int32_t value = 123;
  EXPECT_FALSE(ParseFlag("--a=100", "b", &value));
  EXPECT_EQ(123, value);

  EXPECT_FALSE(ParseFlag("a=100", "a", &value));
  EXPECT_EQ(123, value);
}

TEST(ParseInt32FlagTest, ReturnsDefaultWhenValueOverflows) {
  printf("(expecting 2 warnings)\n");

  int32_t value = 123;
  EXPECT_FALSE(ParseFlag("--abc=12345678987654321", "abc", &value));
  EXPECT_EQ(123, value);

  EXPECT_FALSE(ParseFlag("--abc=-12345678987654321", "abc", &value));
  EXPECT_EQ(123, value);
}

TEST(ParseInt32FlagTest, ReturnsDefaultWhenValueIsInvalid) {
  printf("(expecting 2 warnings)\n");

  int32_t value = 123;
  EXPECT_FALSE(ParseFlag("--abc=A1", "abc", &value));
  EXPECT_EQ(123, value);

  EXPECT_FALSE(ParseFlag("--abc=12X", "abc", &value));
  EXPECT_EQ(123, value);
}

TEST(ParseInt32FlagTest, ParsesAndReturnsValidValue) {
  int32_t value = 123;
  EXPECT_TRUE(ParseFlag("--" GTEST_FLAG_PREFIX_ "abc=456", "abc", &value));
  EXPECT_EQ(456, value);

  EXPECT_TRUE(ParseFlag("--" GTEST_FLAG_PREFIX_ "abc=-789", "abc", &value));
  EXPECT_EQ(-789, value);
}

#ifndef GTEST_OS_WINDOWS_MOBILE
TEST(Int32FromEnvOrDieTest, ParsesAndReturnsValidValue) {
  EXPECT_EQ(333, Int32FromEnvOrDie(GTEST_FLAG_PREFIX_UPPER_ "UnsetVar", 333));
  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "UnsetVar", "123");
  EXPECT_EQ(123, Int32FromEnvOrDie(GTEST_FLAG_PREFIX_UPPER_ "UnsetVar", 333));
  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "UnsetVar", "-123");
  EXPECT_EQ(-123, Int32FromEnvOrDie(GTEST_FLAG_PREFIX_UPPER_ "UnsetVar", 333));
}
#endif

TEST(Int32FromEnvOrDieDeathTest, AbortsOnFailure) {
  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "VAR", "xxx");
  EXPECT_DEATH_IF_SUPPORTED(
      Int32FromEnvOrDie(GTEST_FLAG_PREFIX_UPPER_ "VAR", 123), ".*");
}

TEST(Int32FromEnvOrDieDeathTest, AbortsOnInt32Overflow) {
  SetEnv(GTEST_FLAG_PREFIX_UPPER_ "VAR", "1234567891234567891234");
  EXPECT_DEATH_IF_SUPPORTED(
      Int32FromEnvOrDie(GTEST_FLAG_PREFIX_UPPER_ "VAR", 123), ".*");
}

TEST(ShouldRunTestOnShardTest, IsPartitionWhenThereIsOneShard) {
  EXPECT_TRUE(ShouldRunTestOnShard(1, 0, 0));
  EXPECT_TRUE(ShouldRunTestOnShard(1, 0, 1));
  EXPECT_TRUE(ShouldRunTestOnShard(1, 0, 2));
  EXPECT_TRUE(ShouldRunTestOnShard(1, 0, 3));
  EXPECT_TRUE(ShouldRunTestOnShard(1, 0, 4));
}

class ShouldShardTest : public testing::Test {
 protected:
  void SetUp() override {
    index_var_ = GTEST_FLAG_PREFIX_UPPER_ "INDEX";
    total_var_ = GTEST_FLAG_PREFIX_UPPER_ "TOTAL";
  }

  void TearDown() override {
    SetEnv(index_var_, "");
    SetEnv(total_var_, "");
  }

  const char* index_var_;
  const char* total_var_;
};

TEST_F(ShouldShardTest, ReturnsFalseWhenNeitherEnvVarIsSet) {
  SetEnv(index_var_, "");
  SetEnv(total_var_, "");

  EXPECT_FALSE(ShouldShard(total_var_, index_var_, false));
  EXPECT_FALSE(ShouldShard(total_var_, index_var_, true));
}

TEST_F(ShouldShardTest, ReturnsFalseWhenTotalShardIsOne) {
  SetEnv(index_var_, "0");
  SetEnv(total_var_, "1");
  EXPECT_FALSE(ShouldShard(total_var_, index_var_, false));
  EXPECT_FALSE(ShouldShard(total_var_, index_var_, true));
}

#ifndef GTEST_OS_WINDOWS_MOBILE
TEST_F(ShouldShardTest, WorksWhenShardEnvVarsAreValid) {
  SetEnv(index_var_, "4");
  SetEnv(total_var_, "22");
  EXPECT_TRUE(ShouldShard(total_var_, index_var_, false));
  EXPECT_FALSE(ShouldShard(total_var_, index_var_, true));

  SetEnv(index_var_, "8");
  SetEnv(total_var_, "9");
  EXPECT_TRUE(ShouldShard(total_var_, index_var_, false));
  EXPECT_FALSE(ShouldShard(total_var_, index_var_, true));

  SetEnv(index_var_, "0");
  SetEnv(total_var_, "9");
  EXPECT_TRUE(ShouldShard(total_var_, index_var_, false));
  EXPECT_FALSE(ShouldShard(total_var_, index_var_, true));
}
#endif

typedef ShouldShardTest ShouldShardDeathTest;

TEST_F(ShouldShardDeathTest, AbortsWhenShardingEnvVarsAreInvalid) {
  SetEnv(index_var_, "4");
  SetEnv(total_var_, "4");
  EXPECT_DEATH_IF_SUPPORTED(ShouldShard(total_var_, index_var_, false), ".*");

  SetEnv(index_var_, "4");
  SetEnv(total_var_, "-2");
  EXPECT_DEATH_IF_SUPPORTED(ShouldShard(total_var_, index_var_, false), ".*");

  SetEnv(index_var_, "5");
  SetEnv(total_var_, "");
  EXPECT_DEATH_IF_SUPPORTED(ShouldShard(total_var_, index_var_, false), ".*");

  SetEnv(index_var_, "");
  SetEnv(total_var_, "5");
  EXPECT_DEATH_IF_SUPPORTED(ShouldShard(total_var_, index_var_, false), ".*");
}

TEST(ShouldRunTestOnShardTest, IsPartitionWhenThereAreFiveShards) {

  const int num_tests = 17;
  const int num_shards = 5;

  for (int test_id = 0; test_id < num_tests; test_id++) {
    int prev_selected_shard_index = -1;
    for (int shard_index = 0; shard_index < num_shards; shard_index++) {
      if (ShouldRunTestOnShard(num_shards, shard_index, test_id)) {
        if (prev_selected_shard_index < 0) {
          prev_selected_shard_index = shard_index;
        } else {
          ADD_FAILURE() << "Shard " << prev_selected_shard_index << " and "
                        << shard_index << " are both selected to run test "
                        << test_id;
        }
      }
    }
  }

  for (int shard_index = 0; shard_index < num_shards; shard_index++) {
    int num_tests_on_shard = 0;
    for (int test_id = 0; test_id < num_tests; test_id++) {
      num_tests_on_shard +=
          ShouldRunTestOnShard(num_shards, shard_index, test_id);
    }
    EXPECT_GE(num_tests_on_shard, num_tests / num_shards);
  }
}

TEST(UnitTestTest, CanGetOriginalWorkingDir) {
  ASSERT_TRUE(UnitTest::GetInstance()->original_working_dir() != nullptr);
  EXPECT_STRNE(UnitTest::GetInstance()->original_working_dir(), "");
}

TEST(UnitTestTest, ReturnsPlausibleTimestamp) {
  EXPECT_LT(0, UnitTest::GetInstance()->start_timestamp());
  EXPECT_LE(UnitTest::GetInstance()->start_timestamp(), GetTimeInMillis());
}

void ExpectNonFatalFailureRecordingPropertyWithReservedKey(
    const TestResult& test_result, const char* key) {
  EXPECT_NONFATAL_FAILURE(Test::RecordProperty(key, "1"), "Reserved key");
  ASSERT_EQ(0, test_result.test_property_count())
      << "Property for key '" << key << "' recorded unexpectedly.";
}

void ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTest(
    const char* key) {
  const TestInfo* test_info = UnitTest::GetInstance()->current_test_info();
  ASSERT_TRUE(test_info != nullptr);
  ExpectNonFatalFailureRecordingPropertyWithReservedKey(*test_info->result(),
                                                        key);
}

void ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTestSuite(
    const char* key) {
  const testing::TestSuite* test_suite =
      UnitTest::GetInstance()->current_test_suite();
  ASSERT_TRUE(test_suite != nullptr);
  ExpectNonFatalFailureRecordingPropertyWithReservedKey(
      test_suite->ad_hoc_test_result(), key);
}

void ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
    const char* key) {
  ExpectNonFatalFailureRecordingPropertyWithReservedKey(
      UnitTest::GetInstance()->ad_hoc_test_result(), key);
}

class UnitTestRecordPropertyTest
    : public testing::internal::UnitTestRecordPropertyTestHelper {
 public:
  static void SetUpTestSuite() {
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTestSuite(
        "disabled");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTestSuite(
        "errors");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTestSuite(
        "failures");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTestSuite(
        "name");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTestSuite(
        "tests");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTestSuite(
        "time");

    Test::RecordProperty("test_case_key_1", "1");

    const testing::TestSuite* test_suite =
        UnitTest::GetInstance()->current_test_suite();

    ASSERT_TRUE(test_suite != nullptr);

    ASSERT_EQ(1, test_suite->ad_hoc_test_result().test_property_count());
    EXPECT_STREQ("test_case_key_1",
                 test_suite->ad_hoc_test_result().GetTestProperty(0).key());
    EXPECT_STREQ("1",
                 test_suite->ad_hoc_test_result().GetTestProperty(0).value());
  }
};

TEST_F(UnitTestRecordPropertyTest, OnePropertyFoundWhenAdded) {
  UnitTestRecordProperty("key_1", "1");

  ASSERT_EQ(1, unit_test_.ad_hoc_test_result().test_property_count());

  EXPECT_STREQ("key_1",
               unit_test_.ad_hoc_test_result().GetTestProperty(0).key());
  EXPECT_STREQ("1", unit_test_.ad_hoc_test_result().GetTestProperty(0).value());
}

TEST_F(UnitTestRecordPropertyTest, MultiplePropertiesFoundWhenAdded) {
  UnitTestRecordProperty("key_1", "1");
  UnitTestRecordProperty("key_2", "2");

  ASSERT_EQ(2, unit_test_.ad_hoc_test_result().test_property_count());

  EXPECT_STREQ("key_1",
               unit_test_.ad_hoc_test_result().GetTestProperty(0).key());
  EXPECT_STREQ("1", unit_test_.ad_hoc_test_result().GetTestProperty(0).value());

  EXPECT_STREQ("key_2",
               unit_test_.ad_hoc_test_result().GetTestProperty(1).key());
  EXPECT_STREQ("2", unit_test_.ad_hoc_test_result().GetTestProperty(1).value());
}

TEST_F(UnitTestRecordPropertyTest, OverridesValuesForDuplicateKeys) {
  UnitTestRecordProperty("key_1", "1");
  UnitTestRecordProperty("key_2", "2");
  UnitTestRecordProperty("key_1", "12");
  UnitTestRecordProperty("key_2", "22");

  ASSERT_EQ(2, unit_test_.ad_hoc_test_result().test_property_count());

  EXPECT_STREQ("key_1",
               unit_test_.ad_hoc_test_result().GetTestProperty(0).key());
  EXPECT_STREQ("12",
               unit_test_.ad_hoc_test_result().GetTestProperty(0).value());

  EXPECT_STREQ("key_2",
               unit_test_.ad_hoc_test_result().GetTestProperty(1).key());
  EXPECT_STREQ("22",
               unit_test_.ad_hoc_test_result().GetTestProperty(1).value());
}

TEST_F(UnitTestRecordPropertyTest,
       AddFailureInsideTestsWhenUsingTestSuiteReservedKeys) {
  ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTest("name");
  ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTest(
      "value_param");
  ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTest(
      "type_param");
  ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTest("status");
  ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTest("time");
  ExpectNonFatalFailureRecordingPropertyWithReservedKeyForCurrentTest(
      "classname");
}

TEST_F(UnitTestRecordPropertyTest,
       AddRecordWithReservedKeysGeneratesCorrectPropertyList) {
  EXPECT_NONFATAL_FAILURE(
      Test::RecordProperty("name", "1"),
      "'classname', 'name', 'status', 'time', 'type_param', 'value_param',"
      " 'file', and 'line' are reserved");
}

class UnitTestRecordPropertyTestEnvironment : public Environment {
 public:
  void TearDown() override {
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
        "tests");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
        "failures");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
        "disabled");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
        "errors");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
        "name");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
        "timestamp");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
        "time");
    ExpectNonFatalFailureRecordingPropertyWithReservedKeyOutsideOfTestSuite(
        "random_seed");
  }
};

[[maybe_unused]] static Environment* record_property_env =
    AddGlobalTestEnvironment(new UnitTestRecordPropertyTestEnvironment);

bool IsEven(int n) { return (n % 2) == 0; }

struct IsEvenFunctor {
  bool operator()(int n) { return IsEven(n); }
};

AssertionResult AssertIsEven(const char* expr, int n) {
  if (IsEven(n)) {
    return AssertionSuccess();
  }

  Message msg;
  msg << expr << " evaluates to " << n << ", which is not even.";
  return AssertionFailure(msg);
}

AssertionResult ResultIsEven(int n) {
  if (IsEven(n))
    return AssertionSuccess() << n << " is even";
  else
    return AssertionFailure() << n << " is odd";
}

AssertionResult ResultIsEvenNoExplanation(int n) {
  if (IsEven(n))
    return AssertionSuccess();
  else
    return AssertionFailure() << n << " is odd";
}

struct AssertIsEvenFunctor {
  AssertionResult operator()(const char* expr, int n) {
    return AssertIsEven(expr, n);
  }
};

bool SumIsEven2(int n1, int n2) { return IsEven(n1 + n2); }

struct SumIsEven3Functor {
  bool operator()(int n1, int n2, int n3) { return IsEven(n1 + n2 + n3); }
};

AssertionResult AssertSumIsEven4(const char* e1, const char* e2, const char* e3,
                                 const char* e4, int n1, int n2, int n3,
                                 int n4) {
  const int sum = n1 + n2 + n3 + n4;
  if (IsEven(sum)) {
    return AssertionSuccess();
  }

  Message msg;
  msg << e1 << " + " << e2 << " + " << e3 << " + " << e4 << " (" << n1 << " + "
      << n2 << " + " << n3 << " + " << n4 << ") evaluates to " << sum
      << ", which is not even.";
  return AssertionFailure(msg);
}

struct AssertSumIsEven5Functor {
  AssertionResult operator()(const char* e1, const char* e2, const char* e3,
                             const char* e4, const char* e5, int n1, int n2,
                             int n3, int n4, int n5) {
    const int sum = n1 + n2 + n3 + n4 + n5;
    if (IsEven(sum)) {
      return AssertionSuccess();
    }

    Message msg;
    msg << e1 << " + " << e2 << " + " << e3 << " + " << e4 << " + " << e5
        << " (" << n1 << " + " << n2 << " + " << n3 << " + " << n4 << " + "
        << n5 << ") evaluates to " << sum << ", which is not even.";
    return AssertionFailure(msg);
  }
};

TEST(Pred1Test, WithoutFormat) {

  EXPECT_PRED1(IsEvenFunctor(), 2) << "This failure is UNEXPECTED!";
  ASSERT_PRED1(IsEven, 4);

  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED1(IsEven, 5) << "This failure is expected.";
      },
      "This failure is expected.");
  EXPECT_FATAL_FAILURE(ASSERT_PRED1(IsEvenFunctor(), 5), "evaluates to false");
}

TEST(Pred1Test, WithFormat) {

  EXPECT_PRED_FORMAT1(AssertIsEven, 2);
  ASSERT_PRED_FORMAT1(AssertIsEvenFunctor(), 4)
      << "This failure is UNEXPECTED!";

  const int n = 5;
  EXPECT_NONFATAL_FAILURE(EXPECT_PRED_FORMAT1(AssertIsEvenFunctor(), n),
                          "n evaluates to 5, which is not even.");
  EXPECT_FATAL_FAILURE(
      {
        ASSERT_PRED_FORMAT1(AssertIsEven, 5) << "This failure is expected.";
      },
      "This failure is expected.");
}

TEST(Pred1Test, SingleEvaluationOnFailure) {

  static int n = 0;
  EXPECT_PRED1(IsEven, n++);
  EXPECT_EQ(1, n) << "The argument is not evaluated exactly once.";

  EXPECT_FATAL_FAILURE(
      {
        ASSERT_PRED_FORMAT1(AssertIsEvenFunctor(), n++)
            << "This failure is expected.";
      },
      "This failure is expected.");
  EXPECT_EQ(2, n) << "The argument is not evaluated exactly once.";
}

TEST(PredTest, WithoutFormat) {

  ASSERT_PRED2(SumIsEven2, 2, 4) << "This failure is UNEXPECTED!";
  EXPECT_PRED3(SumIsEven3Functor(), 4, 6, 8);

  const int n1 = 1;
  const int n2 = 2;
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED2(SumIsEven2, n1, n2) << "This failure is expected.";
      },
      "This failure is expected.");
  EXPECT_FATAL_FAILURE(
      {
        ASSERT_PRED3(SumIsEven3Functor(), 1, 2, 4);
      },
      "evaluates to false");
}

TEST(PredTest, WithFormat) {

  ASSERT_PRED_FORMAT4(AssertSumIsEven4, 4, 6, 8, 10)
      << "This failure is UNEXPECTED!";
  EXPECT_PRED_FORMAT5(AssertSumIsEven5Functor(), 2, 4, 6, 8, 10);

  const int n1 = 1;
  const int n2 = 2;
  const int n3 = 4;
  const int n4 = 6;
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED_FORMAT4(AssertSumIsEven4, n1, n2, n3, n4);
      },
      "evaluates to 13, which is not even.");
  EXPECT_FATAL_FAILURE(
      {
        ASSERT_PRED_FORMAT5(AssertSumIsEven5Functor(), 1, 2, 4, 6, 8)
            << "This failure is expected.";
      },
      "This failure is expected.");
}

TEST(PredTest, SingleEvaluationOnFailure) {

  int n1 = 0;
  int n2 = 0;
  EXPECT_PRED2(SumIsEven2, n1++, n2++);
  EXPECT_EQ(1, n1) << "Argument 1 is not evaluated exactly once.";
  EXPECT_EQ(1, n2) << "Argument 2 is not evaluated exactly once.";

  n1 = n2 = 0;
  int n3 = 0;
  int n4 = 0;
  int n5 = 0;
  ASSERT_PRED_FORMAT5(AssertSumIsEven5Functor(), n1++, n2++, n3++, n4++, n5++)
      << "This failure is UNEXPECTED!";
  EXPECT_EQ(1, n1) << "Argument 1 is not evaluated exactly once.";
  EXPECT_EQ(1, n2) << "Argument 2 is not evaluated exactly once.";
  EXPECT_EQ(1, n3) << "Argument 3 is not evaluated exactly once.";
  EXPECT_EQ(1, n4) << "Argument 4 is not evaluated exactly once.";
  EXPECT_EQ(1, n5) << "Argument 5 is not evaluated exactly once.";

  n1 = n2 = n3 = 0;
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED3(SumIsEven3Functor(), ++n1, n2++, n3++)
            << "This failure is expected.";
      },
      "This failure is expected.");
  EXPECT_EQ(1, n1) << "Argument 1 is not evaluated exactly once.";
  EXPECT_EQ(1, n2) << "Argument 2 is not evaluated exactly once.";
  EXPECT_EQ(1, n3) << "Argument 3 is not evaluated exactly once.";

  n1 = n2 = n3 = n4 = 0;
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED_FORMAT4(AssertSumIsEven4, ++n1, n2++, n3++, n4++);
      },
      "evaluates to 1, which is not even.");
  EXPECT_EQ(1, n1) << "Argument 1 is not evaluated exactly once.";
  EXPECT_EQ(1, n2) << "Argument 2 is not evaluated exactly once.";
  EXPECT_EQ(1, n3) << "Argument 3 is not evaluated exactly once.";
  EXPECT_EQ(1, n4) << "Argument 4 is not evaluated exactly once.";
}

TEST(PredTest, ExpectPredEvalFailure) {
  std::set<int> set_a = {2, 1, 3, 4, 5};
  std::set<int> set_b = {0, 4, 8};
  const auto compare_sets = [](std::set<int>, std::set<int>) { return false; };
  EXPECT_NONFATAL_FAILURE(
      EXPECT_PRED2(compare_sets, set_a, set_b),
      "compare_sets(set_a, set_b) evaluates to false, where\nset_a evaluates "
      "to { 1, 2, 3, 4, 5 }\nset_b evaluates to { 0, 4, 8 }");
}

bool IsPositive(double x) { return x > 0; }

template <typename T>
bool IsNegative(T x) {
  return x < 0;
}

template <typename T1, typename T2>
bool GreaterThan(T1 x1, T2 x2) {
  return x1 > x2;
}

TEST(PredicateAssertionTest, AcceptsOverloadedFunction) {

  EXPECT_PRED1((bool (*)(int))(IsPositive), 5);
  ASSERT_PRED1((bool (*)(double))(IsPositive), 6.0);
}

TEST(PredicateAssertionTest, AcceptsTemplateFunction) {
  EXPECT_PRED1(IsNegative<int>, -5);

  ASSERT_PRED2((GreaterThan<int, int>), 5, 0);
}

AssertionResult IsPositiveFormat(const char*           , int n) {
  return n > 0 ? AssertionSuccess() : AssertionFailure(Message() << "Failure");
}

AssertionResult IsPositiveFormat(const char*           , double x) {
  return x > 0 ? AssertionSuccess() : AssertionFailure(Message() << "Failure");
}

template <typename T>
AssertionResult IsNegativeFormat(const char*           , T x) {
  return x < 0 ? AssertionSuccess() : AssertionFailure(Message() << "Failure");
}

template <typename T1, typename T2>
AssertionResult EqualsFormat(const char*            , const char*            ,
                             const T1& x1, const T2& x2) {
  return x1 == x2 ? AssertionSuccess()
                  : AssertionFailure(Message() << "Failure");
}

TEST(PredicateFormatAssertionTest, AcceptsOverloadedFunction) {
  EXPECT_PRED_FORMAT1(IsPositiveFormat, 5);
  ASSERT_PRED_FORMAT1(IsPositiveFormat, 6.0);
}

TEST(PredicateFormatAssertionTest, AcceptsTemplateFunction) {
  EXPECT_PRED_FORMAT1(IsNegativeFormat, -5);
  ASSERT_PRED_FORMAT2(EqualsFormat, 3, 3);
}

TEST(StringAssertionTest, ASSERT_STREQ) {
  const char* const p1 = "good";
  ASSERT_STREQ(p1, p1);

  const char p2[] = "good";
  ASSERT_STREQ(p1, p2);

  EXPECT_FATAL_FAILURE(ASSERT_STREQ("bad", "good"), "  \"bad\"\n  \"good\"");
}

TEST(StringAssertionTest, ASSERT_STREQ_Null) {
  ASSERT_STREQ(static_cast<const char*>(nullptr), nullptr);
  EXPECT_FATAL_FAILURE(ASSERT_STREQ(nullptr, "non-null"), "non-null");
}

TEST(StringAssertionTest, ASSERT_STREQ_Null2) {
  EXPECT_FATAL_FAILURE(ASSERT_STREQ("non-null", nullptr), "non-null");
}

TEST(StringAssertionTest, ASSERT_STRNE) {
  ASSERT_STRNE("hi", "Hi");
  ASSERT_STRNE("Hi", nullptr);
  ASSERT_STRNE(nullptr, "Hi");
  ASSERT_STRNE("", nullptr);
  ASSERT_STRNE(nullptr, "");
  ASSERT_STRNE("", "Hi");
  ASSERT_STRNE("Hi", "");
  EXPECT_FATAL_FAILURE(ASSERT_STRNE("Hi", "Hi"), "\"Hi\" vs \"Hi\"");
}

TEST(StringAssertionTest, ASSERT_STRCASEEQ) {
  ASSERT_STRCASEEQ("hi", "Hi");
  ASSERT_STRCASEEQ(static_cast<const char*>(nullptr), nullptr);

  ASSERT_STRCASEEQ("", "");
  EXPECT_FATAL_FAILURE(ASSERT_STRCASEEQ("Hi", "hi2"), "Ignoring case");
}

TEST(StringAssertionTest, ASSERT_STRCASENE) {
  ASSERT_STRCASENE("hi1", "Hi2");
  ASSERT_STRCASENE("Hi", nullptr);
  ASSERT_STRCASENE(nullptr, "Hi");
  ASSERT_STRCASENE("", nullptr);
  ASSERT_STRCASENE(nullptr, "");
  ASSERT_STRCASENE("", "Hi");
  ASSERT_STRCASENE("Hi", "");
  EXPECT_FATAL_FAILURE(ASSERT_STRCASENE("Hi", "hi"), "(ignoring case)");
}

TEST(StringAssertionTest, STREQ_Wide) {

  ASSERT_STREQ(static_cast<const wchar_t*>(nullptr), nullptr);

  ASSERT_STREQ(L"", L"");

  EXPECT_NONFATAL_FAILURE(EXPECT_STREQ(L"non-null", nullptr), "non-null");

  EXPECT_STREQ(L"Hi", L"Hi");

  EXPECT_NONFATAL_FAILURE(EXPECT_STREQ(L"abc", L"Abc"), "Abc");

  EXPECT_NONFATAL_FAILURE(EXPECT_STREQ(L"abc\x8119", L"abc\x8120"), "abc");

  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_STREQ(L"abc\x8119", L"abc\x8121") << "Expected failure";
      },
      "Expected failure");
}

TEST(StringAssertionTest, STRNE_Wide) {

  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_STRNE(static_cast<const wchar_t*>(nullptr), nullptr);
      },
      "");

  EXPECT_NONFATAL_FAILURE(EXPECT_STRNE(L"", L""), "L\"\"");

  ASSERT_STRNE(L"non-null", nullptr);

  EXPECT_NONFATAL_FAILURE(EXPECT_STRNE(L"Hi", L"Hi"), "L\"Hi\"");

  EXPECT_STRNE(L"abc", L"Abc");

  EXPECT_NONFATAL_FAILURE(EXPECT_STRNE(L"abc\x8119", L"abc\x8119"), "abc");

  ASSERT_STRNE(L"abc\x8119", L"abc\x8120") << "This shouldn't happen";
}

TEST(IsSubstringTest, ReturnsCorrectResultForCString) {
  EXPECT_FALSE(IsSubstring("", "", nullptr, "a"));
  EXPECT_FALSE(IsSubstring("", "", "b", nullptr));
  EXPECT_FALSE(IsSubstring("", "", "needle", "haystack"));

  EXPECT_TRUE(IsSubstring("", "", static_cast<const char*>(nullptr), nullptr));
  EXPECT_TRUE(IsSubstring("", "", "needle", "two needles"));
}

TEST(IsSubstringTest, ReturnsCorrectResultForWideCString) {
  EXPECT_FALSE(IsSubstring("", "", kNull, L"a"));
  EXPECT_FALSE(IsSubstring("", "", L"b", kNull));
  EXPECT_FALSE(IsSubstring("", "", L"needle", L"haystack"));

  EXPECT_TRUE(
      IsSubstring("", "", static_cast<const wchar_t*>(nullptr), nullptr));
  EXPECT_TRUE(IsSubstring("", "", L"needle", L"two needles"));
}

TEST(IsSubstringTest, GeneratesCorrectMessageForCString) {
  EXPECT_STREQ(
      "Value of: needle_expr\n"
      "  Actual: \"needle\"\n"
      "Expected: a substring of haystack_expr\n"
      "Which is: \"haystack\"",
      IsSubstring("needle_expr", "haystack_expr", "needle", "haystack")
          .failure_message());
}

TEST(IsSubstringTest, ReturnsCorrectResultsForStdString) {
  EXPECT_TRUE(IsSubstring("", "", std::string("hello"), "ahellob"));
  EXPECT_FALSE(IsSubstring("", "", "hello", std::string("world")));
}

#if GTEST_HAS_STD_WSTRING

TEST(IsSubstringTest, ReturnsCorrectResultForStdWstring) {
  EXPECT_TRUE(IsSubstring("", "", ::std::wstring(L"needle"), L"two needles"));
  EXPECT_FALSE(IsSubstring("", "", L"needle", ::std::wstring(L"haystack")));
}

TEST(IsSubstringTest, GeneratesCorrectMessageForWstring) {
  EXPECT_STREQ(
      "Value of: needle_expr\n"
      "  Actual: L\"needle\"\n"
      "Expected: a substring of haystack_expr\n"
      "Which is: L\"haystack\"",
      IsSubstring("needle_expr", "haystack_expr", ::std::wstring(L"needle"),
                  L"haystack")
          .failure_message());
}

#endif

TEST(IsNotSubstringTest, ReturnsCorrectResultForCString) {
  EXPECT_TRUE(IsNotSubstring("", "", "needle", "haystack"));
  EXPECT_FALSE(IsNotSubstring("", "", "needle", "two needles"));
}

TEST(IsNotSubstringTest, ReturnsCorrectResultForWideCString) {
  EXPECT_TRUE(IsNotSubstring("", "", L"needle", L"haystack"));
  EXPECT_FALSE(IsNotSubstring("", "", L"needle", L"two needles"));
}

TEST(IsNotSubstringTest, GeneratesCorrectMessageForWideCString) {
  EXPECT_STREQ(
      "Value of: needle_expr\n"
      "  Actual: L\"needle\"\n"
      "Expected: not a substring of haystack_expr\n"
      "Which is: L\"two needles\"",
      IsNotSubstring("needle_expr", "haystack_expr", L"needle", L"two needles")
          .failure_message());
}

TEST(IsNotSubstringTest, ReturnsCorrectResultsForStdString) {
  EXPECT_FALSE(IsNotSubstring("", "", std::string("hello"), "ahellob"));
  EXPECT_TRUE(IsNotSubstring("", "", "hello", std::string("world")));
}

TEST(IsNotSubstringTest, GeneratesCorrectMessageForStdString) {
  EXPECT_STREQ(
      "Value of: needle_expr\n"
      "  Actual: \"needle\"\n"
      "Expected: not a substring of haystack_expr\n"
      "Which is: \"two needles\"",
      IsNotSubstring("needle_expr", "haystack_expr", ::std::string("needle"),
                     "two needles")
          .failure_message());
}

#if GTEST_HAS_STD_WSTRING

TEST(IsNotSubstringTest, ReturnsCorrectResultForStdWstring) {
  EXPECT_FALSE(
      IsNotSubstring("", "", ::std::wstring(L"needle"), L"two needles"));
  EXPECT_TRUE(IsNotSubstring("", "", L"needle", ::std::wstring(L"haystack")));
}

#endif

template <typename RawType>
class FloatingPointTest : public Test {
 protected:

  struct TestValues {
    RawType close_to_positive_zero;
    RawType close_to_negative_zero;
    RawType further_from_negative_zero;

    RawType close_to_one;
    RawType further_from_one;

    RawType infinity;
    RawType close_to_infinity;
    RawType further_from_infinity;

    RawType nan1;
    RawType nan2;
  };

  typedef typename testing::internal::FloatingPoint<RawType> Floating;
  typedef typename Floating::Bits Bits;

  void SetUp() override {
    const uint32_t max_ulps = Floating::kMaxUlps;

    const Bits zero_bits = Floating(0).bits();

    values_.close_to_positive_zero =
        Floating::ReinterpretBits(zero_bits + max_ulps / 2);
    values_.close_to_negative_zero =
        -Floating::ReinterpretBits(zero_bits + max_ulps - max_ulps / 2);
    values_.further_from_negative_zero =
        -Floating::ReinterpretBits(zero_bits + max_ulps + 1 - max_ulps / 2);

    const Bits one_bits = Floating(1).bits();

    values_.close_to_one = Floating::ReinterpretBits(one_bits + max_ulps);
    values_.further_from_one =
        Floating::ReinterpretBits(one_bits + max_ulps + 1);

    values_.infinity = Floating::Infinity();

    const Bits infinity_bits = Floating(values_.infinity).bits();

    values_.close_to_infinity =
        Floating::ReinterpretBits(infinity_bits - max_ulps);
    values_.further_from_infinity =
        Floating::ReinterpretBits(infinity_bits - max_ulps - 1);

    values_.nan1 = Floating::ReinterpretBits(
        Floating::kExponentBitMask |
        (static_cast<Bits>(1) << (Floating::kFractionBitCount - 1)) | 1);
    values_.nan2 = Floating::ReinterpretBits(
        Floating::kExponentBitMask |
        (static_cast<Bits>(1) << (Floating::kFractionBitCount - 1)) | 200);
  }

  void TestSize() { EXPECT_EQ(sizeof(RawType), sizeof(Bits)); }

  static TestValues values_;
};

template <typename RawType>
typename FloatingPointTest<RawType>::TestValues
    FloatingPointTest<RawType>::values_;

typedef FloatingPointTest<float> FloatTest;

TEST_F(FloatTest, Size) { TestSize(); }

TEST_F(FloatTest, Zeros) {
  EXPECT_FLOAT_EQ(0.0, -0.0);
  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(-0.0, 1.0), "1.0");
  EXPECT_FATAL_FAILURE(ASSERT_FLOAT_EQ(0.0, 1.5), "1.5");
}

TEST_F(FloatTest, AlmostZeros) {

  static const FloatTest::TestValues& v = this->values_;

  EXPECT_FLOAT_EQ(0.0, v.close_to_positive_zero);
  EXPECT_FLOAT_EQ(-0.0, v.close_to_negative_zero);
  EXPECT_FLOAT_EQ(v.close_to_positive_zero, v.close_to_negative_zero);

  EXPECT_FATAL_FAILURE(
      {
        ASSERT_FLOAT_EQ(v.close_to_positive_zero, v.further_from_negative_zero);
      },
      "v.further_from_negative_zero");
}

TEST_F(FloatTest, SmallDiff) {
  EXPECT_FLOAT_EQ(1.0, values_.close_to_one);
  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(1.0, values_.further_from_one),
                          "values_.further_from_one");
}

TEST_F(FloatTest, LargeDiff) {
  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(2.5, 3.0), "3.0");
}

TEST_F(FloatTest, Infinity) {
  EXPECT_FLOAT_EQ(values_.infinity, values_.infinity);
  EXPECT_FLOAT_EQ(-values_.infinity, -values_.infinity);
  EXPECT_FLOAT_EQ(values_.infinity, values_.close_to_infinity);
  EXPECT_FLOAT_EQ(-values_.infinity, -values_.close_to_infinity);
  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(values_.infinity, -values_.infinity),
                          "-values_.infinity");

  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(values_.infinity, values_.nan1),
                          "values_.nan1");
}

TEST_F(FloatTest, NaN) {

  static const FloatTest::TestValues& v = this->values_;

  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(v.nan1, v.nan1), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(v.nan1, v.nan2), "v.nan2");
  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(1.0, v.nan1), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(1.0f, v.nan1, 1.0f), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(1.0f, v.nan1, v.infinity), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(v.infinity, v.nan1, 1.0f), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(v.infinity, v.nan1, v.infinity),
                          "v.nan1");

  EXPECT_FATAL_FAILURE(ASSERT_FLOAT_EQ(v.nan1, v.infinity), "v.infinity");
}

TEST_F(FloatTest, Reflexive) {
  EXPECT_FLOAT_EQ(0.0, 0.0);
  EXPECT_FLOAT_EQ(1.0, 1.0);
  ASSERT_FLOAT_EQ(values_.infinity, values_.infinity);
}

TEST_F(FloatTest, Commutative) {

  EXPECT_FLOAT_EQ(values_.close_to_one, 1.0);

  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(values_.further_from_one, 1.0),
                          "1.0");
}

TEST_F(FloatTest, EXPECT_NEAR) {
  static const FloatTest::TestValues& v = this->values_;

  EXPECT_NEAR(-1.0f, -1.1f, 0.2f);
  EXPECT_NEAR(2.0f, 3.0f, 1.0f);
  EXPECT_NEAR(v.infinity, v.infinity, 0.0f);
  EXPECT_NEAR(-v.infinity, -v.infinity, 0.0f);
  EXPECT_NEAR(0.0f, 1.0f, v.infinity);
  EXPECT_NEAR(v.infinity, -v.infinity, v.infinity);
  EXPECT_NEAR(-v.infinity, v.infinity, v.infinity);
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(1.0f, 1.5f, 0.25f),
                          "The difference between 1.0f and 1.5f is 0.5, "
                          "which exceeds 0.25f");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(v.infinity, -v.infinity, 0.0f),
                          "The difference between v.infinity and -v.infinity "
                          "is inf, which exceeds 0.0f");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(-v.infinity, v.infinity, 0.0f),
                          "The difference between -v.infinity and v.infinity "
                          "is inf, which exceeds 0.0f");
  EXPECT_NONFATAL_FAILURE(
      EXPECT_NEAR(v.infinity, v.close_to_infinity, v.further_from_infinity),
      "The difference between v.infinity and v.close_to_infinity is inf, which "
      "exceeds v.further_from_infinity");
}

TEST_F(FloatTest, ASSERT_NEAR) {
  ASSERT_NEAR(-1.0f, -1.1f, 0.2f);
  ASSERT_NEAR(2.0f, 3.0f, 1.0f);
  EXPECT_FATAL_FAILURE(ASSERT_NEAR(1.0f, 1.5f, 0.25f),
                       "The difference between 1.0f and 1.5f is 0.5, "
                       "which exceeds 0.25f");
}

TEST_F(FloatTest, FloatLESucceeds) {
  EXPECT_PRED_FORMAT2(FloatLE, 1.0f, 2.0f);
  ASSERT_PRED_FORMAT2(FloatLE, 1.0f, 1.0f);

  EXPECT_PRED_FORMAT2(FloatLE, values_.close_to_positive_zero, 0.0f);
}

TEST_F(FloatTest, FloatLEFails) {

  EXPECT_NONFATAL_FAILURE(EXPECT_PRED_FORMAT2(FloatLE, 2.0f, 1.0f),
                          "(2.0f) <= (1.0f)");

  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED_FORMAT2(FloatLE, values_.further_from_one, 1.0f);
      },
      "(values_.further_from_one) <= (1.0f)");

  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED_FORMAT2(FloatLE, values_.nan1, values_.infinity);
      },
      "(values_.nan1) <= (values_.infinity)");
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED_FORMAT2(FloatLE, -values_.infinity, values_.nan1);
      },
      "(-values_.infinity) <= (values_.nan1)");
  EXPECT_FATAL_FAILURE(
      {
        ASSERT_PRED_FORMAT2(FloatLE, values_.nan1, values_.nan1);
      },
      "(values_.nan1) <= (values_.nan1)");
}

typedef FloatingPointTest<double> DoubleTest;

TEST_F(DoubleTest, Size) { TestSize(); }

TEST_F(DoubleTest, Zeros) {
  EXPECT_DOUBLE_EQ(0.0, -0.0);
  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(-0.0, 1.0), "1.0");
  EXPECT_FATAL_FAILURE(ASSERT_DOUBLE_EQ(0.0, 1.0), "1.0");
}

TEST_F(DoubleTest, AlmostZeros) {

  static const DoubleTest::TestValues& v = this->values_;

  EXPECT_DOUBLE_EQ(0.0, v.close_to_positive_zero);
  EXPECT_DOUBLE_EQ(-0.0, v.close_to_negative_zero);
  EXPECT_DOUBLE_EQ(v.close_to_positive_zero, v.close_to_negative_zero);

  EXPECT_FATAL_FAILURE(
      {
        ASSERT_DOUBLE_EQ(v.close_to_positive_zero,
                         v.further_from_negative_zero);
      },
      "v.further_from_negative_zero");
}

TEST_F(DoubleTest, SmallDiff) {
  EXPECT_DOUBLE_EQ(1.0, values_.close_to_one);
  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(1.0, values_.further_from_one),
                          "values_.further_from_one");
}

TEST_F(DoubleTest, LargeDiff) {
  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(2.0, 3.0), "3.0");
}

TEST_F(DoubleTest, Infinity) {
  EXPECT_DOUBLE_EQ(values_.infinity, values_.infinity);
  EXPECT_DOUBLE_EQ(-values_.infinity, -values_.infinity);
  EXPECT_DOUBLE_EQ(values_.infinity, values_.close_to_infinity);
  EXPECT_DOUBLE_EQ(-values_.infinity, -values_.close_to_infinity);
  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(values_.infinity, -values_.infinity),
                          "-values_.infinity");

  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(values_.infinity, values_.nan1),
                          "values_.nan1");
}

TEST_F(DoubleTest, NaN) {
  static const DoubleTest::TestValues& v = this->values_;

  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(v.nan1, v.nan1), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(v.nan1, v.nan2), "v.nan2");
  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(1.0, v.nan1), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(1.0, v.nan1, 1.0), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(1.0, v.nan1, v.infinity), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(v.infinity, v.nan1, 1.0), "v.nan1");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(v.infinity, v.nan1, v.infinity),
                          "v.nan1");

  EXPECT_FATAL_FAILURE(ASSERT_DOUBLE_EQ(v.nan1, v.infinity), "v.infinity");
}

TEST_F(DoubleTest, Reflexive) {
  EXPECT_DOUBLE_EQ(0.0, 0.0);
  EXPECT_DOUBLE_EQ(1.0, 1.0);
  ASSERT_DOUBLE_EQ(values_.infinity, values_.infinity);
}

TEST_F(DoubleTest, Commutative) {

  EXPECT_DOUBLE_EQ(values_.close_to_one, 1.0);

  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(values_.further_from_one, 1.0),
                          "1.0");
}

TEST_F(DoubleTest, EXPECT_NEAR) {
  static const DoubleTest::TestValues& v = this->values_;

  EXPECT_NEAR(-1.0, -1.1, 0.2);
  EXPECT_NEAR(2.0, 3.0, 1.0);
  EXPECT_NEAR(v.infinity, v.infinity, 0.0);
  EXPECT_NEAR(-v.infinity, -v.infinity, 0.0);
  EXPECT_NEAR(0.0, 1.0, v.infinity);
  EXPECT_NEAR(v.infinity, -v.infinity, v.infinity);
  EXPECT_NEAR(-v.infinity, v.infinity, v.infinity);
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(1.0, 1.5, 0.25),
                          "The difference between 1.0 and 1.5 is 0.5, "
                          "which exceeds 0.25");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(v.infinity, -v.infinity, 0.0),
                          "The difference between v.infinity and -v.infinity "
                          "is inf, which exceeds 0.0");
  EXPECT_NONFATAL_FAILURE(EXPECT_NEAR(-v.infinity, v.infinity, 0.0),
                          "The difference between -v.infinity and v.infinity "
                          "is inf, which exceeds 0.0");
  EXPECT_NONFATAL_FAILURE(
      EXPECT_NEAR(v.infinity, v.close_to_infinity, v.further_from_infinity),
      "The difference between v.infinity and v.close_to_infinity is inf, which "
      "exceeds v.further_from_infinity");

  EXPECT_NONFATAL_FAILURE(
      EXPECT_NEAR(4.2934311416234112e+18, 4.2934311416234107e+18, 1.0),
      "The abs_error parameter 1.0 evaluates to 1 which is smaller than the "
      "minimum distance between doubles for numbers of this magnitude which is "
      "512");
}

TEST_F(DoubleTest, ASSERT_NEAR) {
  ASSERT_NEAR(-1.0, -1.1, 0.2);
  ASSERT_NEAR(2.0, 3.0, 1.0);
  EXPECT_FATAL_FAILURE(ASSERT_NEAR(1.0, 1.5, 0.25),
                       "The difference between 1.0 and 1.5 is 0.5, "
                       "which exceeds 0.25");
}

TEST_F(DoubleTest, DoubleLESucceeds) {
  EXPECT_PRED_FORMAT2(DoubleLE, 1.0, 2.0);
  ASSERT_PRED_FORMAT2(DoubleLE, 1.0, 1.0);

  EXPECT_PRED_FORMAT2(DoubleLE, values_.close_to_positive_zero, 0.0);
}

TEST_F(DoubleTest, DoubleLEFails) {

  EXPECT_NONFATAL_FAILURE(EXPECT_PRED_FORMAT2(DoubleLE, 2.0, 1.0),
                          "(2.0) <= (1.0)");

  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED_FORMAT2(DoubleLE, values_.further_from_one, 1.0);
      },
      "(values_.further_from_one) <= (1.0)");

  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED_FORMAT2(DoubleLE, values_.nan1, values_.infinity);
      },
      "(values_.nan1) <= (values_.infinity)");
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_PRED_FORMAT2(DoubleLE, -values_.infinity, values_.nan1);
      },
      " (-values_.infinity) <= (values_.nan1)");
  EXPECT_FATAL_FAILURE(
      {
        ASSERT_PRED_FORMAT2(DoubleLE, values_.nan1, values_.nan1);
      },
      "(values_.nan1) <= (values_.nan1)");
}

TEST(DisabledTest, DISABLED_TestShouldNotRun) {
  FAIL() << "Unexpected failure: Disabled test should not be run.";
}

TEST(DisabledTest, NotDISABLED_TestShouldRun) { EXPECT_EQ(1, 1); }

TEST(DISABLED_TestSuite, TestShouldNotRun) {
  FAIL() << "Unexpected failure: Test in disabled test case should not be run.";
}

TEST(DISABLED_TestSuite, DISABLED_TestShouldNotRun) {
  FAIL() << "Unexpected failure: Test in disabled test case should not be run.";
}

class DisabledTestsTest : public Test {
 protected:
  static void SetUpTestSuite() {
    FAIL() << "Unexpected failure: All tests disabled in test case. "
              "SetUpTestSuite() should not be called.";
  }

  static void TearDownTestSuite() {
    FAIL() << "Unexpected failure: All tests disabled in test case. "
              "TearDownTestSuite() should not be called.";
  }
};

TEST_F(DisabledTestsTest, DISABLED_TestShouldNotRun_1) {
  FAIL() << "Unexpected failure: Disabled test should not be run.";
}

TEST_F(DisabledTestsTest, DISABLED_TestShouldNotRun_2) {
  FAIL() << "Unexpected failure: Disabled test should not be run.";
}

template <typename T>
class TypedTest : public Test {};

typedef testing::Types<int, double> NumericTypes;
TYPED_TEST_SUITE(TypedTest, NumericTypes);

TYPED_TEST(TypedTest, DISABLED_ShouldNotRun) {
  FAIL() << "Unexpected failure: Disabled typed test should not run.";
}

template <typename T>
class DISABLED_TypedTest : public Test {};

TYPED_TEST_SUITE(DISABLED_TypedTest, NumericTypes);

TYPED_TEST(DISABLED_TypedTest, ShouldNotRun) {
  FAIL() << "Unexpected failure: Disabled typed test should not run.";
}

template <typename T>
class TypedTestP : public Test {};

TYPED_TEST_SUITE_P(TypedTestP);

TYPED_TEST_P(TypedTestP, DISABLED_ShouldNotRun) {
  FAIL() << "Unexpected failure: "
         << "Disabled type-parameterized test should not run.";
}

REGISTER_TYPED_TEST_SUITE_P(TypedTestP, DISABLED_ShouldNotRun);

INSTANTIATE_TYPED_TEST_SUITE_P(My, TypedTestP, NumericTypes);

template <typename T>
class DISABLED_TypedTestP : public Test {};

TYPED_TEST_SUITE_P(DISABLED_TypedTestP);

TYPED_TEST_P(DISABLED_TypedTestP, ShouldNotRun) {
  FAIL() << "Unexpected failure: "
         << "Disabled type-parameterized test should not run.";
}

REGISTER_TYPED_TEST_SUITE_P(DISABLED_TypedTestP, ShouldNotRun);

INSTANTIATE_TYPED_TEST_SUITE_P(My, DISABLED_TypedTestP, NumericTypes);

class SingleEvaluationTest : public Test {
 public:

  static void CompareAndIncrementCharPtrs() { ASSERT_STREQ(p1_++, p2_++); }

  static void CompareAndIncrementInts() { ASSERT_NE(a_++, b_++); }

 protected:
  SingleEvaluationTest() {
    p1_ = s1_;
    p2_ = s2_;
    a_ = 0;
    b_ = 0;
  }

  static const char* const s1_;
  static const char* const s2_;
  static const char* p1_;
  static const char* p2_;

  static int a_;
  static int b_;
};

const char* const SingleEvaluationTest::s1_ = "01234";
const char* const SingleEvaluationTest::s2_ = "abcde";
const char* SingleEvaluationTest::p1_;
const char* SingleEvaluationTest::p2_;
int SingleEvaluationTest::a_;
int SingleEvaluationTest::b_;

TEST_F(SingleEvaluationTest, FailedASSERT_STREQ) {
  EXPECT_FATAL_FAILURE(SingleEvaluationTest::CompareAndIncrementCharPtrs(),
                       "p2_++");
  EXPECT_EQ(s1_ + 1, p1_);
  EXPECT_EQ(s2_ + 1, p2_);
}

TEST_F(SingleEvaluationTest, ASSERT_STR) {

  EXPECT_STRNE(p1_++, p2_++);
  EXPECT_EQ(s1_ + 1, p1_);
  EXPECT_EQ(s2_ + 1, p2_);

  EXPECT_NONFATAL_FAILURE(EXPECT_STRCASEEQ(p1_++, p2_++), "Ignoring case");
  EXPECT_EQ(s1_ + 2, p1_);
  EXPECT_EQ(s2_ + 2, p2_);
}

TEST_F(SingleEvaluationTest, FailedASSERT_NE) {
  EXPECT_FATAL_FAILURE(SingleEvaluationTest::CompareAndIncrementInts(),
                       "(a_++) != (b_++)");
  EXPECT_EQ(1, a_);
  EXPECT_EQ(1, b_);
}

TEST_F(SingleEvaluationTest, OtherCases) {

  EXPECT_TRUE(0 == a_++);
  EXPECT_EQ(1, a_);

  EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(-1 == a_++), "-1 == a_++");
  EXPECT_EQ(2, a_);

  EXPECT_GT(a_++, b_++);
  EXPECT_EQ(3, a_);
  EXPECT_EQ(1, b_);

  EXPECT_NONFATAL_FAILURE(EXPECT_LT(a_++, b_++), "(a_++) < (b_++)");
  EXPECT_EQ(4, a_);
  EXPECT_EQ(2, b_);

  ASSERT_TRUE(0 < a_++);
  EXPECT_EQ(5, a_);

  ASSERT_GT(a_++, b_++);
  EXPECT_EQ(6, a_);
  EXPECT_EQ(3, b_);
}

#if GTEST_HAS_EXCEPTIONS

#if GTEST_HAS_RTTI

#define ERROR_DESC "std::runtime_error"

#else

#define ERROR_DESC "an std::exception-derived error"

#endif

void ThrowAnInteger() { throw 1; }
void ThrowRuntimeError(const char* what) { throw std::runtime_error(what); }

TEST_F(SingleEvaluationTest, ExceptionTests) {

  EXPECT_THROW(
      {
        a_++;
        ThrowAnInteger();
      },
      int);
  EXPECT_EQ(1, a_);

  EXPECT_NONFATAL_FAILURE(EXPECT_THROW(
                              {
                                a_++;
                                ThrowAnInteger();
                              },
                              bool),
                          "throws a different type");
  EXPECT_EQ(2, a_);

  EXPECT_NONFATAL_FAILURE(EXPECT_THROW(
                              {
                                a_++;
                                ThrowRuntimeError("A description");
                              },
                              bool),
                          "throws " ERROR_DESC
                          " with description \"A description\"");
  EXPECT_EQ(3, a_);

  EXPECT_NONFATAL_FAILURE(EXPECT_THROW(a_++, bool), "throws nothing");
  EXPECT_EQ(4, a_);

  EXPECT_NO_THROW(a_++);
  EXPECT_EQ(5, a_);

  EXPECT_NONFATAL_FAILURE(EXPECT_NO_THROW({
                            a_++;
                            ThrowAnInteger();
                          }),
                          "it throws");
  EXPECT_EQ(6, a_);

  EXPECT_ANY_THROW({
    a_++;
    ThrowAnInteger();
  });
  EXPECT_EQ(7, a_);

  EXPECT_NONFATAL_FAILURE(EXPECT_ANY_THROW(a_++), "it doesn't");
  EXPECT_EQ(8, a_);
}

#endif

class NoFatalFailureTest : public Test {
 protected:
  void Succeeds() {}
  void FailsNonFatal() { ADD_FAILURE() << "some non-fatal failure"; }
  void Fails() { FAIL() << "some fatal failure"; }

  void DoAssertNoFatalFailureOnFails() {
    ASSERT_NO_FATAL_FAILURE(Fails());
    ADD_FAILURE() << "should not reach here.";
  }

  void DoExpectNoFatalFailureOnFails() {
    EXPECT_NO_FATAL_FAILURE(Fails());
    ADD_FAILURE() << "other failure";
  }
};

TEST_F(NoFatalFailureTest, NoFailure) {
  EXPECT_NO_FATAL_FAILURE(Succeeds());
  ASSERT_NO_FATAL_FAILURE(Succeeds());
}

TEST_F(NoFatalFailureTest, NonFatalIsNoFailure) {
  EXPECT_NONFATAL_FAILURE(EXPECT_NO_FATAL_FAILURE(FailsNonFatal()),
                          "some non-fatal failure");
  EXPECT_NONFATAL_FAILURE(ASSERT_NO_FATAL_FAILURE(FailsNonFatal()),
                          "some non-fatal failure");
}

TEST_F(NoFatalFailureTest, AssertNoFatalFailureOnFatalFailure) {
  TestPartResultArray gtest_failures;
  {
    ScopedFakeTestPartResultReporter gtest_reporter(&gtest_failures);
    DoAssertNoFatalFailureOnFails();
  }
  ASSERT_EQ(2, gtest_failures.size());
  EXPECT_EQ(TestPartResult::kFatalFailure,
            gtest_failures.GetTestPartResult(0).type());
  EXPECT_EQ(TestPartResult::kFatalFailure,
            gtest_failures.GetTestPartResult(1).type());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "some fatal failure",
                      gtest_failures.GetTestPartResult(0).message());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "it does",
                      gtest_failures.GetTestPartResult(1).message());
}

TEST_F(NoFatalFailureTest, ExpectNoFatalFailureOnFatalFailure) {
  TestPartResultArray gtest_failures;
  {
    ScopedFakeTestPartResultReporter gtest_reporter(&gtest_failures);
    DoExpectNoFatalFailureOnFails();
  }
  ASSERT_EQ(3, gtest_failures.size());
  EXPECT_EQ(TestPartResult::kFatalFailure,
            gtest_failures.GetTestPartResult(0).type());
  EXPECT_EQ(TestPartResult::kNonFatalFailure,
            gtest_failures.GetTestPartResult(1).type());
  EXPECT_EQ(TestPartResult::kNonFatalFailure,
            gtest_failures.GetTestPartResult(2).type());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "some fatal failure",
                      gtest_failures.GetTestPartResult(0).message());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "it does",
                      gtest_failures.GetTestPartResult(1).message());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "other failure",
                      gtest_failures.GetTestPartResult(2).message());
}

TEST_F(NoFatalFailureTest, MessageIsStreamable) {
  TestPartResultArray gtest_failures;
  {
    ScopedFakeTestPartResultReporter gtest_reporter(&gtest_failures);
    EXPECT_NO_FATAL_FAILURE([] { FAIL() << "foo"; }()) << "my message";
  }
  ASSERT_EQ(2, gtest_failures.size());
  EXPECT_EQ(TestPartResult::kFatalFailure,
            gtest_failures.GetTestPartResult(0).type());
  EXPECT_EQ(TestPartResult::kNonFatalFailure,
            gtest_failures.GetTestPartResult(1).type());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "foo",
                      gtest_failures.GetTestPartResult(0).message());
  EXPECT_PRED_FORMAT2(testing::IsSubstring, "my message",
                      gtest_failures.GetTestPartResult(1).message());
}

std::string EditsToString(const std::vector<EditType>& edits) {
  std::string out;
  for (size_t i = 0; i < edits.size(); ++i) {
    static const char kEdits[] = " +-/";
    out.append(1, kEdits[edits[i]]);
  }
  return out;
}

std::vector<size_t> CharsToIndices(const std::string& str) {
  std::vector<size_t> out;
  for (size_t i = 0; i < str.size(); ++i) {
    out.push_back(static_cast<size_t>(str[i]));
  }
  return out;
}

std::vector<std::string> CharsToLines(const std::string& str) {
  std::vector<std::string> out;
  for (size_t i = 0; i < str.size(); ++i) {
    out.push_back(str.substr(i, 1));
  }
  return out;
}

TEST(EditDistance, TestSuites) {
  struct Case {
    int line;
    const char* left;
    const char* right;
    const char* expected_edits;
    const char* expected_diff;
  };
  static const Case kCases[] = {

      {__LINE__, "A", "A", " ", ""},
      {__LINE__, "ABCDE", "ABCDE", "     ", ""},

      {__LINE__, "X", "XA", " +", "@@ +1,2 @@\n X\n+A\n"},
      {__LINE__, "X", "XABCD", " ++++", "@@ +1,5 @@\n X\n+A\n+B\n+C\n+D\n"},

      {__LINE__, "XA", "X", " -", "@@ -1,2 @@\n X\n-A\n"},
      {__LINE__, "XABCD", "X", " ----", "@@ -1,5 @@\n X\n-A\n-B\n-C\n-D\n"},

      {__LINE__, "A", "a", "/", "@@ -1,1 +1,1 @@\n-A\n+a\n"},
      {__LINE__, "ABCD", "abcd", "////",
       "@@ -1,4 +1,4 @@\n-A\n-B\n-C\n-D\n+a\n+b\n+c\n+d\n"},

      {__LINE__, "ABCDEFGH", "ABXEGH1", "  -/ -  +",
       "@@ -1,8 +1,7 @@\n A\n B\n-C\n-D\n+X\n E\n-F\n G\n H\n+1\n"},
      {__LINE__, "AAAABCCCC", "ABABCDCDC", "- /   + / ",
       "@@ -1,9 +1,9 @@\n-A\n A\n-A\n+B\n A\n B\n C\n+D\n C\n-C\n+D\n C\n"},
      {__LINE__, "ABCDE", "BCDCD", "-   +/",
       "@@ -1,5 +1,5 @@\n-A\n B\n C\n D\n-E\n+C\n+D\n"},
      {__LINE__, "ABCDEFGHIJKL", "BCDCDEFGJKLJK", "- ++     --   ++",
       "@@ -1,4 +1,5 @@\n-A\n B\n+C\n+D\n C\n D\n"
       "@@ -6,7 +7,7 @@\n F\n G\n-H\n-I\n J\n K\n L\n+J\n+K\n"},
      {}};
  for (const Case* c = kCases; c->left; ++c) {
    EXPECT_TRUE(c->expected_edits ==
                EditsToString(CalculateOptimalEdits(CharsToIndices(c->left),
                                                    CharsToIndices(c->right))))
        << "Left <" << c->left << "> Right <" << c->right << "> Edits <"
        << EditsToString(CalculateOptimalEdits(CharsToIndices(c->left),
                                               CharsToIndices(c->right)))
        << ">";
    EXPECT_TRUE(c->expected_diff == CreateUnifiedDiff(CharsToLines(c->left),
                                                      CharsToLines(c->right)))
        << "Left <" << c->left << "> Right <" << c->right << "> Diff <"
        << CreateUnifiedDiff(CharsToLines(c->left), CharsToLines(c->right))
        << ">";
  }
}

TEST(AssertionTest, EqFailure) {
  const std::string foo_val("5"), bar_val("6");
  const std::string msg1(
      EqFailure("foo", "bar", foo_val, bar_val, false).failure_message());
  EXPECT_STREQ(
      "Expected equality of these values:\n"
      "  foo\n"
      "    Which is: 5\n"
      "  bar\n"
      "    Which is: 6",
      msg1.c_str());

  const std::string msg2(
      EqFailure("foo", "6", foo_val, bar_val, false).failure_message());
  EXPECT_STREQ(
      "Expected equality of these values:\n"
      "  foo\n"
      "    Which is: 5\n"
      "  6",
      msg2.c_str());

  const std::string msg3(
      EqFailure("5", "bar", foo_val, bar_val, false).failure_message());
  EXPECT_STREQ(
      "Expected equality of these values:\n"
      "  5\n"
      "  bar\n"
      "    Which is: 6",
      msg3.c_str());

  const std::string msg4(
      EqFailure("5", "6", foo_val, bar_val, false).failure_message());
  EXPECT_STREQ(
      "Expected equality of these values:\n"
      "  5\n"
      "  6",
      msg4.c_str());

  const std::string msg5(
      EqFailure("foo", "bar", std::string("\"x\""), std::string("\"y\""), true)
          .failure_message());
  EXPECT_STREQ(
      "Expected equality of these values:\n"
      "  foo\n"
      "    Which is: \"x\"\n"
      "  bar\n"
      "    Which is: \"y\"\n"
      "Ignoring case",
      msg5.c_str());
}

TEST(AssertionTest, EqFailureWithDiff) {
  const std::string left(
      "1\\n2XXX\\n3\\n5\\n6\\n7\\n8\\n9\\n10\\n11\\n12XXX\\n13\\n14\\n15");
  const std::string right(
      "1\\n2\\n3\\n4\\n5\\n6\\n7\\n8\\n9\\n11\\n12\\n13\\n14");
  const std::string msg1(
      EqFailure("left", "right", left, right, false).failure_message());
  EXPECT_STREQ(
      "Expected equality of these values:\n"
      "  left\n"
      "    Which is: "
      "1\\n2XXX\\n3\\n5\\n6\\n7\\n8\\n9\\n10\\n11\\n12XXX\\n13\\n14\\n15\n"
      "  right\n"
      "    Which is: 1\\n2\\n3\\n4\\n5\\n6\\n7\\n8\\n9\\n11\\n12\\n13\\n14\n"
      "With diff:\n@@ -1,5 +1,6 @@\n 1\n-2XXX\n+2\n 3\n+4\n 5\n 6\n"
      "@@ -7,8 +8,6 @@\n 8\n 9\n-10\n 11\n-12XXX\n+12\n 13\n 14\n-15\n",
      msg1.c_str());
}

TEST(AssertionTest, AppendUserMessage) {
  const std::string foo("foo");

  Message msg;
  EXPECT_STREQ("foo", AppendUserMessage(foo, msg).c_str());

  msg << "bar";
  EXPECT_STREQ("foo\nbar", AppendUserMessage(foo, msg).c_str());
}

#ifdef __BORLANDC__

#pragma option push -w-ccc -w-rch
#endif

TEST(AssertionTest, ASSERT_TRUE) {
  ASSERT_TRUE(2 > 1);
  EXPECT_FATAL_FAILURE(ASSERT_TRUE(2 < 1), "2 < 1");
}

TEST(AssertionTest, AssertTrueWithAssertionResult) {
  ASSERT_TRUE(ResultIsEven(2));
#ifndef __BORLANDC__

  EXPECT_FATAL_FAILURE(ASSERT_TRUE(ResultIsEven(3)),
                       "Value of: ResultIsEven(3)\n"
                       "  Actual: false (3 is odd)\n"
                       "Expected: true");
#endif
  ASSERT_TRUE(ResultIsEvenNoExplanation(2));
  EXPECT_FATAL_FAILURE(ASSERT_TRUE(ResultIsEvenNoExplanation(3)),
                       "Value of: ResultIsEvenNoExplanation(3)\n"
                       "  Actual: false (3 is odd)\n"
                       "Expected: true");
}

TEST(AssertionTest, ASSERT_FALSE) {
  ASSERT_FALSE(2 < 1);
  EXPECT_FATAL_FAILURE(ASSERT_FALSE(2 > 1),
                       "Value of: 2 > 1\n"
                       "  Actual: true\n"
                       "Expected: false");
}

TEST(AssertionTest, AssertFalseWithAssertionResult) {
  ASSERT_FALSE(ResultIsEven(3));
#ifndef __BORLANDC__

  EXPECT_FATAL_FAILURE(ASSERT_FALSE(ResultIsEven(2)),
                       "Value of: ResultIsEven(2)\n"
                       "  Actual: true (2 is even)\n"
                       "Expected: false");
#endif
  ASSERT_FALSE(ResultIsEvenNoExplanation(3));
  EXPECT_FATAL_FAILURE(ASSERT_FALSE(ResultIsEvenNoExplanation(2)),
                       "Value of: ResultIsEvenNoExplanation(2)\n"
                       "  Actual: true\n"
                       "Expected: false");
}

#ifdef __BORLANDC__

#pragma option pop
#endif

TEST(ExpectTest, ASSERT_EQ_Double) {

  ASSERT_EQ(5.6, 5.6);

  EXPECT_FATAL_FAILURE(ASSERT_EQ(5.1, 5.2), "5.1");
}

TEST(AssertionTest, ASSERT_EQ) {
  ASSERT_EQ(5, 2 + 3);

  EXPECT_FATAL_FAILURE(ASSERT_EQ(5, 2*3),
                       "Expected equality of these values:\n"
                       "  5\n"
                       "  2*3\n"
                       "    Which is: 6");

}

TEST(AssertionTest, ASSERT_EQ_NULL) {

  const char* p = nullptr;
  ASSERT_EQ(nullptr, p);

  static int n = 0;
  EXPECT_FATAL_FAILURE(ASSERT_EQ(nullptr, &n), "  &n\n    Which is:");
}

TEST(ExpectTest, ASSERT_EQ_0) {
  int n = 0;

  ASSERT_EQ(0, n);

  EXPECT_FATAL_FAILURE(ASSERT_EQ(0, 5.6), "  0\n  5.6");
}

TEST(AssertionTest, ASSERT_NE) {
  ASSERT_NE(6, 7);
  EXPECT_FATAL_FAILURE(ASSERT_NE('a', 'a'),
                       "Expected: ('a') != ('a'), "
                       "actual: 'a' (97, 0x61) vs 'a' (97, 0x61)");
}

TEST(AssertionTest, ASSERT_LE) {
  ASSERT_LE(2, 3);
  ASSERT_LE(2, 2);
  EXPECT_FATAL_FAILURE(ASSERT_LE(2, 0), "Expected: (2) <= (0), actual: 2 vs 0");
}

TEST(AssertionTest, ASSERT_LT) {
  ASSERT_LT(2, 3);
  EXPECT_FATAL_FAILURE(ASSERT_LT(2, 2), "Expected: (2) < (2), actual: 2 vs 2");
}

TEST(AssertionTest, ASSERT_GE) {
  ASSERT_GE(2, 1);
  ASSERT_GE(2, 2);
  EXPECT_FATAL_FAILURE(ASSERT_GE(2, 3), "Expected: (2) >= (3), actual: 2 vs 3");
}

TEST(AssertionTest, ASSERT_GT) {
  ASSERT_GT(2, 1);
  EXPECT_FATAL_FAILURE(ASSERT_GT(2, 2), "Expected: (2) > (2), actual: 2 vs 2");
}

#if GTEST_HAS_EXCEPTIONS

void ThrowNothing() {}

TEST(AssertionTest, ASSERT_THROW) {
  ASSERT_THROW(ThrowAnInteger(), int);

#ifndef __BORLANDC__

  EXPECT_FATAL_FAILURE(
      ASSERT_THROW(ThrowAnInteger(), bool),
      "Expected: ThrowAnInteger() throws an exception of type bool.\n"
      "  Actual: it throws a different type.");
  EXPECT_FATAL_FAILURE(
      ASSERT_THROW(ThrowRuntimeError("A description"), std::logic_error),
      "Expected: ThrowRuntimeError(\"A description\") "
      "throws an exception of type std::logic_error.\n  "
      "Actual: it throws " ERROR_DESC
      " "
      "with description \"A description\".");
#endif

  EXPECT_FATAL_FAILURE(
      ASSERT_THROW(ThrowNothing(), bool),
      "Expected: ThrowNothing() throws an exception of type bool.\n"
      "  Actual: it throws nothing.");
}

TEST(AssertionTest, ASSERT_NO_THROW) {
  ASSERT_NO_THROW(ThrowNothing());
  EXPECT_FATAL_FAILURE(ASSERT_NO_THROW(ThrowAnInteger()),
                       "Expected: ThrowAnInteger() doesn't throw an exception."
                       "\n  Actual: it throws.");
  EXPECT_FATAL_FAILURE(ASSERT_NO_THROW(ThrowRuntimeError("A description")),
                       "Expected: ThrowRuntimeError(\"A description\") "
                       "doesn't throw an exception.\n  "
                       "Actual: it throws " ERROR_DESC
                       " "
                       "with description \"A description\".");
}

TEST(AssertionTest, ASSERT_ANY_THROW) {
  ASSERT_ANY_THROW(ThrowAnInteger());
  EXPECT_FATAL_FAILURE(ASSERT_ANY_THROW(ThrowNothing()),
                       "Expected: ThrowNothing() throws an exception.\n"
                       "  Actual: it doesn't.");
}

#endif

TEST(AssertionTest, AssertPrecedence) {
  ASSERT_EQ(1 < 2, true);
  bool false_value = false;
  ASSERT_EQ(true && false_value, false);
}

void TestEq1(int x) { ASSERT_EQ(1, x); }

TEST(AssertionTest, NonFixtureSubroutine) {
  EXPECT_FATAL_FAILURE(TestEq1(2), "  x\n    Which is: 2");
}

class Uncopyable {
 public:
  explicit Uncopyable(int a_value) : value_(a_value) {}

  int value() const { return value_; }
  bool operator==(const Uncopyable& rhs) const {
    return value() == rhs.value();
  }

 private:

  Uncopyable(const Uncopyable&);

  int value_;
};

::std::ostream& operator<<(::std::ostream& os, const Uncopyable& value) {
  return os << value.value();
}

bool IsPositiveUncopyable(const Uncopyable& x) { return x.value() > 0; }

void TestAssertNonPositive() {
  Uncopyable y(-1);
  ASSERT_PRED1(IsPositiveUncopyable, y);
}

void TestAssertEqualsUncopyable() {
  Uncopyable x(5);
  Uncopyable y(-1);
  ASSERT_EQ(x, y);
}

TEST(AssertionTest, AssertWorksWithUncopyableObject) {
  Uncopyable x(5);
  ASSERT_PRED1(IsPositiveUncopyable, x);
  ASSERT_EQ(x, x);
  EXPECT_FATAL_FAILURE(
      TestAssertNonPositive(),
      "IsPositiveUncopyable(y) evaluates to false, where\ny evaluates to -1");
  EXPECT_FATAL_FAILURE(TestAssertEqualsUncopyable(),
                       "Expected equality of these values:\n"
                       "  x\n    Which is: 5\n  y\n    Which is: -1");
}

TEST(AssertionTest, ExpectWorksWithUncopyableObject) {
  Uncopyable x(5);
  EXPECT_PRED1(IsPositiveUncopyable, x);
  Uncopyable y(-1);
  EXPECT_NONFATAL_FAILURE(
      EXPECT_PRED1(IsPositiveUncopyable, y),
      "IsPositiveUncopyable(y) evaluates to false, where\ny evaluates to -1");
  EXPECT_EQ(x, x);
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(x, y),
                          "Expected equality of these values:\n"
                          "  x\n    Which is: 5\n  y\n    Which is: -1");
}

enum NamedEnum { kE1 = 0, kE2 = 1 };

TEST(AssertionTest, NamedEnum) {
  EXPECT_EQ(kE1, kE1);
  EXPECT_LT(kE1, kE2);
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(kE1, kE2), "Which is: 0");
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(kE1, kE2), "Which is: 1");
}

#if !defined(__SUNPRO_CC) && !defined(__HP_aCC)

enum {
  kCaseA = -1,

#ifdef GTEST_OS_LINUX

  kCaseB = testing::internal::kMaxBiggestInt,

#else

  kCaseB = INT_MAX,

#endif

  kCaseC = 42
};

TEST(AssertionTest, AnonymousEnum) {
#ifdef GTEST_OS_LINUX

  EXPECT_EQ(static_cast<int>(kCaseA), static_cast<int>(kCaseB));

#endif

  EXPECT_EQ(kCaseA, kCaseA);
  EXPECT_NE(kCaseA, kCaseB);
  EXPECT_LT(kCaseA, kCaseB);
  EXPECT_LE(kCaseA, kCaseB);
  EXPECT_GT(kCaseB, kCaseA);
  EXPECT_GE(kCaseA, kCaseA);
  EXPECT_NONFATAL_FAILURE(EXPECT_GE(kCaseA, kCaseB), "(kCaseA) >= (kCaseB)");
  EXPECT_NONFATAL_FAILURE(EXPECT_GE(kCaseA, kCaseC), "-1 vs 42");

  ASSERT_EQ(kCaseA, kCaseA);
  ASSERT_NE(kCaseA, kCaseB);
  ASSERT_LT(kCaseA, kCaseB);
  ASSERT_LE(kCaseA, kCaseB);
  ASSERT_GT(kCaseB, kCaseA);
  ASSERT_GE(kCaseA, kCaseA);

#ifndef __BORLANDC__

  EXPECT_FATAL_FAILURE(ASSERT_EQ(kCaseA, kCaseB), "  kCaseB\n    Which is: ");
  EXPECT_FATAL_FAILURE(ASSERT_EQ(kCaseA, kCaseC), "\n    Which is: 42");
#endif

  EXPECT_FATAL_FAILURE(ASSERT_EQ(kCaseA, kCaseC), "\n    Which is: -1");
}

#endif

#ifdef GTEST_OS_WINDOWS

static HRESULT UnexpectedHRESULTFailure() { return E_UNEXPECTED; }

static HRESULT OkHRESULTSuccess() { return S_OK; }

static HRESULT FalseHRESULTSuccess() { return S_FALSE; }

TEST(HRESULTAssertionTest, EXPECT_HRESULT_SUCCEEDED) {
  EXPECT_HRESULT_SUCCEEDED(S_OK);
  EXPECT_HRESULT_SUCCEEDED(S_FALSE);

  EXPECT_NONFATAL_FAILURE(EXPECT_HRESULT_SUCCEEDED(UnexpectedHRESULTFailure()),
                          "Expected: (UnexpectedHRESULTFailure()) succeeds.\n"
                          "  Actual: 0x8000FFFF");
}

TEST(HRESULTAssertionTest, ASSERT_HRESULT_SUCCEEDED) {
  ASSERT_HRESULT_SUCCEEDED(S_OK);
  ASSERT_HRESULT_SUCCEEDED(S_FALSE);

  EXPECT_FATAL_FAILURE(ASSERT_HRESULT_SUCCEEDED(UnexpectedHRESULTFailure()),
                       "Expected: (UnexpectedHRESULTFailure()) succeeds.\n"
                       "  Actual: 0x8000FFFF");
}

TEST(HRESULTAssertionTest, EXPECT_HRESULT_FAILED) {
  EXPECT_HRESULT_FAILED(E_UNEXPECTED);

  EXPECT_NONFATAL_FAILURE(EXPECT_HRESULT_FAILED(OkHRESULTSuccess()),
                          "Expected: (OkHRESULTSuccess()) fails.\n"
                          "  Actual: 0x0");
  EXPECT_NONFATAL_FAILURE(EXPECT_HRESULT_FAILED(FalseHRESULTSuccess()),
                          "Expected: (FalseHRESULTSuccess()) fails.\n"
                          "  Actual: 0x1");
}

TEST(HRESULTAssertionTest, ASSERT_HRESULT_FAILED) {
  ASSERT_HRESULT_FAILED(E_UNEXPECTED);

#ifndef __BORLANDC__

  EXPECT_FATAL_FAILURE(ASSERT_HRESULT_FAILED(OkHRESULTSuccess()),
                       "Expected: (OkHRESULTSuccess()) fails.\n"
                       "  Actual: 0x0");
#endif

  EXPECT_FATAL_FAILURE(ASSERT_HRESULT_FAILED(FalseHRESULTSuccess()),
                       "Expected: (FalseHRESULTSuccess()) fails.\n"
                       "  Actual: 0x1");
}

TEST(HRESULTAssertionTest, Streaming) {
  EXPECT_HRESULT_SUCCEEDED(S_OK) << "unexpected failure";
  ASSERT_HRESULT_SUCCEEDED(S_OK) << "unexpected failure";
  EXPECT_HRESULT_FAILED(E_UNEXPECTED) << "unexpected failure";
  ASSERT_HRESULT_FAILED(E_UNEXPECTED) << "unexpected failure";

  EXPECT_NONFATAL_FAILURE(EXPECT_HRESULT_SUCCEEDED(E_UNEXPECTED)
                              << "expected failure",
                          "expected failure");

#ifndef __BORLANDC__

  EXPECT_FATAL_FAILURE(ASSERT_HRESULT_SUCCEEDED(E_UNEXPECTED)
                           << "expected failure",
                       "expected failure");
#endif

  EXPECT_NONFATAL_FAILURE(EXPECT_HRESULT_FAILED(S_OK) << "expected failure",
                          "expected failure");

  EXPECT_FATAL_FAILURE(ASSERT_HRESULT_FAILED(S_OK) << "expected failure",
                       "expected failure");
}

#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-else"
#pragma GCC diagnostic ignored "-Wempty-body"
#pragma GCC diagnostic ignored "-Wpragmas"
#endif

TEST(AssertionSyntaxTest, BasicAssertionsBehavesLikeSingleStatement) {
  if (AlwaysFalse())
    ASSERT_TRUE(false) << "This should never be executed; "
                          "It's a compilation test only.";

  if (AlwaysTrue())
    EXPECT_FALSE(false);
  else
    ;

  if (AlwaysFalse()) ASSERT_LT(1, 3);

  if (AlwaysFalse())
    ;
  else
    EXPECT_GT(3, 2) << "";
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#if GTEST_HAS_EXCEPTIONS

TEST(ExpectThrowTest, DoesNotGenerateUnreachableCodeWarning) {
  int n = 0;

  EXPECT_THROW(throw 1, int);
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW(n++, int), "");
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW(throw n, const char*), "");
  EXPECT_NO_THROW(n++);
  EXPECT_NONFATAL_FAILURE(EXPECT_NO_THROW(throw 1), "");
  EXPECT_ANY_THROW(throw 1);
  EXPECT_NONFATAL_FAILURE(EXPECT_ANY_THROW(n++), "");
}

TEST(ExpectThrowTest, DoesNotGenerateDuplicateCatchClauseWarning) {
  EXPECT_THROW(throw std::exception(), std::exception);
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-else"
#pragma GCC diagnostic ignored "-Wempty-body"
#pragma GCC diagnostic ignored "-Wpragmas"
#endif
TEST(AssertionSyntaxTest, ExceptionAssertionsBehavesLikeSingleStatement) {
  if (AlwaysFalse()) EXPECT_THROW(ThrowNothing(), bool);

  if (AlwaysTrue())
    EXPECT_THROW(ThrowAnInteger(), int);
  else
    ;

  if (AlwaysFalse()) EXPECT_NO_THROW(ThrowAnInteger());

  if (AlwaysTrue())
    EXPECT_NO_THROW(ThrowNothing());
  else
    ;

  if (AlwaysFalse()) EXPECT_ANY_THROW(ThrowNothing());

  if (AlwaysTrue())
    EXPECT_ANY_THROW(ThrowAnInteger());
  else
    ;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-else"
#pragma GCC diagnostic ignored "-Wempty-body"
#pragma GCC diagnostic ignored "-Wpragmas"
#endif
TEST(AssertionSyntaxTest, NoFatalFailureAssertionsBehavesLikeSingleStatement) {
  if (AlwaysFalse())
    EXPECT_NO_FATAL_FAILURE(FAIL())
        << "This should never be executed. " << "It's a compilation test only.";
  else
    ;

  if (AlwaysFalse())
    ASSERT_NO_FATAL_FAILURE(FAIL()) << "";
  else
    ;

  if (AlwaysTrue())
    EXPECT_NO_FATAL_FAILURE(SUCCEED());
  else
    ;

  if (AlwaysFalse())
    ;
  else
    ASSERT_NO_FATAL_FAILURE(SUCCEED());
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

TEST(AssertionSyntaxTest, WorksWithSwitch) {
  switch (0) {
    case 1:
      break;
    default:
      ASSERT_TRUE(true);
  }

  switch (0)
  case 0:
    EXPECT_FALSE(false) << "EXPECT_FALSE failed in switch case";

  switch (0) {
    case 1:
    default:
      ASSERT_EQ(1, 1) << "ASSERT_EQ failed in default switch handler";
  }

  switch (0)
  case 0:
    EXPECT_NE(1, 2);
}

#if GTEST_HAS_EXCEPTIONS

void ThrowAString() { throw "std::string"; }

TEST(AssertionSyntaxTest, WorksWithConst) {
  ASSERT_THROW(ThrowAString(), const char*);

  EXPECT_THROW(ThrowAString(), const char*);
}

#endif

}

namespace testing {

TEST(SuccessfulAssertionTest, SUCCEED) {
  SUCCEED();
  SUCCEED() << "OK";
  EXPECT_EQ(2, GetUnitTestImpl()->current_test_result()->total_part_count());
}

TEST(SuccessfulAssertionTest, EXPECT) {
  EXPECT_TRUE(true);
  EXPECT_EQ(0, GetUnitTestImpl()->current_test_result()->total_part_count());
}

TEST(SuccessfulAssertionTest, EXPECT_STR) {
  EXPECT_STREQ("", "");
  EXPECT_EQ(0, GetUnitTestImpl()->current_test_result()->total_part_count());
}

TEST(SuccessfulAssertionTest, ASSERT) {
  ASSERT_TRUE(true);
  EXPECT_EQ(0, GetUnitTestImpl()->current_test_result()->total_part_count());
}

TEST(SuccessfulAssertionTest, ASSERT_STR) {
  ASSERT_STREQ("", "");
  EXPECT_EQ(0, GetUnitTestImpl()->current_test_result()->total_part_count());
}

}

namespace {

TEST(AssertionWithMessageTest, EXPECT) {
  EXPECT_EQ(1, 1) << "This should succeed.";
  EXPECT_NONFATAL_FAILURE(EXPECT_NE(1, 1) << "Expected failure #1.",
                          "Expected failure #1");
  EXPECT_LE(1, 2) << "This should succeed.";
  EXPECT_NONFATAL_FAILURE(EXPECT_LT(1, 0) << "Expected failure #2.",
                          "Expected failure #2.");
  EXPECT_GE(1, 0) << "This should succeed.";
  EXPECT_NONFATAL_FAILURE(EXPECT_GT(1, 2) << "Expected failure #3.",
                          "Expected failure #3.");

  EXPECT_STREQ("1", "1") << "This should succeed.";
  EXPECT_NONFATAL_FAILURE(EXPECT_STRNE("1", "1") << "Expected failure #4.",
                          "Expected failure #4.");
  EXPECT_STRCASEEQ("a", "A") << "This should succeed.";
  EXPECT_NONFATAL_FAILURE(EXPECT_STRCASENE("a", "A") << "Expected failure #5.",
                          "Expected failure #5.");

  EXPECT_FLOAT_EQ(1, 1) << "This should succeed.";
  EXPECT_NONFATAL_FAILURE(EXPECT_DOUBLE_EQ(1, 1.2) << "Expected failure #6.",
                          "Expected failure #6.");
  EXPECT_NEAR(1, 1.1, 0.2) << "This should succeed.";
}

TEST(AssertionWithMessageTest, ASSERT) {
  ASSERT_EQ(1, 1) << "This should succeed.";
  ASSERT_NE(1, 2) << "This should succeed.";
  ASSERT_LE(1, 2) << "This should succeed.";
  ASSERT_LT(1, 2) << "This should succeed.";
  ASSERT_GE(1, 0) << "This should succeed.";
  EXPECT_FATAL_FAILURE(ASSERT_GT(1, 2) << "Expected failure.",
                       "Expected failure.");
}

TEST(AssertionWithMessageTest, ASSERT_STR) {
  ASSERT_STREQ("1", "1") << "This should succeed.";
  ASSERT_STRNE("1", "2") << "This should succeed.";
  ASSERT_STRCASEEQ("a", "A") << "This should succeed.";
  EXPECT_FATAL_FAILURE(ASSERT_STRCASENE("a", "A") << "Expected failure.",
                       "Expected failure.");
}

TEST(AssertionWithMessageTest, ASSERT_FLOATING) {
  ASSERT_FLOAT_EQ(1, 1) << "This should succeed.";
  ASSERT_DOUBLE_EQ(1, 1) << "This should succeed.";
  EXPECT_FATAL_FAILURE(ASSERT_NEAR(1, 1.2, 0.1) << "Expect failure.",
                       "Expect failure.");
}

TEST(AssertionWithMessageTest, ASSERT_FALSE) {
  ASSERT_FALSE(false) << "This shouldn't fail.";
  EXPECT_FATAL_FAILURE(
      {
        ASSERT_FALSE(true) << "Expected failure: " << 2 << " > " << 1
                           << " evaluates to " << true;
      },
      "Expected failure");
}

TEST(AssertionWithMessageTest, FAIL) { EXPECT_FATAL_FAILURE(FAIL() << 0, "0"); }

TEST(AssertionWithMessageTest, SUCCEED) { SUCCEED() << "Success == " << 1; }

TEST(AssertionWithMessageTest, ASSERT_TRUE) {
  ASSERT_TRUE(true) << "This should succeed.";
  ASSERT_TRUE(true) << true;
  EXPECT_FATAL_FAILURE(
      {
        ASSERT_TRUE(false) << static_cast<const char*>(nullptr)
                           << static_cast<char*>(nullptr);
      },
      "(null)(null)");
}

#ifdef GTEST_OS_WINDOWS

TEST(AssertionWithMessageTest, WideStringMessage) {
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_TRUE(false) << L"This failure is expected.\x8119";
      },
      "This failure is expected.");
  EXPECT_FATAL_FAILURE(
      {
        ASSERT_EQ(1, 2) << "This failure is " << L"expected too.\x8120";
      },
      "This failure is expected too.");
}
#endif

TEST(ExpectTest, EXPECT_TRUE) {
  EXPECT_TRUE(true) << "Intentional success";
  EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(false) << "Intentional failure #1.",
                          "Intentional failure #1.");
  EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(false) << "Intentional failure #2.",
                          "Intentional failure #2.");
  EXPECT_TRUE(2 > 1);
  EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(2 < 1),
                          "Value of: 2 < 1\n"
                          "  Actual: false\n"
                          "Expected: true");
  EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(2 > 3), "2 > 3");
}

TEST(ExpectTest, ExpectTrueWithAssertionResult) {
  EXPECT_TRUE(ResultIsEven(2));
  EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(ResultIsEven(3)),
                          "Value of: ResultIsEven(3)\n"
                          "  Actual: false (3 is odd)\n"
                          "Expected: true");
  EXPECT_TRUE(ResultIsEvenNoExplanation(2));
  EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(ResultIsEvenNoExplanation(3)),
                          "Value of: ResultIsEvenNoExplanation(3)\n"
                          "  Actual: false (3 is odd)\n"
                          "Expected: true");
}

TEST(ExpectTest, EXPECT_FALSE) {
  EXPECT_FALSE(2 < 1);
  EXPECT_FALSE(false) << "Intentional success";
  EXPECT_NONFATAL_FAILURE(EXPECT_FALSE(true) << "Intentional failure #1.",
                          "Intentional failure #1.");
  EXPECT_NONFATAL_FAILURE(EXPECT_FALSE(true) << "Intentional failure #2.",
                          "Intentional failure #2.");
  EXPECT_NONFATAL_FAILURE(EXPECT_FALSE(2 > 1),
                          "Value of: 2 > 1\n"
                          "  Actual: true\n"
                          "Expected: false");
  EXPECT_NONFATAL_FAILURE(EXPECT_FALSE(2 < 3), "2 < 3");
}

TEST(ExpectTest, ExpectFalseWithAssertionResult) {
  EXPECT_FALSE(ResultIsEven(3));
  EXPECT_NONFATAL_FAILURE(EXPECT_FALSE(ResultIsEven(2)),
                          "Value of: ResultIsEven(2)\n"
                          "  Actual: true (2 is even)\n"
                          "Expected: false");
  EXPECT_FALSE(ResultIsEvenNoExplanation(3));
  EXPECT_NONFATAL_FAILURE(EXPECT_FALSE(ResultIsEvenNoExplanation(2)),
                          "Value of: ResultIsEvenNoExplanation(2)\n"
                          "  Actual: true\n"
                          "Expected: false");
}

#ifdef __BORLANDC__

#pragma option pop
#endif

TEST(ExpectTest, EXPECT_EQ) {
  EXPECT_EQ(5, 2 + 3);

  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(5, 2*3),
                          "Expected equality of these values:\n"
                          "  5\n"
                          "  2*3\n"
                          "    Which is: 6");
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(5, 2 - 3), "2 - 3");

}

TEST(ExpectTest, EXPECT_EQ_Double) {

  EXPECT_EQ(5.6, 5.6);

  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(5.1, 5.2), "5.1");
}

TEST(ExpectTest, EXPECT_EQ_NULL) {

  const char* p = nullptr;
  EXPECT_EQ(nullptr, p);

  int n = 0;
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(nullptr, &n), "  &n\n    Which is:");
}

TEST(ExpectTest, EXPECT_EQ_0) {
  int n = 0;

  EXPECT_EQ(0, n);

  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(0, 5.6), "  0\n  5.6");
}

TEST(ExpectTest, EXPECT_NE) {
  EXPECT_NE(6, 7);

  EXPECT_NONFATAL_FAILURE(EXPECT_NE('a', 'a'),
                          "Expected: ('a') != ('a'), "
                          "actual: 'a' (97, 0x61) vs 'a' (97, 0x61)");
  EXPECT_NONFATAL_FAILURE(EXPECT_NE(2, 2), "2");
  char* const p0 = nullptr;
  EXPECT_NONFATAL_FAILURE(EXPECT_NE(p0, p0), "p0");

  void* pv1 = (void*)0x1234;
  char* const p1 = reinterpret_cast<char*>(pv1);
  EXPECT_NONFATAL_FAILURE(EXPECT_NE(p1, p1), "p1");
}

TEST(ExpectTest, EXPECT_LE) {
  EXPECT_LE(2, 3);
  EXPECT_LE(2, 2);
  EXPECT_NONFATAL_FAILURE(EXPECT_LE(2, 0),
                          "Expected: (2) <= (0), actual: 2 vs 0");
  EXPECT_NONFATAL_FAILURE(EXPECT_LE(1.1, 0.9), "(1.1) <= (0.9)");
}

TEST(ExpectTest, EXPECT_LT) {
  EXPECT_LT(2, 3);
  EXPECT_NONFATAL_FAILURE(EXPECT_LT(2, 2),
                          "Expected: (2) < (2), actual: 2 vs 2");
  EXPECT_NONFATAL_FAILURE(EXPECT_LT(2, 1), "(2) < (1)");
}

TEST(ExpectTest, EXPECT_GE) {
  EXPECT_GE(2, 1);
  EXPECT_GE(2, 2);
  EXPECT_NONFATAL_FAILURE(EXPECT_GE(2, 3),
                          "Expected: (2) >= (3), actual: 2 vs 3");
  EXPECT_NONFATAL_FAILURE(EXPECT_GE(0.9, 1.1), "(0.9) >= (1.1)");
}

TEST(ExpectTest, EXPECT_GT) {
  EXPECT_GT(2, 1);
  EXPECT_NONFATAL_FAILURE(EXPECT_GT(2, 2),
                          "Expected: (2) > (2), actual: 2 vs 2");
  EXPECT_NONFATAL_FAILURE(EXPECT_GT(2, 3), "(2) > (3)");
}

#if GTEST_HAS_EXCEPTIONS

TEST(ExpectTest, EXPECT_THROW) {
  EXPECT_THROW(ThrowAnInteger(), int);
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW(ThrowAnInteger(), bool),
                          "Expected: ThrowAnInteger() throws an exception of "
                          "type bool.\n  Actual: it throws a different type.");
  EXPECT_NONFATAL_FAILURE(
      EXPECT_THROW(ThrowRuntimeError("A description"), std::logic_error),
      "Expected: ThrowRuntimeError(\"A description\") "
      "throws an exception of type std::logic_error.\n  "
      "Actual: it throws " ERROR_DESC
      " "
      "with description \"A description\".");
  EXPECT_NONFATAL_FAILURE(
      EXPECT_THROW(ThrowNothing(), bool),
      "Expected: ThrowNothing() throws an exception of type bool.\n"
      "  Actual: it throws nothing.");
}

TEST(ExpectTest, EXPECT_NO_THROW) {
  EXPECT_NO_THROW(ThrowNothing());
  EXPECT_NONFATAL_FAILURE(EXPECT_NO_THROW(ThrowAnInteger()),
                          "Expected: ThrowAnInteger() doesn't throw an "
                          "exception.\n  Actual: it throws.");
  EXPECT_NONFATAL_FAILURE(EXPECT_NO_THROW(ThrowRuntimeError("A description")),
                          "Expected: ThrowRuntimeError(\"A description\") "
                          "doesn't throw an exception.\n  "
                          "Actual: it throws " ERROR_DESC
                          " "
                          "with description \"A description\".");
}

TEST(ExpectTest, EXPECT_ANY_THROW) {
  EXPECT_ANY_THROW(ThrowAnInteger());
  EXPECT_NONFATAL_FAILURE(EXPECT_ANY_THROW(ThrowNothing()),
                          "Expected: ThrowNothing() throws an exception.\n"
                          "  Actual: it doesn't.");
}

#endif

TEST(ExpectTest, ExpectPrecedence) {
  EXPECT_EQ(1 < 2, true);
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(true, true && false),
                          "  true && false\n    Which is: false");
}

TEST(StreamableToStringTest, Scalar) {
  EXPECT_STREQ("5", StreamableToString(5).c_str());
}

TEST(StreamableToStringTest, Pointer) {
  int n = 0;
  int* p = &n;
  EXPECT_STRNE("(null)", StreamableToString(p).c_str());
}

TEST(StreamableToStringTest, NullPointer) {
  int* p = nullptr;
  EXPECT_STREQ("(null)", StreamableToString(p).c_str());
}

TEST(StreamableToStringTest, CString) {
  EXPECT_STREQ("Foo", StreamableToString("Foo").c_str());
}

TEST(StreamableToStringTest, NullCString) {
  char* p = nullptr;
  EXPECT_STREQ("(null)", StreamableToString(p).c_str());
}

TEST(StreamableTest, string) {
  static const std::string str(
      "This failure message is a std::string, and is expected.");
  EXPECT_FATAL_FAILURE(FAIL() << str, str.c_str());
}

TEST(StreamableTest, stringWithEmbeddedNUL) {
  static const char char_array_with_nul[] =
      "Here's a NUL\0 and some more string";
  static const std::string string_with_nul(
      char_array_with_nul,
      sizeof(char_array_with_nul) - 1);
  EXPECT_FATAL_FAILURE(FAIL() << string_with_nul,
                       "Here's a NUL\\0 and some more string");
}

TEST(StreamableTest, NULChar) {
  EXPECT_FATAL_FAILURE(
      {
        FAIL() << "A NUL" << '\0' << " and some more string";
      },
      "A NUL\\0 and some more string");
}

TEST(StreamableTest, int) { EXPECT_FATAL_FAILURE(FAIL() << 900913, "900913"); }

TEST(StreamableTest, NullCharPtr) {
  EXPECT_FATAL_FAILURE(FAIL() << static_cast<const char*>(nullptr), "(null)");
}

TEST(StreamableTest, BasicIoManip) {
  EXPECT_FATAL_FAILURE(
      {
        FAIL() << "Line 1." << std::endl
               << "A NUL char " << std::ends << std::flush << " in line 2.";
      },
      "Line 1.\nA NUL char \\0 in line 2.");
}

void AddFailureHelper(bool* aborted) {
  *aborted = true;
  ADD_FAILURE() << "Intentional failure.";
  *aborted = false;
}

TEST(MacroTest, ADD_FAILURE) {
  bool aborted = true;
  EXPECT_NONFATAL_FAILURE(AddFailureHelper(&aborted), "Intentional failure.");
  EXPECT_FALSE(aborted);
}

TEST(MacroTest, ADD_FAILURE_AT) {

  EXPECT_NONFATAL_FAILURE(ADD_FAILURE_AT("foo.cc", 42) << "Wrong!", "Wrong!");

  EXPECT_NONFATAL_FAILURE(ADD_FAILURE_AT("foo.cc", 42), "Failed");

}

TEST(MacroTest, FAIL) {
  EXPECT_FATAL_FAILURE(FAIL(), "Failed");
  EXPECT_FATAL_FAILURE(FAIL() << "Intentional failure.",
                       "Intentional failure.");
}

TEST(MacroTest, GTEST_FAIL_AT) {

  EXPECT_FATAL_FAILURE(GTEST_FAIL_AT("foo.cc", 42) << "Wrong!", "Wrong!");

  EXPECT_FATAL_FAILURE(GTEST_FAIL_AT("foo.cc", 42), "Failed");

}

TEST(MacroTest, SUCCEED) {
  SUCCEED();
  SUCCEED() << "Explicit success.";
}

TEST(EqAssertionTest, Bool) {
  EXPECT_EQ(true, true);
  EXPECT_FATAL_FAILURE(
      {
        bool false_value = false;
        ASSERT_EQ(false_value, true);
      },
      "  false_value\n    Which is: false\n  true");
}

TEST(EqAssertionTest, Int) {
  ASSERT_EQ(32, 32);
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(32, 33), "  32\n  33");
}

TEST(EqAssertionTest, Time_T) {
  EXPECT_EQ(static_cast<time_t>(0), static_cast<time_t>(0));
  EXPECT_FATAL_FAILURE(
      ASSERT_EQ(static_cast<time_t>(0), static_cast<time_t>(1234)), "1234");
}

TEST(EqAssertionTest, Char) {
  ASSERT_EQ('z', 'z');
  const char ch = 'b';
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ('\0', ch), "  ch\n    Which is: 'b'");
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ('a', ch), "  ch\n    Which is: 'b'");
}

TEST(EqAssertionTest, WideChar) {
  EXPECT_EQ(L'b', L'b');

  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(L'\0', L'x'),
                          "Expected equality of these values:\n"
                          "  L'\0'\n"
                          "    Which is: L'\0' (0, 0x0)\n"
                          "  L'x'\n"
                          "    Which is: L'x' (120, 0x78)");

  static wchar_t wchar;
  wchar = L'b';
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(L'a', wchar), "wchar");
  wchar = 0x8119;
  EXPECT_FATAL_FAILURE(ASSERT_EQ(static_cast<wchar_t>(0x8120), wchar),
                       "  wchar\n    Which is: L'");
}

TEST(EqAssertionTest, StdString) {

  ASSERT_EQ("Test", ::std::string("Test"));

  static const ::std::string str1("A * in the middle");
  static const ::std::string str2(str1);
  EXPECT_EQ(str1, str2);

  EXPECT_NONFATAL_FAILURE(EXPECT_EQ("Test", ::std::string("test")), "\"test\"");

  char* const p1 = const_cast<char*>("foo");
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(::std::string("bar"), p1), "p1");

  static ::std::string str3(str1);
  str3.at(2) = '\0';
  EXPECT_FATAL_FAILURE(ASSERT_EQ(str1, str3),
                       "  str3\n    Which is: \"A \\0 in the middle\"");
}

#if GTEST_HAS_STD_WSTRING

TEST(EqAssertionTest, StdWideString) {

  const ::std::wstring wstr1(L"A * in the middle");
  const ::std::wstring wstr2(wstr1);
  ASSERT_EQ(wstr1, wstr2);

  const wchar_t kTestX8119[] = {'T', 'e', 's', 't', 0x8119, '\0'};
  EXPECT_EQ(::std::wstring(kTestX8119), kTestX8119);

  const wchar_t kTestX8120[] = {'T', 'e', 's', 't', 0x8120, '\0'};
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_EQ(::std::wstring(kTestX8119), kTestX8120);
      },
      "kTestX8120");

  ::std::wstring wstr3(wstr1);
  wstr3.at(2) = L'\0';
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(wstr1, wstr3), "wstr3");

  EXPECT_FATAL_FAILURE(
      {
        ASSERT_EQ(const_cast<wchar_t*>(L"foo"), ::std::wstring(L"bar"));
      },
      "");
}

#endif

TEST(EqAssertionTest, CharPointer) {
  char* const p0 = nullptr;

  void* pv1 = (void*)0x1234;
  void* pv2 = (void*)0xABC0;
  char* const p1 = reinterpret_cast<char*>(pv1);
  char* const p2 = reinterpret_cast<char*>(pv2);
  ASSERT_EQ(p1, p1);

  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(p0, p2), "  p2\n    Which is:");
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(p1, p2), "  p2\n    Which is:");
  EXPECT_FATAL_FAILURE(ASSERT_EQ(reinterpret_cast<char*>(0x1234),
                                 reinterpret_cast<char*>(0xABC0)),
                       "ABC0");
}

TEST(EqAssertionTest, WideCharPointer) {
  wchar_t* const p0 = nullptr;

  void* pv1 = (void*)0x1234;
  void* pv2 = (void*)0xABC0;
  wchar_t* const p1 = reinterpret_cast<wchar_t*>(pv1);
  wchar_t* const p2 = reinterpret_cast<wchar_t*>(pv2);
  EXPECT_EQ(p0, p0);

  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(p0, p2), "  p2\n    Which is:");
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(p1, p2), "  p2\n    Which is:");
  void* pv3 = (void*)0x1234;
  void* pv4 = (void*)0xABC0;
  const wchar_t* p3 = reinterpret_cast<const wchar_t*>(pv3);
  const wchar_t* p4 = reinterpret_cast<const wchar_t*>(pv4);
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(p3, p4), "p4");
}

TEST(EqAssertionTest, OtherPointer) {
  ASSERT_EQ(static_cast<const int*>(nullptr), static_cast<const int*>(nullptr));
  EXPECT_FATAL_FAILURE(ASSERT_EQ(static_cast<const int*>(nullptr),
                                 reinterpret_cast<const int*>(0x1234)),
                       "0x1234");
}

class UnprintableChar {
 public:
  explicit UnprintableChar(char ch) : char_(ch) {}

  bool operator==(const UnprintableChar& rhs) const {
    return char_ == rhs.char_;
  }
  bool operator!=(const UnprintableChar& rhs) const {
    return char_ != rhs.char_;
  }
  bool operator<(const UnprintableChar& rhs) const { return char_ < rhs.char_; }
  bool operator<=(const UnprintableChar& rhs) const {
    return char_ <= rhs.char_;
  }
  bool operator>(const UnprintableChar& rhs) const { return char_ > rhs.char_; }
  bool operator>=(const UnprintableChar& rhs) const {
    return char_ >= rhs.char_;
  }

 private:
  char char_;
};

TEST(ComparisonAssertionTest, AcceptsUnprintableArgs) {
  const UnprintableChar x('x'), y('y');
  ASSERT_EQ(x, x);
  EXPECT_NE(x, y);
  ASSERT_LT(x, y);
  EXPECT_LE(x, y);
  ASSERT_GT(y, x);
  EXPECT_GE(x, x);

  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(x, y), "1-byte object <78>");
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(x, y), "1-byte object <79>");
  EXPECT_NONFATAL_FAILURE(EXPECT_LT(y, y), "1-byte object <79>");
  EXPECT_NONFATAL_FAILURE(EXPECT_GT(x, y), "1-byte object <78>");
  EXPECT_NONFATAL_FAILURE(EXPECT_GT(x, y), "1-byte object <79>");

#ifndef __BORLANDC__

  EXPECT_FATAL_FAILURE(ASSERT_NE(UnprintableChar('x'), UnprintableChar('x')),
                       "1-byte object <78>");
  EXPECT_FATAL_FAILURE(ASSERT_LE(UnprintableChar('y'), UnprintableChar('x')),
                       "1-byte object <78>");
#endif
  EXPECT_FATAL_FAILURE(ASSERT_LE(UnprintableChar('y'), UnprintableChar('x')),
                       "1-byte object <79>");
  EXPECT_FATAL_FAILURE(ASSERT_GE(UnprintableChar('x'), UnprintableChar('y')),
                       "1-byte object <78>");
  EXPECT_FATAL_FAILURE(ASSERT_GE(UnprintableChar('x'), UnprintableChar('y')),
                       "1-byte object <79>");
}

class Foo {
 public:
  Foo() = default;

 private:
  int Bar() const { return 1; }

  FRIEND_TEST(FRIEND_TEST_Test, TEST);
  FRIEND_TEST(FRIEND_TEST_Test2, TEST_F);
};

TEST(FRIEND_TEST_Test, TEST) { ASSERT_EQ(1, Foo().Bar()); }

class FRIEND_TEST_Test2 : public Test {
 protected:
  Foo foo;
};

TEST_F(FRIEND_TEST_Test2, TEST_F) { ASSERT_EQ(1, foo.Bar()); }

class TestLifeCycleTest : public Test {
 protected:

  TestLifeCycleTest() { count_++; }

  ~TestLifeCycleTest() override { count_--; }

  int count() const { return count_; }

 private:
  static int count_;
};

int TestLifeCycleTest::count_ = 0;

TEST_F(TestLifeCycleTest, Test1) {

  ASSERT_EQ(1, count());
}

TEST_F(TestLifeCycleTest, Test2) {

  ASSERT_EQ(1, count());
}

}

TEST(AssertionResultTest, CopyConstructorWorksWhenNotOptimied) {

  AssertionResult r1 = AssertionSuccess();
  AssertionResult r2 = r1;

  r1 << "abc";

  AssertionResult r3 = r1;
  EXPECT_EQ(static_cast<bool>(r3), static_cast<bool>(r1));
  EXPECT_STREQ("abc", r1.message());
}

TEST(AssertionResultTest, ConstructionWorks) {
  AssertionResult r1 = AssertionSuccess();
  EXPECT_TRUE(r1);
  EXPECT_STREQ("", r1.message());

  AssertionResult r2 = AssertionSuccess() << "abc";
  EXPECT_TRUE(r2);
  EXPECT_STREQ("abc", r2.message());

  AssertionResult r3 = AssertionFailure();
  EXPECT_FALSE(r3);
  EXPECT_STREQ("", r3.message());

  AssertionResult r4 = AssertionFailure() << "def";
  EXPECT_FALSE(r4);
  EXPECT_STREQ("def", r4.message());

  AssertionResult r5 = AssertionFailure(Message() << "ghi");
  EXPECT_FALSE(r5);
  EXPECT_STREQ("ghi", r5.message());
}

TEST(AssertionResultTest, NegationWorks) {
  AssertionResult r1 = AssertionSuccess() << "abc";
  EXPECT_FALSE(!r1);
  EXPECT_STREQ("abc", (!r1).message());

  AssertionResult r2 = AssertionFailure() << "def";
  EXPECT_TRUE(!r2);
  EXPECT_STREQ("def", (!r2).message());
}

TEST(AssertionResultTest, StreamingWorks) {
  AssertionResult r = AssertionSuccess();
  r << "abc" << 'd' << 0 << true;
  EXPECT_STREQ("abcd0true", r.message());
}

TEST(AssertionResultTest, CanStreamOstreamManipulators) {
  AssertionResult r = AssertionSuccess();
  r << "Data" << std::endl << std::flush << std::ends << "Will be visible";
  EXPECT_STREQ("Data\n\\0Will be visible", r.message());
}

TEST(AssertionResultTest, ConstructibleFromContextuallyConvertibleToBool) {
  struct ExplicitlyConvertibleToBool {
    explicit operator bool() const { return value; }
    bool value;
  };
  ExplicitlyConvertibleToBool v1 = {false};
  ExplicitlyConvertibleToBool v2 = {true};
  EXPECT_FALSE(v1);
  EXPECT_TRUE(v2);
}

struct ConvertibleToAssertionResult {
  operator AssertionResult() const { return AssertionResult(true); }
};

TEST(AssertionResultTest, ConstructibleFromImplicitlyConvertible) {
  ConvertibleToAssertionResult obj;
  EXPECT_TRUE(obj);
}

class Base {
 public:
  explicit Base(int an_x) : x_(an_x) {}
  int x() const { return x_; }

 private:
  int x_;
};
std::ostream& operator<<(std::ostream& os, const Base& val) {
  return os << val.x();
}
std::ostream& operator<<(std::ostream& os, const Base* pointer) {
  return os << "(" << pointer->x() << ")";
}

TEST(MessageTest, CanStreamUserTypeInGlobalNameSpace) {
  Message msg;
  Base a(1);

  msg << a << &a;
  EXPECT_STREQ("1(1)", msg.GetString().c_str());
}

namespace {
class MyTypeInUnnamedNameSpace : public Base {
 public:
  explicit MyTypeInUnnamedNameSpace(int an_x) : Base(an_x) {}
};
std::ostream& operator<<(std::ostream& os,
                         const MyTypeInUnnamedNameSpace& val) {
  return os << val.x();
}
std::ostream& operator<<(std::ostream& os,
                         const MyTypeInUnnamedNameSpace* pointer) {
  return os << "(" << pointer->x() << ")";
}
}

TEST(MessageTest, CanStreamUserTypeInUnnamedNameSpace) {
  Message msg;
  MyTypeInUnnamedNameSpace a(1);

  msg << a << &a;
  EXPECT_STREQ("1(1)", msg.GetString().c_str());
}

namespace namespace1 {
class MyTypeInNameSpace1 : public Base {
 public:
  explicit MyTypeInNameSpace1(int an_x) : Base(an_x) {}
};
std::ostream& operator<<(std::ostream& os, const MyTypeInNameSpace1& val) {
  return os << val.x();
}
std::ostream& operator<<(std::ostream& os, const MyTypeInNameSpace1* pointer) {
  return os << "(" << pointer->x() << ")";
}
}

TEST(MessageTest, CanStreamUserTypeInUserNameSpace) {
  Message msg;
  namespace1::MyTypeInNameSpace1 a(1);

  msg << a << &a;
  EXPECT_STREQ("1(1)", msg.GetString().c_str());
}

namespace namespace2 {
class MyTypeInNameSpace2 : public ::Base {
 public:
  explicit MyTypeInNameSpace2(int an_x) : Base(an_x) {}
};
}
std::ostream& operator<<(std::ostream& os,
                         const namespace2::MyTypeInNameSpace2& val) {
  return os << val.x();
}
std::ostream& operator<<(std::ostream& os,
                         const namespace2::MyTypeInNameSpace2* pointer) {
  return os << "(" << pointer->x() << ")";
}

TEST(MessageTest, CanStreamUserTypeInUserNameSpaceWithStreamOperatorInGlobal) {
  Message msg;
  namespace2::MyTypeInNameSpace2 a(1);

  msg << a << &a;
  EXPECT_STREQ("1(1)", msg.GetString().c_str());
}

TEST(MessageTest, NullPointers) {
  Message msg;
  char* const p1 = nullptr;
  unsigned char* const p2 = nullptr;
  int* p3 = nullptr;
  double* p4 = nullptr;
  bool* p5 = nullptr;
  Message* p6 = nullptr;

  msg << p1 << p2 << p3 << p4 << p5 << p6;
  ASSERT_STREQ("(null)(null)(null)(null)(null)(null)", msg.GetString().c_str());
}

TEST(MessageTest, WideStrings) {

  const wchar_t* const_wstr = nullptr;
  EXPECT_STREQ("(null)", (Message() << const_wstr).GetString().c_str());

  wchar_t* wstr = nullptr;
  EXPECT_STREQ("(null)", (Message() << wstr).GetString().c_str());

  const_wstr = L"abc\x8119";
  EXPECT_STREQ("abc\xe8\x84\x99",
               (Message() << const_wstr).GetString().c_str());

  wstr = const_cast<wchar_t*>(const_wstr);
  EXPECT_STREQ("abc\xe8\x84\x99", (Message() << wstr).GetString().c_str());
}

namespace testing {

class TestInfoTest : public Test {
 protected:
  static const TestInfo* GetTestInfo(const char* test_name) {
    const TestSuite* const test_suite =
        GetUnitTestImpl()->GetTestSuite("TestInfoTest", "", nullptr, nullptr);

    for (int i = 0; i < test_suite->total_test_count(); ++i) {
      const TestInfo* const test_info = test_suite->GetTestInfo(i);
      if (strcmp(test_name, test_info->name()) == 0) return test_info;
    }
    return nullptr;
  }

  static const TestResult* GetTestResult(const TestInfo* test_info) {
    return test_info->result();
  }
};

TEST_F(TestInfoTest, Names) {
  const TestInfo* const test_info = GetTestInfo("Names");

  ASSERT_STREQ("TestInfoTest", test_info->test_suite_name());
  ASSERT_STREQ("Names", test_info->name());
}

TEST_F(TestInfoTest, result) {
  const TestInfo* const test_info = GetTestInfo("result");

  ASSERT_EQ(0, GetTestResult(test_info)->total_part_count());

  ASSERT_EQ(0, GetTestResult(test_info)->total_part_count());
}

#define VERIFY_CODE_LOCATION                                                \
  const int expected_line = __LINE__ - 1;                                   \
  const TestInfo* const test_info = GetUnitTestImpl()->current_test_info(); \
  ASSERT_TRUE(test_info);                                                   \
  EXPECT_STREQ(__FILE__, test_info->file());                                \
  EXPECT_EQ(expected_line, test_info->line())

TEST(CodeLocationForTEST, Verify) {
  VERIFY_CODE_LOCATION;
}

class CodeLocationForTESTF : public Test {};

TEST_F(CodeLocationForTESTF, Verify) {
  VERIFY_CODE_LOCATION;
}

class CodeLocationForTESTP : public TestWithParam<int> {};

TEST_P(CodeLocationForTESTP, Verify) {
  VERIFY_CODE_LOCATION;
}

INSTANTIATE_TEST_SUITE_P(, CodeLocationForTESTP, Values(0));

template <typename T>
class CodeLocationForTYPEDTEST : public Test {};

TYPED_TEST_SUITE(CodeLocationForTYPEDTEST, int);

TYPED_TEST(CodeLocationForTYPEDTEST, Verify) {
  VERIFY_CODE_LOCATION;
}

template <typename T>
class CodeLocationForTYPEDTESTP : public Test {};

TYPED_TEST_SUITE_P(CodeLocationForTYPEDTESTP);

TYPED_TEST_P(CodeLocationForTYPEDTESTP, Verify) {
  VERIFY_CODE_LOCATION;
}

REGISTER_TYPED_TEST_SUITE_P(CodeLocationForTYPEDTESTP, Verify);

INSTANTIATE_TYPED_TEST_SUITE_P(My, CodeLocationForTYPEDTESTP, int);

#undef VERIFY_CODE_LOCATION

#ifndef GTEST_REMOVE_LEGACY_TEST_CASEAPI_
class SetUpTestCaseTest : public Test {
 protected:

  static void SetUpTestCase() {
    printf("Setting up the test case . . .\n");

    shared_resource_ = "123";

    counter_++;

    EXPECT_EQ(1, counter_);
  }

  static void TearDownTestCase() {
    printf("Tearing down the test case . . .\n");

    counter_--;

    EXPECT_EQ(0, counter_);

    shared_resource_ = nullptr;
  }

  void SetUp() override {

    EXPECT_EQ(1, counter_);
  }

  static int counter_;

  static const char* shared_resource_;
};

int SetUpTestCaseTest::counter_ = 0;
const char* SetUpTestCaseTest::shared_resource_ = nullptr;

TEST_F(SetUpTestCaseTest, Test1) { EXPECT_STRNE(nullptr, shared_resource_); }

TEST_F(SetUpTestCaseTest, Test2) { EXPECT_STREQ("123", shared_resource_); }
#endif

class SetUpTestSuiteTest : public Test {
 protected:

  static void SetUpTestSuite() {
    printf("Setting up the test suite . . .\n");

    shared_resource_ = "123";

    counter_++;

    EXPECT_EQ(1, counter_);
  }

  static void TearDownTestSuite() {
    printf("Tearing down the test suite . . .\n");

    counter_--;

    EXPECT_EQ(0, counter_);

    shared_resource_ = nullptr;
  }

  void SetUp() override {

    EXPECT_EQ(1, counter_);
  }

  static int counter_;

  static const char* shared_resource_;
};

int SetUpTestSuiteTest::counter_ = 0;
const char* SetUpTestSuiteTest::shared_resource_ = nullptr;

TEST_F(SetUpTestSuiteTest, TestSetupTestSuite1) {
  EXPECT_STRNE(nullptr, shared_resource_);
}

TEST_F(SetUpTestSuiteTest, TestSetupTestSuite2) {
  EXPECT_STREQ("123", shared_resource_);
}

struct Flags {

  Flags()
      : also_run_disabled_tests(false),
        break_on_failure(false),
        catch_exceptions(false),
        death_test_use_fork(false),
        fail_fast(false),
        filter(""),
        list_tests(false),
        output(""),
        brief(false),
        print_time(true),
        random_seed(0),
        repeat(1),
        recreate_environments_when_repeating(true),
        shuffle(false),
        stack_trace_depth(kMaxStackTraceDepth),
        stream_result_to(""),
        throw_on_failure(false) {}

  static Flags AlsoRunDisabledTests(bool also_run_disabled_tests) {
    Flags flags;
    flags.also_run_disabled_tests = also_run_disabled_tests;
    return flags;
  }

  static Flags BreakOnFailure(bool break_on_failure) {
    Flags flags;
    flags.break_on_failure = break_on_failure;
    return flags;
  }

  static Flags CatchExceptions(bool catch_exceptions) {
    Flags flags;
    flags.catch_exceptions = catch_exceptions;
    return flags;
  }

  static Flags DeathTestUseFork(bool death_test_use_fork) {
    Flags flags;
    flags.death_test_use_fork = death_test_use_fork;
    return flags;
  }

  static Flags FailFast(bool fail_fast) {
    Flags flags;
    flags.fail_fast = fail_fast;
    return flags;
  }

  static Flags Filter(const char* filter) {
    Flags flags;
    flags.filter = filter;
    return flags;
  }

  static Flags ListTests(bool list_tests) {
    Flags flags;
    flags.list_tests = list_tests;
    return flags;
  }

  static Flags Output(const char* output) {
    Flags flags;
    flags.output = output;
    return flags;
  }

  static Flags Brief(bool brief) {
    Flags flags;
    flags.brief = brief;
    return flags;
  }

  static Flags PrintTime(bool print_time) {
    Flags flags;
    flags.print_time = print_time;
    return flags;
  }

  static Flags RandomSeed(int32_t random_seed) {
    Flags flags;
    flags.random_seed = random_seed;
    return flags;
  }

  static Flags Repeat(int32_t repeat) {
    Flags flags;
    flags.repeat = repeat;
    return flags;
  }

  static Flags RecreateEnvironmentsWhenRepeating(
      bool recreate_environments_when_repeating) {
    Flags flags;
    flags.recreate_environments_when_repeating =
        recreate_environments_when_repeating;
    return flags;
  }

  static Flags Shuffle(bool shuffle) {
    Flags flags;
    flags.shuffle = shuffle;
    return flags;
  }

  static Flags StackTraceDepth(int32_t stack_trace_depth) {
    Flags flags;
    flags.stack_trace_depth = stack_trace_depth;
    return flags;
  }

  static Flags StreamResultTo(const char* stream_result_to) {
    Flags flags;
    flags.stream_result_to = stream_result_to;
    return flags;
  }

  static Flags ThrowOnFailure(bool throw_on_failure) {
    Flags flags;
    flags.throw_on_failure = throw_on_failure;
    return flags;
  }

  bool also_run_disabled_tests;
  bool break_on_failure;
  bool catch_exceptions;
  bool death_test_use_fork;
  bool fail_fast;
  const char* filter;
  bool list_tests;
  const char* output;
  bool brief;
  bool print_time;
  int32_t random_seed;
  int32_t repeat;
  bool recreate_environments_when_repeating;
  bool shuffle;
  int32_t stack_trace_depth;
  const char* stream_result_to;
  bool throw_on_failure;
};

class ParseFlagsTest : public Test {
 protected:

  void SetUp() override {
    GTEST_FLAG_SET(also_run_disabled_tests, false);
    GTEST_FLAG_SET(break_on_failure, false);
    GTEST_FLAG_SET(catch_exceptions, false);
    GTEST_FLAG_SET(death_test_use_fork, false);
    GTEST_FLAG_SET(fail_fast, false);
    GTEST_FLAG_SET(filter, "");
    GTEST_FLAG_SET(list_tests, false);
    GTEST_FLAG_SET(output, "");
    GTEST_FLAG_SET(brief, false);
    GTEST_FLAG_SET(print_time, true);
    GTEST_FLAG_SET(random_seed, 0);
    GTEST_FLAG_SET(repeat, 1);
    GTEST_FLAG_SET(recreate_environments_when_repeating, true);
    GTEST_FLAG_SET(shuffle, false);
    GTEST_FLAG_SET(stack_trace_depth, kMaxStackTraceDepth);
    GTEST_FLAG_SET(stream_result_to, "");
    GTEST_FLAG_SET(throw_on_failure, false);
  }

  template <typename CharType>
  static void AssertStringArrayEq(int size1, CharType** array1, int size2,
                                  CharType** array2) {
    ASSERT_EQ(size1, size2) << " Array sizes different.";

    for (int i = 0; i != size1; i++) {
      ASSERT_STREQ(array1[i], array2[i]) << " where i == " << i;
    }
  }

  static void CheckFlags(const Flags& expected) {
    EXPECT_EQ(expected.also_run_disabled_tests,
              GTEST_FLAG_GET(also_run_disabled_tests));
    EXPECT_EQ(expected.break_on_failure, GTEST_FLAG_GET(break_on_failure));
    EXPECT_EQ(expected.catch_exceptions, GTEST_FLAG_GET(catch_exceptions));
    EXPECT_EQ(expected.death_test_use_fork,
              GTEST_FLAG_GET(death_test_use_fork));
    EXPECT_EQ(expected.fail_fast, GTEST_FLAG_GET(fail_fast));
    EXPECT_STREQ(expected.filter, GTEST_FLAG_GET(filter).c_str());
    EXPECT_EQ(expected.list_tests, GTEST_FLAG_GET(list_tests));
    EXPECT_STREQ(expected.output, GTEST_FLAG_GET(output).c_str());
    EXPECT_EQ(expected.brief, GTEST_FLAG_GET(brief));
    EXPECT_EQ(expected.print_time, GTEST_FLAG_GET(print_time));
    EXPECT_EQ(expected.random_seed, GTEST_FLAG_GET(random_seed));
    EXPECT_EQ(expected.repeat, GTEST_FLAG_GET(repeat));
    EXPECT_EQ(expected.recreate_environments_when_repeating,
              GTEST_FLAG_GET(recreate_environments_when_repeating));
    EXPECT_EQ(expected.shuffle, GTEST_FLAG_GET(shuffle));
    EXPECT_EQ(expected.stack_trace_depth, GTEST_FLAG_GET(stack_trace_depth));
    EXPECT_STREQ(expected.stream_result_to,
                 GTEST_FLAG_GET(stream_result_to).c_str());
    EXPECT_EQ(expected.throw_on_failure, GTEST_FLAG_GET(throw_on_failure));
  }

  template <typename CharType>
  static void TestParsingFlags(int argc1, const CharType** argv1, int argc2,
                               const CharType** argv2, const Flags& expected,
                               bool should_print_help) {
    const bool saved_help_flag = ::testing::internal::g_help_flag;
    ::testing::internal::g_help_flag = false;

#if GTEST_HAS_STREAM_REDIRECTION
    CaptureStdout();
#endif

    internal::ParseGoogleTestFlagsOnly(&argc1, const_cast<CharType**>(argv1));

#if GTEST_HAS_STREAM_REDIRECTION
    const std::string captured_stdout = GetCapturedStdout();
#endif

    CheckFlags(expected);

    AssertStringArrayEq(argc1 + 1, argv1, argc2 + 1, argv2);

    EXPECT_EQ(should_print_help, ::testing::internal::g_help_flag);

#if GTEST_HAS_STREAM_REDIRECTION
    const char* const expected_help_fragment =
        "This program contains tests written using";
    if (should_print_help) {
      EXPECT_PRED_FORMAT2(IsSubstring, expected_help_fragment, captured_stdout);
    } else {
      EXPECT_PRED_FORMAT2(IsNotSubstring, expected_help_fragment,
                          captured_stdout);
    }
#endif

    ::testing::internal::g_help_flag = saved_help_flag;
  }

#define GTEST_TEST_PARSING_FLAGS_(argv1, argv2, expected, should_print_help) \
  TestParsingFlags(sizeof(argv1) / sizeof(*argv1) - 1, argv1,                \
                   sizeof(argv2) / sizeof(*argv2) - 1, argv2, expected,      \
                   should_print_help)
};

TEST_F(ParseFlagsTest, Empty) {
  const char* argv[] = {nullptr};

  const char* argv2[] = {nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags(), false);
}

TEST_F(ParseFlagsTest, NoFlag) {
  const char* argv[] = {"foo.exe", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags(), false);
}

TEST_F(ParseFlagsTest, FailFast) {
  const char* argv[] = {"foo.exe", "--gtest_fail_fast", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::FailFast(true), false);
}

TEST_F(ParseFlagsTest, FilterEmpty) {
  const char* argv[] = {"foo.exe", "--gtest_filter=", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Filter(""), false);
}

TEST_F(ParseFlagsTest, FilterNonEmpty) {
  const char* argv[] = {"foo.exe", "--gtest_filter=abc", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Filter("abc"), false);
}

TEST_F(ParseFlagsTest, BreakOnFailureWithoutValue) {
  const char* argv[] = {"foo.exe", "--gtest_break_on_failure", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::BreakOnFailure(true), false);
}

TEST_F(ParseFlagsTest, BreakOnFailureFalse_0) {
  const char* argv[] = {"foo.exe", "--gtest_break_on_failure=0", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::BreakOnFailure(false), false);
}

TEST_F(ParseFlagsTest, BreakOnFailureFalse_f) {
  const char* argv[] = {"foo.exe", "--gtest_break_on_failure=f", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::BreakOnFailure(false), false);
}

TEST_F(ParseFlagsTest, BreakOnFailureFalse_F) {
  const char* argv[] = {"foo.exe", "--gtest_break_on_failure=F", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::BreakOnFailure(false), false);
}

TEST_F(ParseFlagsTest, BreakOnFailureTrue) {
  const char* argv[] = {"foo.exe", "--gtest_break_on_failure=1", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::BreakOnFailure(true), false);
}

TEST_F(ParseFlagsTest, CatchExceptions) {
  const char* argv[] = {"foo.exe", "--gtest_catch_exceptions", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::CatchExceptions(true), false);
}

TEST_F(ParseFlagsTest, DeathTestUseFork) {
  const char* argv[] = {"foo.exe", "--gtest_death_test_use_fork", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::DeathTestUseFork(true), false);
}

TEST_F(ParseFlagsTest, DuplicatedFlags) {
  const char* argv[] = {"foo.exe", "--gtest_filter=a", "--gtest_filter=b",
                        nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Filter("b"), false);
}

TEST_F(ParseFlagsTest, UnrecognizedFlag) {
  const char* argv[] = {"foo.exe", "--gtest_break_on_failure",
                        "bar",
                        "--gtest_filter=b", nullptr};

  const char* argv2[] = {"foo.exe", "bar", nullptr};

  Flags flags;
  flags.break_on_failure = true;
  flags.filter = "b";
  GTEST_TEST_PARSING_FLAGS_(argv, argv2, flags, false);
}

TEST_F(ParseFlagsTest, ListTestsFlag) {
  const char* argv[] = {"foo.exe", "--gtest_list_tests", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ListTests(true), false);
}

TEST_F(ParseFlagsTest, ListTestsTrue) {
  const char* argv[] = {"foo.exe", "--gtest_list_tests=1", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ListTests(true), false);
}

TEST_F(ParseFlagsTest, ListTestsFalse) {
  const char* argv[] = {"foo.exe", "--gtest_list_tests=0", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ListTests(false), false);
}

TEST_F(ParseFlagsTest, ListTestsFalse_f) {
  const char* argv[] = {"foo.exe", "--gtest_list_tests=f", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ListTests(false), false);
}

TEST_F(ParseFlagsTest, ListTestsFalse_F) {
  const char* argv[] = {"foo.exe", "--gtest_list_tests=F", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ListTests(false), false);
}

TEST_F(ParseFlagsTest, OutputXml) {
  const char* argv[] = {"foo.exe", "--gtest_output=xml", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Output("xml"), false);
}

TEST_F(ParseFlagsTest, OutputXmlFile) {
  const char* argv[] = {"foo.exe", "--gtest_output=xml:file", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Output("xml:file"), false);
}

TEST_F(ParseFlagsTest, OutputXmlDirectory) {
  const char* argv[] = {"foo.exe", "--gtest_output=xml:directory/path/",
                        nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Output("xml:directory/path/"),
                            false);
}

TEST_F(ParseFlagsTest, BriefFlag) {
  const char* argv[] = {"foo.exe", "--gtest_brief", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Brief(true), false);
}

TEST_F(ParseFlagsTest, BriefFlagTrue) {
  const char* argv[] = {"foo.exe", "--gtest_brief=1", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Brief(true), false);
}

TEST_F(ParseFlagsTest, BriefFlagFalse) {
  const char* argv[] = {"foo.exe", "--gtest_brief=0", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Brief(false), false);
}

TEST_F(ParseFlagsTest, PrintTimeFlag) {
  const char* argv[] = {"foo.exe", "--gtest_print_time", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::PrintTime(true), false);
}

TEST_F(ParseFlagsTest, PrintTimeTrue) {
  const char* argv[] = {"foo.exe", "--gtest_print_time=1", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::PrintTime(true), false);
}

TEST_F(ParseFlagsTest, PrintTimeFalse) {
  const char* argv[] = {"foo.exe", "--gtest_print_time=0", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::PrintTime(false), false);
}

TEST_F(ParseFlagsTest, PrintTimeFalse_f) {
  const char* argv[] = {"foo.exe", "--gtest_print_time=f", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::PrintTime(false), false);
}

TEST_F(ParseFlagsTest, PrintTimeFalse_F) {
  const char* argv[] = {"foo.exe", "--gtest_print_time=F", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::PrintTime(false), false);
}

TEST_F(ParseFlagsTest, RandomSeed) {
  const char* argv[] = {"foo.exe", "--gtest_random_seed=1000", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::RandomSeed(1000), false);
}

TEST_F(ParseFlagsTest, Repeat) {
  const char* argv[] = {"foo.exe", "--gtest_repeat=1000", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Repeat(1000), false);
}

TEST_F(ParseFlagsTest, RecreateEnvironmentsWhenRepeating) {
  const char* argv[] = {
      "foo.exe",
      "--gtest_recreate_environments_when_repeating=0",
      nullptr,
  };

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(
      argv, argv2, Flags::RecreateEnvironmentsWhenRepeating(false), false);
}

TEST_F(ParseFlagsTest, AlsoRunDisabledTestsFlag) {
  const char* argv[] = {"foo.exe", "--gtest_also_run_disabled_tests", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::AlsoRunDisabledTests(true),
                            false);
}

TEST_F(ParseFlagsTest, AlsoRunDisabledTestsTrue) {
  const char* argv[] = {"foo.exe", "--gtest_also_run_disabled_tests=1",
                        nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::AlsoRunDisabledTests(true),
                            false);
}

TEST_F(ParseFlagsTest, AlsoRunDisabledTestsFalse) {
  const char* argv[] = {"foo.exe", "--gtest_also_run_disabled_tests=0",
                        nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::AlsoRunDisabledTests(false),
                            false);
}

TEST_F(ParseFlagsTest, ShuffleWithoutValue) {
  const char* argv[] = {"foo.exe", "--gtest_shuffle", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Shuffle(true), false);
}

TEST_F(ParseFlagsTest, ShuffleFalse_0) {
  const char* argv[] = {"foo.exe", "--gtest_shuffle=0", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Shuffle(false), false);
}

TEST_F(ParseFlagsTest, ShuffleTrue) {
  const char* argv[] = {"foo.exe", "--gtest_shuffle=1", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Shuffle(true), false);
}

TEST_F(ParseFlagsTest, StackTraceDepth) {
  const char* argv[] = {"foo.exe", "--gtest_stack_trace_depth=5", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::StackTraceDepth(5), false);
}

TEST_F(ParseFlagsTest, StreamResultTo) {
  const char* argv[] = {"foo.exe", "--gtest_stream_result_to=localhost:1234",
                        nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2,
                            Flags::StreamResultTo("localhost:1234"), false);
}

TEST_F(ParseFlagsTest, ThrowOnFailureWithoutValue) {
  const char* argv[] = {"foo.exe", "--gtest_throw_on_failure", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ThrowOnFailure(true), false);
}

TEST_F(ParseFlagsTest, ThrowOnFailureFalse_0) {
  const char* argv[] = {"foo.exe", "--gtest_throw_on_failure=0", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ThrowOnFailure(false), false);
}

TEST_F(ParseFlagsTest, ThrowOnFailureTrue) {
  const char* argv[] = {"foo.exe", "--gtest_throw_on_failure=1", nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ThrowOnFailure(true), false);
}

TEST_F(ParseFlagsTest, FilterBad) {
  const char* argv[] = {"foo.exe", "--gtest_filter", nullptr};

  const char* argv2[] = {"foo.exe", "--gtest_filter", nullptr};

#if defined(GTEST_HAS_ABSL) && defined(GTEST_HAS_DEATH_TEST)

  EXPECT_EXIT(GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Filter(""), true),
              testing::ExitedWithCode(1),
              "ERROR: Missing the value for the flag 'gtest_filter'");
#elif !defined(GTEST_HAS_ABSL)
  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Filter(""), true);
#else
  static_cast<void>(argv);
  static_cast<void>(argv2);
#endif
}

TEST_F(ParseFlagsTest, OutputEmpty) {
  const char* argv[] = {"foo.exe", "--gtest_output", nullptr};

  const char* argv2[] = {"foo.exe", "--gtest_output", nullptr};

#if defined(GTEST_HAS_ABSL) && defined(GTEST_HAS_DEATH_TEST)

  EXPECT_EXIT(GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags(), true),
              testing::ExitedWithCode(1),
              "ERROR: Missing the value for the flag 'gtest_output'");
#elif !defined(GTEST_HAS_ABSL)
  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags(), true);
#else
  static_cast<void>(argv);
  static_cast<void>(argv2);
#endif
}

#ifdef GTEST_HAS_ABSL
TEST_F(ParseFlagsTest, AbseilPositionalFlags) {
  const char* argv[] = {"foo.exe", "--gtest_throw_on_failure=1", "--",
                        "--other_flag", nullptr};

  const char* argv2[] = {"foo.exe", "--other_flag", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::ThrowOnFailure(true), false);
}
#endif

TEST_F(ParseFlagsTest, UnrecognizedFlags) {
  const char* argv[] = {"foo.exe", "--gtest_filter=abcd", "--other_flag",
                        nullptr};

  const char* argv2[] = {"foo.exe", "--other_flag", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Filter("abcd"), false);
}

#ifdef GTEST_OS_WINDOWS

TEST_F(ParseFlagsTest, WideStrings) {
  const wchar_t* argv[] = {L"foo.exe",
                           L"--gtest_filter=Foo*",
                           L"--gtest_list_tests=1",
                           L"--gtest_break_on_failure",
                           L"--non_gtest_flag",
                           NULL};

  const wchar_t* argv2[] = {L"foo.exe", L"--non_gtest_flag", NULL};

  Flags expected_flags;
  expected_flags.break_on_failure = true;
  expected_flags.filter = "Foo*";
  expected_flags.list_tests = true;

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, expected_flags, false);
}
#endif

#if GTEST_USE_OWN_FLAGFILE_FLAG_
class FlagfileTest : public ParseFlagsTest {
 public:
  void SetUp() override {
    ParseFlagsTest::SetUp();

    testdata_path_.Set(internal::FilePath(
        testing::TempDir() + internal::GetCurrentExecutableName().string() +
        "_flagfile_test"));
    testing::internal::posix::RmDir(testdata_path_.c_str());
    EXPECT_TRUE(testdata_path_.CreateFolder());
  }

  void TearDown() override {
    testing::internal::posix::RmDir(testdata_path_.c_str());
    ParseFlagsTest::TearDown();
  }

  internal::FilePath CreateFlagfile(const char* contents) {
    internal::FilePath file_path(internal::FilePath::GenerateUniqueFileName(
        testdata_path_, internal::FilePath("unique"), "txt"));
    FILE* f = testing::internal::posix::FOpen(file_path.c_str(), "w");
    fprintf(f, "%s", contents);
    fclose(f);
    return file_path;
  }

 private:
  internal::FilePath testdata_path_;
};

TEST_F(FlagfileTest, Empty) {
  internal::FilePath flagfile_path(CreateFlagfile(""));
  std::string flagfile_flag =
      std::string("--" GTEST_FLAG_PREFIX_ "flagfile=") + flagfile_path.c_str();

  const char* argv[] = {"foo.exe", flagfile_flag.c_str(), nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags(), false);
}

TEST_F(FlagfileTest, FilterNonEmpty) {
  internal::FilePath flagfile_path(
      CreateFlagfile("--" GTEST_FLAG_PREFIX_ "filter=abc"));
  std::string flagfile_flag =
      std::string("--" GTEST_FLAG_PREFIX_ "flagfile=") + flagfile_path.c_str();

  const char* argv[] = {"foo.exe", flagfile_flag.c_str(), nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, Flags::Filter("abc"), false);
}

TEST_F(FlagfileTest, SeveralFlags) {
  internal::FilePath flagfile_path(
      CreateFlagfile("--" GTEST_FLAG_PREFIX_ "filter=abc\n"
                     "--" GTEST_FLAG_PREFIX_ "break_on_failure\n"
                     "--" GTEST_FLAG_PREFIX_ "list_tests"));
  std::string flagfile_flag =
      std::string("--" GTEST_FLAG_PREFIX_ "flagfile=") + flagfile_path.c_str();

  const char* argv[] = {"foo.exe", flagfile_flag.c_str(), nullptr};

  const char* argv2[] = {"foo.exe", nullptr};

  Flags expected_flags;
  expected_flags.break_on_failure = true;
  expected_flags.filter = "abc";
  expected_flags.list_tests = true;

  GTEST_TEST_PARSING_FLAGS_(argv, argv2, expected_flags, false);
}
#endif

class CurrentTestInfoTest : public Test {
 protected:

  static void SetUpTestSuite() {

    const TestInfo* test_info = UnitTest::GetInstance()->current_test_info();
    EXPECT_TRUE(test_info == nullptr)
        << "There should be no tests running at this point.";
  }

  static void TearDownTestSuite() {
    const TestInfo* test_info = UnitTest::GetInstance()->current_test_info();
    EXPECT_TRUE(test_info == nullptr)
        << "There should be no tests running at this point.";
  }
};

TEST_F(CurrentTestInfoTest, WorksForFirstTestInATestSuite) {
  const TestInfo* test_info = UnitTest::GetInstance()->current_test_info();
  ASSERT_TRUE(nullptr != test_info)
      << "There is a test running so we should have a valid TestInfo.";
  EXPECT_STREQ("CurrentTestInfoTest", test_info->test_suite_name())
      << "Expected the name of the currently running test suite.";
  EXPECT_STREQ("WorksForFirstTestInATestSuite", test_info->name())
      << "Expected the name of the currently running test.";
}

TEST_F(CurrentTestInfoTest, WorksForSecondTestInATestSuite) {
  const TestInfo* test_info = UnitTest::GetInstance()->current_test_info();
  ASSERT_TRUE(nullptr != test_info)
      << "There is a test running so we should have a valid TestInfo.";
  EXPECT_STREQ("CurrentTestInfoTest", test_info->test_suite_name())
      << "Expected the name of the currently running test suite.";
  EXPECT_STREQ("WorksForSecondTestInATestSuite", test_info->name())
      << "Expected the name of the currently running test.";
}

}

namespace my_namespace {
namespace testing {

class Test {};

class Message {};

class AssertionResult {};

TEST(NestedTestingNamespaceTest, Success) {
  EXPECT_EQ(1, 1) << "This shouldn't fail.";
}

TEST(NestedTestingNamespaceTest, Failure) {
  EXPECT_FATAL_FAILURE(FAIL() << "This failure is expected.",
                       "This failure is expected.");
}

}
}

class ProtectedFixtureMethodsTest : public Test {
 protected:
  void SetUp() override { Test::SetUp(); }
  void TearDown() override { Test::TearDown(); }
};

TEST(StreamingAssertionsTest, Unconditional) {
  SUCCEED() << "expected success";
  EXPECT_NONFATAL_FAILURE(ADD_FAILURE() << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(FAIL() << "expected failure", "expected failure");
}

#ifdef __BORLANDC__

#pragma option push -w-ccc -w-rch
#endif

TEST(StreamingAssertionsTest, Truth) {
  EXPECT_TRUE(true) << "unexpected failure";
  ASSERT_TRUE(true) << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(false) << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_TRUE(false) << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, Truth2) {
  EXPECT_FALSE(false) << "unexpected failure";
  ASSERT_FALSE(false) << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_FALSE(true) << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_FALSE(true) << "expected failure",
                       "expected failure");
}

#ifdef __BORLANDC__

#pragma option pop
#endif

TEST(StreamingAssertionsTest, IntegerEquals) {
  EXPECT_EQ(1, 1) << "unexpected failure";
  ASSERT_EQ(1, 1) << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_EQ(1, 2) << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_EQ(1, 2) << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, IntegerLessThan) {
  EXPECT_LT(1, 2) << "unexpected failure";
  ASSERT_LT(1, 2) << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_LT(2, 1) << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_LT(2, 1) << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, StringsEqual) {
  EXPECT_STREQ("foo", "foo") << "unexpected failure";
  ASSERT_STREQ("foo", "foo") << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_STREQ("foo", "bar") << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_STREQ("foo", "bar") << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, StringsNotEqual) {
  EXPECT_STRNE("foo", "bar") << "unexpected failure";
  ASSERT_STRNE("foo", "bar") << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_STRNE("foo", "foo") << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_STRNE("foo", "foo") << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, StringsEqualIgnoringCase) {
  EXPECT_STRCASEEQ("foo", "FOO") << "unexpected failure";
  ASSERT_STRCASEEQ("foo", "FOO") << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_STRCASEEQ("foo", "bar") << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_STRCASEEQ("foo", "bar") << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, StringNotEqualIgnoringCase) {
  EXPECT_STRCASENE("foo", "bar") << "unexpected failure";
  ASSERT_STRCASENE("foo", "bar") << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_STRCASENE("foo", "FOO") << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_STRCASENE("bar", "BAR") << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, FloatingPointEquals) {
  EXPECT_FLOAT_EQ(1.0, 1.0) << "unexpected failure";
  ASSERT_FLOAT_EQ(1.0, 1.0) << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_FLOAT_EQ(0.0, 1.0) << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_FLOAT_EQ(0.0, 1.0) << "expected failure",
                       "expected failure");
}

#if GTEST_HAS_EXCEPTIONS

TEST(StreamingAssertionsTest, Throw) {
  EXPECT_THROW(ThrowAnInteger(), int) << "unexpected failure";
  ASSERT_THROW(ThrowAnInteger(), int) << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_THROW(ThrowAnInteger(), bool)
                              << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_THROW(ThrowAnInteger(), bool)
                           << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, NoThrow) {
  EXPECT_NO_THROW(ThrowNothing()) << "unexpected failure";
  ASSERT_NO_THROW(ThrowNothing()) << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_NO_THROW(ThrowAnInteger())
                              << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_NO_THROW(ThrowAnInteger()) << "expected failure",
                       "expected failure");
}

TEST(StreamingAssertionsTest, AnyThrow) {
  EXPECT_ANY_THROW(ThrowAnInteger()) << "unexpected failure";
  ASSERT_ANY_THROW(ThrowAnInteger()) << "unexpected failure";
  EXPECT_NONFATAL_FAILURE(EXPECT_ANY_THROW(ThrowNothing())
                              << "expected failure",
                          "expected failure");
  EXPECT_FATAL_FAILURE(ASSERT_ANY_THROW(ThrowNothing()) << "expected failure",
                       "expected failure");
}

#endif

TEST(ColoredOutputTest, UsesColorsWhenGTestColorFlagIsYes) {
  GTEST_FLAG_SET(color, "yes");

  SetEnv("TERM", "xterm");
  EXPECT_TRUE(ShouldUseColor(true));
  EXPECT_TRUE(ShouldUseColor(false));

  SetEnv("TERM", "dumb");
  EXPECT_TRUE(ShouldUseColor(true));
  EXPECT_TRUE(ShouldUseColor(false));
}

TEST(ColoredOutputTest, UsesColorsWhenGTestColorFlagIsAliasOfYes) {
  SetEnv("TERM", "dumb");

  GTEST_FLAG_SET(color, "True");
  EXPECT_TRUE(ShouldUseColor(false));

  GTEST_FLAG_SET(color, "t");
  EXPECT_TRUE(ShouldUseColor(false));

  GTEST_FLAG_SET(color, "1");
  EXPECT_TRUE(ShouldUseColor(false));
}

TEST(ColoredOutputTest, UsesNoColorWhenGTestColorFlagIsNo) {
  GTEST_FLAG_SET(color, "no");

  SetEnv("TERM", "xterm");
  EXPECT_FALSE(ShouldUseColor(true));
  EXPECT_FALSE(ShouldUseColor(false));

  SetEnv("TERM", "dumb");
  EXPECT_FALSE(ShouldUseColor(true));
  EXPECT_FALSE(ShouldUseColor(false));
}

TEST(ColoredOutputTest, UsesNoColorWhenGTestColorFlagIsInvalid) {
  SetEnv("TERM", "xterm");

  GTEST_FLAG_SET(color, "F");
  EXPECT_FALSE(ShouldUseColor(true));

  GTEST_FLAG_SET(color, "0");
  EXPECT_FALSE(ShouldUseColor(true));

  GTEST_FLAG_SET(color, "unknown");
  EXPECT_FALSE(ShouldUseColor(true));
}

TEST(ColoredOutputTest, UsesColorsWhenStdoutIsTty) {
  GTEST_FLAG_SET(color, "auto");

  SetEnv("TERM", "xterm");
  EXPECT_FALSE(ShouldUseColor(false));
  EXPECT_TRUE(ShouldUseColor(true));
}

TEST(ColoredOutputTest, UsesColorsWhenTermSupportsColors) {
  GTEST_FLAG_SET(color, "auto");

#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MINGW)

  SetEnv("TERM", "dumb");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "xterm");
  EXPECT_TRUE(ShouldUseColor(true));
#else

  SetEnv("TERM", "dumb");
  EXPECT_FALSE(ShouldUseColor(true));

  SetEnv("TERM", "emacs");
  EXPECT_FALSE(ShouldUseColor(true));

  SetEnv("TERM", "vt100");
  EXPECT_FALSE(ShouldUseColor(true));

  SetEnv("TERM", "xterm-mono");
  EXPECT_FALSE(ShouldUseColor(true));

  SetEnv("TERM", "xterm");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "xterm-color");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "xterm-kitty");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "alacritty");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "xterm-256color");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "screen");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "screen-256color");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "tmux");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "tmux-256color");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "rxvt-unicode");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "rxvt-unicode-256color");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "linux");
  EXPECT_TRUE(ShouldUseColor(true));

  SetEnv("TERM", "cygwin");
  EXPECT_TRUE(ShouldUseColor(true));
#endif
}

[[maybe_unused]] static bool dummy1 = StaticAssertTypeEq<bool, bool>();
[[maybe_unused]] static bool dummy2 =
    StaticAssertTypeEq<const int, const int>();

template <typename T>
class StaticAssertTypeEqTestHelper {
 public:
  StaticAssertTypeEqTestHelper() { StaticAssertTypeEq<bool, T>(); }
};

TEST(StaticAssertTypeEqTest, WorksInClass) {
  StaticAssertTypeEqTestHelper<bool>();
}

typedef int IntAlias;

TEST(StaticAssertTypeEqTest, CompilesForEqualTypes) {
  StaticAssertTypeEq<int, IntAlias>();
  StaticAssertTypeEq<int*, IntAlias*>();
}

TEST(HasNonfatalFailureTest, ReturnsFalseWhenThereIsNoFailure) {
  EXPECT_FALSE(HasNonfatalFailure());
}

static void FailFatally() { FAIL(); }

TEST(HasNonfatalFailureTest, ReturnsFalseWhenThereIsOnlyFatalFailure) {
  FailFatally();
  const bool has_nonfatal_failure = HasNonfatalFailure();
  ClearCurrentTestPartResults();
  EXPECT_FALSE(has_nonfatal_failure);
}

TEST(HasNonfatalFailureTest, ReturnsTrueWhenThereIsNonfatalFailure) {
  ADD_FAILURE();
  const bool has_nonfatal_failure = HasNonfatalFailure();
  ClearCurrentTestPartResults();
  EXPECT_TRUE(has_nonfatal_failure);
}

TEST(HasNonfatalFailureTest, ReturnsTrueWhenThereAreFatalAndNonfatalFailures) {
  FailFatally();
  ADD_FAILURE();
  const bool has_nonfatal_failure = HasNonfatalFailure();
  ClearCurrentTestPartResults();
  EXPECT_TRUE(has_nonfatal_failure);
}

static bool HasNonfatalFailureHelper() {
  return testing::Test::HasNonfatalFailure();
}

TEST(HasNonfatalFailureTest, WorksOutsideOfTestBody) {
  EXPECT_FALSE(HasNonfatalFailureHelper());
}

TEST(HasNonfatalFailureTest, WorksOutsideOfTestBody2) {
  ADD_FAILURE();
  const bool has_nonfatal_failure = HasNonfatalFailureHelper();
  ClearCurrentTestPartResults();
  EXPECT_TRUE(has_nonfatal_failure);
}

TEST(HasFailureTest, ReturnsFalseWhenThereIsNoFailure) {
  EXPECT_FALSE(HasFailure());
}

TEST(HasFailureTest, ReturnsTrueWhenThereIsFatalFailure) {
  FailFatally();
  const bool has_failure = HasFailure();
  ClearCurrentTestPartResults();
  EXPECT_TRUE(has_failure);
}

TEST(HasFailureTest, ReturnsTrueWhenThereIsNonfatalFailure) {
  ADD_FAILURE();
  const bool has_failure = HasFailure();
  ClearCurrentTestPartResults();
  EXPECT_TRUE(has_failure);
}

TEST(HasFailureTest, ReturnsTrueWhenThereAreFatalAndNonfatalFailures) {
  FailFatally();
  ADD_FAILURE();
  const bool has_failure = HasFailure();
  ClearCurrentTestPartResults();
  EXPECT_TRUE(has_failure);
}

static bool HasFailureHelper() { return testing::Test::HasFailure(); }

TEST(HasFailureTest, WorksOutsideOfTestBody) {
  EXPECT_FALSE(HasFailureHelper());
}

TEST(HasFailureTest, WorksOutsideOfTestBody2) {
  ADD_FAILURE();
  const bool has_failure = HasFailureHelper();
  ClearCurrentTestPartResults();
  EXPECT_TRUE(has_failure);
}

class TestListener : public EmptyTestEventListener {
 public:
  TestListener() : on_start_counter_(nullptr), is_destroyed_(nullptr) {}
  TestListener(int* on_start_counter, bool* is_destroyed)
      : on_start_counter_(on_start_counter), is_destroyed_(is_destroyed) {}

  ~TestListener() override {
    if (is_destroyed_) *is_destroyed_ = true;
  }

 protected:
  void OnTestProgramStart(const UnitTest&              ) override {
    if (on_start_counter_ != nullptr) (*on_start_counter_)++;
  }

 private:
  int* on_start_counter_;
  bool* is_destroyed_;
};

TEST(TestEventListenersTest, ConstructionWorks) {
  TestEventListeners listeners;

  EXPECT_TRUE(TestEventListenersAccessor::GetRepeater(&listeners) != nullptr);
  EXPECT_TRUE(listeners.default_result_printer() == nullptr);
  EXPECT_TRUE(listeners.default_xml_generator() == nullptr);
}

TEST(TestEventListenersTest, DestructionWorks) {
  bool default_result_printer_is_destroyed = false;
  bool default_xml_printer_is_destroyed = false;
  bool extra_listener_is_destroyed = false;
  TestListener* default_result_printer =
      new TestListener(nullptr, &default_result_printer_is_destroyed);
  TestListener* default_xml_printer =
      new TestListener(nullptr, &default_xml_printer_is_destroyed);
  TestListener* extra_listener =
      new TestListener(nullptr, &extra_listener_is_destroyed);

  {
    TestEventListeners listeners;
    TestEventListenersAccessor::SetDefaultResultPrinter(&listeners,
                                                        default_result_printer);
    TestEventListenersAccessor::SetDefaultXmlGenerator(&listeners,
                                                       default_xml_printer);
    listeners.Append(extra_listener);
  }
  EXPECT_TRUE(default_result_printer_is_destroyed);
  EXPECT_TRUE(default_xml_printer_is_destroyed);
  EXPECT_TRUE(extra_listener_is_destroyed);
}

TEST(TestEventListenersTest, Append) {
  int on_start_counter = 0;
  bool is_destroyed = false;
  TestListener* listener = new TestListener(&on_start_counter, &is_destroyed);
  {
    TestEventListeners listeners;
    listeners.Append(listener);
    TestEventListenersAccessor::GetRepeater(&listeners)
        ->OnTestProgramStart(*UnitTest::GetInstance());
    EXPECT_EQ(1, on_start_counter);
  }
  EXPECT_TRUE(is_destroyed);
}

class SequenceTestingListener : public EmptyTestEventListener {
 public:
  SequenceTestingListener(std::vector<std::string>* vector, const char* id)
      : vector_(vector), id_(id) {}

 protected:
  void OnTestProgramStart(const UnitTest&              ) override {
    vector_->push_back(GetEventDescription("OnTestProgramStart"));
  }

  void OnTestProgramEnd(const UnitTest&              ) override {
    vector_->push_back(GetEventDescription("OnTestProgramEnd"));
  }

  void OnTestIterationStart(const UnitTest&              ,
                            int              ) override {
    vector_->push_back(GetEventDescription("OnTestIterationStart"));
  }

  void OnTestIterationEnd(const UnitTest&              ,
                          int              ) override {
    vector_->push_back(GetEventDescription("OnTestIterationEnd"));
  }

 private:
  std::string GetEventDescription(const char* method) {
    Message message;
    message << id_ << "." << method;
    return message.GetString();
  }

  std::vector<std::string>* vector_;
  const char* const id_;

  SequenceTestingListener(const SequenceTestingListener&) = delete;
  SequenceTestingListener& operator=(const SequenceTestingListener&) = delete;
};

TEST(EventListenerTest, AppendKeepsOrder) {
  std::vector<std::string> vec;
  TestEventListeners listeners;
  listeners.Append(new SequenceTestingListener(&vec, "1st"));
  listeners.Append(new SequenceTestingListener(&vec, "2nd"));
  listeners.Append(new SequenceTestingListener(&vec, "3rd"));

  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestProgramStart(*UnitTest::GetInstance());
  ASSERT_EQ(3U, vec.size());
  EXPECT_STREQ("1st.OnTestProgramStart", vec[0].c_str());
  EXPECT_STREQ("2nd.OnTestProgramStart", vec[1].c_str());
  EXPECT_STREQ("3rd.OnTestProgramStart", vec[2].c_str());

  vec.clear();
  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestProgramEnd(*UnitTest::GetInstance());
  ASSERT_EQ(3U, vec.size());
  EXPECT_STREQ("3rd.OnTestProgramEnd", vec[0].c_str());
  EXPECT_STREQ("2nd.OnTestProgramEnd", vec[1].c_str());
  EXPECT_STREQ("1st.OnTestProgramEnd", vec[2].c_str());

  vec.clear();
  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestIterationStart(*UnitTest::GetInstance(), 0);
  ASSERT_EQ(3U, vec.size());
  EXPECT_STREQ("1st.OnTestIterationStart", vec[0].c_str());
  EXPECT_STREQ("2nd.OnTestIterationStart", vec[1].c_str());
  EXPECT_STREQ("3rd.OnTestIterationStart", vec[2].c_str());

  vec.clear();
  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestIterationEnd(*UnitTest::GetInstance(), 0);
  ASSERT_EQ(3U, vec.size());
  EXPECT_STREQ("3rd.OnTestIterationEnd", vec[0].c_str());
  EXPECT_STREQ("2nd.OnTestIterationEnd", vec[1].c_str());
  EXPECT_STREQ("1st.OnTestIterationEnd", vec[2].c_str());
}

TEST(TestEventListenersTest, Release) {
  int on_start_counter = 0;
  bool is_destroyed = false;

  TestListener* listener = new TestListener(&on_start_counter, &is_destroyed);
  {
    TestEventListeners listeners;
    listeners.Append(listener);
    EXPECT_EQ(listener, listeners.Release(listener));
    TestEventListenersAccessor::GetRepeater(&listeners)
        ->OnTestProgramStart(*UnitTest::GetInstance());
    EXPECT_TRUE(listeners.Release(listener) == nullptr);
  }
  EXPECT_EQ(0, on_start_counter);
  EXPECT_FALSE(is_destroyed);
  delete listener;
}

TEST(EventListenerTest, SuppressEventForwarding) {
  int on_start_counter = 0;
  TestListener* listener = new TestListener(&on_start_counter, nullptr);

  TestEventListeners listeners;
  listeners.Append(listener);
  ASSERT_TRUE(TestEventListenersAccessor::EventForwardingEnabled(listeners));
  TestEventListenersAccessor::SuppressEventForwarding(&listeners);
  ASSERT_FALSE(TestEventListenersAccessor::EventForwardingEnabled(listeners));
  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestProgramStart(*UnitTest::GetInstance());
  EXPECT_EQ(0, on_start_counter);
}

TEST(EventListenerDeathTest, EventsNotForwardedInDeathTestSubprocesses) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        GTEST_CHECK_(TestEventListenersAccessor::EventForwardingEnabled(
            *GetUnitTestImpl()->listeners()))
            << "expected failure";
      },
      "expected failure");
}

TEST(EventListenerTest, default_result_printer) {
  int on_start_counter = 0;
  bool is_destroyed = false;
  TestListener* listener = new TestListener(&on_start_counter, &is_destroyed);

  TestEventListeners listeners;
  TestEventListenersAccessor::SetDefaultResultPrinter(&listeners, listener);

  EXPECT_EQ(listener, listeners.default_result_printer());

  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestProgramStart(*UnitTest::GetInstance());

  EXPECT_EQ(1, on_start_counter);

  TestEventListenersAccessor::SetDefaultResultPrinter(&listeners, nullptr);

  EXPECT_TRUE(listeners.default_result_printer() == nullptr);
  EXPECT_TRUE(is_destroyed);

  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestProgramStart(*UnitTest::GetInstance());
  EXPECT_EQ(1, on_start_counter);
}

TEST(EventListenerTest, RemovingDefaultResultPrinterWorks) {
  int on_start_counter = 0;
  bool is_destroyed = false;

  TestListener* listener = new TestListener(&on_start_counter, &is_destroyed);
  {
    TestEventListeners listeners;
    TestEventListenersAccessor::SetDefaultResultPrinter(&listeners, listener);

    EXPECT_EQ(listener, listeners.Release(listener));
    EXPECT_TRUE(listeners.default_result_printer() == nullptr);
    EXPECT_FALSE(is_destroyed);

    TestEventListenersAccessor::GetRepeater(&listeners)
        ->OnTestProgramStart(*UnitTest::GetInstance());
    EXPECT_EQ(0, on_start_counter);
  }

  EXPECT_FALSE(is_destroyed);
  delete listener;
}

TEST(EventListenerTest, default_xml_generator) {
  int on_start_counter = 0;
  bool is_destroyed = false;
  TestListener* listener = new TestListener(&on_start_counter, &is_destroyed);

  TestEventListeners listeners;
  TestEventListenersAccessor::SetDefaultXmlGenerator(&listeners, listener);

  EXPECT_EQ(listener, listeners.default_xml_generator());

  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestProgramStart(*UnitTest::GetInstance());

  EXPECT_EQ(1, on_start_counter);

  TestEventListenersAccessor::SetDefaultXmlGenerator(&listeners, nullptr);

  EXPECT_TRUE(listeners.default_xml_generator() == nullptr);
  EXPECT_TRUE(is_destroyed);

  TestEventListenersAccessor::GetRepeater(&listeners)
      ->OnTestProgramStart(*UnitTest::GetInstance());
  EXPECT_EQ(1, on_start_counter);
}

TEST(EventListenerTest, RemovingDefaultXmlGeneratorWorks) {
  int on_start_counter = 0;
  bool is_destroyed = false;

  TestListener* listener = new TestListener(&on_start_counter, &is_destroyed);
  {
    TestEventListeners listeners;
    TestEventListenersAccessor::SetDefaultXmlGenerator(&listeners, listener);

    EXPECT_EQ(listener, listeners.Release(listener));
    EXPECT_TRUE(listeners.default_xml_generator() == nullptr);
    EXPECT_FALSE(is_destroyed);

    TestEventListenersAccessor::GetRepeater(&listeners)
        ->OnTestProgramStart(*UnitTest::GetInstance());
    EXPECT_EQ(0, on_start_counter);
  }

  EXPECT_FALSE(is_destroyed);
  delete listener;
}

GTEST_TEST(AlternativeNameTest, Works) {
  GTEST_SUCCEED() << "OK";

  EXPECT_FATAL_FAILURE(GTEST_FAIL() << "An expected failure",
                       "An expected failure");

  GTEST_ASSERT_EQ(0, 0);
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_EQ(0, 1) << "An expected failure",
                       "An expected failure");
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_EQ(1, 0) << "An expected failure",
                       "An expected failure");

  GTEST_ASSERT_NE(0, 1);
  GTEST_ASSERT_NE(1, 0);
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_NE(0, 0) << "An expected failure",
                       "An expected failure");

  GTEST_ASSERT_LE(0, 0);
  GTEST_ASSERT_LE(0, 1);
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_LE(1, 0) << "An expected failure",
                       "An expected failure");

  GTEST_ASSERT_LT(0, 1);
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_LT(0, 0) << "An expected failure",
                       "An expected failure");
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_LT(1, 0) << "An expected failure",
                       "An expected failure");

  GTEST_ASSERT_GE(0, 0);
  GTEST_ASSERT_GE(1, 0);
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_GE(0, 1) << "An expected failure",
                       "An expected failure");

  GTEST_ASSERT_GT(1, 0);
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_GT(0, 1) << "An expected failure",
                       "An expected failure");
  EXPECT_FATAL_FAILURE(GTEST_ASSERT_GT(1, 1) << "An expected failure",
                       "An expected failure");
}

class ConversionHelperBase {};
class ConversionHelperDerived : public ConversionHelperBase {};

struct HasDebugStringMethods {
  std::string DebugString() const { return ""; }
  std::string ShortDebugString() const { return ""; }
};

struct InheritsDebugStringMethods : public HasDebugStringMethods {};

struct WrongTypeDebugStringMethod {
  std::string DebugString() const { return ""; }
  int ShortDebugString() const { return 1; }
};

struct NotConstDebugStringMethod {
  std::string DebugString() { return ""; }
  std::string ShortDebugString() const { return ""; }
};

struct MissingDebugStringMethod {
  std::string DebugString() { return ""; }
};

struct IncompleteType;

TEST(HasDebugStringAndShortDebugStringTest, ValueIsCompileTimeConstant) {
  static_assert(HasDebugStringAndShortDebugString<HasDebugStringMethods>::value,
                "const_true");
  static_assert(
      HasDebugStringAndShortDebugString<InheritsDebugStringMethods>::value,
      "const_true");
  static_assert(HasDebugStringAndShortDebugString<
                    const InheritsDebugStringMethods>::value,
                "const_true");
  static_assert(
      !HasDebugStringAndShortDebugString<WrongTypeDebugStringMethod>::value,
      "const_false");
  static_assert(
      !HasDebugStringAndShortDebugString<NotConstDebugStringMethod>::value,
      "const_false");
  static_assert(
      !HasDebugStringAndShortDebugString<MissingDebugStringMethod>::value,
      "const_false");
  static_assert(!HasDebugStringAndShortDebugString<IncompleteType>::value,
                "const_false");
  static_assert(!HasDebugStringAndShortDebugString<int>::value, "const_false");
}

TEST(HasDebugStringAndShortDebugStringTest,
     ValueIsTrueWhenTypeHasDebugStringAndShortDebugString) {
  EXPECT_TRUE(
      HasDebugStringAndShortDebugString<InheritsDebugStringMethods>::value);
}

TEST(HasDebugStringAndShortDebugStringTest,
     ValueIsFalseWhenTypeIsNotAProtocolMessage) {
  EXPECT_FALSE(HasDebugStringAndShortDebugString<int>::value);
  EXPECT_FALSE(
      HasDebugStringAndShortDebugString<const ConversionHelperBase>::value);
}

template <typename T1, typename T2>
void TestGTestRemoveReferenceAndConst() {
  static_assert(std::is_same<T1, GTEST_REMOVE_REFERENCE_AND_CONST_(T2)>::value,
                "GTEST_REMOVE_REFERENCE_AND_CONST_ failed.");
}

TEST(RemoveReferenceToConstTest, Works) {
  TestGTestRemoveReferenceAndConst<int, int>();
  TestGTestRemoveReferenceAndConst<double, double&>();
  TestGTestRemoveReferenceAndConst<char, const char>();
  TestGTestRemoveReferenceAndConst<char, const char&>();
  TestGTestRemoveReferenceAndConst<const char*, const char*>();
}

template <typename T1, typename T2>
void TestGTestReferenceToConst() {
  static_assert(std::is_same<T1, GTEST_REFERENCE_TO_CONST_(T2)>::value,
                "GTEST_REFERENCE_TO_CONST_ failed.");
}

TEST(GTestReferenceToConstTest, Works) {
  TestGTestReferenceToConst<const char&, char>();
  TestGTestReferenceToConst<const int&, const int>();
  TestGTestReferenceToConst<const double&, double>();
  TestGTestReferenceToConst<const std::string&, const std::string&>();
}

class NonContainer {};

TEST(IsContainerTestTest, WorksForNonContainer) {
  EXPECT_EQ(sizeof(IsNotContainer), sizeof(IsContainerTest<int>(0)));
  EXPECT_EQ(sizeof(IsNotContainer), sizeof(IsContainerTest<char[5]>(0)));
  EXPECT_EQ(sizeof(IsNotContainer), sizeof(IsContainerTest<NonContainer>(0)));
}

TEST(IsContainerTestTest, WorksForContainer) {
  EXPECT_EQ(sizeof(IsContainer), sizeof(IsContainerTest<std::vector<bool>>(0)));
  EXPECT_EQ(sizeof(IsContainer),
            sizeof(IsContainerTest<std::map<int, double>>(0)));
}

struct ConstOnlyContainerWithPointerIterator {
  using const_iterator = int*;
  const_iterator begin() const;
  const_iterator end() const;
};

struct ConstOnlyContainerWithClassIterator {
  struct const_iterator {
    const int& operator*() const;
    const_iterator& operator++(                   );
  };
  const_iterator begin() const;
  const_iterator end() const;
};

TEST(IsContainerTestTest, ConstOnlyContainer) {
  EXPECT_EQ(sizeof(IsContainer),
            sizeof(IsContainerTest<ConstOnlyContainerWithPointerIterator>(0)));
  EXPECT_EQ(sizeof(IsContainer),
            sizeof(IsContainerTest<ConstOnlyContainerWithClassIterator>(0)));
}

struct AHashTable {
  typedef void hasher;
};
struct NotReallyAHashTable {
  typedef void hasher;
  typedef void reverse_iterator;
};
TEST(IsHashTable, Basic) {
  EXPECT_TRUE(testing::internal::IsHashTable<AHashTable>::value);
  EXPECT_FALSE(testing::internal::IsHashTable<NotReallyAHashTable>::value);
  EXPECT_FALSE(testing::internal::IsHashTable<std::vector<int>>::value);
  EXPECT_TRUE(testing::internal::IsHashTable<std::unordered_set<int>>::value);
}

TEST(ArrayEqTest, WorksForDegeneratedArrays) {
  EXPECT_TRUE(ArrayEq(5, 5L));
  EXPECT_FALSE(ArrayEq('a', 0));
}

TEST(ArrayEqTest, WorksForOneDimensionalArrays) {

  const int a[] = {0, 1};
  long b[] = {0, 1};
  EXPECT_TRUE(ArrayEq(a, b));
  EXPECT_TRUE(ArrayEq(a, 2, b));

  b[0] = 2;
  EXPECT_FALSE(ArrayEq(a, b));
  EXPECT_FALSE(ArrayEq(a, 1, b));
}

TEST(ArrayEqTest, WorksForTwoDimensionalArrays) {
  const char a[][3] = {"hi", "lo"};
  const char b[][3] = {"hi", "lo"};
  const char c[][3] = {"hi", "li"};

  EXPECT_TRUE(ArrayEq(a, b));
  EXPECT_TRUE(ArrayEq(a, 2, b));

  EXPECT_FALSE(ArrayEq(a, c));
  EXPECT_FALSE(ArrayEq(a, 2, c));
}

TEST(ArrayAwareFindTest, WorksForOneDimensionalArray) {
  const char a[] = "hello";
  EXPECT_EQ(a + 4, ArrayAwareFind(a, a + 5, 'o'));
  EXPECT_EQ(a + 5, ArrayAwareFind(a, a + 5, 'x'));
}

TEST(ArrayAwareFindTest, WorksForTwoDimensionalArray) {
  int a[][2] = {{0, 1}, {2, 3}, {4, 5}};
  const int b[2] = {2, 3};
  EXPECT_EQ(a + 1, ArrayAwareFind(a, a + 3, b));

  const int c[2] = {6, 7};
  EXPECT_EQ(a + 3, ArrayAwareFind(a, a + 3, c));
}

TEST(CopyArrayTest, WorksForDegeneratedArrays) {
  int n = 0;
  CopyArray('a', &n);
  EXPECT_EQ('a', n);
}

TEST(CopyArrayTest, WorksForOneDimensionalArrays) {
  const char a[3] = "hi";
  int b[3];
#ifndef __BORLANDC__
  CopyArray(a, &b);
  EXPECT_TRUE(ArrayEq(a, b));
#endif

  int c[3];
  CopyArray(a, 3, c);
  EXPECT_TRUE(ArrayEq(a, c));
}

TEST(CopyArrayTest, WorksForTwoDimensionalArrays) {
  const int a[2][3] = {{0, 1, 2}, {3, 4, 5}};
  int b[2][3];
#ifndef __BORLANDC__
  CopyArray(a, &b);
  EXPECT_TRUE(ArrayEq(a, b));
#endif

  int c[2][3];
  CopyArray(a, 2, c);
  EXPECT_TRUE(ArrayEq(a, c));
}

TEST(NativeArrayTest, ConstructorFromArrayWorks) {
  const int a[3] = {0, 1, 2};
  NativeArray<int> na(a, 3, RelationToSourceReference());
  EXPECT_EQ(3U, na.size());
  EXPECT_EQ(a, na.begin());
}

TEST(NativeArrayTest, CreatesAndDeletesCopyOfArrayWhenAskedTo) {
  typedef int Array[2];
  Array* a = new Array[1];
  (*a)[0] = 0;
  (*a)[1] = 1;
  NativeArray<int> na(*a, 2, RelationToSourceCopy());
  EXPECT_NE(*a, na.begin());
  delete[] a;
  EXPECT_EQ(0, na.begin()[0]);
  EXPECT_EQ(1, na.begin()[1]);

}

TEST(NativeArrayTest, TypeMembersAreCorrect) {
  StaticAssertTypeEq<char, NativeArray<char>::value_type>();
  StaticAssertTypeEq<int[2], NativeArray<int[2]>::value_type>();

  StaticAssertTypeEq<const char*, NativeArray<char>::const_iterator>();
  StaticAssertTypeEq<const bool(*)[2], NativeArray<bool[2]>::const_iterator>();
}

TEST(NativeArrayTest, MethodsWork) {
  const int a[3] = {0, 1, 2};
  NativeArray<int> na(a, 3, RelationToSourceCopy());
  ASSERT_EQ(3U, na.size());
  EXPECT_EQ(3, na.end() - na.begin());

  NativeArray<int>::const_iterator it = na.begin();
  EXPECT_EQ(0, *it);
  ++it;
  EXPECT_EQ(1, *it);
  it++;
  EXPECT_EQ(2, *it);
  ++it;
  EXPECT_EQ(na.end(), it);

  EXPECT_TRUE(na == na);

  NativeArray<int> na2(a, 3, RelationToSourceReference());
  EXPECT_TRUE(na == na2);

  const int b1[3] = {0, 1, 1};
  const int b2[4] = {0, 1, 2, 3};
  EXPECT_FALSE(na == NativeArray<int>(b1, 3, RelationToSourceReference()));
  EXPECT_FALSE(na == NativeArray<int>(b2, 4, RelationToSourceCopy()));
}

TEST(NativeArrayTest, WorksForTwoDimensionalArray) {
  const char a[2][3] = {"hi", "lo"};
  NativeArray<char[3]> na(a, 2, RelationToSourceReference());
  ASSERT_EQ(2U, na.size());
  EXPECT_EQ(a, na.begin());
}

TEST(ElemFromList, Basic) {
  using testing::internal::ElemFromList;
  EXPECT_TRUE(
      (std::is_same<int, ElemFromList<0, int, double, char>::type>::value));
  EXPECT_TRUE(
      (std::is_same<double, ElemFromList<1, int, double, char>::type>::value));
  EXPECT_TRUE(
      (std::is_same<char, ElemFromList<2, int, double, char>::type>::value));
  EXPECT_TRUE((
      std::is_same<char, ElemFromList<7, int, int, int, int, int, int, int,
                                      char, int, int, int, int>::type>::value));
}

TEST(FlatTuple, Basic) {
  using testing::internal::FlatTuple;

  FlatTuple<int, double, const char*> tuple = {};
  EXPECT_EQ(0, tuple.Get<0>());
  EXPECT_EQ(0.0, tuple.Get<1>());
  EXPECT_EQ(nullptr, tuple.Get<2>());

  tuple = FlatTuple<int, double, const char*>(
      testing::internal::FlatTupleConstructTag{}, 7, 3.2, "Foo");
  EXPECT_EQ(7, tuple.Get<0>());
  EXPECT_EQ(3.2, tuple.Get<1>());
  EXPECT_EQ(std::string("Foo"), tuple.Get<2>());

  tuple.Get<1>() = 5.1;
  EXPECT_EQ(5.1, tuple.Get<1>());
}

namespace {
std::string AddIntToString(int i, const std::string& s) {
  return s + std::to_string(i);
}
}

TEST(FlatTuple, Apply) {
  using testing::internal::FlatTuple;

  FlatTuple<int, std::string> tuple{testing::internal::FlatTupleConstructTag{},
                                    5, "Hello"};

  EXPECT_TRUE(tuple.Apply([](int i, const std::string& s) -> bool {
    return i == static_cast<int>(s.size());
  }));

  EXPECT_EQ(tuple.Apply(AddIntToString), "Hello5");

  tuple.Apply([](int& i, std::string& s) {
    ++i;
    s += s;
  });
  EXPECT_EQ(tuple.Get<0>(), 6);
  EXPECT_EQ(tuple.Get<1>(), "HelloHello");
}

struct ConstructionCounting {
  ConstructionCounting() { ++default_ctor_calls; }
  ~ConstructionCounting() { ++dtor_calls; }
  ConstructionCounting(const ConstructionCounting&) { ++copy_ctor_calls; }
  ConstructionCounting(ConstructionCounting&&) noexcept { ++move_ctor_calls; }
  ConstructionCounting& operator=(const ConstructionCounting&) {
    ++copy_assignment_calls;
    return *this;
  }
  ConstructionCounting& operator=(ConstructionCounting&&) noexcept {
    ++move_assignment_calls;
    return *this;
  }

  static void Reset() {
    default_ctor_calls = 0;
    dtor_calls = 0;
    copy_ctor_calls = 0;
    move_ctor_calls = 0;
    copy_assignment_calls = 0;
    move_assignment_calls = 0;
  }

  static int default_ctor_calls;
  static int dtor_calls;
  static int copy_ctor_calls;
  static int move_ctor_calls;
  static int copy_assignment_calls;
  static int move_assignment_calls;
};

int ConstructionCounting::default_ctor_calls = 0;
int ConstructionCounting::dtor_calls = 0;
int ConstructionCounting::copy_ctor_calls = 0;
int ConstructionCounting::move_ctor_calls = 0;
int ConstructionCounting::copy_assignment_calls = 0;
int ConstructionCounting::move_assignment_calls = 0;

TEST(FlatTuple, ConstructorCalls) {
  using testing::internal::FlatTuple;

  ConstructionCounting::Reset();
  { FlatTuple<ConstructionCounting> tuple; }
  EXPECT_EQ(ConstructionCounting::default_ctor_calls, 1);
  EXPECT_EQ(ConstructionCounting::dtor_calls, 1);
  EXPECT_EQ(ConstructionCounting::copy_ctor_calls, 0);
  EXPECT_EQ(ConstructionCounting::move_ctor_calls, 0);
  EXPECT_EQ(ConstructionCounting::copy_assignment_calls, 0);
  EXPECT_EQ(ConstructionCounting::move_assignment_calls, 0);

  ConstructionCounting::Reset();
  {
    ConstructionCounting elem;
    FlatTuple<ConstructionCounting> tuple{
        testing::internal::FlatTupleConstructTag{}, elem};
  }
  EXPECT_EQ(ConstructionCounting::default_ctor_calls, 1);
  EXPECT_EQ(ConstructionCounting::dtor_calls, 2);
  EXPECT_EQ(ConstructionCounting::copy_ctor_calls, 1);
  EXPECT_EQ(ConstructionCounting::move_ctor_calls, 0);
  EXPECT_EQ(ConstructionCounting::copy_assignment_calls, 0);
  EXPECT_EQ(ConstructionCounting::move_assignment_calls, 0);

  ConstructionCounting::Reset();
  {
    FlatTuple<ConstructionCounting> tuple{
        testing::internal::FlatTupleConstructTag{}, ConstructionCounting{}};
  }
  EXPECT_EQ(ConstructionCounting::default_ctor_calls, 1);
  EXPECT_EQ(ConstructionCounting::dtor_calls, 2);
  EXPECT_EQ(ConstructionCounting::copy_ctor_calls, 0);
  EXPECT_EQ(ConstructionCounting::move_ctor_calls, 1);
  EXPECT_EQ(ConstructionCounting::copy_assignment_calls, 0);
  EXPECT_EQ(ConstructionCounting::move_assignment_calls, 0);

  ConstructionCounting::Reset();
  {
    FlatTuple<ConstructionCounting> tuple;
    ConstructionCounting elem;
    tuple.Get<0>() = elem;
  }
  EXPECT_EQ(ConstructionCounting::default_ctor_calls, 2);
  EXPECT_EQ(ConstructionCounting::dtor_calls, 2);
  EXPECT_EQ(ConstructionCounting::copy_ctor_calls, 0);
  EXPECT_EQ(ConstructionCounting::move_ctor_calls, 0);
  EXPECT_EQ(ConstructionCounting::copy_assignment_calls, 1);
  EXPECT_EQ(ConstructionCounting::move_assignment_calls, 0);

  ConstructionCounting::Reset();
  {
    FlatTuple<ConstructionCounting> tuple;
    tuple.Get<0>() = ConstructionCounting{};
  }
  EXPECT_EQ(ConstructionCounting::default_ctor_calls, 2);
  EXPECT_EQ(ConstructionCounting::dtor_calls, 2);
  EXPECT_EQ(ConstructionCounting::copy_ctor_calls, 0);
  EXPECT_EQ(ConstructionCounting::move_ctor_calls, 0);
  EXPECT_EQ(ConstructionCounting::copy_assignment_calls, 0);
  EXPECT_EQ(ConstructionCounting::move_assignment_calls, 1);

  ConstructionCounting::Reset();
}

TEST(FlatTuple, ManyTypes) {
  using testing::internal::FlatTuple;

#define GTEST_FLAT_TUPLE_INT8 int, int, int, int, int, int, int, int,
#define GTEST_FLAT_TUPLE_INT16 GTEST_FLAT_TUPLE_INT8 GTEST_FLAT_TUPLE_INT8
#define GTEST_FLAT_TUPLE_INT32 GTEST_FLAT_TUPLE_INT16 GTEST_FLAT_TUPLE_INT16
#define GTEST_FLAT_TUPLE_INT64 GTEST_FLAT_TUPLE_INT32 GTEST_FLAT_TUPLE_INT32
#define GTEST_FLAT_TUPLE_INT128 GTEST_FLAT_TUPLE_INT64 GTEST_FLAT_TUPLE_INT64
#define GTEST_FLAT_TUPLE_INT256 GTEST_FLAT_TUPLE_INT128 GTEST_FLAT_TUPLE_INT128

  FlatTuple<GTEST_FLAT_TUPLE_INT256 int> tuple;

  tuple.Get<0>() = 7;
  tuple.Get<99>() = 17;
  tuple.Get<256>() = 1000;
  EXPECT_EQ(7, tuple.Get<0>());
  EXPECT_EQ(17, tuple.Get<99>());
  EXPECT_EQ(1000, tuple.Get<256>());
}

TEST(SkipPrefixTest, SkipsWhenPrefixMatches) {
  const char* const str = "hello";

  const char* p = str;
  EXPECT_TRUE(SkipPrefix("", &p));
  EXPECT_EQ(str, p);

  p = str;
  EXPECT_TRUE(SkipPrefix("hell", &p));
  EXPECT_EQ(str + 4, p);
}

TEST(SkipPrefixTest, DoesNotSkipWhenPrefixDoesNotMatch) {
  const char* const str = "world";

  const char* p = str;
  EXPECT_FALSE(SkipPrefix("W", &p));
  EXPECT_EQ(str, p);

  p = str;
  EXPECT_FALSE(SkipPrefix("world!", &p));
  EXPECT_EQ(str, p);
}

TEST(AdHocTestResultTest, AdHocTestResultForUnitTestDoesNotShowFailure) {
  const testing::TestResult& test_result =
      testing::UnitTest::GetInstance()->ad_hoc_test_result();
  EXPECT_FALSE(test_result.Failed());
}

class DynamicUnitTestFixture : public testing::Test {};

class DynamicTest : public DynamicUnitTestFixture {
  void TestBody() override { EXPECT_TRUE(true); }
};

auto* dynamic_test = testing::RegisterTest(
    "DynamicUnitTestFixture", "DynamicTest", "TYPE", "VALUE", __FILE__,
    __LINE__, []() -> DynamicUnitTestFixture* { return new DynamicTest; });

TEST(RegisterTest, WasRegistered) {
  const auto& unittest = testing::UnitTest::GetInstance();
  for (int i = 0; i < unittest->total_test_suite_count(); ++i) {
    auto* tests = unittest->GetTestSuite(i);
    if (tests->name() != std::string("DynamicUnitTestFixture")) continue;
    for (int j = 0; j < tests->total_test_count(); ++j) {
      if (tests->GetTestInfo(j)->name() != std::string("DynamicTest")) continue;

      EXPECT_STREQ(tests->GetTestInfo(j)->value_param(), "VALUE");
      EXPECT_STREQ(tests->GetTestInfo(j)->type_param(), "TYPE");
      return;
    }
  }

  FAIL() << "Didn't find the test!";
}

TEST(PatternGlobbingTest, MatchesFilterLinearRuntime) {
  std::string name(100, 'a');
  name.push_back('b');

  std::string pattern;
  for (int i = 0; i < 100; ++i) {
    pattern.append("a*");
  }
  pattern.push_back('b');

  EXPECT_TRUE(
      testing::internal::UnitTestOptions::MatchesFilter(name, pattern.c_str()));
}

TEST(PatternGlobbingTest, MatchesFilterWithMultiplePatterns) {
  const std::string name = "aaaa";
  EXPECT_TRUE(testing::internal::UnitTestOptions::MatchesFilter(name, "a*"));
  EXPECT_TRUE(testing::internal::UnitTestOptions::MatchesFilter(name, "a*:"));
  EXPECT_FALSE(testing::internal::UnitTestOptions::MatchesFilter(name, "ab"));
  EXPECT_FALSE(testing::internal::UnitTestOptions::MatchesFilter(name, "ab:"));
  EXPECT_TRUE(testing::internal::UnitTestOptions::MatchesFilter(name, "ab:a*"));
}

TEST(PatternGlobbingTest, MatchesFilterEdgeCases) {
  EXPECT_FALSE(testing::internal::UnitTestOptions::MatchesFilter("", "*a"));
  EXPECT_TRUE(testing::internal::UnitTestOptions::MatchesFilter("", "*"));
  EXPECT_FALSE(testing::internal::UnitTestOptions::MatchesFilter("a", ""));
  EXPECT_TRUE(testing::internal::UnitTestOptions::MatchesFilter("", ""));
}
