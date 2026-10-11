// -*- mode: C++ -*-

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

// Abstract interface to return function/file/line info for a memory address.

#ifndef GOOGLE_BREAKPAD_PROCESSOR_SOURCE_LINE_RESOLVER_INTERFACE_H__
#define GOOGLE_BREAKPAD_PROCESSOR_SOURCE_LINE_RESOLVER_INTERFACE_H__

#include <string>

#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/code_module.h"

namespace google_breakpad {

struct StackFrame;
struct WindowsFrameInfo;
class CFIFrameInfo;

class SourceLineResolverInterface {
 public:
  typedef uint64_t MemAddr;

  virtual ~SourceLineResolverInterface() {}

  virtual bool LoadModule(const CodeModule *module,
                          const string &map_file) = 0;

  virtual bool LoadModuleUsingMapBuffer(const CodeModule *module,
                                        const string &map_buffer) = 0;

  virtual bool LoadModuleUsingMemoryBuffer(const CodeModule *module,
                                           char *memory_buffer,
                                           size_t memory_buffer_size) = 0;

  virtual bool ShouldDeleteMemoryBufferAfterLoadModule() = 0;

  virtual void UnloadModule(const CodeModule *module) = 0;

  virtual bool HasModule(const CodeModule *module) = 0;

  virtual bool IsModuleCorrupt(const CodeModule *module) = 0;

  virtual void FillSourceLineInfo(StackFrame *frame) = 0;

  virtual WindowsFrameInfo *FindWindowsFrameInfo(const StackFrame *frame) = 0;

  virtual CFIFrameInfo *FindCFIFrameInfo(const StackFrame *frame) = 0;

 protected:

  SourceLineResolverInterface() {}
};

}

#endif
