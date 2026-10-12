// -*- mode: c++ -*-

// Copyright (c) 2012, Google Inc.
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

// dwarf2reader_test_common.h: Define TestCompilationUnit and
// TestAbbrevTable, classes for creating properly (and improperly)
// formatted DWARF compilation unit data for unit tests.

#ifndef COMMON_DWARF_DWARF2READER_TEST_COMMON_H__
#define COMMON_DWARF_DWARF2READER_TEST_COMMON_H__

#include "common/test_assembler.h"
#include "common/dwarf/dwarf2enums.h"

class TestCompilationUnit: public google_breakpad::test_assembler::Section {
 public:
  typedef dwarf2reader::DwarfTag DwarfTag;
  typedef dwarf2reader::DwarfAttribute DwarfAttribute;
  typedef dwarf2reader::DwarfForm DwarfForm;
  typedef google_breakpad::test_assembler::Label Label;

  void set_format_size(size_t format_size) {
    assert(format_size == 4 || format_size == 8);
    format_size_ = format_size;
  }

  template<typename T>
  void SectionOffset(T offset) {
    if (format_size_ == 4)
      D32(offset);
    else
      D64(offset);
  }

  TestCompilationUnit &Header(int version, const Label &abbrev_offset,
                              size_t address_size) {
    if (format_size_ == 4) {
      D32(length_);
    } else {
      D32(0xffffffff);
      D64(length_);
    }
    post_length_offset_ = Size();
    D16(version);
    SectionOffset(abbrev_offset);
    D8(address_size);
    return *this;
  }

  TestCompilationUnit &Finish() {
    length_ = Size() - post_length_offset_;
    return *this;
  }

 private:

  size_t format_size_;

  uint64_t post_length_offset_;

  Label length_;
};

class TestAbbrevTable: public google_breakpad::test_assembler::Section {
 public:
  typedef dwarf2reader::DwarfTag DwarfTag;
  typedef dwarf2reader::DwarfAttribute DwarfAttribute;
  typedef dwarf2reader::DwarfForm DwarfForm;
  typedef dwarf2reader::DwarfHasChild DwarfHasChild;
  typedef google_breakpad::test_assembler::Label Label;

  TestAbbrevTable &Abbrev(int code, DwarfTag tag, DwarfHasChild has_children) {
    assert(code != 0);
    ULEB128(code);
    ULEB128(static_cast<unsigned>(tag));
    D8(static_cast<unsigned>(has_children));
    return *this;
  };

  TestAbbrevTable &Attribute(DwarfAttribute name, DwarfForm form) {
    ULEB128(static_cast<unsigned>(name));
    ULEB128(static_cast<unsigned>(form));
    return *this;
  }

  TestAbbrevTable &EndAbbrev() {
    ULEB128(0);
    ULEB128(0);
    return *this;
  }

  TestAbbrevTable &EndTable() {
    ULEB128(0);
    return *this;
  }
};

#endif
