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

// cfi_assembler.h: Define CFISection, a class for creating properly
// (and improperly) formatted DWARF CFI data for unit tests.

#ifndef PROCESSOR_CFI_ASSEMBLER_H_
#define PROCESSOR_CFI_ASSEMBLER_H_

#include <string>

#include "common/dwarf/dwarf2enums.h"
#include "common/test_assembler.h"
#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {

using dwarf2reader::DwarfPointerEncoding;
using google_breakpad::test_assembler::Endianness;
using google_breakpad::test_assembler::Label;
using google_breakpad::test_assembler::Section;

class CFISection: public Section {
 public:

  struct EncodedPointerBases {
    EncodedPointerBases() : cfi(), text(), data() { }

    uint64_t cfi;

    uint64_t text;

    uint64_t data;
  };

  CFISection(Endianness endianness, size_t address_size,
             bool eh_frame = false)
      : Section(endianness), address_size_(address_size), eh_frame_(eh_frame),
        pointer_encoding_(dwarf2reader::DW_EH_PE_absptr),
        encoded_pointer_bases_(), entry_length_(NULL), in_fde_(false) {

    start() = 0;
  }

  size_t AddressSize() const { return address_size_; }

  bool ContainsEHFrame() const { return eh_frame_; }

  void SetPointerEncoding(DwarfPointerEncoding encoding) {
    pointer_encoding_ = encoding;
  }

  void SetEncodedPointerBases(const EncodedPointerBases &bases) {
    encoded_pointer_bases_ = bases;
  }

  CFISection &CIEHeader(uint64_t code_alignment_factor,
                        int data_alignment_factor,
                        unsigned return_address_register,
                        uint8_t version = 3,
                        const string &augmentation = "",
                        bool dwarf64 = false);

  CFISection &FDEHeader(Label cie_pointer,
                        uint64_t initial_location,
                        uint64_t address_range,
                        bool dwarf64 = false);

  CFISection &FinishEntry();

  CFISection &Block(const string &block) {
    ULEB128(block.size());
    Append(block);
    return *this;
  }

  CFISection &Address(uint64_t address) {
    Section::Append(endianness(), address_size_, address);
    return *this;
  }
  CFISection &Address(Label address) {
    Section::Append(endianness(), address_size_, address);
    return *this;
  }

  CFISection &EncodedPointer(uint64_t address) {
    return EncodedPointer(address, pointer_encoding_, encoded_pointer_bases_);
  }
  CFISection &EncodedPointer(uint64_t address, DwarfPointerEncoding encoding) {
    return EncodedPointer(address, encoding, encoded_pointer_bases_);
  }
  CFISection &EncodedPointer(uint64_t address, DwarfPointerEncoding encoding,
                             const EncodedPointerBases &bases);

  CFISection &Mark(Label *label)   { Section::Mark(label); return *this; }
  CFISection &D8(uint8_t v)       { Section::D8(v);       return *this; }
  CFISection &D16(uint16_t v)     { Section::D16(v);      return *this; }
  CFISection &D16(Label v)         { Section::D16(v);      return *this; }
  CFISection &D32(uint32_t v)     { Section::D32(v);      return *this; }
  CFISection &D32(const Label &v)  { Section::D32(v);      return *this; }
  CFISection &D64(uint64_t v)     { Section::D64(v);      return *this; }
  CFISection &D64(const Label &v)  { Section::D64(v);      return *this; }
  CFISection &LEB128(long long v)  { Section::LEB128(v);   return *this; }
  CFISection &ULEB128(uint64_t v) { Section::ULEB128(v);  return *this; }

 private:

  struct PendingLength {
    Label length;
    Label start;
  };

  static const uint32_t kDwarf64InitialLengthMarker = 0xffffffffU;

  static const uint32_t kDwarf32CIEIdentifier = ~(uint32_t)0;
  static const uint64_t kDwarf64CIEIdentifier = ~(uint64_t)0;
  static const uint32_t kEHFrame32CIEIdentifier = 0;
  static const uint64_t kEHFrame64CIEIdentifier = 0;

  size_t address_size_;

  bool eh_frame_;

  DwarfPointerEncoding pointer_encoding_;

  EncodedPointerBases encoded_pointer_bases_;

  PendingLength *entry_length_;

  bool in_fde_;

  uint64_t fde_start_address_;
};

}

#endif
