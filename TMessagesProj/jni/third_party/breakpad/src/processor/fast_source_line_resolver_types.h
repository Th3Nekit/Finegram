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
// fast_source_line_resolver_types.h: definition of nested classes/structs in
// FastSourceLineResolver.  It moves the definitions out of
// fast_source_line_resolver.cc, so that other classes could have access
// to these private nested types without including fast_source_line_resolver.cc
//
// Author: lambxsy@google.com (Siyang Xie)

#ifndef PROCESSOR_FAST_SOURCE_LINE_RESOLVER_TYPES_H__
#define PROCESSOR_FAST_SOURCE_LINE_RESOLVER_TYPES_H__

#include "google_breakpad/processor/fast_source_line_resolver.h"
#include "processor/source_line_resolver_base_types.h"

#include <map>
#include <string>

#include "google_breakpad/processor/stack_frame.h"
#include "processor/cfi_frame_info.h"
#include "processor/static_address_map-inl.h"
#include "processor/static_contained_range_map-inl.h"
#include "processor/static_map.h"
#include "processor/static_range_map-inl.h"
#include "processor/windows_frame_info.h"

namespace google_breakpad {

struct FastSourceLineResolver::Line : public SourceLineResolverBase::Line {
  void CopyFrom(const Line *line_ptr) {
    const char *raw = reinterpret_cast<const char*>(line_ptr);
    CopyFrom(raw);
  }

  void CopyFrom(const char *raw) {
    address = *(reinterpret_cast<const MemAddr*>(raw));
    size = *(reinterpret_cast<const MemAddr*>(raw + sizeof(address)));
    source_file_id = *(reinterpret_cast<const int32_t *>(
        raw + 2 * sizeof(address)));
    line = *(reinterpret_cast<const int32_t*>(
        raw + 2 * sizeof(address) + sizeof(source_file_id)));
  }
};

struct FastSourceLineResolver::Function :
public SourceLineResolverBase::Function {
  void CopyFrom(const Function *func_ptr) {
    const char *raw = reinterpret_cast<const char*>(func_ptr);
    CopyFrom(raw);
  }

  void CopyFrom(const char *raw) {
    size_t name_size = strlen(raw) + 1;
    name = raw;
    address = *(reinterpret_cast<const MemAddr*>(raw + name_size));
    size = *(reinterpret_cast<const MemAddr*>(
        raw + name_size + sizeof(MemAddr)));
    parameter_size = *(reinterpret_cast<const int32_t*>(
        raw + name_size + 2 * sizeof(MemAddr)));
    lines = StaticRangeMap<MemAddr, Line>(
        raw + name_size + 2 * sizeof(MemAddr) + sizeof(int32_t));
  }

  StaticRangeMap<MemAddr, Line> lines;
};

struct FastSourceLineResolver::PublicSymbol :
public SourceLineResolverBase::PublicSymbol {
  void CopyFrom(const PublicSymbol *public_symbol_ptr) {
    const char *raw = reinterpret_cast<const char*>(public_symbol_ptr);
    CopyFrom(raw);
  }

  void CopyFrom(const char *raw) {
    size_t name_size = strlen(raw) + 1;
    name = raw;
    address = *(reinterpret_cast<const MemAddr*>(raw + name_size));
    parameter_size = *(reinterpret_cast<const int32_t*>(
        raw + name_size + sizeof(MemAddr)));
  }
};

class FastSourceLineResolver::Module: public SourceLineResolverBase::Module {
 public:
  explicit Module(const string &name) : name_(name), is_corrupt_(false) { }
  virtual ~Module() { }

  virtual void LookupAddress(StackFrame *frame) const;

  virtual bool LoadMapFromMemory(char *memory_buffer,
                                 size_t memory_buffer_size);

  virtual bool IsCorrupt() const { return is_corrupt_; }

  virtual WindowsFrameInfo *FindWindowsFrameInfo(const StackFrame *frame) const;

  virtual CFIFrameInfo *FindCFIFrameInfo(const StackFrame *frame) const;

  static const int kNumberMaps_ = 5 + WindowsFrameInfo::STACK_INFO_LAST;

 private:
  friend class FastSourceLineResolver;
  friend class ModuleComparer;
  typedef StaticMap<int, char> FileMap;

  string name_;
  StaticMap<int, char> files_;
  StaticRangeMap<MemAddr, Function> functions_;
  StaticAddressMap<MemAddr, PublicSymbol> public_symbols_;
  bool is_corrupt_;

  StaticContainedRangeMap<MemAddr, char>
    windows_frame_info_[WindowsFrameInfo::STACK_INFO_LAST];

  StaticRangeMap<MemAddr, char> cfi_initial_rules_;

  StaticMap<MemAddr, char> cfi_delta_rules_;
};

}

#endif
