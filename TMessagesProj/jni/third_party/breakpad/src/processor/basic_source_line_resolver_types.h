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
//
// basic_source_line_types.h: definition of nested classes/structs in
// BasicSourceLineResolver.  It moves the definitions out of
// basic_source_line_resolver.cc, so that other classes could have access
// to these private nested types without including basic_source_line_resolver.cc
//
// Author: Siyang Xie (lambxsy@google.com)

#ifndef PROCESSOR_BASIC_SOURCE_LINE_RESOLVER_TYPES_H__
#define PROCESSOR_BASIC_SOURCE_LINE_RESOLVER_TYPES_H__

#include <map>
#include <string>

#include "common/scoped_ptr.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "processor/source_line_resolver_base_types.h"

#include "processor/address_map-inl.h"
#include "processor/range_map-inl.h"
#include "processor/contained_range_map-inl.h"

#include "processor/linked_ptr.h"
#include "google_breakpad/processor/stack_frame.h"
#include "processor/cfi_frame_info.h"
#include "processor/windows_frame_info.h"

namespace google_breakpad {

struct
BasicSourceLineResolver::Function : public SourceLineResolverBase::Function {
  Function(const string &function_name,
           MemAddr function_address,
           MemAddr code_size,
           int set_parameter_size) : Base(function_name,
                                          function_address,
                                          code_size,
                                          set_parameter_size),
                                     lines() { }
  RangeMap< MemAddr, linked_ptr<Line> > lines;
 private:
  typedef SourceLineResolverBase::Function Base;
};

class BasicSourceLineResolver::Module : public SourceLineResolverBase::Module {
 public:
  explicit Module(const string &name) : name_(name), is_corrupt_(false) { }
  virtual ~Module() { }

  virtual bool LoadMapFromMemory(char *memory_buffer,
                                 size_t memory_buffer_size);

  virtual bool IsCorrupt() const { return is_corrupt_; }

  virtual void LookupAddress(StackFrame *frame) const;

  virtual WindowsFrameInfo *FindWindowsFrameInfo(const StackFrame *frame) const;

  virtual CFIFrameInfo *FindCFIFrameInfo(const StackFrame *frame) const;

 private:

  friend class BasicSourceLineResolver;
  friend class ModuleComparer;
  friend class ModuleSerializer;

  typedef std::map<int, string> FileMap;

  static void LogParseError(
      const string &message,
      int line_number,
      int *num_errors);

  bool ParseFile(char *file_line);

  Function* ParseFunction(char *function_line);

  Line* ParseLine(char *line_line);

  bool ParsePublicSymbol(char *public_line);

  bool ParseStackInfo(char *stack_info_line);

  bool ParseCFIFrameInfo(char *stack_info_line);

  string name_;
  FileMap files_;
  RangeMap< MemAddr, linked_ptr<Function> > functions_;
  AddressMap< MemAddr, linked_ptr<PublicSymbol> > public_symbols_;
  bool is_corrupt_;

  ContainedRangeMap< MemAddr, linked_ptr<WindowsFrameInfo> >
    windows_frame_info_[WindowsFrameInfo::STACK_INFO_LAST];

  RangeMap<MemAddr, string> cfi_initial_rules_;

  std::map<MemAddr, string> cfi_delta_rules_;
};

}

#endif
