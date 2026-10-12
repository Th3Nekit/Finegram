/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/stacktrace/stacktrace.h"

#include <dlfcn.h>
#include <errno.h>
#include <linux/futex.h>
#include <sys/ptrace.h>
#include <sys/ucontext.h>
#include <syscall.h>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>

#include <atomic>

#if defined(DS)
#undef DS
#endif

#include "absl/base/attributes.h"
#include "rtc_base/logging.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {

namespace {

constexpr size_t kMaxStackSize = 100;

constexpr int kSignal = SIGURG;

class AsyncSafeWaitableEvent {
 public:
  AsyncSafeWaitableEvent() {
    std::atomic_store_explicit(&futex_, 0, std::memory_order_release);
  }

  ~AsyncSafeWaitableEvent() {}

  bool Wait() {

    while (true) {
      int res = syscall(SYS_futex, &futex_, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0,
                        nullptr, nullptr, 0);
      if (std::atomic_load_explicit(&futex_, std::memory_order_acquire) != 0)
        return true;
      if (res != 0)
        return false;
    }
  }

  void Signal() {
    std::atomic_store_explicit(&futex_, 1, std::memory_order_release);
    syscall(SYS_futex, &futex_, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr,
            nullptr, 0);
  }

 private:
  std::atomic<int> futex_;
};

struct SignalHandlerOutputState {

  static _Unwind_Reason_Code UnwindBacktrace(
      struct _Unwind_Context* unwind_context,
      void* unwind_output_state);

  AsyncSafeWaitableEvent signal_handler_finish_event;

  size_t stack_size_counter = 0;

  uintptr_t addresses[kMaxStackSize];
};

_Unwind_Reason_Code SignalHandlerOutputState::UnwindBacktrace(
    struct _Unwind_Context* unwind_context,
    void* unwind_output_state) {
  SignalHandlerOutputState* const output_state =
      static_cast<SignalHandlerOutputState*>(unwind_output_state);

  if (output_state == nullptr)
    return _URC_END_OF_STACK;

  if (output_state->stack_size_counter >= kMaxStackSize)
    return _URC_END_OF_STACK;

  output_state->addresses[output_state->stack_size_counter] =
      _Unwind_GetIP(unwind_context) - 2;
  ++output_state->stack_size_counter;

  return _URC_NO_REASON;
}

class GlobalStackUnwinder {
 public:
  static GlobalStackUnwinder& Get() {
    static GlobalStackUnwinder* const instance = new GlobalStackUnwinder();
    return *instance;
  }
  const char* CaptureRawStacktrace(int pid,
                                   int tid,
                                   SignalHandlerOutputState* params);

 private:
  GlobalStackUnwinder() { current_output_state_.store(nullptr); }

  static void SignalHandler(int signum, siginfo_t* info, void* ptr);

  Mutex mutex_;

  static std::atomic<SignalHandlerOutputState*> current_output_state_;

  static_assert(std::atomic<SignalHandlerOutputState*>::is_always_lock_free);
};

std::atomic<SignalHandlerOutputState*>
    GlobalStackUnwinder::current_output_state_;

void GlobalStackUnwinder::SignalHandler(int signum,
                                        siginfo_t* info,
                                        void* ptr) {

  SignalHandlerOutputState* signal_handler_output_state =
      current_output_state_.load();
  if (signal_handler_output_state != nullptr) {
    _Unwind_Backtrace(&SignalHandlerOutputState::UnwindBacktrace,
                      signal_handler_output_state);
    signal_handler_output_state->signal_handler_finish_event.Signal();
  }
}

const char* GlobalStackUnwinder::CaptureRawStacktrace(
    int pid,
    int tid,
    SignalHandlerOutputState* params) {

  struct sigaction act;
  struct sigaction old_act;
  memset(&act, 0, sizeof(act));
  act.sa_sigaction = &SignalHandler;
  act.sa_flags = SA_RESTART | SA_SIGINFO;
  sigemptyset(&act.sa_mask);

  MutexLock loch(&mutex_);
  current_output_state_.store(params);

  if (sigaction(kSignal, &act, &old_act) != 0)
    return "Failed to change signal action";

  if (tgkill(pid, tid, kSignal) != 0)
    return "Failed to interrupt thread";

  if (!params->signal_handler_finish_event.Wait())
    return "Failed to wait for thread to finish stack trace";

  sigaction(kSignal, &old_act,                nullptr);

  return nullptr;
}

std::vector<StackTraceElement> FormatStackTrace(
    const SignalHandlerOutputState& params) {
  std::vector<StackTraceElement> stack_trace;
  for (size_t i = 0; i < params.stack_size_counter; ++i) {
    const uintptr_t address = params.addresses[i];

    Dl_info dl_info = {};
    if (!dladdr(reinterpret_cast<void*>(address), &dl_info)) {
      RTC_LOG(LS_WARNING)
          << "Could not translate address to symbolic information for address "
          << address << " at stack depth " << i;
      continue;
    }

    StackTraceElement stack_trace_element;
    stack_trace_element.shared_object_path = dl_info.dli_fname;
    stack_trace_element.relative_address = static_cast<uint32_t>(
        address - reinterpret_cast<uintptr_t>(dl_info.dli_fbase));
    stack_trace_element.symbol_name = dl_info.dli_sname;

    stack_trace.push_back(stack_trace_element);
  }

  return stack_trace;
}

}

std::vector<StackTraceElement> GetStackTrace(int tid) {

  SignalHandlerOutputState params;

  const char* error_string =
      GlobalStackUnwinder::Get().CaptureRawStacktrace(getpid(), tid, &params);
  if (error_string != nullptr) {
    RTC_LOG(LS_ERROR) << error_string << ". tid: " << tid
                      << ". errno: " << errno;
    return {};
  }
  if (params.stack_size_counter >= kMaxStackSize) {
    RTC_LOG(LS_WARNING) << "Stack trace for thread " << tid << " was truncated";
  }
  return FormatStackTrace(params);
}

std::vector<StackTraceElement> GetStackTrace() {
  SignalHandlerOutputState params;
  _Unwind_Backtrace(&SignalHandlerOutputState::UnwindBacktrace, &params);
  if (params.stack_size_counter >= kMaxStackSize) {
    RTC_LOG(LS_WARNING) << "Stack trace was truncated";
  }
  return FormatStackTrace(params);
}

std::string StackTraceToString(
    const std::vector<StackTraceElement>& stack_trace) {
  rtc::StringBuilder string_builder;

  for (size_t i = 0; i < stack_trace.size(); ++i) {
    const StackTraceElement& stack_trace_element = stack_trace[i];
    string_builder.AppendFormat(
        "#%02zu pc %08x %s", i,
        static_cast<uint32_t>(stack_trace_element.relative_address),
        stack_trace_element.shared_object_path);

    if (stack_trace_element.symbol_name != nullptr)
      string_builder.AppendFormat(" %s", stack_trace_element.symbol_name);

    string_builder.AppendFormat("\n");
  }

  return string_builder.Release();
}

}
