/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CHECKS_H_
#define RTC_BASE_CHECKS_H_

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
#define RTC_DCHECK_IS_ON 1
#else
#define RTC_DCHECK_IS_ON 0
#endif

#if defined(_MSC_VER)
#define RTC_NORETURN __declspec(noreturn)
#elif defined(__GNUC__)
#define RTC_NORETURN __attribute__((__noreturn__))
#else
#define RTC_NORETURN
#endif

#ifdef __cplusplus
extern "C" {
#endif
RTC_NORETURN void rtc_FatalMessage(const char* file, int line, const char* msg);
#ifdef __cplusplus
}
#endif

#ifdef RTC_DISABLE_CHECK_MSG
#define RTC_CHECK_MSG_ENABLED 0
#else
#define RTC_CHECK_MSG_ENABLED 1
#endif

#if RTC_CHECK_MSG_ENABLED
#define RTC_CHECK_EVAL_MESSAGE(message) message
#else
#define RTC_CHECK_EVAL_MESSAGE(message) ""
#endif

#ifdef __cplusplus

#include <string>

#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "rtc_base/numerics/safe_compare.h"
#include "rtc_base/system/inline.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {
namespace webrtc_checks_impl {
enum class CheckArgType : int8_t {
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

  kCheckOp,
};

RTC_NORETURN void WriteFatalLog(const char* file,
                                int line,
                                absl::string_view output);
RTC_NORETURN void WriteFatalLog(absl::string_view output);

#if RTC_CHECK_MSG_ENABLED
RTC_NORETURN RTC_EXPORT void FatalLog(const char* file,
                                      int line,
                                      const char* message,
                                      const CheckArgType* fmt,
                                      ...);
#else
RTC_NORETURN RTC_EXPORT void FatalLog(const char* file, int line);
#endif

template <CheckArgType N, typename T>
struct Val {
  static constexpr CheckArgType Type() { return N; }
  T GetVal() const { return val; }
  T val;
};

struct ToStringVal {
  static constexpr CheckArgType Type() { return CheckArgType::kStdString; }
  const std::string* GetVal() const { return &val; }
  std::string val;
};

inline Val<CheckArgType::kInt, int> MakeVal(int x) {
  return {x};
}
inline Val<CheckArgType::kLong, long> MakeVal(long x) {
  return {x};
}
inline Val<CheckArgType::kLongLong, long long> MakeVal(long long x) {
  return {x};
}
inline Val<CheckArgType::kUInt, unsigned int> MakeVal(unsigned int x) {
  return {x};
}
inline Val<CheckArgType::kULong, unsigned long> MakeVal(unsigned long x) {
  return {x};
}
inline Val<CheckArgType::kULongLong, unsigned long long> MakeVal(
    unsigned long long x) {
  return {x};
}

inline Val<CheckArgType::kDouble, double> MakeVal(double x) {
  return {x};
}
inline Val<CheckArgType::kLongDouble, long double> MakeVal(long double x) {
  return {x};
}

inline Val<CheckArgType::kCharP, const char*> MakeVal(const char* x) {
  return {x};
}
inline Val<CheckArgType::kStdString, const std::string*> MakeVal(
    const std::string& x) {
  return {&x};
}
inline Val<CheckArgType::kStringView, const absl::string_view*> MakeVal(
    const absl::string_view& x) {
  return {&x};
}

inline Val<CheckArgType::kVoidP, const void*> MakeVal(const void* x) {
  return {x};
}

template <typename T>
inline Val<CheckArgType::kVoidP, const void*> MakeVal(
    const rtc::scoped_refptr<T>& p) {
  return {p.get()};
}

template <typename T,
          absl::enable_if_t<std::is_enum<T>::value &&
                            !std::is_arithmetic<T>::value>* = nullptr>
inline decltype(MakeVal(std::declval<absl::underlying_type_t<T>>())) MakeVal(
    T x) {
  return {static_cast<absl::underlying_type_t<T>>(x)};
}

template <typename T, decltype(ToLogString(std::declval<T>()))* = nullptr>
ToStringVal MakeVal(const T& x) {
  return {ToLogString(x)};
}

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

#if RTC_CHECK_MSG_ENABLED
  template <typename... Us>
  RTC_NORETURN RTC_FORCE_INLINE static void Call(const char* file,
                                                 const int line,
                                                 const char* message,
                                                 const Us&... args) {
    static constexpr CheckArgType t[] = {Us::Type()..., CheckArgType::kEnd};
    FatalLog(file, line, message, t, args.GetVal()...);
  }

  template <typename... Us>
  RTC_NORETURN RTC_FORCE_INLINE static void CallCheckOp(const char* file,
                                                        const int line,
                                                        const char* message,
                                                        const Us&... args) {
    static constexpr CheckArgType t[] = {CheckArgType::kCheckOp, Us::Type()...,
                                         CheckArgType::kEnd};
    FatalLog(file, line, message, t, args.GetVal()...);
  }
#else
  template <typename... Us>
  RTC_NORETURN RTC_FORCE_INLINE static void Call(const char* file,
                                                 const int line) {
    FatalLog(file, line);
  }
#endif
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

#if RTC_CHECK_MSG_ENABLED
  template <typename... Us>
  RTC_NORETURN RTC_FORCE_INLINE void Call(const char* file,
                                          const int line,
                                          const char* message,
                                          const Us&... args) const {
    prior_->Call(file, line, message, arg_, args...);
  }

  template <typename... Us>
  RTC_NORETURN RTC_FORCE_INLINE void CallCheckOp(const char* file,
                                                 const int line,
                                                 const char* message,
                                                 const Us&... args) const {
    prior_->CallCheckOp(file, line, message, arg_, args...);
  }
#else
  template <typename... Us>
  RTC_NORETURN RTC_FORCE_INLINE void Call(const char* file,
                                          const int line) const {
    prior_->Call(file, line);
  }
#endif

 private:

  T arg_;

  const LogStreamer<Ts...>* prior_;
};

template <bool isCheckOp>
class FatalLogCall final {
 public:
  FatalLogCall(const char* file, int line, const char* message)
      : file_(file), line_(line), message_(message) {}

  template <typename... Ts>
  RTC_NORETURN RTC_FORCE_INLINE void operator&(
      const LogStreamer<Ts...>& streamer) {
#if RTC_CHECK_MSG_ENABLED
    isCheckOp ? streamer.CallCheckOp(file_, line_, message_)
              : streamer.Call(file_, line_, message_);
#else
    streamer.Call(file_, line_);
#endif
  }

 private:
  const char* file_;
  int line_;
  const char* message_;
};

#if RTC_DCHECK_IS_ON

#define RTC_UNREACHABLE_FILE_AND_LINE_CALL_ARGS __FILE__, __LINE__
RTC_NORETURN RTC_EXPORT void UnreachableCodeReached(const char* file, int line);

#else

#define RTC_UNREACHABLE_FILE_AND_LINE_CALL_ARGS
RTC_NORETURN RTC_EXPORT void UnreachableCodeReached();

#endif

}

#define RTC_EAT_STREAM_PARAMETERS(ignored)                          \
  (true ? true : ((void)(ignored), true))                           \
      ? static_cast<void>(0)                                        \
      : ::rtc::webrtc_checks_impl::FatalLogCall<false>("", 0, "") & \
            ::rtc::webrtc_checks_impl::LogStreamer<>()

#define RTC_EAT_STREAM_PARAMETERS_OP(op, a, b) \
  RTC_EAT_STREAM_PARAMETERS(((void)::rtc::Safe##op(a, b)))

#if RTC_CHECK_MSG_ENABLED
#define RTC_CHECK(condition)                                    \
  (condition) ? static_cast<void>(0)                            \
              : ::rtc::webrtc_checks_impl::FatalLogCall<false>( \
                    __FILE__, __LINE__, #condition) &           \
                    ::rtc::webrtc_checks_impl::LogStreamer<>()

#define RTC_CHECK_OP(name, op, val1, val2)                 \
  ::rtc::Safe##name((val1), (val2))                        \
      ? static_cast<void>(0)                               \
      : ::rtc::webrtc_checks_impl::FatalLogCall<true>(     \
            __FILE__, __LINE__, #val1 " " #op " " #val2) & \
            ::rtc::webrtc_checks_impl::LogStreamer<>() << (val1) << (val2)
#else
#define RTC_CHECK(condition)                                                  \
  (condition) ? static_cast<void>(0)                                          \
  : true ? ::rtc::webrtc_checks_impl::FatalLogCall<false>(__FILE__, __LINE__, \
                                                          "") &               \
               ::rtc::webrtc_checks_impl::LogStreamer<>()                     \
         : ::rtc::webrtc_checks_impl::FatalLogCall<false>("", 0, "") &        \
               ::rtc::webrtc_checks_impl::LogStreamer<>()

#define RTC_CHECK_OP(name, op, val1, val2)                                   \
  ::rtc::Safe##name((val1), (val2)) ? static_cast<void>(0)                   \
  : true ? ::rtc::webrtc_checks_impl::FatalLogCall<true>(__FILE__, __LINE__, \
                                                         "") &               \
               ::rtc::webrtc_checks_impl::LogStreamer<>()                    \
         : ::rtc::webrtc_checks_impl::FatalLogCall<false>("", 0, "") &       \
               ::rtc::webrtc_checks_impl::LogStreamer<>()
#endif

#define RTC_CHECK_EQ(val1, val2) RTC_CHECK_OP(Eq, ==, val1, val2)
#define RTC_CHECK_NE(val1, val2) RTC_CHECK_OP(Ne, !=, val1, val2)
#define RTC_CHECK_LE(val1, val2) RTC_CHECK_OP(Le, <=, val1, val2)
#define RTC_CHECK_LT(val1, val2) RTC_CHECK_OP(Lt, <, val1, val2)
#define RTC_CHECK_GE(val1, val2) RTC_CHECK_OP(Ge, >=, val1, val2)
#define RTC_CHECK_GT(val1, val2) RTC_CHECK_OP(Gt, >, val1, val2)

#if RTC_DCHECK_IS_ON
#define RTC_DCHECK(condition) RTC_CHECK(condition)
#define RTC_DCHECK_EQ(v1, v2) RTC_CHECK_EQ(v1, v2)
#define RTC_DCHECK_NE(v1, v2) RTC_CHECK_NE(v1, v2)
#define RTC_DCHECK_LE(v1, v2) RTC_CHECK_LE(v1, v2)
#define RTC_DCHECK_LT(v1, v2) RTC_CHECK_LT(v1, v2)
#define RTC_DCHECK_GE(v1, v2) RTC_CHECK_GE(v1, v2)
#define RTC_DCHECK_GT(v1, v2) RTC_CHECK_GT(v1, v2)
#else
#define RTC_DCHECK(condition) RTC_EAT_STREAM_PARAMETERS(condition)
#define RTC_DCHECK_EQ(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Eq, v1, v2)
#define RTC_DCHECK_NE(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Ne, v1, v2)
#define RTC_DCHECK_LE(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Le, v1, v2)
#define RTC_DCHECK_LT(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Lt, v1, v2)
#define RTC_DCHECK_GE(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Ge, v1, v2)
#define RTC_DCHECK_GT(v1, v2) RTC_EAT_STREAM_PARAMETERS_OP(Gt, v1, v2)
#endif

#define RTC_UNREACHABLE_CODE_HIT false
#define RTC_DCHECK_NOTREACHED() RTC_DCHECK(RTC_UNREACHABLE_CODE_HIT)

#define RTC_CHECK_NOTREACHED()                         \
  do {                                                 \
    ::rtc::webrtc_checks_impl::UnreachableCodeReached( \
        RTC_UNREACHABLE_FILE_AND_LINE_CALL_ARGS);      \
  } while (0)

#define RTC_FATAL()                                                  \
  ::rtc::webrtc_checks_impl::FatalLogCall<false>(__FILE__, __LINE__, \
                                                 "FATAL()") &        \
      ::rtc::webrtc_checks_impl::LogStreamer<>()

template <typename T>
inline T CheckedDivExact(T a, T b) {
  RTC_CHECK_EQ(a % b, 0) << a << " is not evenly divisible by " << b;
  return a / b;
}

}

#else

#define RTC_CHECK(condition)                                                 \
  do {                                                                       \
    if (!(condition)) {                                                      \
      rtc_FatalMessage(__FILE__, __LINE__,                                   \
                       RTC_CHECK_EVAL_MESSAGE("CHECK failed: " #condition)); \
    }                                                                        \
  } while (0)

#define RTC_CHECK_EQ(a, b) RTC_CHECK((a) == (b))
#define RTC_CHECK_NE(a, b) RTC_CHECK((a) != (b))
#define RTC_CHECK_LE(a, b) RTC_CHECK((a) <= (b))
#define RTC_CHECK_LT(a, b) RTC_CHECK((a) < (b))
#define RTC_CHECK_GE(a, b) RTC_CHECK((a) >= (b))
#define RTC_CHECK_GT(a, b) RTC_CHECK((a) > (b))

#define RTC_DCHECK(condition)                                                 \
  do {                                                                        \
    if (RTC_DCHECK_IS_ON && !(condition)) {                                   \
      rtc_FatalMessage(__FILE__, __LINE__,                                    \
                       RTC_CHECK_EVAL_MESSAGE("DCHECK failed: " #condition)); \
    }                                                                         \
  } while (0)

#define RTC_DCHECK_EQ(a, b) RTC_DCHECK((a) == (b))
#define RTC_DCHECK_NE(a, b) RTC_DCHECK((a) != (b))
#define RTC_DCHECK_LE(a, b) RTC_DCHECK((a) <= (b))
#define RTC_DCHECK_LT(a, b) RTC_DCHECK((a) < (b))
#define RTC_DCHECK_GE(a, b) RTC_DCHECK((a) >= (b))
#define RTC_DCHECK_GT(a, b) RTC_DCHECK((a) > (b))

#endif

#endif
