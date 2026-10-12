// Copyright (c) 2011, Google Inc.
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

// crash_generator.h: Define the google_breakpad::CrashGenerator class,
// which is used to generate a crash (and a core dump file) for testing.

#ifndef COMMON_LINUX_TESTS_CRASH_GENERATOR_H_
#define COMMON_LINUX_TESTS_CRASH_GENERATOR_H_

#include <sys/resource.h>

#include <string>

#include "common/tests/auto_tempdir.h"
#include "common/using_std_string.h"

namespace google_breakpad {

class CrashGenerator {
 public:
  CrashGenerator();

  ~CrashGenerator();

  bool HasDefaultCorePattern() const;

  string GetCoreFilePath() const;

  string GetDirectoryOfProcFilesCopy() const;

  bool CreateChildCrash(unsigned num_threads, unsigned crash_thread,
                        int crash_signal, pid_t* child_pid);

  pid_t GetThreadId(unsigned index) const;

 private:

  bool CopyProcFiles(pid_t pid, const char* path) const;

  void CreateThreadsInChildProcess(unsigned num_threads);

  bool SetCoreFileSizeLimit(rlim_t limit) const;

  bool MapSharedMemory(size_t memory_size);

  bool UnmapSharedMemory();

  pid_t* GetThreadIdPointer(unsigned index);

  AutoTempDir temp_dir_;

  void* shared_memory_;

  size_t shared_memory_size_;
};

}

#endif
