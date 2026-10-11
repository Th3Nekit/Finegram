// Copyright (c) 2008, Google Inc.
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
// Author: Satoru Takabayashi
//
// Implementation of InstallFailureSignalHandler().

#include "utilities.h"
#include "stacktrace.h"
#include "symbolize.h"
#include "glog/logging.h"

#include <signal.h>
#include <time.h>
#ifdef HAVE_UCONTEXT_H
# include <ucontext.h>
#endif
#ifdef HAVE_SYS_UCONTEXT_H
# include <sys/ucontext.h>
#endif
#include <algorithm>

_START_GOOGLE_NAMESPACE_

namespace {

const struct {
  int number;
  const char *name;
} kFailureSignals[] = {
  { SIGSEGV, "SIGSEGV" },
  { SIGILL, "SIGILL" },
  { SIGFPE, "SIGFPE" },
  { SIGABRT, "SIGABRT" },
  { SIGBUS, "SIGBUS" },
  { SIGTERM, "SIGTERM" },
};

void* GetPC(void* ucontext_in_void) {
#if (defined(HAVE_UCONTEXT_H) || defined(HAVE_SYS_UCONTEXT_H)) && defined(PC_FROM_UCONTEXT)
  if (ucontext_in_void != NULL) {
    ucontext_t *context = reinterpret_cast<ucontext_t *>(ucontext_in_void);
    return (void*)context->PC_FROM_UCONTEXT;
  }
#endif
  return NULL;
}

class MinimalFormatter {
 public:
  MinimalFormatter(char *buffer, int size)
      : buffer_(buffer),
        cursor_(buffer),
        end_(buffer + size) {
  }

  int num_bytes_written() const { return cursor_ - buffer_; }

  void AppendString(const char* str) {
    int i = 0;
    while (str[i] != '\0' && cursor_ + i < end_) {
      cursor_[i] = str[i];
      ++i;
    }
    cursor_ += i;
  }

  void AppendUint64(uint64 number, int radix) {
    int i = 0;
    while (cursor_ + i < end_) {
      const int tmp = number % radix;
      number /= radix;
      cursor_[i] = (tmp < 10 ? '0' + tmp : 'a' + tmp - 10);
      ++i;
      if (number == 0) {
        break;
      }
    }

    std::reverse(cursor_, cursor_ + i);
    cursor_ += i;
  }

  void AppendHexWithPadding(uint64 number, int width) {
    char* start = cursor_;
    AppendString("0x");
    AppendUint64(number, 16);

    if (cursor_ < start + width) {
      const int64 delta = start + width - cursor_;
      std::copy(start, cursor_, start + delta);
      std::fill(start, start + delta, ' ');
      cursor_ = start + width;
    }
  }

 private:
  char *buffer_;
  char *cursor_;
  const char * const end_;
};

void WriteToStderr(const char* data, int size) {
  if (write(STDERR_FILENO, data, size) < 0) {

  }
}

void (*g_failure_writer)(const char* data, int size) = WriteToStderr;

void DumpTimeInfo() {
  time_t time_in_sec = time(NULL);
  char buf[256];
  MinimalFormatter formatter(buf, sizeof(buf));
  formatter.AppendString("*** Aborted at ");
  formatter.AppendUint64(time_in_sec, 10);
  formatter.AppendString(" (unix time)");
  formatter.AppendString(" try \"date -d @");
  formatter.AppendUint64(time_in_sec, 10);
  formatter.AppendString("\" if you are using GNU date ***\n");
  g_failure_writer(buf, formatter.num_bytes_written());
}

void DumpSignalInfo(int signal_number, siginfo_t *siginfo) {

  const char* signal_name = NULL;
  for (int i = 0; i < ARRAYSIZE(kFailureSignals); ++i) {
    if (signal_number == kFailureSignals[i].number) {
      signal_name = kFailureSignals[i].name;
    }
  }

  char buf[256];
  MinimalFormatter formatter(buf, sizeof(buf));

  formatter.AppendString("*** ");
  if (signal_name) {
    formatter.AppendString(signal_name);
  } else {

    formatter.AppendString("Signal ");
    formatter.AppendUint64(signal_number, 10);
  }
  formatter.AppendString(" (@0x");
  formatter.AppendUint64(reinterpret_cast<uintptr_t>(siginfo->si_addr), 16);
  formatter.AppendString(")");
  formatter.AppendString(" received by PID ");
  formatter.AppendUint64(getpid(), 10);
  formatter.AppendString(" (TID 0x");

  formatter.AppendUint64((uintptr_t)pthread_self(), 16);
  formatter.AppendString(") ");

#ifdef OS_LINUX
  formatter.AppendString("from PID ");
  formatter.AppendUint64(siginfo->si_pid, 10);
  formatter.AppendString("; ");
#endif
  formatter.AppendString("stack trace: ***\n");
  g_failure_writer(buf, formatter.num_bytes_written());
}

void DumpStackFrameInfo(const char* prefix, void* pc) {

  const char *symbol = "(unknown)";
  char symbolized[1024];

  if (Symbolize(reinterpret_cast<char *>(pc) - 1,
                symbolized, sizeof(symbolized))) {
    symbol = symbolized;
  }

  char buf[1024];
  MinimalFormatter formatter(buf, sizeof(buf));

  formatter.AppendString(prefix);
  formatter.AppendString("@ ");
  const int width = 2 * sizeof(void*) + 2;
  formatter.AppendHexWithPadding(reinterpret_cast<uintptr_t>(pc), width);
  formatter.AppendString(" ");
  formatter.AppendString(symbol);
  formatter.AppendString("\n");
  g_failure_writer(buf, formatter.num_bytes_written());
}

void InvokeDefaultSignalHandler(int signal_number) {
  struct sigaction sig_action;
  memset(&sig_action, 0, sizeof(sig_action));
  sigemptyset(&sig_action.sa_mask);
  sig_action.sa_handler = SIG_DFL;
  sigaction(signal_number, &sig_action, NULL);
  kill(getpid(), signal_number);
}

static pthread_t* g_entered_thread_id_pointer = NULL;

void FailureSignalHandler(int signal_number,
                          siginfo_t *signal_info,
                          void *ucontext) {

  pthread_t my_thread_id = pthread_self();

  pthread_t* old_thread_id_pointer =
      glog_internal_namespace_::sync_val_compare_and_swap(
          &g_entered_thread_id_pointer,
          static_cast<pthread_t*>(NULL),
          &my_thread_id);
  if (old_thread_id_pointer != NULL) {

    if (pthread_equal(my_thread_id, *g_entered_thread_id_pointer)) {

      InvokeDefaultSignalHandler(signal_number);
    }

    while (true) {
      sleep(1);
    }
  }

  DumpTimeInfo();

  void *pc = GetPC(ucontext);
  DumpStackFrameInfo("PC: ", pc);

#ifdef HAVE_STACKTRACE

  void *stack[32];

  const int depth = GetStackTrace(stack, ARRAYSIZE(stack), 1);
  DumpSignalInfo(signal_number, signal_info);

  for (int i = 0; i < depth; ++i) {
    DumpStackFrameInfo("    ", stack[i]);
  }
#endif

  FlushLogFilesUnsafe(0);

  InvokeDefaultSignalHandler(signal_number);
}

}

void InstallFailureSignalHandler() {

  struct sigaction sig_action;
  memset(&sig_action, 0, sizeof(sig_action));
  sigemptyset(&sig_action.sa_mask);
  sig_action.sa_flags |= SA_SIGINFO;
  sig_action.sa_sigaction = &FailureSignalHandler;

  for (int i = 0; i < ARRAYSIZE(kFailureSignals); ++i) {
    CHECK_ERR(sigaction(kFailureSignals[i].number, &sig_action, NULL));
  }
}

void InstallFailureWriter(void (*writer)(const char* data, int size)) {
  g_failure_writer = writer;
}

_END_GOOGLE_NAMESPACE_
