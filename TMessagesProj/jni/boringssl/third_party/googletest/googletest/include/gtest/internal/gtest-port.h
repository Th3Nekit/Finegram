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

// Low-level types and utilities for porting Google Test to various
// platforms.  All macros ending with _ and symbols defined in an
// internal namespace are subject to change without notice.  Code
// outside Google Test MUST NOT USE THEM DIRECTLY.  Macros that don't
// end with _ are part of Google Test's public API and can be used by
// code outside Google Test.
//
// This file is fundamental to Google Test.  All other Google Test source
// files are expected to #include this.  Therefore, it cannot #include
// any other Google Test header.

// IWYU pragma: private, include "gtest/gtest.h"
// IWYU pragma: friend gtest/.*
// IWYU pragma: friend gmock/.*

#ifndef GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_PORT_H_
#define GOOGLETEST_INCLUDE_GTEST_INTERNAL_GTEST_PORT_H_

#if defined(_MSVC_LANG)
#define GTEST_INTERNAL_CPLUSPLUS_LANG _MSVC_LANG
#elif defined(__cplusplus)
#define GTEST_INTERNAL_CPLUSPLUS_LANG __cplusplus
#endif

#if !defined(GTEST_INTERNAL_CPLUSPLUS_LANG) || \
    GTEST_INTERNAL_CPLUSPLUS_LANG < 201703L
#error C++ versions less than C++17 are not supported.
#endif

#ifdef __has_include
#define GTEST_INTERNAL_HAS_INCLUDE __has_include
#else
#define GTEST_INTERNAL_HAS_INCLUDE(...) 0
#endif

#if GTEST_INTERNAL_CPLUSPLUS_LANG >= 202002L && \
    (!defined(__has_include) || GTEST_INTERNAL_HAS_INCLUDE(<version>))
#include <version>
#elif (!defined(__has_include) || GTEST_INTERNAL_HAS_INCLUDE(<ciso646>))
#include <ciso646>
#endif

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cerrno>

#include <cstdint>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <ostream>
#include <string>

#include <tuple>
#include <type_traits>
#include <vector>

#ifndef _WIN32_WCE
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined __APPLE__
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#endif

#include "gtest/internal/custom/gtest-port.h"
#include "gtest/internal/gtest-port-arch.h"

#ifndef GTEST_HAS_MUTEX_AND_THREAD_LOCAL_
#define GTEST_HAS_MUTEX_AND_THREAD_LOCAL_ 0
#endif

#ifndef GTEST_HAS_NOTIFICATION_
#define GTEST_HAS_NOTIFICATION_ 0
#endif

#if defined(GTEST_HAS_ABSL) && !defined(GTEST_NO_ABSL_FLAGS)
#define GTEST_INTERNAL_HAS_ABSL_FLAGS
#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/reflection.h"
#endif

#if !defined(GTEST_DEV_EMAIL_)
#define GTEST_DEV_EMAIL_ "googletestframework@@googlegroups.com"
#define GTEST_FLAG_PREFIX_ "gtest_"
#define GTEST_FLAG_PREFIX_DASH_ "gtest-"
#define GTEST_FLAG_PREFIX_UPPER_ "GTEST_"
#define GTEST_NAME_ "Google Test"
#define GTEST_PROJECT_URL_ "https://github.com/google/googletest/"
#endif

#if !defined(GTEST_INIT_GOOGLE_TEST_NAME_)
#define GTEST_INIT_GOOGLE_TEST_NAME_ "testing::InitGoogleTest"
#endif

#ifdef __GNUC__

#define GTEST_GCC_VER_ \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#if defined(_MSC_VER)
#define GTEST_DISABLE_MSC_WARNINGS_PUSH_(warnings) \
  __pragma(warning(push)) __pragma(warning(disable : warnings))
#define GTEST_DISABLE_MSC_WARNINGS_POP_() __pragma(warning(pop))
#else

#define GTEST_DISABLE_MSC_WARNINGS_PUSH_(warnings)
#define GTEST_DISABLE_MSC_WARNINGS_POP_()
#endif

#ifdef __clang__
#define GTEST_DISABLE_MSC_DEPRECATED_PUSH_()                            \
  _Pragma("clang diagnostic push")                                      \
      _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"") \
          _Pragma("clang diagnostic ignored \"-Wdeprecated-implementations\"")
#define GTEST_DISABLE_MSC_DEPRECATED_POP_() _Pragma("clang diagnostic pop")
#else
#define GTEST_DISABLE_MSC_DEPRECATED_PUSH_() \
  GTEST_DISABLE_MSC_WARNINGS_PUSH_(4996)
#define GTEST_DISABLE_MSC_DEPRECATED_POP_() GTEST_DISABLE_MSC_WARNINGS_POP_()
#endif

#ifdef GTEST_OS_WINDOWS
#ifndef GTEST_OS_WINDOWS_MOBILE
#include <direct.h>
#include <io.h>
#endif

#if defined(GTEST_OS_WINDOWS_MINGW) && !defined(__MINGW64_VERSION_MAJOR)

typedef struct _CRITICAL_SECTION GTEST_CRITICAL_SECTION;
#else

typedef struct _RTL_CRITICAL_SECTION GTEST_CRITICAL_SECTION;
#endif
#elif defined(GTEST_OS_XTENSA)
#include <unistd.h>

#else

#include <strings.h>
#include <unistd.h>
#endif

#ifdef GTEST_OS_LINUX_ANDROID

#include <android/api-level.h>
#endif

#ifndef GTEST_HAS_POSIX_RE
#ifdef GTEST_OS_LINUX_ANDROID

#define GTEST_HAS_POSIX_RE (__ANDROID_API__ >= 9)
#else
#if !(defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_XTENSA) || \
      defined(GTEST_OS_QURT))
#define GTEST_HAS_POSIX_RE 1
#else
#define GTEST_HAS_POSIX_RE 0
#endif
#endif
#endif

#ifdef GTEST_HAS_ABSL

#include "absl/strings/string_view.h"
#include "re2/re2.h"
#define GTEST_USES_RE2 1
#elif GTEST_HAS_POSIX_RE
#include <regex.h>
#define GTEST_USES_POSIX_RE 1
#else

#define GTEST_USES_SIMPLE_RE 1
#endif

#ifndef GTEST_HAS_EXCEPTIONS

#if defined(_MSC_VER) && defined(_CPPUNWIND)

#define GTEST_HAS_EXCEPTIONS 1
#elif defined(__BORLANDC__)

#ifndef _HAS_EXCEPTIONS
#define _HAS_EXCEPTIONS 1
#endif
#define GTEST_HAS_EXCEPTIONS _HAS_EXCEPTIONS
#elif defined(__clang__)

#if defined(__EXCEPTIONS) && __EXCEPTIONS && __has_feature(cxx_exceptions)
#define GTEST_HAS_EXCEPTIONS 1
#else
#define GTEST_HAS_EXCEPTIONS 0
#endif
#elif defined(__GNUC__) && defined(__EXCEPTIONS) && __EXCEPTIONS

#define GTEST_HAS_EXCEPTIONS 1
#elif defined(__SUNPRO_CC)

#define GTEST_HAS_EXCEPTIONS 1
#elif defined(__IBMCPP__) && defined(__EXCEPTIONS) && __EXCEPTIONS

#define GTEST_HAS_EXCEPTIONS 1
#elif defined(__HP_aCC)

#define GTEST_HAS_EXCEPTIONS 1
#else

#define GTEST_HAS_EXCEPTIONS 0
#endif
#endif

#ifndef GTEST_HAS_STD_WSTRING

#if (!(defined(GTEST_OS_LINUX_ANDROID) || defined(GTEST_OS_CYGWIN) || \
       defined(GTEST_OS_SOLARIS) || defined(GTEST_OS_HAIKU) ||        \
       defined(GTEST_OS_ESP32) || defined(GTEST_OS_ESP8266) ||        \
       defined(GTEST_OS_XTENSA) || defined(GTEST_OS_QURT) ||          \
       defined(GTEST_OS_NXP_QN9090) || defined(GTEST_OS_NRF52)))
#define GTEST_HAS_STD_WSTRING 1
#else
#define GTEST_HAS_STD_WSTRING 0
#endif
#endif

#ifndef GTEST_HAS_FILE_SYSTEM

#define GTEST_HAS_FILE_SYSTEM 1
#endif

#ifndef GTEST_HAS_RTTI

#ifdef _MSC_VER

#ifdef _CPPRTTI
#define GTEST_HAS_RTTI 1
#else
#define GTEST_HAS_RTTI 0
#endif

#elif defined(__GNUC__)

#ifdef __GXX_RTTI

#if defined(GTEST_OS_LINUX_ANDROID) && defined(_STLPORT_MAJOR) && \
    !defined(__EXCEPTIONS)
#define GTEST_HAS_RTTI 0
#else
#define GTEST_HAS_RTTI 1
#endif
#else
#define GTEST_HAS_RTTI 0
#endif

#elif defined(__clang__)

#define GTEST_HAS_RTTI __has_feature(cxx_rtti)

#elif defined(__IBMCPP__) && (__IBMCPP__ >= 900)

#ifdef __RTTI_ALL__
#define GTEST_HAS_RTTI 1
#else
#define GTEST_HAS_RTTI 0
#endif

#else

#define GTEST_HAS_RTTI 1

#endif

#endif

#if GTEST_HAS_RTTI
#include <typeinfo>
#endif

#ifndef GTEST_HAS_PTHREAD

#if (defined(GTEST_OS_LINUX) || defined(GTEST_OS_MAC) ||              \
     defined(GTEST_OS_HPUX) || defined(GTEST_OS_QNX) ||               \
     defined(GTEST_OS_FREEBSD) || defined(GTEST_OS_NACL) ||           \
     defined(GTEST_OS_NETBSD) || defined(GTEST_OS_FUCHSIA) ||         \
     defined(GTEST_OS_DRAGONFLY) || defined(GTEST_OS_GNU_KFREEBSD) || \
     defined(GTEST_OS_OPENBSD) || defined(GTEST_OS_HAIKU) ||          \
     defined(GTEST_OS_GNU_HURD) || defined(GTEST_OS_SOLARIS) ||       \
     defined(GTEST_OS_AIX) || defined(GTEST_OS_ZOS))
#define GTEST_HAS_PTHREAD 1
#else
#define GTEST_HAS_PTHREAD 0
#endif
#endif

#if GTEST_HAS_PTHREAD

#include <pthread.h>

#include <time.h>
#endif

#ifndef GTEST_HAS_CLONE

#if defined(GTEST_OS_LINUX) && !defined(__ia64__)
#if defined(GTEST_OS_LINUX_ANDROID)

#if defined(__LP64__) || (defined(__arm__) && __ANDROID_API__ >= 9) || \
    (defined(__mips__) && __ANDROID_API__ >= 12) ||                    \
    (defined(__i386__) && __ANDROID_API__ >= 17)
#define GTEST_HAS_CLONE 1
#else
#define GTEST_HAS_CLONE 0
#endif
#else
#define GTEST_HAS_CLONE 1
#endif
#else
#define GTEST_HAS_CLONE 0
#endif

#endif

#ifndef GTEST_HAS_STREAM_REDIRECTION

#if defined(GTEST_OS_WINDOWS_MOBILE) || defined(GTEST_OS_WINDOWS_PHONE) || \
    defined(GTEST_OS_WINDOWS_RT) || defined(GTEST_OS_WINDOWS_GAMES) ||     \
    defined(GTEST_OS_ESP8266) || defined(GTEST_OS_XTENSA) ||               \
    defined(GTEST_OS_QURT) || !GTEST_HAS_FILE_SYSTEM
#define GTEST_HAS_STREAM_REDIRECTION 0
#else
#define GTEST_HAS_STREAM_REDIRECTION 1
#endif
#endif

#if (defined(GTEST_OS_LINUX) || defined(GTEST_OS_CYGWIN) ||           \
     defined(GTEST_OS_SOLARIS) || defined(GTEST_OS_ZOS) ||            \
     (defined(GTEST_OS_MAC) && !defined(GTEST_OS_IOS)) ||             \
     (defined(GTEST_OS_WINDOWS_DESKTOP) && _MSC_VER) ||               \
     defined(GTEST_OS_WINDOWS_MINGW) || defined(GTEST_OS_AIX) ||      \
     defined(GTEST_OS_HPUX) || defined(GTEST_OS_OPENBSD) ||           \
     defined(GTEST_OS_QNX) || defined(GTEST_OS_FREEBSD) ||            \
     defined(GTEST_OS_NETBSD) || defined(GTEST_OS_FUCHSIA) ||         \
     defined(GTEST_OS_DRAGONFLY) || defined(GTEST_OS_GNU_KFREEBSD) || \
     defined(GTEST_OS_HAIKU) || defined(GTEST_OS_GNU_HURD))

#if GTEST_HAS_FILE_SYSTEM
#define GTEST_HAS_DEATH_TEST 1
#endif
#endif

#if defined(__GNUC__) || defined(_MSC_VER) || defined(__SUNPRO_CC) || \
    defined(__IBMCPP__) || defined(__HP_aCC)
#define GTEST_HAS_TYPED_TEST 1
#define GTEST_HAS_TYPED_TEST_P 1
#endif

#if defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_CYGWIN) || \
    defined(GTEST_OS_AIX) || defined(GTEST_OS_OS2)
#define GTEST_WIDE_STRING_USES_UTF16_ 1
#else
#define GTEST_WIDE_STRING_USES_UTF16_ 0
#endif

#if defined(GTEST_OS_LINUX) || defined(GTEST_OS_GNU_KFREEBSD) || \
    defined(GTEST_OS_DRAGONFLY) || defined(GTEST_OS_FREEBSD) ||  \
    defined(GTEST_OS_NETBSD) || defined(GTEST_OS_OPENBSD) ||     \
    defined(GTEST_OS_GNU_HURD) || defined(GTEST_OS_MAC)
#define GTEST_CAN_STREAM_RESULTS_ 1
#else
#define GTEST_CAN_STREAM_RESULTS_ 0
#endif

#ifdef __INTEL_COMPILER
#define GTEST_AMBIGUOUS_ELSE_BLOCKER_
#else
#define GTEST_AMBIGUOUS_ELSE_BLOCKER_ \
  switch (0)                          \
  case 0:                             \
  default:  // NOLINT
#endif

#ifdef __has_attribute
#define GTEST_HAVE_ATTRIBUTE_(x) __has_attribute(x)
#else
#define GTEST_HAVE_ATTRIBUTE_(x) 0
#endif

#if defined(__has_cpp_attribute)

#define GTEST_INTERNAL_HAVE_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#define GTEST_INTERNAL_HAVE_CPP_ATTRIBUTE(x) 0
#endif

#ifdef __has_feature
#define GTEST_HAVE_FEATURE_(x) __has_feature(x)
#else
#define GTEST_HAVE_FEATURE_(x) 0
#endif

#if GTEST_HAVE_ATTRIBUTE_(format) && defined(__MINGW_PRINTF_FORMAT)

#define GTEST_ATTRIBUTE_PRINTF_(string_index, first_to_check) \
  __attribute__((format(__MINGW_PRINTF_FORMAT, string_index, first_to_check)))
#elif GTEST_HAVE_ATTRIBUTE_(format)
#define GTEST_ATTRIBUTE_PRINTF_(string_index, first_to_check) \
  __attribute__((format(printf, string_index, first_to_check)))
#else
#define GTEST_ATTRIBUTE_PRINTF_(string_index, first_to_check)
#endif

#define GTEST_INTENTIONAL_CONST_COND_PUSH_() \
  GTEST_DISABLE_MSC_WARNINGS_PUSH_(4127)
#define GTEST_INTENTIONAL_CONST_COND_POP_() GTEST_DISABLE_MSC_WARNINGS_POP_()

#ifndef GTEST_HAS_SEH

#if defined(_MSC_VER) || defined(__BORLANDC__)

#define GTEST_HAS_SEH 1
#else

#define GTEST_HAS_SEH 0
#endif

#endif

#ifndef GTEST_IS_THREADSAFE

#if (GTEST_HAS_MUTEX_AND_THREAD_LOCAL_ ||                              \
     (defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_PHONE) && \
      !defined(GTEST_OS_WINDOWS_RT)) ||                                \
     GTEST_HAS_PTHREAD)
#define GTEST_IS_THREADSAFE 1
#endif

#endif

#ifdef GTEST_IS_THREADSAFE

#include <condition_variable>
#include <mutex>
#endif

#ifndef GTEST_API_

#ifdef _MSC_VER
#if defined(GTEST_LINKED_AS_SHARED_LIBRARY) && GTEST_LINKED_AS_SHARED_LIBRARY
#define GTEST_API_ __declspec(dllimport)
#elif defined(GTEST_CREATE_SHARED_LIBRARY) && GTEST_CREATE_SHARED_LIBRARY
#define GTEST_API_ __declspec(dllexport)
#endif
#elif GTEST_HAVE_ATTRIBUTE_(visibility)
#define GTEST_API_ __attribute__((visibility("default")))
#endif

#endif

#ifndef GTEST_API_
#define GTEST_API_
#endif

#ifndef GTEST_DEFAULT_DEATH_TEST_STYLE
#define GTEST_DEFAULT_DEATH_TEST_STYLE "fast"
#endif

#if GTEST_HAVE_ATTRIBUTE_(noinline)

#define GTEST_NO_INLINE_ __attribute__((noinline))
#else
#define GTEST_NO_INLINE_
#endif

#if GTEST_HAVE_ATTRIBUTE_(disable_tail_calls)

#define GTEST_NO_TAIL_CALL_ __attribute__((disable_tail_calls))
#elif defined(__GNUC__) && !defined(__NVCOMPILER)
#define GTEST_NO_TAIL_CALL_ \
  __attribute__((optimize("no-optimize-sibling-calls")))
#else
#define GTEST_NO_TAIL_CALL_
#endif

#if !defined(GTEST_HAS_CXXABI_H_)
#if defined(__GLIBCXX__) || (defined(_LIBCPP_VERSION) && !defined(_MSC_VER))
#define GTEST_HAS_CXXABI_H_ 1
#else
#define GTEST_HAS_CXXABI_H_ 0
#endif
#endif

#if GTEST_HAVE_ATTRIBUTE_(no_sanitize_memory)
#define GTEST_ATTRIBUTE_NO_SANITIZE_MEMORY_ __attribute__((no_sanitize_memory))
#else
#define GTEST_ATTRIBUTE_NO_SANITIZE_MEMORY_
#endif

#if GTEST_HAVE_ATTRIBUTE_(no_sanitize_address)
#define GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_ \
  __attribute__((no_sanitize_address))
#else
#define GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
#endif

#if GTEST_HAVE_FEATURE_(hwaddress_sanitizer) && \
    GTEST_HAVE_ATTRIBUTE_(no_sanitize)
#define GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_ \
  __attribute__((no_sanitize("hwaddress")))
#else
#define GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
#endif

#if GTEST_HAVE_ATTRIBUTE_(no_sanitize_thread)
#define GTEST_ATTRIBUTE_NO_SANITIZE_THREAD_ __attribute((no_sanitize_thread))
#else
#define GTEST_ATTRIBUTE_NO_SANITIZE_THREAD_
#endif

namespace testing {

class Message;

using std::get;
using std::make_tuple;
using std::tuple;
using std::tuple_element;
using std::tuple_size;

namespace internal {

class Secret {
  Secret(const Secret&) = delete;
};

GTEST_API_ bool IsTrue(bool condition);

#ifdef GTEST_USES_RE2

class GTEST_API_ RE {
 public:
  RE(absl::string_view regex) : regex_(regex) {}
  RE(const char* regex) : RE(absl::string_view(regex)) {}
  RE(const std::string& regex) : RE(absl::string_view(regex)) {}
  RE(const RE& other) : RE(other.pattern()) {}

  const std::string& pattern() const { return regex_.pattern(); }

  static bool FullMatch(absl::string_view str, const RE& re) {
    return RE2::FullMatch(str, re.regex_);
  }
  static bool PartialMatch(absl::string_view str, const RE& re) {
    return RE2::PartialMatch(str, re.regex_);
  }

 private:
  RE2 regex_;
};

#elif defined(GTEST_USES_POSIX_RE) || defined(GTEST_USES_SIMPLE_RE)
GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

class GTEST_API_ RE {
 public:

  RE(const RE& other) { Init(other.pattern()); }

  RE(const ::std::string& regex) { Init(regex.c_str()); }

  RE(const char* regex) { Init(regex); }
  ~RE();

  const char* pattern() const { return pattern_.c_str(); }

  static bool FullMatch(const ::std::string& str, const RE& re) {
    return FullMatch(str.c_str(), re);
  }
  static bool PartialMatch(const ::std::string& str, const RE& re) {
    return PartialMatch(str.c_str(), re);
  }

  static bool FullMatch(const char* str, const RE& re);
  static bool PartialMatch(const char* str, const RE& re);

 private:
  void Init(const char* regex);
  std::string pattern_;
  bool is_valid_;

#ifdef GTEST_USES_POSIX_RE

  regex_t full_regex_;
  regex_t partial_regex_;

#else

  std::string full_pattern_;

#endif
};
GTEST_DISABLE_MSC_WARNINGS_POP_()
#endif

GTEST_API_ ::std::string FormatFileLocation(const char* file, int line);

GTEST_API_ ::std::string FormatCompilerIndependentFileLocation(const char* file,
                                                               int line);

enum GTestLogSeverity { GTEST_INFO, GTEST_WARNING, GTEST_ERROR, GTEST_FATAL };

class GTEST_API_ GTestLog {
 public:
  GTestLog(GTestLogSeverity severity, const char* file, int line);

  ~GTestLog();

  ::std::ostream& GetStream() { return ::std::cerr; }

 private:
  const GTestLogSeverity severity_;

  GTestLog(const GTestLog&) = delete;
  GTestLog& operator=(const GTestLog&) = delete;
};

#if !defined(GTEST_LOG_)

#define GTEST_LOG_(severity)                                           \
  ::testing::internal::GTestLog(::testing::internal::GTEST_##severity, \
                                __FILE__, __LINE__)                    \
      .GetStream()

inline void LogToStderr() {}
inline void FlushInfoLog() { fflush(nullptr); }

#endif

#if !defined(GTEST_CHECK_)

#define GTEST_CHECK_(condition)               \
  GTEST_AMBIGUOUS_ELSE_BLOCKER_               \
  if (::testing::internal::IsTrue(condition)) \
    ;                                         \
  else                                        \
    GTEST_LOG_(FATAL) << "Condition " #condition " failed. "
#endif

#define GTEST_CHECK_POSIX_SUCCESS_(posix_call) \
  if (const int gtest_error = (posix_call))    \
  GTEST_LOG_(FATAL) << #posix_call << "failed with error " << gtest_error

template <typename T>
struct ConstRef {
  typedef const T& type;
};
template <typename T>
struct ConstRef<T&> {
  typedef T& type;
};

#define GTEST_REFERENCE_TO_CONST_(T) \
  typename ::testing::internal::ConstRef<T>::type

template <typename To>
inline To ImplicitCast_(To x) {
  return x;
}

template <class Derived, class Base>
Derived* CheckedDowncastToActualType(Base* base) {
  static_assert(std::is_base_of<Base, Derived>::value,
                "target type not derived from source type");
#if GTEST_HAS_RTTI
  GTEST_CHECK_(base == nullptr || dynamic_cast<Derived*>(base) != nullptr);
#endif
  return static_cast<Derived*>(base);
}

#if GTEST_HAS_STREAM_REDIRECTION

GTEST_API_ void CaptureStdout();
GTEST_API_ std::string GetCapturedStdout();
GTEST_API_ void CaptureStderr();
GTEST_API_ std::string GetCapturedStderr();

#endif

GTEST_API_ size_t GetFileSize(FILE* file);

GTEST_API_ std::string ReadEntireFile(FILE* file);

GTEST_API_ std::vector<std::string> GetArgvs();

#ifdef GTEST_HAS_DEATH_TEST

std::vector<std::string> GetInjectableArgvs();

void SetInjectableArgvs(const std::vector<std::string>* new_argvs);
void SetInjectableArgvs(const std::vector<std::string>& new_argvs);
void ClearInjectableArgvs();

#endif

#ifdef GTEST_IS_THREADSAFE

#ifdef GTEST_OS_WINDOWS

class GTEST_API_ AutoHandle {
 public:

  typedef void* Handle;
  AutoHandle();
  explicit AutoHandle(Handle handle);

  ~AutoHandle();

  Handle Get() const;
  void Reset();
  void Reset(Handle handle);

 private:

  bool IsCloseable() const;

  Handle handle_;

  AutoHandle(const AutoHandle&) = delete;
  AutoHandle& operator=(const AutoHandle&) = delete;
};
#endif

#if GTEST_HAS_NOTIFICATION_

#else
GTEST_DISABLE_MSC_WARNINGS_PUSH_(4251 \
                                                                          )

class GTEST_API_ Notification {
 public:
  Notification() : notified_(false) {}
  Notification(const Notification&) = delete;
  Notification& operator=(const Notification&) = delete;

  void Notify() {
    std::lock_guard<std::mutex> lock(mu_);
    notified_ = true;
    cv_.notify_all();
  }

  void WaitForNotification() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [this]() { return notified_; });
  }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  bool notified_;
};
GTEST_DISABLE_MSC_WARNINGS_POP_()
#endif

#if GTEST_HAS_PTHREAD && !defined(GTEST_OS_WINDOWS_MINGW)

class ThreadWithParamBase {
 public:
  virtual ~ThreadWithParamBase() = default;
  virtual void Run() = 0;
};

extern "C" inline void* ThreadFuncWithCLinkage(void* thread) {
  static_cast<ThreadWithParamBase*>(thread)->Run();
  return nullptr;
}

template <typename T>
class ThreadWithParam : public ThreadWithParamBase {
 public:
  typedef void UserThreadFunc(T);

  ThreadWithParam(UserThreadFunc* func, T param, Notification* thread_can_start)
      : func_(func),
        param_(param),
        thread_can_start_(thread_can_start),
        finished_(false) {
    ThreadWithParamBase* const base = this;

    GTEST_CHECK_POSIX_SUCCESS_(
        pthread_create(&thread_, nullptr, &ThreadFuncWithCLinkage, base));
  }
  ~ThreadWithParam() override { Join(); }

  void Join() {
    if (!finished_) {
      GTEST_CHECK_POSIX_SUCCESS_(pthread_join(thread_, nullptr));
      finished_ = true;
    }
  }

  void Run() override {
    if (thread_can_start_ != nullptr) thread_can_start_->WaitForNotification();
    func_(param_);
  }

 private:
  UserThreadFunc* const func_;
  const T param_;

  Notification* const thread_can_start_;
  bool finished_;

  pthread_t thread_;

  ThreadWithParam(const ThreadWithParam&) = delete;
  ThreadWithParam& operator=(const ThreadWithParam&) = delete;
};
#endif

#if GTEST_HAS_MUTEX_AND_THREAD_LOCAL_

#elif defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT)

class GTEST_API_ Mutex {
 public:
  enum MutexType { kStatic = 0, kDynamic = 1 };

  enum StaticConstructorSelector { kStaticMutex = 0 };

  explicit Mutex(StaticConstructorSelector          ) {}

  Mutex();
  ~Mutex();

  void Lock();

  void Unlock();

  void AssertHeld();

 private:

  void ThreadSafeLazyInit();

  unsigned int owner_thread_id_;

  MutexType type_;
  long critical_section_init_phase_;
  GTEST_CRITICAL_SECTION* critical_section_;

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
};

#define GTEST_DECLARE_STATIC_MUTEX_(mutex) \
  extern ::testing::internal::Mutex mutex

#define GTEST_DEFINE_STATIC_MUTEX_(mutex) \
  ::testing::internal::Mutex mutex(::testing::internal::Mutex::kStaticMutex)

class GTestMutexLock {
 public:
  explicit GTestMutexLock(Mutex* mutex) : mutex_(mutex) { mutex_->Lock(); }

  ~GTestMutexLock() { mutex_->Unlock(); }

 private:
  Mutex* const mutex_;

  GTestMutexLock(const GTestMutexLock&) = delete;
  GTestMutexLock& operator=(const GTestMutexLock&) = delete;
};

typedef GTestMutexLock MutexLock;

class ThreadLocalValueHolderBase {
 public:
  virtual ~ThreadLocalValueHolderBase() {}
};

class ThreadLocalBase {
 public:

  virtual ThreadLocalValueHolderBase* NewValueForCurrentThread() const = 0;

 protected:
  ThreadLocalBase() {}
  virtual ~ThreadLocalBase() {}

 private:
  ThreadLocalBase(const ThreadLocalBase&) = delete;
  ThreadLocalBase& operator=(const ThreadLocalBase&) = delete;
};

class GTEST_API_ ThreadLocalRegistry {
 public:

  static ThreadLocalValueHolderBase* GetValueOnCurrentThread(
      const ThreadLocalBase* thread_local_instance);

  static void OnThreadLocalDestroyed(
      const ThreadLocalBase* thread_local_instance);
};

class GTEST_API_ ThreadWithParamBase {
 public:
  void Join();

 protected:
  class Runnable {
   public:
    virtual ~Runnable() {}
    virtual void Run() = 0;
  };

  ThreadWithParamBase(Runnable* runnable, Notification* thread_can_start);
  virtual ~ThreadWithParamBase();

 private:
  AutoHandle thread_;
};

template <typename T>
class ThreadWithParam : public ThreadWithParamBase {
 public:
  typedef void UserThreadFunc(T);

  ThreadWithParam(UserThreadFunc* func, T param, Notification* thread_can_start)
      : ThreadWithParamBase(new RunnableImpl(func, param), thread_can_start) {}
  virtual ~ThreadWithParam() {}

 private:
  class RunnableImpl : public Runnable {
   public:
    RunnableImpl(UserThreadFunc* func, T param) : func_(func), param_(param) {}
    virtual ~RunnableImpl() {}
    virtual void Run() { func_(param_); }

   private:
    UserThreadFunc* const func_;
    const T param_;

    RunnableImpl(const RunnableImpl&) = delete;
    RunnableImpl& operator=(const RunnableImpl&) = delete;
  };

  ThreadWithParam(const ThreadWithParam&) = delete;
  ThreadWithParam& operator=(const ThreadWithParam&) = delete;
};

template <typename T>
class ThreadLocal : public ThreadLocalBase {
 public:
  ThreadLocal() : default_factory_(new DefaultValueHolderFactory()) {}
  explicit ThreadLocal(const T& value)
      : default_factory_(new InstanceValueHolderFactory(value)) {}

  ~ThreadLocal() override { ThreadLocalRegistry::OnThreadLocalDestroyed(this); }

  T* pointer() { return GetOrCreateValue(); }
  const T* pointer() const { return GetOrCreateValue(); }
  const T& get() const { return *pointer(); }
  void set(const T& value) { *pointer() = value; }

 private:

  class ValueHolder : public ThreadLocalValueHolderBase {
   public:
    ValueHolder() : value_() {}
    explicit ValueHolder(const T& value) : value_(value) {}

    T* pointer() { return &value_; }

   private:
    T value_;
    ValueHolder(const ValueHolder&) = delete;
    ValueHolder& operator=(const ValueHolder&) = delete;
  };

  T* GetOrCreateValue() const {
    return static_cast<ValueHolder*>(
               ThreadLocalRegistry::GetValueOnCurrentThread(this))
        ->pointer();
  }

  ThreadLocalValueHolderBase* NewValueForCurrentThread() const override {
    return default_factory_->MakeNewHolder();
  }

  class ValueHolderFactory {
   public:
    ValueHolderFactory() {}
    virtual ~ValueHolderFactory() {}
    virtual ValueHolder* MakeNewHolder() const = 0;

   private:
    ValueHolderFactory(const ValueHolderFactory&) = delete;
    ValueHolderFactory& operator=(const ValueHolderFactory&) = delete;
  };

  class DefaultValueHolderFactory : public ValueHolderFactory {
   public:
    DefaultValueHolderFactory() {}
    ValueHolder* MakeNewHolder() const override { return new ValueHolder(); }

   private:
    DefaultValueHolderFactory(const DefaultValueHolderFactory&) = delete;
    DefaultValueHolderFactory& operator=(const DefaultValueHolderFactory&) =
        delete;
  };

  class InstanceValueHolderFactory : public ValueHolderFactory {
   public:
    explicit InstanceValueHolderFactory(const T& value) : value_(value) {}
    ValueHolder* MakeNewHolder() const override {
      return new ValueHolder(value_);
    }

   private:
    const T value_;

    InstanceValueHolderFactory(const InstanceValueHolderFactory&) = delete;
    InstanceValueHolderFactory& operator=(const InstanceValueHolderFactory&) =
        delete;
  };

  std::unique_ptr<ValueHolderFactory> default_factory_;

  ThreadLocal(const ThreadLocal&) = delete;
  ThreadLocal& operator=(const ThreadLocal&) = delete;
};

#elif GTEST_HAS_PTHREAD

class MutexBase {
 public:

  void Lock() {
    GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_lock(&mutex_));
    owner_ = pthread_self();
    has_owner_ = true;
  }

  void Unlock() {

    has_owner_ = false;
    GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_unlock(&mutex_));
  }

  void AssertHeld() const {
    GTEST_CHECK_(has_owner_ && pthread_equal(owner_, pthread_self()))
        << "The current thread is not holding the mutex @" << this;
  }

 public:
  pthread_mutex_t mutex_;

  bool has_owner_;
  pthread_t owner_;
};

#define GTEST_DECLARE_STATIC_MUTEX_(mutex) \
  extern ::testing::internal::MutexBase mutex

#define GTEST_DEFINE_STATIC_MUTEX_(mutex) \
  ::testing::internal::MutexBase mutex = {PTHREAD_MUTEX_INITIALIZER, false, 0}

class Mutex : public MutexBase {
 public:
  Mutex() {
    GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_init(&mutex_, nullptr));
    has_owner_ = false;
  }
  ~Mutex() { GTEST_CHECK_POSIX_SUCCESS_(pthread_mutex_destroy(&mutex_)); }

 private:
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;
};

class GTestMutexLock {
 public:
  explicit GTestMutexLock(MutexBase* mutex) : mutex_(mutex) { mutex_->Lock(); }

  ~GTestMutexLock() { mutex_->Unlock(); }

 private:
  MutexBase* const mutex_;

  GTestMutexLock(const GTestMutexLock&) = delete;
  GTestMutexLock& operator=(const GTestMutexLock&) = delete;
};

typedef GTestMutexLock MutexLock;

class GTEST_API_ ThreadLocalValueHolderBase {
 public:
  virtual ~ThreadLocalValueHolderBase() = default;
};

extern "C" inline void DeleteThreadLocalValue(void* value_holder) {
  delete static_cast<ThreadLocalValueHolderBase*>(value_holder);
}

template <typename T>
class GTEST_API_ ThreadLocal {
 public:
  ThreadLocal()
      : key_(CreateKey()), default_factory_(new DefaultValueHolderFactory()) {}
  explicit ThreadLocal(const T& value)
      : key_(CreateKey()),
        default_factory_(new InstanceValueHolderFactory(value)) {}

  ~ThreadLocal() {

    DeleteThreadLocalValue(pthread_getspecific(key_));

    GTEST_CHECK_POSIX_SUCCESS_(pthread_key_delete(key_));
  }

  T* pointer() { return GetOrCreateValue(); }
  const T* pointer() const { return GetOrCreateValue(); }
  const T& get() const { return *pointer(); }
  void set(const T& value) { *pointer() = value; }

 private:

  class ValueHolder : public ThreadLocalValueHolderBase {
   public:
    ValueHolder() : value_() {}
    explicit ValueHolder(const T& value) : value_(value) {}

    T* pointer() { return &value_; }

   private:
    T value_;
    ValueHolder(const ValueHolder&) = delete;
    ValueHolder& operator=(const ValueHolder&) = delete;
  };

  static pthread_key_t CreateKey() {
    pthread_key_t key;

    GTEST_CHECK_POSIX_SUCCESS_(
        pthread_key_create(&key, &DeleteThreadLocalValue));
    return key;
  }

  T* GetOrCreateValue() const {
    ThreadLocalValueHolderBase* const holder =
        static_cast<ThreadLocalValueHolderBase*>(pthread_getspecific(key_));
    if (holder != nullptr) {
      return CheckedDowncastToActualType<ValueHolder>(holder)->pointer();
    }

    ValueHolder* const new_holder = default_factory_->MakeNewHolder();
    ThreadLocalValueHolderBase* const holder_base = new_holder;
    GTEST_CHECK_POSIX_SUCCESS_(pthread_setspecific(key_, holder_base));
    return new_holder->pointer();
  }

  class ValueHolderFactory {
   public:
    ValueHolderFactory() = default;
    virtual ~ValueHolderFactory() = default;
    virtual ValueHolder* MakeNewHolder() const = 0;

   private:
    ValueHolderFactory(const ValueHolderFactory&) = delete;
    ValueHolderFactory& operator=(const ValueHolderFactory&) = delete;
  };

  class DefaultValueHolderFactory : public ValueHolderFactory {
   public:
    DefaultValueHolderFactory() = default;
    ValueHolder* MakeNewHolder() const override { return new ValueHolder(); }

   private:
    DefaultValueHolderFactory(const DefaultValueHolderFactory&) = delete;
    DefaultValueHolderFactory& operator=(const DefaultValueHolderFactory&) =
        delete;
  };

  class InstanceValueHolderFactory : public ValueHolderFactory {
   public:
    explicit InstanceValueHolderFactory(const T& value) : value_(value) {}
    ValueHolder* MakeNewHolder() const override {
      return new ValueHolder(value_);
    }

   private:
    const T value_;

    InstanceValueHolderFactory(const InstanceValueHolderFactory&) = delete;
    InstanceValueHolderFactory& operator=(const InstanceValueHolderFactory&) =
        delete;
  };

  const pthread_key_t key_;
  std::unique_ptr<ValueHolderFactory> default_factory_;

  ThreadLocal(const ThreadLocal&) = delete;
  ThreadLocal& operator=(const ThreadLocal&) = delete;
};

#endif

#else

class Mutex {
 public:
  Mutex() {}
  void Lock() {}
  void Unlock() {}
  void AssertHeld() const {}
};

#define GTEST_DECLARE_STATIC_MUTEX_(mutex) \
  extern ::testing::internal::Mutex mutex

#define GTEST_DEFINE_STATIC_MUTEX_(mutex) ::testing::internal::Mutex mutex

class GTestMutexLock {
 public:
  explicit GTestMutexLock(Mutex*) {}
};

typedef GTestMutexLock MutexLock;

template <typename T>
class GTEST_API_ ThreadLocal {
 public:
  ThreadLocal() : value_() {}
  explicit ThreadLocal(const T& value) : value_(value) {}
  T* pointer() { return &value_; }
  const T* pointer() const { return &value_; }
  const T& get() const { return value_; }
  void set(const T& value) { value_ = value; }

 private:
  T value_;
};

#endif

GTEST_API_ size_t GetThreadCount();

#ifdef GTEST_OS_WINDOWS
#define GTEST_PATH_SEP_ "\\"
#define GTEST_HAS_ALT_PATH_SEP_ 1
#else
#define GTEST_PATH_SEP_ "/"
#define GTEST_HAS_ALT_PATH_SEP_ 0
#endif

inline bool IsAlpha(char ch) {
  return isalpha(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsAlNum(char ch) {
  return isalnum(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsDigit(char ch) {
  return isdigit(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsLower(char ch) {
  return islower(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsSpace(char ch) {
  return isspace(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsUpper(char ch) {
  return isupper(static_cast<unsigned char>(ch)) != 0;
}
inline bool IsXDigit(char ch) {
  return isxdigit(static_cast<unsigned char>(ch)) != 0;
}
#ifdef __cpp_lib_char8_t
inline bool IsXDigit(char8_t ch) {
  return isxdigit(static_cast<unsigned char>(ch)) != 0;
}
#endif
inline bool IsXDigit(char16_t ch) {
  const unsigned char low_byte = static_cast<unsigned char>(ch);
  return ch == low_byte && isxdigit(low_byte) != 0;
}
inline bool IsXDigit(char32_t ch) {
  const unsigned char low_byte = static_cast<unsigned char>(ch);
  return ch == low_byte && isxdigit(low_byte) != 0;
}
inline bool IsXDigit(wchar_t ch) {
  const unsigned char low_byte = static_cast<unsigned char>(ch);
  return ch == low_byte && isxdigit(low_byte) != 0;
}

inline char ToLower(char ch) {
  return static_cast<char>(tolower(static_cast<unsigned char>(ch)));
}
inline char ToUpper(char ch) {
  return static_cast<char>(toupper(static_cast<unsigned char>(ch)));
}

inline std::string StripTrailingSpaces(std::string str) {
  std::string::iterator it = str.end();
  while (it != str.begin() && IsSpace(*--it)) it = str.erase(it);
  return str;
}

namespace posix {

#ifdef GTEST_OS_WINDOWS

typedef struct _stat StatStruct;

#ifdef GTEST_OS_WINDOWS_MOBILE
inline int FileNo(FILE* file) { return reinterpret_cast<int>(_fileno(file)); }

#else
inline int FileNo(FILE* file) { return _fileno(file); }
#if GTEST_HAS_FILE_SYSTEM
inline int Stat(const char* path, StatStruct* buf) { return _stat(path, buf); }
inline int RmDir(const char* dir) { return _rmdir(dir); }
inline bool IsDir(const StatStruct& st) { return (_S_IFDIR & st.st_mode) != 0; }
#endif
#endif

#elif defined(GTEST_OS_ESP8266)
typedef struct stat StatStruct;

inline int FileNo(FILE* file) { return fileno(file); }
#if GTEST_HAS_FILE_SYSTEM
inline int Stat(const char* path, StatStruct* buf) {

  return 0;
}
inline int RmDir(const char* dir) { return rmdir(dir); }
inline bool IsDir(const StatStruct& st) { return S_ISDIR(st.st_mode); }
#endif

#else

typedef struct stat StatStruct;

inline int FileNo(FILE* file) { return fileno(file); }
#if GTEST_HAS_FILE_SYSTEM
inline int Stat(const char* path, StatStruct* buf) { return stat(path, buf); }
#ifdef GTEST_OS_QURT

inline int RmDir(const char*) { return 0; }
#else
inline int RmDir(const char* dir) { return rmdir(dir); }
#endif
inline bool IsDir(const StatStruct& st) { return S_ISDIR(st.st_mode); }
#endif

#endif

#ifdef GTEST_OS_WINDOWS

#ifdef __BORLANDC__
inline int DoIsATTY(int fd) { return isatty(fd); }
inline int StrCaseCmp(const char* s1, const char* s2) {
  return stricmp(s1, s2);
}
#else
#if defined(GTEST_OS_WINDOWS_MOBILE) || defined(GTEST_OS_ZOS) || \
    defined(GTEST_OS_IOS) || defined(GTEST_OS_WINDOWS_PHONE) ||  \
    defined(GTEST_OS_WINDOWS_RT) || defined(ESP_PLATFORM)
inline int DoIsATTY(int         ) { return 0; }
#else
inline int DoIsATTY(int fd) { return _isatty(fd); }
#endif
inline int StrCaseCmp(const char* s1, const char* s2) {
  return _stricmp(s1, s2);
}
#endif

#else

inline int DoIsATTY(int fd) { return isatty(fd); }
inline int StrCaseCmp(const char* s1, const char* s2) {
  return strcasecmp(s1, s2);
}

#endif

inline int IsATTY(int fd) {

  int savedErrno = errno;
  int isAttyValue = DoIsATTY(fd);
  errno = savedErrno;

  return isAttyValue;
}

GTEST_DISABLE_MSC_DEPRECATED_PUSH_()

#if GTEST_HAS_FILE_SYSTEM
#if !defined(GTEST_OS_WINDOWS_MOBILE) && !defined(GTEST_OS_WINDOWS_PHONE) && \
    !defined(GTEST_OS_WINDOWS_RT) && !defined(GTEST_OS_WINDOWS_GAMES) &&     \
    !defined(GTEST_OS_ESP8266) && !defined(GTEST_OS_XTENSA) &&               \
    !defined(GTEST_OS_QURT)
inline int ChDir(const char* dir) { return chdir(dir); }
#endif
inline FILE* FOpen(const char* path, const char* mode) {
#if defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_WINDOWS_MINGW)
  struct wchar_codecvt : public std::codecvt<wchar_t, char, std::mbstate_t> {};
  std::wstring_convert<wchar_codecvt> converter;
  std::wstring wide_path = converter.from_bytes(path);
  std::wstring wide_mode = converter.from_bytes(mode);
  return _wfopen(wide_path.c_str(), wide_mode.c_str());
#else
  return fopen(path, mode);
#endif
}
#if !defined(GTEST_OS_WINDOWS_MOBILE) && !defined(GTEST_OS_QURT)
inline FILE* FReopen(const char* path, const char* mode, FILE* stream) {
  return freopen(path, mode, stream);
}
inline FILE* FDOpen(int fd, const char* mode) { return fdopen(fd, mode); }
#endif
inline int FClose(FILE* fp) { return fclose(fp); }
#if !defined(GTEST_OS_WINDOWS_MOBILE) && !defined(GTEST_OS_QURT)
inline int Read(int fd, void* buf, unsigned int count) {
  return static_cast<int>(read(fd, buf, count));
}
inline int Write(int fd, const void* buf, unsigned int count) {
  return static_cast<int>(write(fd, buf, count));
}
inline int Close(int fd) { return close(fd); }
#endif
#endif

#if !defined(GTEST_OS_WINDOWS_MOBILE) && !defined(GTEST_OS_QURT)
inline const char* StrError(int errnum) { return strerror(errnum); }
#endif

inline const char* GetEnv(const char* name) {
#if defined(GTEST_OS_WINDOWS_MOBILE) || defined(GTEST_OS_WINDOWS_PHONE) || \
    defined(GTEST_OS_ESP8266) || defined(GTEST_OS_XTENSA) ||               \
    defined(GTEST_OS_QURT)

  static_cast<void>(name);
  return nullptr;
#elif defined(__BORLANDC__) || defined(__SunOS_5_8) || defined(__SunOS_5_9)

  const char* const env = getenv(name);
  return (env != nullptr && env[0] != '\0') ? env : nullptr;
#else
  return getenv(name);
#endif
}

GTEST_DISABLE_MSC_DEPRECATED_POP_()

#ifdef GTEST_OS_WINDOWS_MOBILE

[[noreturn]] void Abort();
#else
[[noreturn]] inline void Abort() { abort(); }
#endif

}

#if defined(_MSC_VER) && !defined(GTEST_OS_WINDOWS_MOBILE)

#define GTEST_SNPRINTF_(buffer, size, format, ...) \
  _snprintf_s(buffer, size, size, format, __VA_ARGS__)
#elif defined(_MSC_VER)

#define GTEST_SNPRINTF_ _snprintf
#else
#define GTEST_SNPRINTF_ snprintf
#endif

using BiggestInt = long long;

constexpr BiggestInt kMaxBiggestInt = (std::numeric_limits<BiggestInt>::max)();

template <size_t size>
class TypeWithSize {
 public:

  using UInt = void;
};

template <>
class TypeWithSize<4> {
 public:
  using Int = std::int32_t;
  using UInt = std::uint32_t;
};

template <>
class TypeWithSize<8> {
 public:
  using Int = std::int64_t;
  using UInt = std::uint64_t;
};

using TimeInMillis = int64_t;

#if !defined(GTEST_FLAG)
#define GTEST_FLAG_NAME_(name) gtest_##name
#define GTEST_FLAG(name) FLAGS_gtest_##name
#endif

#ifdef GTEST_INTERNAL_HAS_ABSL_FLAGS

#define GTEST_DEFINE_bool_(name, default_val, doc) \
  ABSL_FLAG(bool, GTEST_FLAG_NAME_(name), default_val, doc)
#define GTEST_DEFINE_int32_(name, default_val, doc) \
  ABSL_FLAG(int32_t, GTEST_FLAG_NAME_(name), default_val, doc)
#define GTEST_DEFINE_string_(name, default_val, doc) \
  ABSL_FLAG(std::string, GTEST_FLAG_NAME_(name), default_val, doc)

#define GTEST_DECLARE_bool_(name) \
  ABSL_DECLARE_FLAG(bool, GTEST_FLAG_NAME_(name))
#define GTEST_DECLARE_int32_(name) \
  ABSL_DECLARE_FLAG(int32_t, GTEST_FLAG_NAME_(name))
#define GTEST_DECLARE_string_(name) \
  ABSL_DECLARE_FLAG(std::string, GTEST_FLAG_NAME_(name))

#define GTEST_FLAG_SAVER_ ::absl::FlagSaver

#define GTEST_FLAG_GET(name) ::absl::GetFlag(GTEST_FLAG(name))
#define GTEST_FLAG_SET(name, value) \
  (void)(::absl::SetFlag(&GTEST_FLAG(name), value))
#define GTEST_USE_OWN_FLAGFILE_FLAG_ 0

#undef GTEST_INTERNAL_HAS_ABSL_FLAGS
#else

#define GTEST_DEFINE_bool_(name, default_val, doc)  \
  namespace testing {                               \
  GTEST_API_ bool GTEST_FLAG(name) = (default_val); \
  }                                                 \
  static_assert(true, "no-op to require trailing semicolon")
#define GTEST_DEFINE_int32_(name, default_val, doc)         \
  namespace testing {                                       \
  GTEST_API_ std::int32_t GTEST_FLAG(name) = (default_val); \
  }                                                         \
  static_assert(true, "no-op to require trailing semicolon")
#define GTEST_DEFINE_string_(name, default_val, doc)         \
  namespace testing {                                        \
  GTEST_API_ ::std::string GTEST_FLAG(name) = (default_val); \
  }                                                          \
  static_assert(true, "no-op to require trailing semicolon")

#define GTEST_DECLARE_bool_(name)          \
  namespace testing {                      \
  GTEST_API_ extern bool GTEST_FLAG(name); \
  }                                        \
  static_assert(true, "no-op to require trailing semicolon")
#define GTEST_DECLARE_int32_(name)                 \
  namespace testing {                              \
  GTEST_API_ extern std::int32_t GTEST_FLAG(name); \
  }                                                \
  static_assert(true, "no-op to require trailing semicolon")
#define GTEST_DECLARE_string_(name)                 \
  namespace testing {                               \
  GTEST_API_ extern ::std::string GTEST_FLAG(name); \
  }                                                 \
  static_assert(true, "no-op to require trailing semicolon")

#define GTEST_FLAG_SAVER_ ::testing::internal::GTestFlagSaver

#define GTEST_FLAG_GET(name) ::testing::GTEST_FLAG(name)
#define GTEST_FLAG_SET(name, value) (void)(::testing::GTEST_FLAG(name) = value)
#define GTEST_USE_OWN_FLAGFILE_FLAG_ 1

#endif

#if !defined(GTEST_EXCLUSIVE_LOCK_REQUIRED_)
#define GTEST_EXCLUSIVE_LOCK_REQUIRED_(locks)
#define GTEST_LOCK_EXCLUDED_(locks)
#endif

GTEST_API_ bool ParseInt32(const Message& src_text, const char* str,
                           int32_t* value);

bool BoolFromGTestEnv(const char* flag, bool default_val);
GTEST_API_ int32_t Int32FromGTestEnv(const char* flag, int32_t default_val);
std::string OutputFlagAlsoCheckEnvVar();
const char* StringFromGTestEnv(const char* flag, const char* default_val);

}
}

#ifdef GTEST_HAS_ABSL

#define GTEST_INTERNAL_HAS_ANY 1
#include "absl/types/any.h"
namespace testing {
namespace internal {
using Any = ::absl::any;
}
}
#else
#if defined(__cpp_lib_any) || (GTEST_INTERNAL_HAS_INCLUDE(<any>) &&        \
                               GTEST_INTERNAL_CPLUSPLUS_LANG >= 201703L && \
                               (!defined(_MSC_VER) || GTEST_HAS_RTTI))

#define GTEST_INTERNAL_HAS_ANY 1
#include <any>
namespace testing {
namespace internal {
using Any = ::std::any;
}
}

#endif
#endif

#ifndef GTEST_INTERNAL_HAS_ANY
#define GTEST_INTERNAL_HAS_ANY 0
#endif

#ifdef GTEST_HAS_ABSL

#define GTEST_INTERNAL_HAS_OPTIONAL 1
#include "absl/types/optional.h"
namespace testing {
namespace internal {
template <typename T>
using Optional = ::absl::optional<T>;
inline ::absl::nullopt_t Nullopt() { return ::absl::nullopt; }
}
}
#else
#if defined(__cpp_lib_optional) || (GTEST_INTERNAL_HAS_INCLUDE(<optional>) && \
                                    GTEST_INTERNAL_CPLUSPLUS_LANG >= 201703L)

#define GTEST_INTERNAL_HAS_OPTIONAL 1
#include <optional>
namespace testing {
namespace internal {
template <typename T>
using Optional = ::std::optional<T>;
inline ::std::nullopt_t Nullopt() { return ::std::nullopt; }
}
}

#endif
#endif

#ifndef GTEST_INTERNAL_HAS_OPTIONAL
#define GTEST_INTERNAL_HAS_OPTIONAL 0
#endif

#if defined(__cpp_lib_span) || (GTEST_INTERNAL_HAS_INCLUDE(<span>) && \
                                GTEST_INTERNAL_CPLUSPLUS_LANG >= 202002L)
#define GTEST_INTERNAL_HAS_STD_SPAN 1
#endif

#ifndef GTEST_INTERNAL_HAS_STD_SPAN
#define GTEST_INTERNAL_HAS_STD_SPAN 0
#endif

#ifdef GTEST_HAS_ABSL

#define GTEST_INTERNAL_HAS_STRING_VIEW 1
#include "absl/strings/string_view.h"
namespace testing {
namespace internal {
using StringView = ::absl::string_view;
}
}
#else
#if defined(__cpp_lib_string_view) ||             \
    (GTEST_INTERNAL_HAS_INCLUDE(<string_view>) && \
     GTEST_INTERNAL_CPLUSPLUS_LANG >= 201703L)

#define GTEST_INTERNAL_HAS_STRING_VIEW 1
#include <string_view>
namespace testing {
namespace internal {
using StringView = ::std::string_view;
}
}

#endif
#endif

#ifndef GTEST_INTERNAL_HAS_STRING_VIEW
#define GTEST_INTERNAL_HAS_STRING_VIEW 0
#endif

#ifdef GTEST_HAS_ABSL

#define GTEST_INTERNAL_HAS_VARIANT 1
#include "absl/types/variant.h"
namespace testing {
namespace internal {
template <typename... T>
using Variant = ::absl::variant<T...>;
}
}
#else
#if defined(__cpp_lib_variant) || (GTEST_INTERNAL_HAS_INCLUDE(<variant>) && \
                                   GTEST_INTERNAL_CPLUSPLUS_LANG >= 201703L)

#define GTEST_INTERNAL_HAS_VARIANT 1
#include <variant>
namespace testing {
namespace internal {
template <typename... T>
using Variant = ::std::variant<T...>;
}
}

#endif
#endif

#ifndef GTEST_INTERNAL_HAS_VARIANT
#define GTEST_INTERNAL_HAS_VARIANT 0
#endif

#if (defined(__cpp_lib_three_way_comparison) || \
     (GTEST_INTERNAL_HAS_INCLUDE(<compare>) &&  \
      GTEST_INTERNAL_CPLUSPLUS_LANG >= 201907L))
#define GTEST_INTERNAL_HAS_COMPARE_LIB 1
#else
#define GTEST_INTERNAL_HAS_COMPARE_LIB 0
#endif

#endif
