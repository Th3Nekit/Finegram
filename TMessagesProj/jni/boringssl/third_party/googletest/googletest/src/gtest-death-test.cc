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
// This file implements death tests.

#include "gtest/gtest-death-test.h"

#include <stdlib.h>

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "gtest/internal/custom/gtest.h"
#include "gtest/internal/gtest-port.h"

#ifdef GTEST_HAS_DEATH_TEST

#ifdef GTEST_OS_MAC
#include <crt_externs.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#ifdef GTEST_OS_LINUX
#include <signal.h>
#endif

#include <stdarg.h>

#ifdef GTEST_OS_WINDOWS
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/wait.h>
#endif

#ifdef GTEST_OS_QNX
#include <spawn.h>
#endif

#ifdef GTEST_OS_FUCHSIA
#include <lib/fdio/fd.h>
#include <lib/fdio/io.h>
#include <lib/fdio/spawn.h>
#include <lib/zx/channel.h>
#include <lib/zx/port.h>
#include <lib/zx/process.h>
#include <lib/zx/socket.h>
#include <zircon/processargs.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/policy.h>
#include <zircon/syscalls/port.h>
#endif

#endif

#include "gtest/gtest-message.h"
#include "gtest/internal/gtest-string.h"
#include "src/gtest-internal-inl.h"

namespace testing {

static const char kDefaultDeathTestStyle[] = GTEST_DEFAULT_DEATH_TEST_STYLE;

}

GTEST_DEFINE_string_(
    death_test_style,
    testing::internal::StringFromGTestEnv("death_test_style",
                                          testing::kDefaultDeathTestStyle),
    "Indicates how to run a death test in a forked child process: "
    "\"threadsafe\" (child process re-executes the test binary "
    "from the beginning, running only the specific death test) or "
    "\"fast\" (child process runs the death test immediately "
    "after forking).");

GTEST_DEFINE_bool_(
    death_test_use_fork,
    testing::internal::BoolFromGTestEnv("death_test_use_fork", false),
    "Instructs to use fork()/_Exit() instead of clone() in death tests. "
    "Ignored and always uses fork() on POSIX systems where clone() is not "
    "implemented. Useful when running under valgrind or similar tools if "
    "those do not support clone(). Valgrind 3.3.1 will just fail if "
    "it sees an unsupported combination of clone() flags. "
    "It is not recommended to use this flag w/o valgrind though it will "
    "work in 99% of the cases. Once valgrind is fixed, this flag will "
    "most likely be removed.");

GTEST_DEFINE_string_(
    internal_run_death_test, "",
    "Indicates the file, line number, temporal index of "
    "the single death test to run, and a file descriptor to "
    "which a success code may be sent, all separated by "
    "the '|' characters.  This flag is specified if and only if the "
    "current process is a sub-process launched for running a thread-safe "
    "death test.  FOR INTERNAL USE ONLY.");

namespace testing {

#ifdef GTEST_HAS_DEATH_TEST

namespace internal {

#if !defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_FUCHSIA)
static bool g_in_fast_death_test_child = false;
#endif

bool InDeathTestChild() {
#if defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_FUCHSIA)

  return !GTEST_FLAG_GET(internal_run_death_test).empty();

#else

  if (GTEST_FLAG_GET(death_test_style) == "threadsafe")
    return !GTEST_FLAG_GET(internal_run_death_test).empty();
  else
    return g_in_fast_death_test_child;
#endif
}

}

ExitedWithCode::ExitedWithCode(int exit_code) : exit_code_(exit_code) {}

bool ExitedWithCode::operator()(int exit_status) const {
#if defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_FUCHSIA)

  return exit_status == exit_code_;

#else

  return WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == exit_code_;

#endif
}

#if !defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_FUCHSIA)

KilledBySignal::KilledBySignal(int signum) : signum_(signum) {}

bool KilledBySignal::operator()(int exit_status) const {
#if defined(GTEST_KILLED_BY_SIGNAL_OVERRIDE_)
  {
    bool result;
    if (GTEST_KILLED_BY_SIGNAL_OVERRIDE_(signum_, exit_status, &result)) {
      return result;
    }
  }
#endif
  return WIFSIGNALED(exit_status) && WTERMSIG(exit_status) == signum_;
}
#endif

namespace internal {

static std::string ExitSummary(int exit_code) {
  Message m;

#if defined(GTEST_OS_WINDOWS) || defined(GTEST_OS_FUCHSIA)

  m << "Exited with exit status " << exit_code;

#else

  if (WIFEXITED(exit_code)) {
    m << "Exited with exit status " << WEXITSTATUS(exit_code);
  } else if (WIFSIGNALED(exit_code)) {
    m << "Terminated by signal " << WTERMSIG(exit_code);
  }
#ifdef WCOREDUMP
  if (WCOREDUMP(exit_code)) {
    m << " (core dumped)";
  }
#endif
#endif

  return m.GetString();
}

bool ExitedUnsuccessfully(int exit_status) {
  return !ExitedWithCode(0)(exit_status);
}

#if !defined(GTEST_OS_WINDOWS) && !defined(GTEST_OS_FUCHSIA)

static std::string DeathTestThreadWarning(size_t thread_count) {
  Message msg;
  msg << "Death tests use fork(), which is unsafe particularly"
      << " in a threaded context. For this test, " << GTEST_NAME_ << " ";
  if (thread_count == 0) {
    msg << "couldn't detect the number of threads.";
  } else {
    msg << "detected " << thread_count << " threads.";
  }
  msg << " See "
         "https://github.com/google/googletest/blob/main/docs/"
         "advanced.md#death-tests-and-threads"
      << " for more explanation and suggested solutions, especially if"
      << " this is the last message you see before your test times out.";
  return msg.GetString();
}
#endif

static const char kDeathTestLived = 'L';
static const char kDeathTestReturned = 'R';
static const char kDeathTestThrew = 'T';
static const char kDeathTestInternalError = 'I';

#ifdef GTEST_OS_FUCHSIA

static const int kFuchsiaReadPipeFd = 3;

#endif

enum DeathTestOutcome { IN_PROGRESS, DIED, LIVED, RETURNED, THREW };

[[noreturn]] static void DeathTestAbort(const std::string& message) {

  const InternalRunDeathTestFlag* const flag =
      GetUnitTestImpl()->internal_run_death_test_flag();
  if (flag != nullptr) {
    FILE* parent = posix::FDOpen(flag->write_fd(), "w");
    fputc(kDeathTestInternalError, parent);
    fprintf(parent, "%s", message.c_str());
    fflush(parent);
    _Exit(1);
  } else {
    fprintf(stderr, "%s", message.c_str());
    fflush(stderr);
    posix::Abort();
  }
}

#define GTEST_DEATH_TEST_CHECK_(expression)                              \
  do {                                                                   \
    if (!::testing::internal::IsTrue(expression)) {                      \
      DeathTestAbort(::std::string("CHECK failed: File ") + __FILE__ +   \
                     ", line " +                                         \
                     ::testing::internal::StreamableToString(__LINE__) + \
                     ": " + #expression);                                \
    }                                                                    \
  } while (::testing::internal::AlwaysFalse())

#define GTEST_DEATH_TEST_CHECK_SYSCALL_(expression)                      \
  do {                                                                   \
    int gtest_retval;                                                    \
    do {                                                                 \
      gtest_retval = (expression);                                       \
    } while (gtest_retval == -1 && errno == EINTR);                      \
    if (gtest_retval == -1) {                                            \
      DeathTestAbort(::std::string("CHECK failed: File ") + __FILE__ +   \
                     ", line " +                                         \
                     ::testing::internal::StreamableToString(__LINE__) + \
                     ": " + #expression + " != -1");                     \
    }                                                                    \
  } while (::testing::internal::AlwaysFalse())

std::string GetLastErrnoDescription() {
  return errno == 0 ? "" : posix::StrError(errno);
}

static void FailFromInternalError(int fd) {
  Message error;
  char buffer[256];
  int num_read;

  do {
    while ((num_read = posix::Read(fd, buffer, 255)) > 0) {
      buffer[num_read] = '\0';
      error << buffer;
    }
  } while (num_read == -1 && errno == EINTR);

  if (num_read == 0) {
    GTEST_LOG_(FATAL) << error.GetString();
  } else {
    const int last_error = errno;
    GTEST_LOG_(FATAL) << "Error while reading death test internal: "
                      << GetLastErrnoDescription() << " [" << last_error << "]";
  }
}

DeathTest::DeathTest() {
  TestInfo* const info = GetUnitTestImpl()->current_test_info();
  if (info == nullptr) {
    DeathTestAbort(
        "Cannot run a death test outside of a TEST or "
        "TEST_F construct");
  }
}

bool DeathTest::Create(const char* statement,
                       Matcher<const std::string&> matcher, const char* file,
                       int line, DeathTest** test) {
  return GetUnitTestImpl()->death_test_factory()->Create(
      statement, std::move(matcher), file, line, test);
}

const char* DeathTest::LastMessage() {
  return last_death_test_message_.c_str();
}

void DeathTest::set_last_death_test_message(const std::string& message) {
  last_death_test_message_ = message;
}

std::string DeathTest::last_death_test_message_;

class DeathTestImpl : public DeathTest {
 protected:
  DeathTestImpl(const char* a_statement, Matcher<const std::string&> matcher)
      : statement_(a_statement),
        matcher_(std::move(matcher)),
        spawned_(false),
        status_(-1),
        outcome_(IN_PROGRESS),
        read_fd_(-1),
        write_fd_(-1) {}

  ~DeathTestImpl() override { GTEST_DEATH_TEST_CHECK_(read_fd_ == -1); }

  void Abort(AbortReason reason) override;
  bool Passed(bool status_ok) override;

  const char* statement() const { return statement_; }
  bool spawned() const { return spawned_; }
  void set_spawned(bool is_spawned) { spawned_ = is_spawned; }
  int status() const { return status_; }
  void set_status(int a_status) { status_ = a_status; }
  DeathTestOutcome outcome() const { return outcome_; }
  void set_outcome(DeathTestOutcome an_outcome) { outcome_ = an_outcome; }
  int read_fd() const { return read_fd_; }
  void set_read_fd(int fd) { read_fd_ = fd; }
  int write_fd() const { return write_fd_; }
  void set_write_fd(int fd) { write_fd_ = fd; }

  void ReadAndInterpretStatusByte();

  virtual std::string GetErrorLogs();

 private:

  const char* const statement_;

  Matcher<const std::string&> matcher_;

  bool spawned_;

  int status_;

  DeathTestOutcome outcome_;

  int read_fd_;

  int write_fd_;
};

void DeathTestImpl::ReadAndInterpretStatusByte() {
  char flag;
  int bytes_read;

  do {
    bytes_read = posix::Read(read_fd(), &flag, 1);
  } while (bytes_read == -1 && errno == EINTR);

  if (bytes_read == 0) {
    set_outcome(DIED);
  } else if (bytes_read == 1) {
    switch (flag) {
      case kDeathTestReturned:
        set_outcome(RETURNED);
        break;
      case kDeathTestThrew:
        set_outcome(THREW);
        break;
      case kDeathTestLived:
        set_outcome(LIVED);
        break;
      case kDeathTestInternalError:
        FailFromInternalError(read_fd());
        break;
      default:
        GTEST_LOG_(FATAL) << "Death test child process reported "
                          << "unexpected status byte ("
                          << static_cast<unsigned int>(flag) << ")";
    }
  } else {
    GTEST_LOG_(FATAL) << "Read from death test child process failed: "
                      << GetLastErrnoDescription();
  }
  GTEST_DEATH_TEST_CHECK_SYSCALL_(posix::Close(read_fd()));
  set_read_fd(-1);
}

std::string DeathTestImpl::GetErrorLogs() { return GetCapturedStderr(); }

void DeathTestImpl::Abort(AbortReason reason) {

  const char status_ch = reason == TEST_DID_NOT_DIE       ? kDeathTestLived
                         : reason == TEST_THREW_EXCEPTION ? kDeathTestThrew
                                                          : kDeathTestReturned;

  GTEST_DEATH_TEST_CHECK_SYSCALL_(posix::Write(write_fd(), &status_ch, 1));

  _Exit(1);
}

static ::std::string FormatDeathTestOutput(const ::std::string& output) {
  ::std::string ret;
  for (size_t at = 0;;) {
    const size_t line_end = output.find('\n', at);
    ret += "[  DEATH   ] ";
    if (line_end == ::std::string::npos) {
      ret += output.substr(at);
      break;
    }
    ret += output.substr(at, line_end + 1 - at);
    at = line_end + 1;
  }
  return ret;
}

bool DeathTestImpl::Passed(bool status_ok) {
  if (!spawned()) return false;

  const std::string error_message = GetErrorLogs();

  bool success = false;
  Message buffer;

  buffer << "Death test: " << statement() << "\n";
  switch (outcome()) {
    case LIVED:
      buffer << "    Result: failed to die.\n"
             << " Error msg:\n"
             << FormatDeathTestOutput(error_message);
      break;
    case THREW:
      buffer << "    Result: threw an exception.\n"
             << " Error msg:\n"
             << FormatDeathTestOutput(error_message);
      break;
    case RETURNED:
      buffer << "    Result: illegal return in test statement.\n"
             << " Error msg:\n"
             << FormatDeathTestOutput(error_message);
      break;
    case DIED:
      if (status_ok) {
        if (matcher_.Matches(error_message)) {
          success = true;
        } else {
          std::ostringstream stream;
          matcher_.DescribeTo(&stream);
          buffer << "    Result: died but not with expected error.\n"
                 << "  Expected: " << stream.str() << "\n"
                 << "Actual msg:\n"
                 << FormatDeathTestOutput(error_message);
        }
      } else {
        buffer << "    Result: died but not with expected exit code:\n"
               << "            " << ExitSummary(status()) << "\n"
               << "Actual msg:\n"
               << FormatDeathTestOutput(error_message);
      }
      break;
    case IN_PROGRESS:
    default:
      GTEST_LOG_(FATAL)
          << "DeathTest::Passed somehow called before conclusion of test";
  }

  DeathTest::set_last_death_test_message(buffer.GetString());
  return success;
}

#ifndef GTEST_OS_WINDOWS

static std::vector<char*> CreateArgvFromArgs(std::vector<std::string>& args) {
  std::vector<char*> result;
  result.reserve(args.size() + 1);
  for (auto& arg : args) {
    result.push_back(&arg[0]);
  }
  result.push_back(nullptr);
  return result;
}
#endif

#ifdef GTEST_OS_WINDOWS

class WindowsDeathTest : public DeathTestImpl {
 public:
  WindowsDeathTest(const char* a_statement, Matcher<const std::string&> matcher,
                   const char* file, int line)
      : DeathTestImpl(a_statement, std::move(matcher)),
        file_(file),
        line_(line) {}

  virtual int Wait();
  virtual TestRole AssumeRole();

 private:

  const char* const file_;

  const int line_;

  AutoHandle write_handle_;

  AutoHandle child_handle_;

  AutoHandle event_handle_;
};

int WindowsDeathTest::Wait() {
  if (!spawned()) return 0;

  const HANDLE wait_handles[2] = {child_handle_.Get(), event_handle_.Get()};
  switch (::WaitForMultipleObjects(2, wait_handles,
                                   FALSE,
                                   INFINITE)) {
    case WAIT_OBJECT_0:
    case WAIT_OBJECT_0 + 1:
      break;
    default:
      GTEST_DEATH_TEST_CHECK_(false);
  }

  write_handle_.Reset();
  event_handle_.Reset();

  ReadAndInterpretStatusByte();

  GTEST_DEATH_TEST_CHECK_(WAIT_OBJECT_0 ==
                          ::WaitForSingleObject(child_handle_.Get(), INFINITE));
  DWORD status_code;
  GTEST_DEATH_TEST_CHECK_(
      ::GetExitCodeProcess(child_handle_.Get(), &status_code) != FALSE);
  child_handle_.Reset();
  set_status(static_cast<int>(status_code));
  return status();
}

DeathTest::TestRole WindowsDeathTest::AssumeRole() {
  const UnitTestImpl* const impl = GetUnitTestImpl();
  const InternalRunDeathTestFlag* const flag =
      impl->internal_run_death_test_flag();
  const TestInfo* const info = impl->current_test_info();
  const int death_test_index = info->result()->death_test_count();

  if (flag != nullptr) {

    set_write_fd(flag->write_fd());
    return EXECUTE_TEST;
  }

  SECURITY_ATTRIBUTES handles_are_inheritable = {sizeof(SECURITY_ATTRIBUTES),
                                                 nullptr, TRUE};
  HANDLE read_handle, write_handle;
  GTEST_DEATH_TEST_CHECK_(::CreatePipe(&read_handle, &write_handle,
                                       &handles_are_inheritable,
                                       0)
                          != FALSE);
  set_read_fd(
      ::_open_osfhandle(reinterpret_cast<intptr_t>(read_handle), O_RDONLY));
  write_handle_.Reset(write_handle);
  event_handle_.Reset(::CreateEvent(
      &handles_are_inheritable,
      TRUE,
      FALSE,
      nullptr));
  GTEST_DEATH_TEST_CHECK_(event_handle_.Get() != nullptr);
  const std::string filter_flag = std::string("--") + GTEST_FLAG_PREFIX_ +
                                  "filter=" + info->test_suite_name() + "." +
                                  info->name();
  const std::string internal_flag =
      std::string("--") + GTEST_FLAG_PREFIX_ +
      "internal_run_death_test=" + file_ + "|" + StreamableToString(line_) +
      "|" + StreamableToString(death_test_index) + "|" +
      StreamableToString(static_cast<unsigned int>(::GetCurrentProcessId())) +

      "|" + StreamableToString(reinterpret_cast<size_t>(write_handle)) + "|" +
      StreamableToString(reinterpret_cast<size_t>(event_handle_.Get()));

  char executable_path[_MAX_PATH + 1];
  GTEST_DEATH_TEST_CHECK_(_MAX_PATH + 1 != ::GetModuleFileNameA(nullptr,
                                                                executable_path,
                                                                _MAX_PATH));

  std::string command_line = std::string(::GetCommandLineA()) + " " +
                             filter_flag + " \"" + internal_flag + "\"";

  DeathTest::set_last_death_test_message("");

  CaptureStderr();

  FlushInfoLog();

  STARTUPINFOA startup_info;
  memset(&startup_info, 0, sizeof(STARTUPINFO));
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
  startup_info.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
  startup_info.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);

  PROCESS_INFORMATION process_info;
  GTEST_DEATH_TEST_CHECK_(
      ::CreateProcessA(
          executable_path, const_cast<char*>(command_line.c_str()),
          nullptr,
          nullptr,
          TRUE,
          0x0,
          nullptr,
          UnitTest::GetInstance()->original_working_dir(), &startup_info,
          &process_info) != FALSE);
  child_handle_.Reset(process_info.hProcess);
  ::CloseHandle(process_info.hThread);
  set_spawned(true);
  return OVERSEE_TEST;
}

#elif defined(GTEST_OS_FUCHSIA)

class FuchsiaDeathTest : public DeathTestImpl {
 public:
  FuchsiaDeathTest(const char* a_statement, Matcher<const std::string&> matcher,
                   const char* file, int line)
      : DeathTestImpl(a_statement, std::move(matcher)),
        file_(file),
        line_(line) {}

  int Wait() override;
  TestRole AssumeRole() override;
  std::string GetErrorLogs() override;

 private:

  const char* const file_;

  const int line_;

  std::string captured_stderr_;

  zx::process child_process_;
  zx::channel exception_channel_;
  zx::socket stderr_socket_;
};

int FuchsiaDeathTest::Wait() {
  const int kProcessKey = 0;
  const int kSocketKey = 1;
  const int kExceptionKey = 2;

  if (!spawned()) return 0;

  zx_status_t status_zx;
  zx::port port;
  status_zx = zx::port::create(0, &port);
  GTEST_DEATH_TEST_CHECK_(status_zx == ZX_OK);

  status_zx =
      child_process_.wait_async(port, kProcessKey, ZX_PROCESS_TERMINATED, 0);
  GTEST_DEATH_TEST_CHECK_(status_zx == ZX_OK);

  status_zx = stderr_socket_.wait_async(
      port, kSocketKey, ZX_SOCKET_READABLE | ZX_SOCKET_PEER_CLOSED, 0);
  GTEST_DEATH_TEST_CHECK_(status_zx == ZX_OK);

  status_zx = exception_channel_.wait_async(port, kExceptionKey,
                                            ZX_CHANNEL_READABLE, 0);
  GTEST_DEATH_TEST_CHECK_(status_zx == ZX_OK);

  bool process_terminated = false;
  bool socket_closed = false;
  do {
    zx_port_packet_t packet = {};
    status_zx = port.wait(zx::time::infinite(), &packet);
    GTEST_DEATH_TEST_CHECK_(status_zx == ZX_OK);

    if (packet.key == kExceptionKey) {

      status_zx = child_process_.kill();
      GTEST_DEATH_TEST_CHECK_(status_zx == ZX_OK);
    } else if (packet.key == kProcessKey) {

      GTEST_DEATH_TEST_CHECK_(ZX_PKT_IS_SIGNAL_ONE(packet.type));
      GTEST_DEATH_TEST_CHECK_(packet.signal.observed & ZX_PROCESS_TERMINATED);
      process_terminated = true;
    } else if (packet.key == kSocketKey) {
      GTEST_DEATH_TEST_CHECK_(ZX_PKT_IS_SIGNAL_ONE(packet.type));
      if (packet.signal.observed & ZX_SOCKET_READABLE) {

        constexpr size_t kBufferSize = 1024;
        do {
          size_t old_length = captured_stderr_.length();
          size_t bytes_read = 0;
          captured_stderr_.resize(old_length + kBufferSize);
          status_zx =
              stderr_socket_.read(0, &captured_stderr_.front() + old_length,
                                  kBufferSize, &bytes_read);
          captured_stderr_.resize(old_length + bytes_read);
        } while (status_zx == ZX_OK);
        if (status_zx == ZX_ERR_PEER_CLOSED) {
          socket_closed = true;
        } else {
          GTEST_DEATH_TEST_CHECK_(status_zx == ZX_ERR_SHOULD_WAIT);
          status_zx = stderr_socket_.wait_async(
              port, kSocketKey, ZX_SOCKET_READABLE | ZX_SOCKET_PEER_CLOSED, 0);
          GTEST_DEATH_TEST_CHECK_(status_zx == ZX_OK);
        }
      } else {
        GTEST_DEATH_TEST_CHECK_(packet.signal.observed & ZX_SOCKET_PEER_CLOSED);
        socket_closed = true;
      }
    }
  } while (!process_terminated && !socket_closed);

  ReadAndInterpretStatusByte();

  zx_info_process_t buffer;
  status_zx = child_process_.get_info(ZX_INFO_PROCESS, &buffer, sizeof(buffer),
                                      nullptr, nullptr);
  GTEST_DEATH_TEST_CHECK_(status_zx == ZX_OK);

  GTEST_DEATH_TEST_CHECK_(buffer.flags & ZX_INFO_PROCESS_FLAG_EXITED);
  set_status(static_cast<int>(buffer.return_code));
  return status();
}

DeathTest::TestRole FuchsiaDeathTest::AssumeRole() {
  const UnitTestImpl* const impl = GetUnitTestImpl();
  const InternalRunDeathTestFlag* const flag =
      impl->internal_run_death_test_flag();
  const TestInfo* const info = impl->current_test_info();
  const int death_test_index = info->result()->death_test_count();

  if (flag != nullptr) {

    set_write_fd(kFuchsiaReadPipeFd);
    return EXECUTE_TEST;
  }

  FlushInfoLog();

  const std::string filter_flag = std::string("--") + GTEST_FLAG_PREFIX_ +
                                  "filter=" + info->test_suite_name() + "." +
                                  info->name();
  const std::string internal_flag = std::string("--") + GTEST_FLAG_PREFIX_ +
                                    kInternalRunDeathTestFlag + "=" + file_ +
                                    "|" + StreamableToString(line_) + "|" +
                                    StreamableToString(death_test_index);

  std::vector<std::string> args = GetInjectableArgvs();
  args.push_back(filter_flag);
  args.push_back(internal_flag);

  zx_status_t status;
  zx_handle_t child_pipe_handle;
  int child_pipe_fd;
  status = fdio_pipe_half(&child_pipe_fd, &child_pipe_handle);
  GTEST_DEATH_TEST_CHECK_(status == ZX_OK);
  set_read_fd(child_pipe_fd);

  fdio_spawn_action_t spawn_actions[2] = {};
  fdio_spawn_action_t* add_handle_action = &spawn_actions[0];
  add_handle_action->action = FDIO_SPAWN_ACTION_ADD_HANDLE;
  add_handle_action->h.id = PA_HND(PA_FD, kFuchsiaReadPipeFd);
  add_handle_action->h.handle = child_pipe_handle;

  zx::socket stderr_producer_socket;
  status = zx::socket::create(0, &stderr_producer_socket, &stderr_socket_);
  GTEST_DEATH_TEST_CHECK_(status >= 0);
  int stderr_producer_fd = -1;
  status =
      fdio_fd_create(stderr_producer_socket.release(), &stderr_producer_fd);
  GTEST_DEATH_TEST_CHECK_(status >= 0);

  GTEST_DEATH_TEST_CHECK_(fcntl(stderr_producer_fd, F_SETFL, 0) == 0);

  fdio_spawn_action_t* add_stderr_action = &spawn_actions[1];
  add_stderr_action->action = FDIO_SPAWN_ACTION_CLONE_FD;
  add_stderr_action->fd.local_fd = stderr_producer_fd;
  add_stderr_action->fd.target_fd = STDERR_FILENO;

  zx_handle_t child_job = ZX_HANDLE_INVALID;
  status = zx_job_create(zx_job_default(), 0, &child_job);
  GTEST_DEATH_TEST_CHECK_(status == ZX_OK);
  zx_policy_basic_t policy;
  policy.condition = ZX_POL_NEW_ANY;
  policy.policy = ZX_POL_ACTION_ALLOW;
  status = zx_job_set_policy(child_job, ZX_JOB_POL_RELATIVE, ZX_JOB_POL_BASIC,
                             &policy, 1);
  GTEST_DEATH_TEST_CHECK_(status == ZX_OK);

  status = zx_task_create_exception_channel(
      child_job, 0, exception_channel_.reset_and_get_address());
  GTEST_DEATH_TEST_CHECK_(status == ZX_OK);

  std::vector<char*> argv = CreateArgvFromArgs(args);
  status = fdio_spawn_etc(child_job, FDIO_SPAWN_CLONE_ALL, argv[0], argv.data(),
                          nullptr, 2, spawn_actions,
                          child_process_.reset_and_get_address(), nullptr);
  GTEST_DEATH_TEST_CHECK_(status == ZX_OK);

  set_spawned(true);
  return OVERSEE_TEST;
}

std::string FuchsiaDeathTest::GetErrorLogs() { return captured_stderr_; }

#else

class ForkingDeathTest : public DeathTestImpl {
 public:
  ForkingDeathTest(const char* statement, Matcher<const std::string&> matcher);

  int Wait() override;

 protected:
  void set_child_pid(pid_t child_pid) { child_pid_ = child_pid; }

 private:

  pid_t child_pid_;
};

ForkingDeathTest::ForkingDeathTest(const char* a_statement,
                                   Matcher<const std::string&> matcher)
    : DeathTestImpl(a_statement, std::move(matcher)), child_pid_(-1) {}

int ForkingDeathTest::Wait() {
  if (!spawned()) return 0;

  ReadAndInterpretStatusByte();

  int status_value;
  GTEST_DEATH_TEST_CHECK_SYSCALL_(waitpid(child_pid_, &status_value, 0));
  set_status(status_value);
  return status_value;
}

class NoExecDeathTest : public ForkingDeathTest {
 public:
  NoExecDeathTest(const char* a_statement, Matcher<const std::string&> matcher)
      : ForkingDeathTest(a_statement, std::move(matcher)) {}
  TestRole AssumeRole() override;
};

DeathTest::TestRole NoExecDeathTest::AssumeRole() {
  const size_t thread_count = GetThreadCount();
  if (thread_count != 1) {
    GTEST_LOG_(WARNING) << DeathTestThreadWarning(thread_count);
  }

  int pipe_fd[2];
  GTEST_DEATH_TEST_CHECK_(pipe(pipe_fd) != -1);

  DeathTest::set_last_death_test_message("");
  CaptureStderr();

  FlushInfoLog();

  const pid_t child_pid = fork();
  GTEST_DEATH_TEST_CHECK_(child_pid != -1);
  set_child_pid(child_pid);
  if (child_pid == 0) {
    GTEST_DEATH_TEST_CHECK_SYSCALL_(close(pipe_fd[0]));
    set_write_fd(pipe_fd[1]);

    LogToStderr();

    GetUnitTestImpl()->listeners()->SuppressEventForwarding(true);
    g_in_fast_death_test_child = true;
    return EXECUTE_TEST;
  } else {
    GTEST_DEATH_TEST_CHECK_SYSCALL_(close(pipe_fd[1]));
    set_read_fd(pipe_fd[0]);
    set_spawned(true);
    return OVERSEE_TEST;
  }
}

class ExecDeathTest : public ForkingDeathTest {
 public:
  ExecDeathTest(const char* a_statement, Matcher<const std::string&> matcher,
                const char* file, int line)
      : ForkingDeathTest(a_statement, std::move(matcher)),
        file_(file),
        line_(line) {}
  TestRole AssumeRole() override;

 private:
  static ::std::vector<std::string> GetArgvsForDeathTestChildProcess() {
    ::std::vector<std::string> args = GetInjectableArgvs();
#if defined(GTEST_EXTRA_DEATH_TEST_COMMAND_LINE_ARGS_)
    ::std::vector<std::string> extra_args =
        GTEST_EXTRA_DEATH_TEST_COMMAND_LINE_ARGS_();
    args.insert(args.end(), extra_args.begin(), extra_args.end());
#endif
    return args;
  }

  const char* const file_;

  const int line_;
};

struct ExecDeathTestArgs {
  char* const* argv;
  int close_fd;
};

#ifdef GTEST_OS_QNX
extern "C" char** environ;
#else

static int ExecDeathTestChildMain(void* child_arg) {
  ExecDeathTestArgs* const args = static_cast<ExecDeathTestArgs*>(child_arg);
  GTEST_DEATH_TEST_CHECK_SYSCALL_(close(args->close_fd));

  const char* const original_dir =
      UnitTest::GetInstance()->original_working_dir();

  if (chdir(original_dir) != 0) {
    DeathTestAbort(std::string("chdir(\"") + original_dir +
                   "\") failed: " + GetLastErrnoDescription());
    return EXIT_FAILURE;
  }

  execv(args->argv[0], args->argv);
  DeathTestAbort(std::string("execv(") + args->argv[0] + ", ...) in " +
                 original_dir + " failed: " + GetLastErrnoDescription());
  return EXIT_FAILURE;
}
#endif

#if GTEST_HAS_CLONE

static void StackLowerThanAddress(const void* ptr,
                                  bool* result) GTEST_NO_INLINE_;

GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
static void StackLowerThanAddress(const void* ptr, bool* result) {
  int dummy = 0;
  *result = std::less<const void*>()(&dummy, ptr);
}

GTEST_ATTRIBUTE_NO_SANITIZE_ADDRESS_
GTEST_ATTRIBUTE_NO_SANITIZE_HWADDRESS_
static bool StackGrowsDown() {
  int dummy = 0;
  bool result;
  StackLowerThanAddress(&dummy, &result);
  return result;
}
#endif

static pid_t ExecDeathTestSpawnChild(char* const* argv, int close_fd) {
  ExecDeathTestArgs args = {argv, close_fd};
  pid_t child_pid = -1;

#ifdef GTEST_OS_QNX

  const int cwd_fd = open(".", O_RDONLY);
  GTEST_DEATH_TEST_CHECK_(cwd_fd != -1);
  GTEST_DEATH_TEST_CHECK_SYSCALL_(fcntl(cwd_fd, F_SETFD, FD_CLOEXEC));

  const char* const original_dir =
      UnitTest::GetInstance()->original_working_dir();

  if (chdir(original_dir) != 0) {
    DeathTestAbort(std::string("chdir(\"") + original_dir +
                   "\") failed: " + GetLastErrnoDescription());
    return EXIT_FAILURE;
  }

  int fd_flags;

  GTEST_DEATH_TEST_CHECK_SYSCALL_(fd_flags = fcntl(close_fd, F_GETFD));
  GTEST_DEATH_TEST_CHECK_SYSCALL_(
      fcntl(close_fd, F_SETFD, fd_flags | FD_CLOEXEC));
  struct inheritance inherit = {0};

  child_pid = spawn(args.argv[0], 0, nullptr, &inherit, args.argv, environ);

  GTEST_DEATH_TEST_CHECK_(fchdir(cwd_fd) != -1);
  GTEST_DEATH_TEST_CHECK_SYSCALL_(close(cwd_fd));

#else
#ifdef GTEST_OS_LINUX

  struct sigaction saved_sigprof_action;
  struct sigaction ignore_sigprof_action;
  memset(&ignore_sigprof_action, 0, sizeof(ignore_sigprof_action));
  sigemptyset(&ignore_sigprof_action.sa_mask);
  ignore_sigprof_action.sa_handler = SIG_IGN;
  GTEST_DEATH_TEST_CHECK_SYSCALL_(
      sigaction(SIGPROF, &ignore_sigprof_action, &saved_sigprof_action));
#endif

#if GTEST_HAS_CLONE
  const bool use_fork = GTEST_FLAG_GET(death_test_use_fork);

  if (!use_fork) {
    static const bool stack_grows_down = StackGrowsDown();
    const auto stack_size = static_cast<size_t>(getpagesize() * 2);

    void* const stack = mmap(nullptr, stack_size, PROT_READ | PROT_WRITE,
                             MAP_ANON | MAP_PRIVATE, -1, 0);
    GTEST_DEATH_TEST_CHECK_(stack != MAP_FAILED);

    const size_t kMaxStackAlignment = 64;
    void* const stack_top =
        static_cast<char*>(stack) +
        (stack_grows_down ? stack_size - kMaxStackAlignment : 0);
    GTEST_DEATH_TEST_CHECK_(
        static_cast<size_t>(stack_size) > kMaxStackAlignment &&
        reinterpret_cast<uintptr_t>(stack_top) % kMaxStackAlignment == 0);

    child_pid = clone(&ExecDeathTestChildMain, stack_top, SIGCHLD, &args);

    GTEST_DEATH_TEST_CHECK_(munmap(stack, stack_size) != -1);
  }
#else
  const bool use_fork = true;
#endif

  if (use_fork && (child_pid = fork()) == 0) {
    _Exit(ExecDeathTestChildMain(&args));
  }
#endif
#ifdef GTEST_OS_LINUX
  GTEST_DEATH_TEST_CHECK_SYSCALL_(
      sigaction(SIGPROF, &saved_sigprof_action, nullptr));
#endif

  GTEST_DEATH_TEST_CHECK_(child_pid != -1);
  return child_pid;
}

DeathTest::TestRole ExecDeathTest::AssumeRole() {
  const UnitTestImpl* const impl = GetUnitTestImpl();
  const InternalRunDeathTestFlag* const flag =
      impl->internal_run_death_test_flag();
  const TestInfo* const info = impl->current_test_info();
  const int death_test_index = info->result()->death_test_count();

  if (flag != nullptr) {
    set_write_fd(flag->write_fd());
    return EXECUTE_TEST;
  }

  int pipe_fd[2];
  GTEST_DEATH_TEST_CHECK_(pipe(pipe_fd) != -1);

  GTEST_DEATH_TEST_CHECK_(fcntl(pipe_fd[1], F_SETFD, 0) != -1);

  const std::string filter_flag = std::string("--") + GTEST_FLAG_PREFIX_ +
                                  "filter=" + info->test_suite_name() + "." +
                                  info->name();
  const std::string internal_flag = std::string("--") + GTEST_FLAG_PREFIX_ +
                                    "internal_run_death_test=" + file_ + "|" +
                                    StreamableToString(line_) + "|" +
                                    StreamableToString(death_test_index) + "|" +
                                    StreamableToString(pipe_fd[1]);
  std::vector<std::string> args = GetArgvsForDeathTestChildProcess();
  args.push_back(filter_flag);
  args.push_back(internal_flag);

  DeathTest::set_last_death_test_message("");

  CaptureStderr();

  FlushInfoLog();

  std::vector<char*> argv = CreateArgvFromArgs(args);
  const pid_t child_pid = ExecDeathTestSpawnChild(argv.data(), pipe_fd[0]);
  GTEST_DEATH_TEST_CHECK_SYSCALL_(close(pipe_fd[1]));
  set_child_pid(child_pid);
  set_read_fd(pipe_fd[0]);
  set_spawned(true);
  return OVERSEE_TEST;
}

#endif

bool DefaultDeathTestFactory::Create(const char* statement,
                                     Matcher<const std::string&> matcher,
                                     const char* file, int line,
                                     DeathTest** test) {
  UnitTestImpl* const impl = GetUnitTestImpl();
  const InternalRunDeathTestFlag* const flag =
      impl->internal_run_death_test_flag();
  const int death_test_index =
      impl->current_test_info()->increment_death_test_count();

  if (flag != nullptr) {
    if (death_test_index > flag->index()) {
      DeathTest::set_last_death_test_message(
          "Death test count (" + StreamableToString(death_test_index) +
          ") somehow exceeded expected maximum (" +
          StreamableToString(flag->index()) + ")");
      return false;
    }

    if (!(flag->file() == file && flag->line() == line &&
          flag->index() == death_test_index)) {
      *test = nullptr;
      return true;
    }
  }

#ifdef GTEST_OS_WINDOWS

  if (GTEST_FLAG_GET(death_test_style) == "threadsafe" ||
      GTEST_FLAG_GET(death_test_style) == "fast") {
    *test = new WindowsDeathTest(statement, std::move(matcher), file, line);
  }

#elif defined(GTEST_OS_FUCHSIA)

  if (GTEST_FLAG_GET(death_test_style) == "threadsafe" ||
      GTEST_FLAG_GET(death_test_style) == "fast") {
    *test = new FuchsiaDeathTest(statement, std::move(matcher), file, line);
  }

#else

  if (GTEST_FLAG_GET(death_test_style) == "threadsafe") {
    *test = new ExecDeathTest(statement, std::move(matcher), file, line);
  } else if (GTEST_FLAG_GET(death_test_style) == "fast") {
    *test = new NoExecDeathTest(statement, std::move(matcher));
  }

#endif

  else {
    DeathTest::set_last_death_test_message("Unknown death test style \"" +
                                           GTEST_FLAG_GET(death_test_style) +
                                           "\" encountered");
    return false;
  }

  return true;
}

#ifdef GTEST_OS_WINDOWS

static int GetStatusFileDescriptor(unsigned int parent_process_id,
                                   size_t write_handle_as_size_t,
                                   size_t event_handle_as_size_t) {
  AutoHandle parent_process_handle(::OpenProcess(PROCESS_DUP_HANDLE,
                                                 FALSE,
                                                 parent_process_id));
  if (parent_process_handle.Get() == INVALID_HANDLE_VALUE) {
    DeathTestAbort("Unable to open parent process " +
                   StreamableToString(parent_process_id));
  }

  GTEST_CHECK_(sizeof(HANDLE) <= sizeof(size_t));

  const HANDLE write_handle = reinterpret_cast<HANDLE>(write_handle_as_size_t);
  HANDLE dup_write_handle;

  if (!::DuplicateHandle(parent_process_handle.Get(), write_handle,
                         ::GetCurrentProcess(), &dup_write_handle,
                         0x0,

                         FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    DeathTestAbort("Unable to duplicate the pipe handle " +
                   StreamableToString(write_handle_as_size_t) +
                   " from the parent process " +
                   StreamableToString(parent_process_id));
  }

  const HANDLE event_handle = reinterpret_cast<HANDLE>(event_handle_as_size_t);
  HANDLE dup_event_handle;

  if (!::DuplicateHandle(parent_process_handle.Get(), event_handle,
                         ::GetCurrentProcess(), &dup_event_handle, 0x0, FALSE,
                         DUPLICATE_SAME_ACCESS)) {
    DeathTestAbort("Unable to duplicate the event handle " +
                   StreamableToString(event_handle_as_size_t) +
                   " from the parent process " +
                   StreamableToString(parent_process_id));
  }

  const int write_fd =
      ::_open_osfhandle(reinterpret_cast<intptr_t>(dup_write_handle), O_APPEND);
  if (write_fd == -1) {
    DeathTestAbort("Unable to convert pipe handle " +
                   StreamableToString(write_handle_as_size_t) +
                   " to a file descriptor");
  }

  ::SetEvent(dup_event_handle);

  return write_fd;
}
#endif

InternalRunDeathTestFlag* ParseInternalRunDeathTestFlag() {
  if (GTEST_FLAG_GET(internal_run_death_test).empty()) return nullptr;

  int line = -1;
  int index = -1;
  ::std::vector< ::std::string> fields;
  SplitString(GTEST_FLAG_GET(internal_run_death_test), '|', &fields);
  int write_fd = -1;

#ifdef GTEST_OS_WINDOWS

  unsigned int parent_process_id = 0;
  size_t write_handle_as_size_t = 0;
  size_t event_handle_as_size_t = 0;

  if (fields.size() != 6 || !ParseNaturalNumber(fields[1], &line) ||
      !ParseNaturalNumber(fields[2], &index) ||
      !ParseNaturalNumber(fields[3], &parent_process_id) ||
      !ParseNaturalNumber(fields[4], &write_handle_as_size_t) ||
      !ParseNaturalNumber(fields[5], &event_handle_as_size_t)) {
    DeathTestAbort("Bad --gtest_internal_run_death_test flag: " +
                   GTEST_FLAG_GET(internal_run_death_test));
  }
  write_fd = GetStatusFileDescriptor(parent_process_id, write_handle_as_size_t,
                                     event_handle_as_size_t);

#elif defined(GTEST_OS_FUCHSIA)

  if (fields.size() != 3 || !ParseNaturalNumber(fields[1], &line) ||
      !ParseNaturalNumber(fields[2], &index)) {
    DeathTestAbort("Bad --gtest_internal_run_death_test flag: " +
                   GTEST_FLAG_GET(internal_run_death_test));
  }

#else

  if (fields.size() != 4 || !ParseNaturalNumber(fields[1], &line) ||
      !ParseNaturalNumber(fields[2], &index) ||
      !ParseNaturalNumber(fields[3], &write_fd)) {
    DeathTestAbort("Bad --gtest_internal_run_death_test flag: " +
                   GTEST_FLAG_GET(internal_run_death_test));
  }

#endif

  return new InternalRunDeathTestFlag(fields[0], line, index, write_fd);
}

}

#endif

}
