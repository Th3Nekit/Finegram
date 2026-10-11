// Copyright (c) 2011, Google Inc.
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

// elf_core_dump.h: Define the google_breakpad::ElfCoreDump class, which
// encapsulates an ELF core dump file mapped into memory.

#ifndef COMMON_LINUX_ELF_CORE_DUMP_H_
#define COMMON_LINUX_ELF_CORE_DUMP_H_

#include <elf.h>
#include <link.h>
#include <stddef.h>

#include "common/memory_range.h"

namespace google_breakpad {

class ElfCoreDump {
 public:

  typedef ElfW(Ehdr) Ehdr;
  typedef ElfW(Nhdr) Nhdr;
  typedef ElfW(Phdr) Phdr;
  typedef ElfW(Word) Word;
  typedef ElfW(Addr) Addr;
#if __WORDSIZE == 32
  static const int kClass = ELFCLASS32;
#elif __WORDSIZE == 64
  static const int kClass = ELFCLASS64;
#else
#error "Unsupported __WORDSIZE for ElfCoreDump."
#endif

  class Note {
   public:
    Note();

    explicit Note(const MemoryRange& content);

    bool IsValid() const;

    const Nhdr* GetHeader() const;

    Word GetType() const;

    MemoryRange GetName() const;

    MemoryRange GetDescription() const;

    Note GetNextNote() const;

   private:

    static size_t AlignedSize(size_t size);

    MemoryRange content_;
  };

  ElfCoreDump();

  explicit ElfCoreDump(const MemoryRange& content);

  void SetContent(const MemoryRange& content);

  bool IsValid() const;

  const Ehdr* GetHeader() const;

  const Phdr* GetProgramHeader(unsigned index) const;

  const Phdr* GetFirstProgramHeaderOfType(Word type) const;

  unsigned GetProgramHeaderCount() const;

  bool CopyData(void* buffer, Addr virtual_address, size_t length);

  Note GetFirstNote() const;

 private:

  MemoryRange content_;
};

}

#endif
