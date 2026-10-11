// Copyright 2023 The Chromium Authors

#include "third_party/jni_zero/logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <atomic>
#include <memory>
#ifndef JNI_ZERO_IS_ROBOLECTRIC
#include <android/log.h>
#endif

namespace jni_zero {

std::atomic<LogMessageCallback> g_log_callback{};

void SetLogMessageCallback(LogMessageCallback callback) {
  g_log_callback.store(callback, std::memory_order_relaxed);
}

void LogMessage(LogLev level,
                const char* fname,
                int line,
                const char* fmt,
                ...) {
  char stack_buf[512];
  std::unique_ptr<char[]> large_buf;
  char* log_msg = &stack_buf[0];

  for (size_t max_len = sizeof(stack_buf);;) {
    va_list args;
    va_start(args, fmt);
    int res = vsnprintf(log_msg, max_len, fmt, args);
    va_end(args);

    if (res < 0) {
      snprintf(log_msg, max_len, "%s", "[printf format error]");
      break;
    }

    if (res < static_cast<int>(max_len) || max_len >= 128 * 1024) {
      break;
    }

    max_len *= 4;
    large_buf.reset(new char[max_len]);
    log_msg = &large_buf[0];
  }

  LogMessageCallback cb = g_log_callback.load(std::memory_order_relaxed);
  if (cb) {
    cb({level, line, fname, log_msg});
    return;
  }

#ifdef JNI_ZERO_IS_ROBOLECTRIC
  fprintf(stderr, "%s:%d %s\n", fname, line, log_msg);
#else
  __android_log_print(int{ANDROID_LOG_DEBUG} + level, "jni_zero", "%s:%d %s",
                      fname, line, log_msg);
#endif
  if (level >= kLogFatal) {
    JNI_ZERO_IMMEDIATE_CRASH();
  }
}

}
