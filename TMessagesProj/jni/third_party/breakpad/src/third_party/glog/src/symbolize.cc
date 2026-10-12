// Copyright (c) 2006, Google Inc.
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
//
// Author: Satoru Takabayashi
// Stack-footprint reduction work done by Raksit Ashok
//
// Implementation note:
//
// We don't use heaps but only use stacks.  We want to reduce the
// stack consumption so that the symbolizer can run on small stacks.
//
// Here are some numbers collected with GCC 4.1.0 on x86:
// - sizeof(Elf32_Sym)  = 16
// - sizeof(Elf32_Shdr) = 40
// - sizeof(Elf64_Sym)  = 24
// - sizeof(Elf64_Shdr) = 64
//
// This implementation is intended to be async-signal-safe but uses
// some functions which are not guaranteed to be so, such as memchr()
// and memmove().  We assume they are async-signal-safe.
//

#include "utilities.h"

#if defined(HAVE_SYMBOLIZE)

#include <limits>

#include "symbolize.h"
#include "demangle.h"

_START_GOOGLE_NAMESPACE_

static int AssertFail() {
  abort();
  return 0;
}

#define SAFE_ASSERT(expr) ((expr) ? 0 : AssertFail())

static SymbolizeCallback g_symbolize_callback = NULL;
void InstallSymbolizeCallback(SymbolizeCallback callback) {
  g_symbolize_callback = callback;
}

static ATTRIBUTE_NOINLINE void DemangleInplace(char *out, int out_size) {
  char demangled[256];
  if (Demangle(out, demangled, sizeof(demangled))) {

    int len = strlen(demangled);
    if (len + 1 <= out_size) {
      SAFE_ASSERT(len < sizeof(demangled));
      memmove(out, demangled, len + 1);
    }
  }
}

_END_GOOGLE_NAMESPACE_

#if defined(__ELF__)

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "symbolize.h"
#include "config.h"
#include "glog/raw_logging.h"

#define NO_INTR(fn)   do {} while ((fn) < 0 && errno == EINTR)

_START_GOOGLE_NAMESPACE_

static ssize_t ReadPersistent(const int fd, void *buf, const size_t count) {
  SAFE_ASSERT(fd >= 0);
  SAFE_ASSERT(count >= 0 && count <= std::numeric_limits<ssize_t>::max());
  char *buf0 = reinterpret_cast<char *>(buf);
  ssize_t num_bytes = 0;
  while (num_bytes < count) {
    ssize_t len;
    NO_INTR(len = read(fd, buf0 + num_bytes, count - num_bytes));
    if (len < 0) {
      return -1;
    }
    if (len == 0) {
      break;
    }
    num_bytes += len;
  }
  SAFE_ASSERT(num_bytes <= count);
  return num_bytes;
}

static ssize_t ReadFromOffset(const int fd, void *buf,
                              const size_t count, const off_t offset) {
  off_t off = lseek(fd, offset, SEEK_SET);
  if (off == (off_t)-1) {
    return -1;
  }
  return ReadPersistent(fd, buf, count);
}

static bool ReadFromOffsetExact(const int fd, void *buf,
                                const size_t count, const off_t offset) {
  ssize_t len = ReadFromOffset(fd, buf, count, offset);
  return len == count;
}

static int FileGetElfType(const int fd) {
  ElfW(Ehdr) elf_header;
  if (!ReadFromOffsetExact(fd, &elf_header, sizeof(elf_header), 0)) {
    return -1;
  }
  if (memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) {
    return -1;
  }
  return elf_header.e_type;
}

static ATTRIBUTE_NOINLINE bool
GetSectionHeaderByType(const int fd, ElfW(Half) sh_num, const off_t sh_offset,
                       ElfW(Word) type, ElfW(Shdr) *out) {

  ElfW(Shdr) buf[16];
  for (int i = 0; i < sh_num;) {
    const ssize_t num_bytes_left = (sh_num - i) * sizeof(buf[0]);
    const ssize_t num_bytes_to_read =
        (sizeof(buf) > num_bytes_left) ? num_bytes_left : sizeof(buf);
    const ssize_t len = ReadFromOffset(fd, buf, num_bytes_to_read,
                                       sh_offset + i * sizeof(buf[0]));
    SAFE_ASSERT(len % sizeof(buf[0]) == 0);
    const ssize_t num_headers_in_buf = len / sizeof(buf[0]);
    SAFE_ASSERT(num_headers_in_buf <= sizeof(buf) / sizeof(buf[0]));
    for (int j = 0; j < num_headers_in_buf; ++j) {
      if (buf[j].sh_type == type) {
        *out = buf[j];
        return true;
      }
    }
    i += num_headers_in_buf;
  }
  return false;
}

const int kMaxSectionNameLen = 64;

bool GetSectionHeaderByName(int fd, const char *name, size_t name_len,
                            ElfW(Shdr) *out) {
  ElfW(Ehdr) elf_header;
  if (!ReadFromOffsetExact(fd, &elf_header, sizeof(elf_header), 0)) {
    return false;
  }

  ElfW(Shdr) shstrtab;
  off_t shstrtab_offset = (elf_header.e_shoff +
                           elf_header.e_shentsize * elf_header.e_shstrndx);
  if (!ReadFromOffsetExact(fd, &shstrtab, sizeof(shstrtab), shstrtab_offset)) {
    return false;
  }

  for (int i = 0; i < elf_header.e_shnum; ++i) {
    off_t section_header_offset = (elf_header.e_shoff +
                                   elf_header.e_shentsize * i);
    if (!ReadFromOffsetExact(fd, out, sizeof(*out), section_header_offset)) {
      return false;
    }
    char header_name[kMaxSectionNameLen];
    if (sizeof(header_name) < name_len) {
      RAW_LOG(WARNING, "Section name '%s' is too long (%"PRIuS"); "
              "section will not be found (even if present).", name, name_len);

      return false;
    }
    off_t name_offset = shstrtab.sh_offset + out->sh_name;
    ssize_t n_read = ReadFromOffset(fd, &header_name, name_len, name_offset);
    if (n_read == -1) {
      return false;
    } else if (n_read != name_len) {

      continue;
    }
    if (memcmp(header_name, name, name_len) == 0) {
      return true;
    }
  }
  return false;
}

static ATTRIBUTE_NOINLINE bool
FindSymbol(uint64_t pc, const int fd, char *out, int out_size,
           uint64_t symbol_offset, const ElfW(Shdr) *strtab,
           const ElfW(Shdr) *symtab) {
  if (symtab == NULL) {
    return false;
  }
  const int num_symbols = symtab->sh_size / symtab->sh_entsize;
  for (int i = 0; i < num_symbols;) {
    off_t offset = symtab->sh_offset + i * symtab->sh_entsize;

#if __WORDSIZE == 64
#define NUM_SYMBOLS 32
#else
#define NUM_SYMBOLS 64
#endif

    ElfW(Sym) buf[NUM_SYMBOLS];
    const ssize_t len = ReadFromOffset(fd, &buf, sizeof(buf), offset);
    SAFE_ASSERT(len % sizeof(buf[0]) == 0);
    const ssize_t num_symbols_in_buf = len / sizeof(buf[0]);
    SAFE_ASSERT(num_symbols_in_buf <= sizeof(buf)/sizeof(buf[0]));
    for (int j = 0; j < num_symbols_in_buf; ++j) {
      const ElfW(Sym)& symbol = buf[j];
      uint64_t start_address = symbol.st_value;
      start_address += symbol_offset;
      uint64_t end_address = start_address + symbol.st_size;
      if (symbol.st_value != 0 &&
          symbol.st_shndx != 0 &&
          start_address <= pc && pc < end_address) {
        ssize_t len1 = ReadFromOffset(fd, out, out_size,
                                      strtab->sh_offset + symbol.st_name);
        if (len1 <= 0 || memchr(out, '\0', out_size) == NULL) {
          return false;
        }
        return true;
      }
    }
    i += num_symbols_in_buf;
  }
  return false;
}

static bool GetSymbolFromObjectFile(const int fd, uint64_t pc,
                                    char *out, int out_size,
                                    uint64_t map_start_address) {

  ElfW(Ehdr) elf_header;
  if (!ReadFromOffsetExact(fd, &elf_header, sizeof(elf_header), 0)) {
    return false;
  }

  uint64_t symbol_offset = 0;
  if (elf_header.e_type == ET_DYN) {
    symbol_offset = map_start_address;
  }

  ElfW(Shdr) symtab, strtab;

  if (!GetSectionHeaderByType(fd, elf_header.e_shnum, elf_header.e_shoff,
                              SHT_SYMTAB, &symtab)) {
    return false;
  }
  if (!ReadFromOffsetExact(fd, &strtab, sizeof(strtab), elf_header.e_shoff +
                           symtab.sh_link * sizeof(symtab))) {
    return false;
  }
  if (FindSymbol(pc, fd, out, out_size, symbol_offset,
                 &strtab, &symtab)) {
    return true;
  }

  if (!GetSectionHeaderByType(fd, elf_header.e_shnum, elf_header.e_shoff,
                              SHT_DYNSYM, &symtab)) {
    return false;
  }
  if (!ReadFromOffsetExact(fd, &strtab, sizeof(strtab), elf_header.e_shoff +
                           symtab.sh_link * sizeof(symtab))) {
    return false;
  }
  if (FindSymbol(pc, fd, out, out_size, symbol_offset,
                 &strtab, &symtab)) {
    return true;
  }

  return false;
}

namespace {

struct FileDescriptor {
  const int fd_;
  explicit FileDescriptor(int fd) : fd_(fd) {}
  ~FileDescriptor() {
    if (fd_ >= 0) {
      NO_INTR(close(fd_));
    }
  }
  int get() { return fd_; }

 private:
  explicit FileDescriptor(const FileDescriptor&);
  void operator=(const FileDescriptor&);
};

class LineReader {
 public:
  explicit LineReader(int fd, char *buf, int buf_len) : fd_(fd),
    buf_(buf), buf_len_(buf_len), bol_(buf), eol_(buf), eod_(buf) {
  }

  bool ReadLine(const char **bol, const char **eol) {
    if (BufferIsEmpty()) {
      const ssize_t num_bytes = ReadPersistent(fd_, buf_, buf_len_);
      if (num_bytes <= 0) {
        return false;
      }
      eod_ = buf_ + num_bytes;
      bol_ = buf_;
    } else {
      bol_ = eol_ + 1;
      SAFE_ASSERT(bol_ <= eod_);
      if (!HasCompleteLine()) {
        const int incomplete_line_length = eod_ - bol_;

        memmove(buf_, bol_, incomplete_line_length);

        char * const append_pos = buf_ + incomplete_line_length;
        const int capacity_left = buf_len_ - incomplete_line_length;
        const ssize_t num_bytes = ReadPersistent(fd_, append_pos,
                                                 capacity_left);
        if (num_bytes <= 0) {
          return false;
        }
        eod_ = append_pos + num_bytes;
        bol_ = buf_;
      }
    }
    eol_ = FindLineFeed();
    if (eol_ == NULL) {
      return false;
    }
    *eol_ = '\0';

    *bol = bol_;
    *eol = eol_;
    return true;
  }

  const char *bol() {
    return bol_;
  }

  const char *eol() {
    return eol_;
  }

 private:
  explicit LineReader(const LineReader&);
  void operator=(const LineReader&);

  char *FindLineFeed() {
    return reinterpret_cast<char *>(memchr(bol_, '\n', eod_ - bol_));
  }

  bool BufferIsEmpty() {
    return buf_ == eod_;
  }

  bool HasCompleteLine() {
    return !BufferIsEmpty() && FindLineFeed() != NULL;
  }

  const int fd_;
  char * const buf_;
  const int buf_len_;
  char *bol_;
  char *eol_;
  const char *eod_;
};
}

static char *GetHex(const char *start, const char *end, uint64_t *hex) {
  *hex = 0;
  const char *p;
  for (p = start; p < end; ++p) {
    int ch = *p;
    if ((ch >= '0' && ch <= '9') ||
        (ch >= 'A' && ch <= 'F') || (ch >= 'a' && ch <= 'f')) {
      *hex = (*hex << 4) | (ch < 'A' ? ch - '0' : (ch & 0xF) + 9);
    } else {
      break;
    }
  }
  SAFE_ASSERT(p <= end);
  return const_cast<char *>(p);
}

static ATTRIBUTE_NOINLINE int
OpenObjectFileContainingPcAndGetStartAddress(uint64_t pc,
                                             uint64_t &start_address) {
  int object_fd;

  int maps_fd;
  NO_INTR(maps_fd = open("/proc/self/maps", O_RDONLY));
  FileDescriptor wrapped_maps_fd(maps_fd);
  if (wrapped_maps_fd.get() < 0) {
    return -1;
  }

  char buf[1024];
  LineReader reader(wrapped_maps_fd.get(), buf, sizeof(buf));
  while (true) {
    const char *cursor;
    const char *eol;
    if (!reader.ReadLine(&cursor, &eol)) {
      return -1;
    }

    cursor = GetHex(cursor, eol, &start_address);
    if (cursor == eol || *cursor != '-') {
      return -1;
    }
    ++cursor;

    uint64_t end_address;
    cursor = GetHex(cursor, eol, &end_address);
    if (cursor == eol || *cursor != ' ') {
      return -1;
    }
    ++cursor;

    if (!(start_address <= pc && pc < end_address)) {
      continue;
    }

    const char * const flags_start = cursor;
    while (cursor < eol && *cursor != ' ') {
      ++cursor;
    }

    if (cursor == eol || cursor < flags_start + 4) {
      return -1;
    }

    if (memcmp(flags_start, "r-x", 3) != 0) {
      continue;
    }
    ++cursor;

    int num_spaces = 0;
    while (cursor < eol) {
      if (*cursor == ' ') {
        ++num_spaces;
      } else if (num_spaces >= 3) {

        break;
      }
      ++cursor;
    }
    if (cursor == eol) {
      return -1;
    }

    NO_INTR(object_fd = open(cursor, O_RDONLY));
    if (object_fd < 0) {
      return -1;
    }
    return object_fd;
  }
}

static ATTRIBUTE_NOINLINE bool SymbolizeAndDemangle(void *pc, char *out,
                                                    int out_size) {
  uint64_t pc0 = reinterpret_cast<uintptr_t>(pc);
  uint64_t start_address = 0;

  int object_fd = OpenObjectFileContainingPcAndGetStartAddress(pc0,
                                                               start_address);
  if (object_fd == -1) {
    return false;
  }
  FileDescriptor wrapped_object_fd(object_fd);
  int elf_type = FileGetElfType(wrapped_object_fd.get());
  if (elf_type == -1) {
    return false;
  }
  if (g_symbolize_callback) {

    uint64 relocation = (elf_type == ET_DYN) ? start_address : 0;
    int num_bytes_written = g_symbolize_callback(wrapped_object_fd.get(),
                                                 pc, out, out_size,
                                                 relocation);
    if (num_bytes_written > 0) {
      out += num_bytes_written;
      out_size -= num_bytes_written;
    }
  }
  if (!GetSymbolFromObjectFile(wrapped_object_fd.get(), pc0,
                               out, out_size, start_address)) {
    return false;
  }

  DemangleInplace(out, out_size);
  return true;
}

_END_GOOGLE_NAMESPACE_

#elif defined(OS_MACOSX) && defined(HAVE_DLADDR)

#include <dlfcn.h>
#include <string.h>

_START_GOOGLE_NAMESPACE_

static ATTRIBUTE_NOINLINE bool SymbolizeAndDemangle(void *pc, char *out,
                                                    int out_size) {
  Dl_info info;
  if (dladdr(pc, &info)) {
    if (strlen(info.dli_sname) < out_size) {
      strcpy(out, info.dli_sname);

      DemangleInplace(out, out_size);
      return true;
    }
  }
  return false;
}

_END_GOOGLE_NAMESPACE_

#else
# error BUG: HAVE_SYMBOLIZE was wrongly set
#endif

_START_GOOGLE_NAMESPACE_

bool Symbolize(void *pc, char *out, int out_size) {
  SAFE_ASSERT(out_size >= 0);
  return SymbolizeAndDemangle(pc, out, out_size);
}

_END_GOOGLE_NAMESPACE_

#else

#include <assert.h>

#include "config.h"

_START_GOOGLE_NAMESPACE_

bool Symbolize(void *pc, char *out, int out_size) {
  assert(0);
  return false;
}

_END_GOOGLE_NAMESPACE_

#endif
