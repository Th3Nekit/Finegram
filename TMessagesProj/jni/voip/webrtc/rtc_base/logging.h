/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// RTC_LOG(...) an ostream target that can be used to send formatted
// output to a variety of logging targets, such as debugger console, stderr,
// or any LogSink.
// The severity level passed as the first argument to the logging
// functions is used as a filter, to limit the verbosity of the logging.
// Static members of LogMessage documented below are used to control the
// verbosity and target of the output.
// There are several variations on the RTC_LOG macro which facilitate logging
// of common error conditions, detailed below.

// RTC_LOG(sev) logs the given stream at severity "sev", which must be a
//     compile-time constant of the LoggingSeverity type, without the namespace
//     prefix.
// RTC_LOG_IF(sev, condition) logs the given stream at severitye "sev" if
//     "condition" is true.
// RTC_LOG_V(sev) Like RTC_LOG(), but sev is a run-time variable of the
//     LoggingSeverity type (basically, it just doesn't prepend the namespace).
// RTC_LOG_F(sev) Like RTC_LOG(), but includes the name of the current function.
// RTC_LOG_IF_F(sev, condition), Like RTC_LOG_IF(), but includes the name of
//     the current function.
// RTC_LOG_T(sev) Like RTC_LOG(), but includes the this pointer.
// RTC_LOG_T_F(sev) Like RTC_LOG_F(), but includes the this pointer.
// RTC_LOG_GLE(sev [, mod]) attempt to add a string description of the
//     HRESULT returned by GetLastError.
// RTC_LOG_ERRNO(sev) attempts to add a string description of an errno-derived
//     error. errno and associated facilities exist on both Windows and POSIX,
//     but on Windows they only apply to the C/C++ runtime.
// RTC_LOG_ERR(sev) is an alias for the platform's normal error system, i.e.
//     _GLE on Windows and _ERRNO on POSIX.
// (The above three also all have _EX versions that let you specify the error
// code, rather than using the last one.)
// RTC_LOG_E(sev, ctx, err, ...) logs a detailed error interpreted using the
//     specified context.
// RTC_LOG_CHECK_LEVEL(sev) (and RTC_LOG_CHECK_LEVEL_V(sev)) can be used as a
//     test before performing expensive or sensitive operations whose sole
//     purpose is to output logging data at the desired level.

#ifndef RTC_BASE_LOGGING_H_
#define RTC_BASE_LOGGING_H_

#include <errno.h>

#include <atomic>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/system/inline.h"

#if !defined(NDEBUG) || defined(DLOG_ALWAYS_ON)
#define RTC_DLOG_IS_ON 1
#else
#define RTC_DLOG_IS_ON 0
#endif

#if defined(RTC_DISABLE_LOGGING)
#define RTC_LOG_ENABLED() 0
#else
#define RTC_LOG_ENABLED() 1
#endif

namespace rtc {

enum LoggingSeverity {
  LS_VERBOSE,
  LS_INFO,
  LS_WARNING,
  LS_ERROR,
  LS_NONE,
};

enum LogErrorContext {
  ERRCTX_NONE,
  ERRCTX_ERRNO,
  ERRCTX_HRESULT,

  ERRCTX_EN = ERRCTX_ERRNO,
  ERRCTX_HR = ERRCTX_HRESULT,
};

class LogMessage;

class LogLineRef {
 public:
  absl::string_view message() const { return message_; }
  absl::string_view filename() const { return filename_; }
  int line() const { return line_; }
  absl::optional<PlatformThreadId> thread_id() const { return thread_id_; }
  webrtc::Timestamp timestamp() const { return timestamp_; }
  absl::string_view tag() const { return tag_; }
  LoggingSeverity severity() const { return severity_; }

#if RTC_LOG_ENABLED()
  std::string DefaultLogLine() const;
#else
  std::string DefaultLogLine() const { return ""; }
#endif

 private:
  friend class LogMessage;
  void set_message(std::string message) { message_ = std::move(message); }
  void set_filename(absl::string_view filename) { filename_ = filename; }
  void set_line(int line) { line_ = line; }
  void set_thread_id(absl::optional<PlatformThreadId> thread_id) {
    thread_id_ = thread_id;
  }
  void set_timestamp(webrtc::Timestamp timestamp) { timestamp_ = timestamp; }
  void set_tag(absl::string_view tag) { tag_ = tag; }
  void set_severity(LoggingSeverity severity) { severity_ = severity; }

  std::string message_;
  absl::string_view filename_;
  int line_ = 0;
  absl::optional<PlatformThreadId> thread_id_;
  webrtc::Timestamp timestamp_ = webrtc::Timestamp::MinusInfinity();

  absl::string_view tag_ = "libjingle";

  LoggingSeverity severity_;
};

class LogSink {
 public:
  LogSink() {}
  virtual ~LogSink() {}
  virtual void OnLogMessage(const std::string& msg,
                            LoggingSeverity severity,
                            const char* tag);
  virtual void OnLogMessage(const std::string& message,
                            LoggingSeverity severity);
  virtual void OnLogMessage(const std::string& message) = 0;

  virtual void OnLogMessage(absl::string_view msg,
                            LoggingSeverity severity,
                            const char* tag);
  virtual void OnLogMessage(absl::string_view message,
                            LoggingSeverity severity);
  virtual void OnLogMessage(absl::string_view message);
  virtual void OnLogMessage(const LogLineRef& line);

 private:
  friend class ::rtc::LogMessage;
#if RTC_LOG_ENABLED()

  LogSink* next_ = nullptr;
  LoggingSeverity min_severity_;
#endif
};

namespace webrtc_logging_impl {

class LogMetadata {
 public:
  LogMetadata(const char* file, int line, LoggingSeverity severity)
      : file_(file),
        line_and_sev_(static_cast<uint32_t>(line) << 3 | severity) {}
  LogMetadata() = default;

  const char* File() const { return file_; }
  int Line() const { return line_and_sev_ >> 3; }
  LoggingSeverity Severity() const {
    return static_cast<LoggingSeverity>(line_and_sev_ & 0x7);
  }

 private:
  const char* file_;

  uint32_t line_and_sev_;
};
static_assert(std::is_trivial<LogMetadata>::value, "");

struct LogMetadataErr {
  LogMetadata meta;
  LogErrorContext err_ctx;
  int err;
};

#ifdef WEBRTC_ANDROID
struct LogMetadataTag {
  LoggingSeverity severity;
  const char* tag;
};
#endif

enum class LogArgType : int8_t {
  kEnd = 0,
  kInt,
  kLong,
  kLongLong,
  kUInt,
  kULong,
  kULongLong,
  kDouble,
  kLongDouble,
  kCharP,
  kStdString,
  kStringView,
  kVoidP,
  kLogMetadata,
  kLogMetadataErr,
#ifdef WEBRTC_ANDROID
  kLogMetadataTag,
#endif
};

template <LogArgType N, typename T>
struct Val {
  static constexpr LogArgType Type() { return N; }
  T GetVal() const { return val; }
  T val;
};

struct ToStringVal {
  static constexpr LogArgType Type() { return LogArgType::kStdString; }
  const std::string* GetVal() const { return &val; }
  std::string val;
};

inline Val<LogArgType::kInt, int> MakeVal(int x) {
  return {x};
}
inline Val<LogArgType::kLong, long> MakeVal(long x) {
  return {x};
}
inline Val<LogArgType::kLongLong, long long> MakeVal(long long x) {
  return {x};
}
inline Val<LogArgType::kUInt, unsigned int> MakeVal(unsigned int x) {
  return {x};
}
inline Val<LogArgType::kULong, unsigned long> MakeVal(unsigned long x) {
  return {x};
}
inline Val<LogArgType::kULongLong, unsigned long long> MakeVal(
    unsigned long long x) {
  return {x};
}

inline Val<LogArgType::kDouble, double> MakeVal(double x) {
  return {x};
}
inline Val<LogArgType::kLongDouble, long double> MakeVal(long double x) {
  return {x};
}

inline Val<LogArgType::kCharP, const char*> MakeVal(const char* x) {
  return {x};
}
inline Val<LogArgType::kStdString, const std::string*> MakeVal(
    const std::string& x) {
  return {&x};
}
inline Val<LogArgType::kStringView, const absl::string_view*> MakeVal(
    const absl::string_view& x) {
  return {&x};
}

inline Val<LogArgType::kVoidP, const void*> MakeVal(const void* x) {
  return {x};
}

inline Val<LogArgType::kLogMetadata, LogMetadata> MakeVal(
    const LogMetadata& x) {
  return {x};
}
inline Val<LogArgType::kLogMetadataErr, LogMetadataErr> MakeVal(
    const LogMetadataErr& x) {
  return {x};
}

template <typename T,
          absl::enable_if_t<std::is_enum<T>::value &&
                            !std::is_arithmetic<T>::value>* = nullptr>
inline decltype(MakeVal(std::declval<absl::underlying_type_t<T>>())) MakeVal(
    T x) {
  return {static_cast<absl::underlying_type_t<T>>(x)};
}

#ifdef WEBRTC_ANDROID
inline Val<LogArgType::kLogMetadataTag, LogMetadataTag> MakeVal(
    const LogMetadataTag& x) {
  return {x};
}
#endif

template <typename T, class = void>
struct has_to_log_string : std::false_type {};
template <typename T>
struct has_to_log_string<T,
                         absl::enable_if_t<std::is_convertible<
                             decltype(ToLogString(std::declval<T>())),
                             std::string>::value>> : std::true_type {};

template <typename T, absl::enable_if_t<has_to_log_string<T>::value>* = nullptr>
ToStringVal MakeVal(const T& x) {
  return {ToLogString(x)};
}

template <
    typename T,
    typename T1 = absl::decay_t<T>,
    absl::enable_if_t<std::is_class<T1>::value &&
                      !std::is_same<T1, std::string>::value &&
                      !std::is_same<T1, LogMetadata>::value &&
                      !has_to_log_string<T1>::value &&
#ifdef WEBRTC_ANDROID
                      !std::is_same<T1, LogMetadataTag>::value &&
#endif
                      !std::is_same<T1, LogMetadataErr>::value>* = nullptr>
ToStringVal MakeVal(const T& x) {
  std::ostringstream os;
  os << x;
  return {os.str()};
}

#if RTC_LOG_ENABLED()
void Log(const LogArgType* fmt, ...);
#else
inline void Log(const LogArgType* fmt, ...) {

}
#endif

template <typename... Ts>
class LogStreamer;

template <>
class LogStreamer<> final {
 public:
  template <typename U,
            typename V = decltype(MakeVal(std::declval<U>())),
            absl::enable_if_t<std::is_arithmetic<U>::value ||
                              std::is_enum<U>::value>* = nullptr>
  RTC_FORCE_INLINE LogStreamer<V> operator<<(U arg) const {
    return LogStreamer<V>(MakeVal(arg), this);
  }

  template <typename U,
            typename V = decltype(MakeVal(std::declval<U>())),
            absl::enable_if_t<!std::is_arithmetic<U>::value &&
                              !std::is_enum<U>::value>* = nullptr>
  RTC_FORCE_INLINE LogStreamer<V> operator<<(const U& arg) const {
    return LogStreamer<V>(MakeVal(arg), this);
  }

  template <typename... Us>
  RTC_FORCE_INLINE static void Call(const Us&... args) {
    static constexpr LogArgType t[] = {Us::Type()..., LogArgType::kEnd};
    Log(t, args.GetVal()...);
  }
};

template <typename T, typename... Ts>
class LogStreamer<T, Ts...> final {
 public:
  RTC_FORCE_INLINE LogStreamer(T arg, const LogStreamer<Ts...>* prior)
      : arg_(arg), prior_(prior) {}

  template <typename U,
            typename V = decltype(MakeVal(std::declval<U>())),
            absl::enable_if_t<std::is_arithmetic<U>::value ||
                              std::is_enum<U>::value>* = nullptr>
  RTC_FORCE_INLINE LogStreamer<V, T, Ts...> operator<<(U arg) const {
    return LogStreamer<V, T, Ts...>(MakeVal(arg), this);
  }

  template <typename U,
            typename V = decltype(MakeVal(std::declval<U>())),
            absl::enable_if_t<!std::is_arithmetic<U>::value &&
                              !std::is_enum<U>::value>* = nullptr>
  RTC_FORCE_INLINE LogStreamer<V, T, Ts...> operator<<(const U& arg) const {
    return LogStreamer<V, T, Ts...>(MakeVal(arg), this);
  }

  template <typename... Us>
  RTC_FORCE_INLINE void Call(const Us&... args) const {
    prior_->Call(arg_, args...);
  }

 private:

  T arg_;

  const LogStreamer<Ts...>* prior_;
};

class LogCall final {
 public:

  template <typename... Ts>
  RTC_FORCE_INLINE bool operator&(const LogStreamer<Ts...>& streamer) {
    streamer.Call();
    return true;
  }
};

class LogMessageVoidify {
 public:
  LogMessageVoidify() = default;

  template <typename... Ts>
  void operator&(LogStreamer<Ts...>&& streamer) {}
};

}

class LogMessage {
 public:

  template <LoggingSeverity S>
  RTC_NO_INLINE LogMessage(const char* file,
                           int line,
                           std::integral_constant<LoggingSeverity, S>)
      : LogMessage(file, line, S) {}

#if RTC_LOG_ENABLED()
  LogMessage(const char* file, int line, LoggingSeverity sev);
  LogMessage(const char* file,
             int line,
             LoggingSeverity sev,
             LogErrorContext err_ctx,
             int err);
#if defined(WEBRTC_ANDROID)
  LogMessage(const char* file, int line, LoggingSeverity sev, const char* tag);
#endif
  ~LogMessage();

  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;

  void AddTag(const char* tag);
  rtc::StringBuilder& stream();

  static int64_t LogStartTime();

  static uint32_t WallClockStartTime();

  static void LogThreads(bool on = true);

  static void LogTimestamps(bool on = true);

  static void LogToDebug(LoggingSeverity min_sev);
  static LoggingSeverity GetLogToDebug();

  static void SetLogToStderr(bool log_to_stderr);

  static void AddLogToStream(LogSink* stream, LoggingSeverity min_sev);

  static void RemoveLogToStream(LogSink* stream);

  static int GetLogToStream(LogSink* stream = nullptr);

  static int GetMinLogSeverity();

  static void ConfigureLogging(absl::string_view params);

  static bool IsNoop(LoggingSeverity severity);

  template <LoggingSeverity S>
  RTC_NO_INLINE static bool IsNoop() {
    return IsNoop(S);
  }
#else

  LogMessage(const char* file, int line, LoggingSeverity sev) {}
  LogMessage(const char* file,
             int line,
             LoggingSeverity sev,
             LogErrorContext err_ctx,
             int err) {}
#if defined(WEBRTC_ANDROID)
  LogMessage(const char* file, int line, LoggingSeverity sev, const char* tag) {
  }
#endif
  ~LogMessage() = default;

  inline void AddTag(const char* tag) {}
  inline rtc::StringBuilder& stream() { return print_stream_; }
  inline static int64_t LogStartTime() { return 0; }
  inline static uint32_t WallClockStartTime() { return 0; }
  inline static void LogThreads(bool on = true) {}
  inline static void LogTimestamps(bool on = true) {}
  inline static void LogToDebug(LoggingSeverity min_sev) {}
  inline static LoggingSeverity GetLogToDebug() {
    return LoggingSeverity::LS_INFO;
  }
  inline static void SetLogToStderr(bool log_to_stderr) {}
  inline static void AddLogToStream(LogSink* stream, LoggingSeverity min_sev) {}
  inline static void RemoveLogToStream(LogSink* stream) {}
  inline static int GetLogToStream(LogSink* stream = nullptr) { return 0; }
  inline static int GetMinLogSeverity() { return 0; }
  inline static void ConfigureLogging(absl::string_view params) {}
  static constexpr bool IsNoop(LoggingSeverity severity) { return true; }
  template <LoggingSeverity S>
  static constexpr bool IsNoop() {
    return IsNoop(S);
  }
#endif

 private:
  friend class LogMessageForTesting;

#if RTC_LOG_ENABLED()

  static void UpdateMinLogSeverity();

  static void OutputToDebug(const LogLineRef& log_line_ref);

  void FinishPrintStream();

  LogLineRef log_line_;

  std::string extra_;

  static LogSink* streams_;

  static std::atomic<bool> streams_empty_;

  static bool log_thread_;
  static bool log_timestamp_;

  static bool log_to_stderr_;
#else

  inline static void UpdateMinLogSeverity() {}
#if defined(WEBRTC_ANDROID)
  inline static void OutputToDebug(absl::string_view filename,
                                   int line,
                                   absl::string_view msg,
                                   LoggingSeverity severity,
                                   const char* tag) {}
#else
  inline static void OutputToDebug(absl::string_view filename,
                                   int line,
                                   absl::string_view msg,
                                   LoggingSeverity severity) {}
#endif
  inline void FinishPrintStream() {}
#endif

  rtc::StringBuilder print_stream_;
};

#define RTC_LOG_FILE_LINE(sev, file, line)        \
  ::rtc::webrtc_logging_impl::LogCall() &         \
      ::rtc::webrtc_logging_impl::LogStreamer<>() \
          << ::rtc::webrtc_logging_impl::LogMetadata(file, line, sev)

#define RTC_LOG(sev)                          \
  !::rtc::LogMessage::IsNoop<::rtc::sev>() && \
      RTC_LOG_FILE_LINE(::rtc::sev, __FILE__, __LINE__)

#define RTC_LOG_IF(sev, condition)                           \
  !::rtc::LogMessage::IsNoop<::rtc::sev>() && (condition) && \
      RTC_LOG_FILE_LINE(::rtc::sev, __FILE__, __LINE__)

#define RTC_LOG_V(sev) \
  !::rtc::LogMessage::IsNoop(sev) && RTC_LOG_FILE_LINE(sev, __FILE__, __LINE__)

#if (defined(__GNUC__) && !defined(NDEBUG)) || defined(WANT_PRETTY_LOG_F)
#define RTC_LOG_F(sev) RTC_LOG(sev) << __PRETTY_FUNCTION__ << ": "
#define RTC_LOG_IF_F(sev, condition) \
  RTC_LOG_IF(sev, condition) << __PRETTY_FUNCTION__ << ": "
#define RTC_LOG_T_F(sev) \
  RTC_LOG(sev) << this << ": " << __PRETTY_FUNCTION__ << ": "
#else
#define RTC_LOG_F(sev) RTC_LOG(sev) << __FUNCTION__ << ": "
#define RTC_LOG_IF_F(sev, condition) \
  RTC_LOG_IF(sev, condition) << __FUNCTION__ << ": "
#define RTC_LOG_T_F(sev) RTC_LOG(sev) << this << ": " << __FUNCTION__ << ": "
#endif

#define RTC_LOG_CHECK_LEVEL(sev) ::rtc::LogCheckLevel(::rtc::sev)
#define RTC_LOG_CHECK_LEVEL_V(sev) ::rtc::LogCheckLevel(sev)

inline bool LogCheckLevel(LoggingSeverity sev) {
  return (LogMessage::GetMinLogSeverity() <= sev);
}

#define RTC_LOG_E(sev, ctx, err)                                 \
  !::rtc::LogMessage::IsNoop<::rtc::sev>() &&                    \
      ::rtc::webrtc_logging_impl::LogCall() &                    \
          ::rtc::webrtc_logging_impl::LogStreamer<>()            \
              << ::rtc::webrtc_logging_impl::LogMetadataErr {    \
    {__FILE__, __LINE__, ::rtc::sev}, ::rtc::ERRCTX_##ctx, (err) \
  }

#define RTC_LOG_T(sev) RTC_LOG(sev) << this << ": "

#define RTC_LOG_ERRNO_EX(sev, err) RTC_LOG_E(sev, ERRNO, err)
#define RTC_LOG_ERRNO(sev) RTC_LOG_ERRNO_EX(sev, errno)

#if defined(WEBRTC_WIN)
#define RTC_LOG_GLE_EX(sev, err) RTC_LOG_E(sev, HRESULT, err)
#define RTC_LOG_GLE(sev) RTC_LOG_GLE_EX(sev, static_cast<int>(GetLastError()))
#define RTC_LOG_ERR_EX(sev, err) RTC_LOG_GLE_EX(sev, err)
#define RTC_LOG_ERR(sev) RTC_LOG_GLE(sev)
#elif defined(__native_client__) && __native_client__
#define RTC_LOG_ERR_EX(sev, err) RTC_LOG(sev)
#define RTC_LOG_ERR(sev) RTC_LOG(sev)
#elif defined(WEBRTC_POSIX)
#define RTC_LOG_ERR_EX(sev, err) RTC_LOG_ERRNO_EX(sev, err)
#define RTC_LOG_ERR(sev) RTC_LOG_ERRNO(sev)
#endif

#ifdef WEBRTC_ANDROID

namespace webrtc_logging_impl {

inline const char* AdaptString(const char* str) {
  return str;
}
inline const char* AdaptString(const std::string& str) {
  return str.c_str();
}
}

#define RTC_LOG_TAG(sev, tag)                                 \
  !::rtc::LogMessage::IsNoop(sev) &&                          \
      ::rtc::webrtc_logging_impl::LogCall() &                 \
          ::rtc::webrtc_logging_impl::LogStreamer<>()         \
              << ::rtc::webrtc_logging_impl::LogMetadataTag { \
    sev, ::rtc::webrtc_logging_impl::AdaptString(tag)         \
  }

#else

#define RTC_LOG_TAG(sev, tag) RTC_LOG_V(sev)

#endif

#if RTC_DLOG_IS_ON
#define RTC_DLOG(sev) RTC_LOG(sev)
#define RTC_DLOG_IF(sev, condition) RTC_LOG_IF(sev, condition)
#define RTC_DLOG_V(sev) RTC_LOG_V(sev)
#define RTC_DLOG_F(sev) RTC_LOG_F(sev)
#define RTC_DLOG_IF_F(sev, condition) RTC_LOG_IF_F(sev, condition)
#else
#define RTC_DLOG_EAT_STREAM_PARAMS()                \
  while (false)                                     \
  ::rtc::webrtc_logging_impl::LogMessageVoidify() & \
      (::rtc::webrtc_logging_impl::LogStreamer<>())
#define RTC_DLOG(sev) RTC_DLOG_EAT_STREAM_PARAMS()
#define RTC_DLOG_IF(sev, condition) RTC_DLOG_EAT_STREAM_PARAMS()
#define RTC_DLOG_V(sev) RTC_DLOG_EAT_STREAM_PARAMS()
#define RTC_DLOG_F(sev) RTC_DLOG_EAT_STREAM_PARAMS()
#define RTC_DLOG_IF_F(sev, condition) RTC_DLOG_EAT_STREAM_PARAMS()
#endif

}

#endif
