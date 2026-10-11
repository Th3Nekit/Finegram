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

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// This file implements the google_breakpad::StabsReader class.
// See stabs_reader.h.

#include "common/stabs_reader.h"

#include <assert.h>
#include <stab.h>
#include <string.h>

#include <string>

#include "common/using_std_string.h"

using std::vector;

namespace google_breakpad {

StabsReader::EntryIterator::EntryIterator(const ByteBuffer *buffer,
                                          bool big_endian, size_t value_size)
    : value_size_(value_size), cursor_(buffer, big_endian) {

  assert(value_size == 4 || value_size == 8);
  entry_.index = 0;
  Fetch();
}

void StabsReader::EntryIterator::Fetch() {
  cursor_
      .Read(4, false, &entry_.name_offset)
      .Read(1, false, &entry_.type)
      .Read(1, false, &entry_.other)
      .Read(2, false, &entry_.descriptor)
      .Read(value_size_, false, &entry_.value);
  entry_.at_end = !cursor_;
}

StabsReader::StabsReader(const uint8_t *stab,    size_t stab_size,
                         const uint8_t *stabstr, size_t stabstr_size,
                         bool big_endian, size_t value_size, bool unitized,
                         StabsHandler *handler)
    : entries_(stab, stab_size),
      strings_(stabstr, stabstr_size),
      iterator_(&entries_, big_endian, value_size),
      unitized_(unitized),
      handler_(handler),
      string_offset_(0),
      next_cu_string_offset_(0),
      current_source_file_(NULL) { }

const char *StabsReader::SymbolString() {
  ptrdiff_t offset = string_offset_ + iterator_->name_offset;
  if (offset < 0 || (size_t) offset >= strings_.Size()) {
    handler_->Warning("symbol %d: name offset outside the string section\n",
                      iterator_->index);

    offset = 0;
  }
  return reinterpret_cast<const char *>(strings_.start + offset);
}

bool StabsReader::Process() {
  while (!iterator_->at_end) {
    if (iterator_->type == N_SO) {
      if (! ProcessCompilationUnit())
        return false;
    } else if (iterator_->type == N_UNDF && unitized_) {

      string_offset_ = next_cu_string_offset_;
      next_cu_string_offset_ = iterator_->value;
      ++iterator_;
    }
#if defined(HAVE_MACH_O_NLIST_H)

    else if ((iterator_->type & N_STAB) == 0 &&
               (iterator_->type & N_TYPE) == N_SECT) {
      ProcessExtern();
    }
#endif
    else {
      ++iterator_;
    }
  }
  return true;
}

bool StabsReader::ProcessCompilationUnit() {
  assert(!iterator_->at_end && iterator_->type == N_SO);

  const char *build_directory = NULL;
  {
    const char *name = SymbolString();
    if (name[0] && name[strlen(name) - 1] == '/') {
      build_directory = name;
      ++iterator_;
    }
  }

  {
    if (iterator_->at_end || iterator_->type != N_SO)
      return true;
    const char *name = SymbolString();
    if (name[0] == '\0') {

      ++iterator_;
      return true;
    }
    current_source_file_ = name;
  }

  if (! handler_->StartCompilationUnit(current_source_file_,
                                       iterator_->value,
                                       build_directory))
    return false;

  ++iterator_;

  while (!iterator_->at_end && iterator_->type != N_SO) {
    if (iterator_->type == N_FUN) {
      if (! ProcessFunction())
        return false;
    } else if (iterator_->type == N_SLINE) {

      Line line;

      line.address = iterator_->value;
      line.filename = current_source_file_;

      line.number = (uint16_t) iterator_->descriptor;
      queued_lines_.push_back(line);
      ++iterator_;
    } else if (iterator_->type == N_SOL) {
      current_source_file_ = SymbolString();
      ++iterator_;
    } else {

      ++iterator_;
    }
  }

  uint64_t ending_address = 0;
  if (!iterator_->at_end) {
    assert(iterator_->type == N_SO);
    const char *name = SymbolString();
    if (name[0] == '\0') {
      ending_address = iterator_->value;
      ++iterator_;
    }
  }

  if (! handler_->EndCompilationUnit(ending_address))
    return false;

  queued_lines_.clear();

  return true;
}

bool StabsReader::ProcessFunction() {
  assert(!iterator_->at_end && iterator_->type == N_FUN);

  uint64_t function_address = iterator_->value;

  const char *stab_string = SymbolString();
  const char *name_end = strchr(stab_string, ':');
  if (! name_end)
    name_end = stab_string + strlen(stab_string);
  string name(stab_string, name_end - stab_string);
  if (! handler_->StartFunction(name, function_address))
    return false;
  ++iterator_;

  for (vector<Line>::const_iterator it = queued_lines_.begin();
       it != queued_lines_.end(); it++) {
    if (!handler_->Line(it->address, it->filename, it->number))
      return false;
  }
  queued_lines_.clear();

  while (!iterator_->at_end) {
    if (iterator_->type == N_SO || iterator_->type == N_FUN)
      break;
    else if (iterator_->type == N_SLINE) {

      uint64_t line_address = function_address + iterator_->value;

      uint16_t line_number = iterator_->descriptor;
      if (! handler_->Line(line_address, current_source_file_, line_number))
        return false;
      ++iterator_;
    } else if (iterator_->type == N_SOL) {
      current_source_file_ = SymbolString();
      ++iterator_;
    } else

      ++iterator_;
  }

  uint64_t ending_address = 0;
  if (!iterator_->at_end) {
    assert(iterator_->type == N_SO || iterator_->type == N_FUN);
    if (iterator_->type == N_FUN) {
      const char *symbol_name = SymbolString();
      if (symbol_name[0] == '\0') {

        ending_address = function_address + iterator_->value;
        ++iterator_;
      } else {

        ending_address = iterator_->value;
      }
    } else {

      ending_address = iterator_->value;
    }
  }

  if (! handler_->EndFunction(ending_address))
    return false;

  return true;
}

bool StabsReader::ProcessExtern() {
#if defined(HAVE_MACH_O_NLIST_H)
  assert(!iterator_->at_end &&
         (iterator_->type & N_STAB) == 0 &&
         (iterator_->type & N_TYPE) == N_SECT);
#endif

  if (!handler_->Extern(SymbolString(), iterator_->value))
    return false;

  ++iterator_;
  return true;
}

}
