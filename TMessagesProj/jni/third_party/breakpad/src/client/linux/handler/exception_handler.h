// Copyright (c) 2010 Google Inc.
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

#ifndef CLIENT_LINUX_HANDLER_EXCEPTION_HANDLER_H_
#define CLIENT_LINUX_HANDLER_EXCEPTION_HANDLER_H_

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ucontext.h>

#include <string>

#include "client/linux/crash_generation/crash_generation_client.h"
#include "client/linux/handler/minidump_descriptor.h"
#include "client/linux/minidump_writer/minidump_writer.h"
#include "common/scoped_ptr.h"
#include "common/using_std_string.h"
#include "google_breakpad/common/minidump_format.h"

namespace google_breakpad {

class ExceptionHandler {
 public:

  typedef bool (*FilterCallback)(void *context);

  typedef bool (*MinidumpCallback)(const MinidumpDescriptor& descriptor,
                                   void* context,
                                   bool succeeded);

  typedef bool (*HandlerCallback)(const void* crash_context,
                                  size_t crash_context_size,
                                  void* context);

  ExceptionHandler(const MinidumpDescriptor& descriptor,
                   FilterCallback filter,
                   MinidumpCallback callback,
                   void* callback_context,
                   bool install_handler,
                   const int server_fd);
  ~ExceptionHandler();

  const MinidumpDescriptor& minidump_descriptor() const {
    return minidump_descriptor_;
  }

  void set_minidump_descriptor(const MinidumpDescriptor& descriptor) {
    minidump_descriptor_ = descriptor;
  }

  void set_crash_handler(HandlerCallback callback) {
    crash_handler_ = callback;
  }

  void set_crash_generation_client(CrashGenerationClient* client) {
    crash_generation_client_.reset(client);
  }

  bool WriteMinidump();

  static bool WriteMinidump(const string& dump_path,
                            MinidumpCallback callback,
                            void* callback_context);

  static bool WriteMinidumpForChild(pid_t child,
                                    pid_t child_blamed_thread,
                                    const string& dump_path,
                                    MinidumpCallback callback,
                                    void* callback_context);

  struct CrashContext {
    siginfo_t siginfo;
    pid_t tid;
    struct ucontext context;
#if !defined(__ARM_EABI__) && !defined(__mips__)

    fpstate_t float_state;
#endif
  };

  bool IsOutOfProcess() const {
    return crash_generation_client_.get() != NULL;
  }

  void AddMappingInfo(const string& name,
                      const uint8_t identifier[sizeof(MDGUID)],
                      uintptr_t start_address,
                      size_t mapping_size,
                      size_t file_offset);

  void RegisterAppMemory(void* ptr, size_t length);

  void UnregisterAppMemory(void* ptr);

  bool SimulateSignalDelivery(int sig);

  bool HandleSignal(int sig, siginfo_t* info, void* uc);

 private:

  static bool InstallHandlersLocked();

  static void RestoreHandlersLocked();

  void PreresolveSymbols();
  bool GenerateDump(CrashContext *context);
  void SendContinueSignalToChild();
  void WaitForContinueSignal();

  static void SignalHandler(int sig, siginfo_t* info, void* uc);
  static int ThreadEntry(void* arg);
  bool DoDump(pid_t crashing_process, const void* context,
              size_t context_size);

  const FilterCallback filter_;
  const MinidumpCallback callback_;
  void* const callback_context_;

  scoped_ptr<CrashGenerationClient> crash_generation_client_;

  MinidumpDescriptor minidump_descriptor_;

  volatile HandlerCallback crash_handler_;

  int fdes[2];

  MappingList mapping_list_;

  AppMemoryList app_memory_list_;
};

}

#endif
