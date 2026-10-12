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
// source_line_resolver_base.h: SourceLineResolverBase, an (incomplete)
// implementation of SourceLineResolverInterface.  It serves as a common base
// class for concrete implementations: FastSourceLineResolver and
// BasicSourceLineResolver.  It is designed for refactoring that removes
// code redundancy in the two concrete source line resolver classes.
//
// See "google_breakpad/processor/source_line_resolver_interface.h" for more
// documentation.

// Author: Siyang Xie (lambxsy@google.com)

#ifndef GOOGLE_BREAKPAD_PROCESSOR_SOURCE_LINE_RESOLVER_BASE_H__
#define GOOGLE_BREAKPAD_PROCESSOR_SOURCE_LINE_RESOLVER_BASE_H__

#include <map>
#include <set>
#include <string>

#include "google_breakpad/processor/source_line_resolver_interface.h"

namespace google_breakpad {

using std::map;
using std::set;

class ModuleFactory;

class SourceLineResolverBase : public SourceLineResolverInterface {
 public:

  static bool ReadSymbolFile(const string &file_name,
                             char **symbol_data,
                             size_t *symbol_data_size);

 protected:

  SourceLineResolverBase(ModuleFactory *module_factory);
  virtual ~SourceLineResolverBase();

  virtual bool LoadModule(const CodeModule *module, const string &map_file);
  virtual bool LoadModuleUsingMapBuffer(const CodeModule *module,
                                        const string &map_buffer);
  virtual bool LoadModuleUsingMemoryBuffer(const CodeModule *module,
                                           char *memory_buffer,
                                           size_t memory_buffer_size);
  virtual bool ShouldDeleteMemoryBufferAfterLoadModule();
  virtual void UnloadModule(const CodeModule *module);
  virtual bool HasModule(const CodeModule *module);
  virtual bool IsModuleCorrupt(const CodeModule *module);
  virtual void FillSourceLineInfo(StackFrame *frame);
  virtual WindowsFrameInfo *FindWindowsFrameInfo(const StackFrame *frame);
  virtual CFIFrameInfo *FindCFIFrameInfo(const StackFrame *frame);

  struct Line;
  struct Function;
  struct PublicSymbol;
  struct CompareString {
    bool operator()(const string &s1, const string &s2) const;
  };

  class Module;
  class AutoFileCloser;

  typedef map<string, Module*, CompareString> ModuleMap;
  ModuleMap *modules_;

  typedef set<string, CompareString> ModuleSet;
  ModuleSet *corrupt_modules_;

  typedef std::map<string, char*, CompareString> MemoryMap;
  MemoryMap *memory_buffers_;

  ModuleFactory *module_factory_;

 private:

  friend class ModuleFactory;

  SourceLineResolverBase(const SourceLineResolverBase&);
  void operator=(const SourceLineResolverBase&);
};

}

#endif
