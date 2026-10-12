// -*- mode: c++ -*-

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

// The DwarfLineToModule class accepts line number information from a
// DWARF parser and adds it to a google_breakpad::Module. The Module
// can write that data out as a Breakpad symbol file.

#ifndef COMMON_LINUX_DWARF_LINE_TO_MODULE_H
#define COMMON_LINUX_DWARF_LINE_TO_MODULE_H

#include <string>

#include "common/module.h"
#include "common/dwarf/dwarf2reader.h"
#include "common/using_std_string.h"

namespace google_breakpad {

class DwarfLineToModule: public dwarf2reader::LineInfoHandler {
 public:

  DwarfLineToModule(Module *module, const string& compilation_dir,
                    vector<Module::Line> *lines)
      : module_(module),
        compilation_dir_(compilation_dir),
        lines_(lines),
        highest_file_number_(-1),
        omitted_line_end_(0),
        warned_bad_file_number_(false),
        warned_bad_directory_number_(false) { }

  ~DwarfLineToModule() { }

  void DefineDir(const string &name, uint32 dir_num);
  void DefineFile(const string &name, int32 file_num,
                  uint32 dir_num, uint64 mod_time,
                  uint64 length);
  void AddLine(uint64 address, uint64 length,
               uint32 file_num, uint32 line_num, uint32 column_num);

 private:

  typedef std::map<uint32, string> DirectoryTable;
  typedef std::map<uint32, Module::File *> FileTable;

  Module *module_;

  string compilation_dir_;

  vector<Module::Line> *lines_;

  DirectoryTable directories_;

  FileTable files_;

  int32 highest_file_number_;

  uint64 omitted_line_end_;

  bool warned_bad_file_number_;
  bool warned_bad_directory_number_;
};

}

#endif
