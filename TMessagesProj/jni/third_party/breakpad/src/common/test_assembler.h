// -*- mode: C++ -*-

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

// test-assembler.h: interface to class for building complex binary streams.

// To test the Breakpad symbol dumper and processor thoroughly, for
// all combinations of host system and minidump processor
// architecture, we need to be able to easily generate complex test
// data like debugging information and minidump files.
//
// For example, if we want our unit tests to provide full code
// coverage for stack walking, it may be difficult to persuade the
// compiler to generate every possible sort of stack walking
// information that we want to support; there are probably DWARF CFI
// opcodes that GCC never emits. Similarly, if we want to test our
// error handling, we will need to generate damaged minidumps or
// debugging information that (we hope) the client or compiler will
// never produce on its own.
//
// google_breakpad::TestAssembler provides a predictable and
// (relatively) simple way to generate complex formatted data streams
// like minidumps and CFI. Furthermore, because TestAssembler is
// portable, developers without access to (say) Visual Studio or a
// SPARC assembler can still work on test data for those targets.

#ifndef PROCESSOR_TEST_ASSEMBLER_H_
#define PROCESSOR_TEST_ASSEMBLER_H_

#include <list>
#include <vector>
#include <string>

#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {

using std::list;
using std::vector;

namespace test_assembler {

class Label {
 public:
  Label();
  Label(uint64_t value);
  Label(const Label &value);
  ~Label();

  uint64_t Value() const;

  Label &operator=(uint64_t value);
  Label &operator=(const Label &value);
  Label operator+(uint64_t addend) const;
  Label operator-(uint64_t subtrahend) const;
  uint64_t operator-(const Label &subtrahend) const;

  bool IsKnownConstant(uint64_t *value_p = NULL) const;

  bool IsKnownOffsetFrom(const Label &label, uint64_t *offset_p = NULL) const;

 private:

  class Binding {
   public:
    Binding();
    Binding(uint64_t addend);
    ~Binding();

    void Acquire() { reference_count_++; };

    bool Release() { return --reference_count_ == 0; }

    void Set(Binding *binding, uint64_t value);

    void Get(Binding **base, uint64_t *addend);

   private:

    Binding *base_;
    uint64_t addend_;

    int reference_count_;
  };

  Binding *value_;
};

inline Label operator+(uint64_t a, const Label &l) { return l + a; }

enum Endianness {
  kBigEndian,
  kLittleEndian,
  kUnsetEndian,
};

class Section {
 public:
  Section(Endianness endianness = kUnsetEndian)
      : endianness_(endianness) { };

  virtual ~Section() { };

  void set_endianness(Endianness endianness) {
    endianness_ = endianness;
  }

  Endianness endianness() const { return endianness_; }

  Section &Append(const uint8_t *data, size_t size) {
    contents_.append(reinterpret_cast<const char *>(data), size);
    return *this;
  };
  Section &Append(const string &data) {
    contents_.append(data);
    return *this;
  };

  Section &Append(size_t size, uint8_t byte) {
    contents_.append(size, (char) byte);
    return *this;
  }

  Section &Append(Endianness endianness, size_t size, uint64_t number);
  Section &Append(Endianness endianness, size_t size, const Label &label);

  Section &Append(const Section &section);

  Section &AppendCString(const string &data) {
    Append(data);
    contents_ += '\0';
    return *this;
  }

  Section &AppendCString(const string &data, size_t size) {
    contents_.append(data, 0, size);
    if (data.size() < size)
      Append(size - data.size(), 0);
    return *this;
  }

  Section &L8(uint8_t value) { contents_ += value; return *this; }
  Section &B8(uint8_t value) { contents_ += value; return *this; }
  Section &D8(uint8_t value) { contents_ += value; return *this; }
  Section &L16(uint16_t), &L32(uint32_t), &L64(uint64_t),
          &B16(uint16_t), &B32(uint32_t), &B64(uint64_t),
          &D16(uint16_t), &D32(uint32_t), &D64(uint64_t);
  Section &L8(const Label &label),  &L16(const Label &label),
          &L32(const Label &label), &L64(const Label &label),
          &B8(const Label &label),  &B16(const Label &label),
          &B32(const Label &label), &B64(const Label &label),
          &D8(const Label &label),  &D16(const Label &label),
          &D32(const Label &label), &D64(const Label &label);

  Section &LEB128(long long value);

  Section &ULEB128(uint64_t value);

  Section &Align(size_t alignment, uint8_t pad_byte = 0);

  void Clear();

  size_t Size() const { return contents_.size(); }

  Label start() const { return start_; }

  Label Here() const { return start_ + Size(); }

  Section &Mark(Label *label) { *label = Here(); return *this; }

  bool GetContents(string *contents);

 private:

  struct Reference {
    Reference(size_t set_offset, Endianness set_endianness,  size_t set_size,
              const Label &set_label)
        : offset(set_offset), endianness(set_endianness), size(set_size),
          label(set_label) { }

    size_t offset;

    Endianness endianness;

    size_t size;

    Label label;
  };

  Endianness endianness_;

  string contents_;

  vector<Reference> references_;

  Label start_;
};

}
}

#endif
