// -*- mode: C++ -*-

// Copyright (c) 2012 Google Inc.
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

// Helper class that encapsulates the logic of how symbol supplier interacts
// with source line resolver to fill stack frame information.

#ifndef GOOGLE_BREAKPAD_PROCESSOR_STACK_FRAME_SYMBOLIZER_H__
#define GOOGLE_BREAKPAD_PROCESSOR_STACK_FRAME_SYMBOLIZER_H__

#include <set>
#include <string>

#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/code_module.h"

namespace google_breakpad {
class CFIFrameInfo;
class CodeModules;
class SymbolSupplier;
class SourceLineResolverInterface;
struct StackFrame;
struct SystemInfo;
struct WindowsFrameInfo;

class StackFrameSymbolizer {
 public:
  enum SymbolizerResult {

    kNoError,

    kError,

    kInterrupt,

    kWarningCorruptSymbols,
  };

  StackFrameSymbolizer(SymbolSupplier* supplier,
                       SourceLineResolverInterface* resolver);

  virtual ~StackFrameSymbolizer() { }

  virtual SymbolizerResult FillSourceLineInfo(const CodeModules* modules,
                                              const SystemInfo* system_info,
                                              StackFrame* stack_frame);

  virtual WindowsFrameInfo* FindWindowsFrameInfo(const StackFrame* frame);

  virtual CFIFrameInfo* FindCFIFrameInfo(const StackFrame* frame);

  virtual void Reset() { no_symbol_modules_.clear(); }

  virtual bool HasImplementation() { return resolver_ && supplier_; }

  SourceLineResolverInterface* resolver() { return resolver_; }
  SymbolSupplier* supplier() { return supplier_; }

 protected:
  SymbolSupplier* supplier_;
  SourceLineResolverInterface* resolver_;

  std::set<string> no_symbol_modules_;
};

}

#endif
