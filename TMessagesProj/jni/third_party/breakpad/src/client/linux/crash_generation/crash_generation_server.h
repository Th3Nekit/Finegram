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

#ifndef CLIENT_LINUX_CRASH_GENERATION_CRASH_GENERATION_SERVER_H_
#define CLIENT_LINUX_CRASH_GENERATION_CRASH_GENERATION_SERVER_H_

#include <pthread.h>

#include <string>

#include "common/using_std_string.h"

namespace google_breakpad {

class ClientInfo;

class CrashGenerationServer {
public:

  typedef void (*OnClientDumpRequestCallback)(void* context,
                                              const ClientInfo* client_info,
                                              const string* file_path);

  typedef void (*OnClientExitingCallback)(void* context,
                                          const ClientInfo* client_info);

  CrashGenerationServer(const int listen_fd,
                        OnClientDumpRequestCallback dump_callback,
                        void* dump_context,
                        OnClientExitingCallback exit_callback,
                        void* exit_context,
                        bool generate_dumps,
                        const string* dump_path);

  ~CrashGenerationServer();

  bool Start();

  void Stop();

  static bool CreateReportChannel(int* server_fd, int* client_fd);

private:

  void Run();

  bool ClientEvent(short revents);

  bool ControlEvent(short revents);

  bool MakeMinidumpFilename(string& outFilename);

  static void* ThreadMain(void* arg);

  int server_fd_;

  OnClientDumpRequestCallback dump_callback_;
  void* dump_context_;

  OnClientExitingCallback exit_callback_;
  void* exit_context_;

  bool generate_dumps_;

  string dump_dir_;

  bool started_;

  pthread_t thread_;
  int control_pipe_in_;
  int control_pipe_out_;

  CrashGenerationServer(const CrashGenerationServer&);
  CrashGenerationServer& operator=(const CrashGenerationServer&);
};

}

#endif
