// -*- mode: c++ -*-

// Copyright (c) 2010, Google Inc.
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

// dwarf_cfi_to_module.h: Define the DwarfCFIToModule class, which
// accepts parsed DWARF call frame info and adds it to a
// google_breakpad::Module object, which can write that information to
// a Breakpad symbol file.

#ifndef COMMON_LINUX_DWARF_CFI_TO_MODULE_H
#define COMMON_LINUX_DWARF_CFI_TO_MODULE_H

#include <assert.h>
#include <stdio.h>

#include <set>
#include <string>
#include <vector>

#include "common/module.h"
#include "common/dwarf/dwarf2reader.h"
#include "common/using_std_string.h"

namespace google_breakpad {

using dwarf2reader::CallFrameInfo;
using google_breakpad::Module;
using std::set;
using std::vector;

class DwarfCFIToModule: public CallFrameInfo::Handler {
 public:

  class Reporter {
   public:

    Reporter(const string &file, const string &section)
      : file_(file), section_(section) { }
    virtual ~Reporter() { }

    virtual void UnnamedRegister(size_t offset, int reg);

    virtual void UndefinedNotSupported(size_t offset, const string &reg);

    virtual void ExpressionsNotSupported(size_t offset, const string &reg);

  protected:
    string file_, section_;
  };

  class RegisterNames {
   public:

    static vector<string> I386();

    static vector<string> X86_64();

    static vector<string> ARM();

    static vector<string> ARM64();

    static vector<string> MIPS();

   private:

    static vector<string> MakeVector(const char * const *strings, size_t size);
  };

  DwarfCFIToModule(Module *module, const vector<string> &register_names,
                   Reporter *reporter)
      : module_(module), register_names_(register_names), reporter_(reporter),
        entry_(NULL), return_address_(-1), cfa_name_(".cfa"), ra_name_(".ra") {
  }
  virtual ~DwarfCFIToModule() { delete entry_; }

  virtual bool Entry(size_t offset, uint64 address, uint64 length,
                     uint8 version, const string &augmentation,
                     unsigned return_address);
  virtual bool UndefinedRule(uint64 address, int reg);
  virtual bool SameValueRule(uint64 address, int reg);
  virtual bool OffsetRule(uint64 address, int reg,
                          int base_register, long offset);
  virtual bool ValOffsetRule(uint64 address, int reg,
                             int base_register, long offset);
  virtual bool RegisterRule(uint64 address, int reg, int base_register);
  virtual bool ExpressionRule(uint64 address, int reg,
                              const string &expression);
  virtual bool ValExpressionRule(uint64 address, int reg,
                                 const string &expression);
  virtual bool End();

 private:

  string RegisterName(int i);

  void Record(Module::Address address, int reg, const string &rule);

  Module *module_;

  const vector<string> &register_names_;

  Reporter *reporter_;

  Module::StackFrameEntry *entry_;

  size_t entry_offset_;

  unsigned return_address_;

  string cfa_name_, ra_name_;

  set<string> common_strings_;
};

}

#endif
