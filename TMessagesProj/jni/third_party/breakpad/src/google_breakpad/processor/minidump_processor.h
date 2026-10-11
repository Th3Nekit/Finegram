// Copyright (c) 2006, Google Inc.
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

#ifndef GOOGLE_BREAKPAD_PROCESSOR_MINIDUMP_PROCESSOR_H__
#define GOOGLE_BREAKPAD_PROCESSOR_MINIDUMP_PROCESSOR_H__

#include <assert.h>
#include <string>

#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/process_result.h"

namespace google_breakpad {

class Minidump;
class ProcessState;
class StackFrameSymbolizer;
class SourceLineResolverInterface;
class SymbolSupplier;
struct SystemInfo;

class MinidumpProcessor {
 public:

  MinidumpProcessor(SymbolSupplier* supplier,
                    SourceLineResolverInterface* resolver);

  MinidumpProcessor(SymbolSupplier* supplier,
                    SourceLineResolverInterface* resolver,
                    bool enable_exploitability);

  MinidumpProcessor(StackFrameSymbolizer* stack_frame_symbolizer,
                    bool enable_exploitability);

  ~MinidumpProcessor();

  ProcessResult Process(const string &minidump_file,
                        ProcessState* process_state);

  ProcessResult Process(Minidump* minidump,
                        ProcessState* process_state);

  static bool GetCPUInfo(Minidump* dump, SystemInfo* info);

  static bool GetOSInfo(Minidump* dump, SystemInfo* info);

  static bool GetProcessCreateTime(Minidump* dump,
                                   uint32_t* process_create_time);

  static string GetCrashReason(Minidump* dump, uint64_t* address);

  static bool IsErrorUnrecoverable(ProcessResult p) {
    assert(p !=  PROCESS_OK);
    return (p != PROCESS_SYMBOL_SUPPLIER_INTERRUPTED);
  }

  static string GetAssertion(Minidump* dump);

 private:
  StackFrameSymbolizer* frame_symbolizer_;

  bool own_frame_symbolizer_;

  bool enable_exploitability_;
};

}

#endif
