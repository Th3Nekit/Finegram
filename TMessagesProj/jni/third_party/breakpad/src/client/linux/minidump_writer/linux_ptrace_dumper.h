// Copyright (c) 2012, Google Inc.
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

// linux_ptrace_dumper.h: Define the google_breakpad::LinuxPtraceDumper
// class, which is derived from google_breakpad::LinuxDumper to extract
// information from a crashed process via ptrace.
// This class was originally splitted from google_breakpad::LinuxDumper.

#ifndef CLIENT_LINUX_MINIDUMP_WRITER_LINUX_PTRACE_DUMPER_H_
#define CLIENT_LINUX_MINIDUMP_WRITER_LINUX_PTRACE_DUMPER_H_

#include "client/linux/minidump_writer/linux_dumper.h"

namespace google_breakpad {

class LinuxPtraceDumper : public LinuxDumper {
 public:

  explicit LinuxPtraceDumper(pid_t pid);

  virtual bool BuildProcPath(char* path, pid_t pid, const char* node) const;

  virtual bool CopyFromProcess(void* dest, pid_t child, const void* src,
                               size_t length);

  virtual bool GetThreadInfoByIndex(size_t index, ThreadInfo* info);

  virtual bool IsPostMortem() const;

  virtual bool ThreadsSuspend();

  virtual bool ThreadsResume();

 protected:

  virtual bool EnumerateThreads();

 private:

  bool threads_suspended_;
};

}

#endif
