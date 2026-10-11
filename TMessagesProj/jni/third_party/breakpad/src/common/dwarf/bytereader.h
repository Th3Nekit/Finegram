// -*- mode: C++ -*-

// Copyright (c) 2010 Google Inc. All Rights Reserved.
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

#ifndef COMMON_DWARF_BYTEREADER_H__
#define COMMON_DWARF_BYTEREADER_H__

#include <string>
#include "common/dwarf/types.h"
#include "common/dwarf/dwarf2enums.h"

namespace dwarf2reader {

enum Endianness {
  ENDIANNESS_BIG,
  ENDIANNESS_LITTLE
};

class ByteReader {
 public:

  explicit ByteReader(enum Endianness endianness);
  virtual ~ByteReader();

  uint8 ReadOneByte(const char* buffer) const;

  uint16 ReadTwoBytes(const char* buffer) const;

  uint64 ReadFourBytes(const char* buffer) const;

  uint64 ReadEightBytes(const char* buffer) const;

  uint64 ReadUnsignedLEB128(const char* buffer, size_t* len) const;

  int64 ReadSignedLEB128(const char* buffer, size_t* len) const;

  void SetAddressSize(uint8 size);

  uint8 AddressSize() const { return address_size_; }

  uint64 ReadAddress(const char* buffer) const;

  uint64 ReadInitialLength(const char* start, size_t* len);

  uint64 ReadOffset(const char* buffer) const;

  uint8 OffsetSize() const { return offset_size_; }

  void SetOffsetSize(uint8 size);

  void SetCFIDataBase(uint64 section_base, const char *buffer_base);

  void SetTextBase(uint64 text_base);

  void SetDataBase(uint64 data_base);

  void SetFunctionBase(uint64 function_base);

  void ClearFunctionBase();

  bool ValidEncoding(DwarfPointerEncoding encoding) const;

  bool UsableEncoding(DwarfPointerEncoding encoding) const;

  uint64 ReadEncodedPointer(const char *buffer, DwarfPointerEncoding encoding,
                            size_t *len) const;

 private:

  typedef uint64 (ByteReader::*AddressReader)(const char*) const;

  AddressReader offset_reader_;

  AddressReader address_reader_;

  Endianness endian_;
  uint8 address_size_;
  uint8 offset_size_;

  bool have_section_base_, have_text_base_, have_data_base_;
  bool have_function_base_;
  uint64 section_base_, text_base_, data_base_, function_base_;
  const char *buffer_base_;
};

}

#endif
