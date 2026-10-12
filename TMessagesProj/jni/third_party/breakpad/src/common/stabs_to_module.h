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

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// dump_stabs.h: Define the StabsToModule class, which receives
// STABS debugging information from a parser and adds it to a Breakpad
// symbol file.

#ifndef BREAKPAD_COMMON_STABS_TO_MODULE_H_
#define BREAKPAD_COMMON_STABS_TO_MODULE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "common/module.h"
#include "common/stabs_reader.h"
#include "common/using_std_string.h"

namespace google_breakpad {

using std::vector;

class StabsToModule: public google_breakpad::StabsHandler {
 public:

  StabsToModule(Module *module) :
      module_(module),
      in_compilation_unit_(false),
      comp_unit_base_address_(0),
      current_function_(NULL),
      current_source_file_(NULL),
      current_source_file_name_(NULL) { }
  ~StabsToModule();

  bool StartCompilationUnit(const char *name, uint64_t address,
                            const char *build_directory);
  bool EndCompilationUnit(uint64_t address);
  bool StartFunction(const string &name, uint64_t address);
  bool EndFunction(uint64_t address);
  bool Line(uint64_t address, const char *name, int number);
  bool Extern(const string &name, uint64_t address);
  void Warning(const char *format, ...);

  void Finalize();

 private:

  static const uint64_t kFallbackSize = 0x10000000;

  Module *module_;

  vector<Module::Function *> functions_;

  vector<Module::Address> boundaries_;

  bool in_compilation_unit_;

  Module::Address comp_unit_base_address_;

  Module::Function *current_function_;

  Module::File *current_source_file_;

  const char *current_source_file_name_;
};

}

#endif
