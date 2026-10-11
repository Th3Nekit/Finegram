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

// dump_stabs.cc --- implement the StabsToModule class.

#include <assert.h>
#include <cxxabi.h>
#include <stdarg.h>
#include <stdio.h>

#include <algorithm>

#include "common/stabs_to_module.h"
#include "common/using_std_string.h"

namespace google_breakpad {

static string Demangle(const string &mangled) {
  int status = 0;
  char *demangled = abi::__cxa_demangle(mangled.c_str(), NULL, NULL, &status);
  if (status == 0 && demangled != NULL) {
    string str(demangled);
    free(demangled);
    return str;
  }
  return string(mangled);
}

StabsToModule::~StabsToModule() {

  for (vector<Module::Function *>::const_iterator func_it = functions_.begin();
       func_it != functions_.end(); func_it++)
    delete *func_it;

  delete current_function_;
}

bool StabsToModule::StartCompilationUnit(const char *name, uint64_t address,
                                         const char *build_directory) {
  assert(!in_compilation_unit_);
  in_compilation_unit_ = true;
  current_source_file_name_ = name;
  current_source_file_ = module_->FindFile(name);
  comp_unit_base_address_ = address;
  boundaries_.push_back(static_cast<Module::Address>(address));
  return true;
}

bool StabsToModule::EndCompilationUnit(uint64_t address) {
  assert(in_compilation_unit_);
  in_compilation_unit_ = false;
  comp_unit_base_address_ = 0;
  current_source_file_ = NULL;
  current_source_file_name_ = NULL;
  if (address)
    boundaries_.push_back(static_cast<Module::Address>(address));
  return true;
}

bool StabsToModule::StartFunction(const string &name,
                                  uint64_t address) {
  assert(!current_function_);
  Module::Function *f = new Module::Function(Demangle(name), address);
  f->size = 0;
  f->parameter_size = 0;
  current_function_ = f;
  boundaries_.push_back(static_cast<Module::Address>(address));
  return true;
}

bool StabsToModule::EndFunction(uint64_t address) {
  assert(current_function_);

  if (current_function_->address >= comp_unit_base_address_)
    functions_.push_back(current_function_);
  else
    delete current_function_;
  current_function_ = NULL;
  if (address)
    boundaries_.push_back(static_cast<Module::Address>(address));
  return true;
}

bool StabsToModule::Line(uint64_t address, const char *name, int number) {
  assert(current_function_);
  assert(current_source_file_);
  if (name != current_source_file_name_) {
    current_source_file_ = module_->FindFile(name);
    current_source_file_name_ = name;
  }
  Module::Line line;
  line.address = address;
  line.size = 0;
  line.file = current_source_file_;
  line.number = number;
  current_function_->lines.push_back(line);
  return true;
}

bool StabsToModule::Extern(const string &name, uint64_t address) {
  Module::Extern *ext = new Module::Extern(address);

  if (name.compare(0, 3, "__Z") == 0) {
    ext->name = Demangle(name.substr(1));
  } else if (name[0] == '_') {
    ext->name = name.substr(1);
  } else {
    ext->name = name;
  }
  module_->AddExtern(ext);
  return true;
}

void StabsToModule::Warning(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
}

void StabsToModule::Finalize() {

  sort(boundaries_.begin(), boundaries_.end());

  sort(functions_.begin(), functions_.end(),
       Module::Function::CompareByAddress);

  for (vector<Module::Function *>::const_iterator func_it = functions_.begin();
       func_it != functions_.end();
       func_it++) {
    Module::Function *f = *func_it;

    vector<Module::Address>::const_iterator boundary
        = std::upper_bound(boundaries_.begin(), boundaries_.end(), f->address);
    if (boundary != boundaries_.end())
      f->size = *boundary - f->address;
    else

      f->size = kFallbackSize;

    if (!f->lines.empty()) {
      stable_sort(f->lines.begin(), f->lines.end(),
                  Module::Line::CompareByAddress);
      vector<Module::Line>::iterator last_line = f->lines.end() - 1;
      for (vector<Module::Line>::iterator line_it = f->lines.begin();
           line_it != last_line; line_it++)
        line_it[0].size = line_it[1].address - line_it[0].address;

      last_line->size = (f->address + f->size) - last_line->address;
    }
  }

  module_->AddFunctions(functions_.begin(), functions_.end());
  functions_.clear();
}

}
