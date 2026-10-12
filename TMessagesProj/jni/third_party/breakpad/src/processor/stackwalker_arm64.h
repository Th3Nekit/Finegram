// -*- mode: C++ -*-

// Copyright (c) 2013 Google Inc.
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

// stackwalker_arm64.h: arm64-specific stackwalker.
//
// Provides stack frames given arm64 register context and a memory region
// corresponding to an arm64 stack.
//
// Author: Mark Mentovai, Ted Mielczarek, Colin Blundell

#ifndef PROCESSOR_STACKWALKER_ARM64_H__
#define PROCESSOR_STACKWALKER_ARM64_H__

#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/common/minidump_format.h"
#include "google_breakpad/processor/stackwalker.h"

namespace google_breakpad {

class CodeModules;

class StackwalkerARM64 : public Stackwalker {
 public:

  StackwalkerARM64(const SystemInfo* system_info,
                   const MDRawContextARM64* context,
                   MemoryRegion* memory,
                   const CodeModules* modules,
                   StackFrameSymbolizer* frame_symbolizer);

  void SetContextFrameValidity(uint64_t valid) {
    context_frame_validity_ = valid;
  }

 private:

  virtual StackFrame* GetContextFrame();
  virtual StackFrame* GetCallerFrame(const CallStack* stack,
                                     bool stack_scan_allowed);

  StackFrameARM64* GetCallerByCFIFrameInfo(const vector<StackFrame*> &frames,
                                           CFIFrameInfo* cfi_frame_info);

  StackFrameARM64* GetCallerByFramePointer(const vector<StackFrame*> &frames);

  StackFrameARM64* GetCallerByStackScan(const vector<StackFrame*> &frames);

  const MDRawContextARM64* context_;

  uint64_t context_frame_validity_;
};

}

#endif
