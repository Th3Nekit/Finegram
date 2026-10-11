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

// dwarf_line_to_module.cc: Implementation of DwarfLineToModule class.
// See dwarf_line_to_module.h for details.

#include <stdio.h>

#include <string>

#include "common/dwarf_line_to_module.h"
#include "common/using_std_string.h"

static bool PathIsAbsolute(const string &path) {
  return (path.size() >= 1 && path[0] == '/');
}

static bool HasTrailingSlash(const string &path) {
  return (path.size() >= 1 && path[path.size() - 1] == '/');
}

static string ExpandPath(const string &path,
                         const string &base) {
  if (PathIsAbsolute(path) || base.empty())
    return path;
  return base + (HasTrailingSlash(base) ? "" : "/") + path;
}

namespace google_breakpad {

void DwarfLineToModule::DefineDir(const string &name, uint32 dir_num) {

  if (dir_num != 0)
    directories_[dir_num] = ExpandPath(name, compilation_dir_);
}

void DwarfLineToModule::DefineFile(const string &name, int32 file_num,
                                   uint32 dir_num, uint64 mod_time,
                                   uint64 length) {
  if (file_num == -1)
    file_num = ++highest_file_number_;
  else if (file_num > highest_file_number_)
    highest_file_number_ = file_num;

  string dir_name;
  if (dir_num == 0) {

    dir_name = compilation_dir_;
  } else {
    DirectoryTable::const_iterator directory_it = directories_.find(dir_num);
    if (directory_it != directories_.end()) {
      dir_name = directory_it->second;
    } else {
      if (!warned_bad_directory_number_) {
        fprintf(stderr, "warning: DWARF line number data refers to undefined"
                " directory numbers\n");
        warned_bad_directory_number_ = true;
      }
    }
  }

  string full_name = ExpandPath(name, dir_name);

  files_[file_num] = module_->FindFile(full_name);
}

void DwarfLineToModule::AddLine(uint64 address, uint64 length,
                                uint32 file_num, uint32 line_num,
                                uint32 column_num) {
  if (length == 0)
    return;

  if (address + length < address)
    length = -address;

  if (address == 0 || address == omitted_line_end_) {
    omitted_line_end_ = address + length;
    return;
  } else {
    omitted_line_end_ = 0;
  }

  Module::File *file = files_[file_num];
  if (!file) {
    if (!warned_bad_file_number_) {
      fprintf(stderr, "warning: DWARF line number data refers to "
              "undefined file numbers\n");
      warned_bad_file_number_ = true;
    }
    return;
  }
  Module::Line line;
  line.address = address;

  line.size = length;
  line.file = file;
  line.number = line_num;
  lines_->push_back(line);
}

}
