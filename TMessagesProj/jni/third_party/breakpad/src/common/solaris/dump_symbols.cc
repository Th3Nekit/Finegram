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

// Author: Alfred Peng

#include <demangle.h>
#include <fcntl.h>
#include <gelf.h>
#include <link.h>
#include <sys/mman.h>
#include <stab.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <map>
#include <vector>

#include "common/scoped_ptr.h"
#include "common/solaris/dump_symbols.h"
#include "common/solaris/file_id.h"
#include "common/solaris/guid_creator.h"

namespace {

using std::make_pair;

#if defined(_LP64)
typedef Elf64_Sym   Elf_Sym;
#else
typedef Elf32_Sym   Elf_Sym;
#endif

struct slist {

  unsigned int n_strx;

  unsigned char n_type;
  char n_other;
  short n_desc;
  unsigned long n_value;
};

struct SymbolEntry {

  GElf_Addr offset;

  GElf_Word size;
};

struct LineInfo {

  GElf_Off rva_to_func;

  GElf_Off rva_to_base;

  uint32_t size;

  uint32_t line_num;
};

struct FuncInfo {

  const char *name;

  GElf_Off rva_to_base;

  GElf_Addr addr;

  uint32_t size;

  uint32_t stack_param_size;

  std::vector<struct LineInfo> line_info;
};

struct SourceFileInfo {

  const char *name;

  GElf_Addr addr;

  int source_id;

  std::vector<struct FuncInfo> func_info;
};

struct CompareString {
  bool operator()(const char *s1, const char *s2) const {
    return strcmp(s1, s2) < 0;
  }
};

typedef std::map<const char *, struct SymbolEntry *, CompareString> SymbolMap;

struct SymbolInfo {
  std::vector<struct SourceFileInfo> source_file_info;

  SymbolMap symbol_entries;
};

const char *kStabName = ".stab";

const char *kStabStrName = ".stabstr";

const char *kSymtabName = ".symtab";

const char *kStrtabName = ".strtab";

const int demangleLen = 20000;

uint64_t stringOffset = 0;

inline void RecalculateOffset(struct slist* cur_list, char *stabstr) {
  while ((--cur_list)->n_strx == 0) ;
  stringOffset += cur_list->n_strx;

  char *temp = stabstr + stringOffset;
  while (*temp != '\0') {
    ++stringOffset;
    ++temp;
  }

  ++stringOffset;
}

std::string Demangle(const char *mangled) {
  int status = 0;
  std::string str(mangled);
  char *demangled = (char *)malloc(demangleLen);

  if (!demangled) {
    fprintf(stderr, "no enough memory.\n");
    goto out;
  }

  if ((status = cplus_demangle(mangled, demangled, demangleLen)) ==
      DEMANGLE_ESPACE) {
    fprintf(stderr, "incorrect demangle.\n");
    goto out;
  }

  str = demangled;
  free(demangled);

out:
  return str;
}

bool WriteFormat(int fd, const char *fmt, ...) {
  va_list list;
  char buffer[4096];
  ssize_t expected, written;
  va_start(list, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, list);
  expected = strlen(buffer);
  written = write(fd, buffer, strlen(buffer));
  va_end(list);
  return expected == written;
}

bool IsValidElf(const GElf_Ehdr *elf_header) {
  return memcmp(elf_header, ELFMAG, SELFMAG) == 0;
}

static bool FindSectionByName(Elf *elf, const char *name,
                              int shstrndx,
                              GElf_Shdr *shdr) {
  assert(name != NULL);

  if (strlen(name) == 0)
    return false;

  Elf_Scn *scn = NULL;

  while ((scn = elf_nextscn(elf, scn)) != NULL) {
    if (gelf_getshdr(scn, shdr) == (GElf_Shdr *)0) {
      fprintf(stderr, "failed to read section header: %s\n", elf_errmsg(0));
      return false;
    }

    const char *section_name = elf_strptr(elf, shstrndx, shdr->sh_name);
    if (!section_name) {
      fprintf(stderr, "Section name error: %s\n", elf_errmsg(-1));
      continue;
    }

    if (strcmp(section_name, name) == 0)
      return true;
  }

  return false;
}

int LoadStackParamSize(struct slist *list,
                       struct slist *list_end,
                       struct FuncInfo *func_info) {
  struct slist *cur_list = list;
  int step = 1;
  while (cur_list < list_end && cur_list->n_type == N_PSYM) {
    ++cur_list;
    ++step;
  }

  func_info->stack_param_size = 0;
  return step;
}

int LoadLineInfo(struct slist *list,
                 struct slist *list_end,
                 struct FuncInfo *func_info) {
  struct slist *cur_list = list;
  do {

    while (cur_list < list_end && cur_list->n_type != N_SLINE) {

      if (cur_list->n_type == N_FUN || cur_list->n_type == N_SO ||
          cur_list->n_type == N_ENDM) {
        return cur_list - list;
      }
      ++cur_list;
    }
    struct LineInfo line;
    while (cur_list < list_end && cur_list->n_type == N_SLINE) {
      line.rva_to_func = cur_list->n_value;

      line.line_num = (unsigned short)cur_list->n_desc;
      func_info->line_info.push_back(line);
      ++cur_list;
    }
    if (cur_list == list_end && cur_list->n_type == N_ENDM)
      break;
  } while (list < list_end);

  return cur_list - list;
}

int LoadFuncSymbols(struct slist *list,
                    struct slist *list_end,
                    char *stabstr,
                    GElf_Word base,
                    struct SourceFileInfo *source_file_info) {
  struct slist *cur_list = list;
  assert(cur_list->n_type == N_SO);
  ++cur_list;

  source_file_info->func_info.clear();
  while (cur_list < list_end) {

    while (cur_list < list_end && cur_list->n_type != N_FUN) {
      if (cur_list->n_type == N_SO) {
        return cur_list - list;
      }
      ++cur_list;
      if (cur_list->n_type == N_ENDM)
        RecalculateOffset(cur_list, stabstr);
      continue;
    }
    while (cur_list->n_type == N_FUN) {
      struct FuncInfo func_info;
      memset(&func_info, 0, sizeof(func_info));
      func_info.name = stabstr + cur_list->n_strx + stringOffset;

      func_info.addr = cur_list->n_value;
      ++cur_list;
      if (cur_list->n_type == N_ENDM)
        RecalculateOffset(cur_list, stabstr);
      if (cur_list->n_type != N_ESYM && cur_list->n_type != N_ISYM &&
          cur_list->n_type != N_FUN) {

        cur_list += LoadStackParamSize(cur_list, list_end, &func_info);

        cur_list += LoadLineInfo(cur_list, list_end, &func_info);
      }
      if (cur_list < list_end && cur_list->n_type == N_ENDM)
        RecalculateOffset(cur_list, stabstr);

      if (func_info.addr >= source_file_info->addr) {
        source_file_info->func_info.push_back(func_info);
      }
    }
  }
  return cur_list - list;
}

bool ComputeSizeAndRVA(struct SymbolInfo *symbols) {
  std::vector<struct SourceFileInfo> *sorted_files =
    &(symbols->source_file_info);
  SymbolMap *symbol_entries = &(symbols->symbol_entries);
  for (size_t i = 0; i < sorted_files->size(); ++i) {
    struct SourceFileInfo &source_file = (*sorted_files)[i];
    std::vector<struct FuncInfo> *sorted_functions = &(source_file.func_info);
    int func_size = sorted_functions->size();

    for (size_t j = 0; j < func_size; ++j) {
      struct FuncInfo &func_info = (*sorted_functions)[j];
      int line_count = func_info.line_info.size();

      std::string func_name(func_info.name);
      std::string::size_type last_colon = func_name.find_first_of(':');
      if (last_colon != std::string::npos)
        func_name = func_name.substr(0, last_colon);

      SymbolMap::const_iterator it = symbol_entries->find(func_name.c_str());
      if (it->second) {
        func_info.rva_to_base = it->second->offset;
        func_info.size = (line_count == 0) ? 0 : it->second->size;
      } else {
        func_info.rva_to_base = 0;
        func_info.size = 0;
      }

      for (size_t k = 0; k < line_count; ++k) {
        struct LineInfo &line_info = func_info.line_info[k];

        line_info.rva_to_base = line_info.rva_to_func + func_info.rva_to_base;
        if (k == line_count - 1) {
          line_info.size = func_info.size - line_info.rva_to_func;
        } else {
          struct LineInfo &next_line = func_info.line_info[k + 1];
          line_info.size = next_line.rva_to_func - line_info.rva_to_func;
        }
      }
    }
  }
  for (SymbolMap::iterator it = symbol_entries->begin();
       it != symbol_entries->end(); ++it) {
    free(it->second);
  }
  return true;
}

bool LoadAllSymbols(const GElf_Shdr *stab_section,
                    const GElf_Shdr *stabstr_section,
                    GElf_Word base,
                    struct SymbolInfo *symbols) {
  if (stab_section == NULL || stabstr_section == NULL)
    return false;

  char *stabstr =
    reinterpret_cast<char *>(stabstr_section->sh_offset + base);
  struct slist *lists =
    reinterpret_cast<struct slist *>(stab_section->sh_offset + base);
  int nstab = stab_section->sh_size / sizeof(struct slist);
  int source_id = 0;

  for (int i = 0; i < nstab; ) {
    int step = 1;
    struct slist *cur_list = lists + i;
    if (cur_list->n_type == N_SO) {

      struct SourceFileInfo source_file_info;
      source_file_info.name = stabstr + cur_list->n_strx + stringOffset;

      source_file_info.addr = cur_list->n_value;
      if (strchr(source_file_info.name, '.'))
        source_file_info.source_id = source_id++;
      else
        source_file_info.source_id = -1;
      step = LoadFuncSymbols(cur_list, lists + nstab - 1, stabstr,
                             base, &source_file_info);
      symbols->source_file_info.push_back(source_file_info);
    }
    i += step;
  }

  return ComputeSizeAndRVA(symbols);
}

bool LoadSymbols(Elf *elf, GElf_Ehdr *elf_header, struct SymbolInfo *symbols,
                 void *obj_base) {
  GElf_Word base = reinterpret_cast<GElf_Word>(obj_base);

  const GElf_Shdr *sections =
    reinterpret_cast<GElf_Shdr *>(elf_header->e_shoff + base);
  GElf_Shdr stab_section;
  if (!FindSectionByName(elf, kStabName, elf_header->e_shstrndx,
                         &stab_section)) {
    fprintf(stderr, "Stab section not found.\n");
    return false;
  }
  GElf_Shdr stabstr_section;
  if (!FindSectionByName(elf, kStabStrName, elf_header->e_shstrndx,
                         &stabstr_section)) {
    fprintf(stderr, "Stabstr section not found.\n");
    return false;
  }
  GElf_Shdr symtab_section;
  if (!FindSectionByName(elf, kSymtabName, elf_header->e_shstrndx,
                         &symtab_section)) {
    fprintf(stderr, "Symtab section not found.\n");
    return false;
  }
  GElf_Shdr strtab_section;
  if (!FindSectionByName(elf, kStrtabName, elf_header->e_shstrndx,
                         &strtab_section)) {
    fprintf(stderr, "Strtab section not found.\n");
    return false;
  }

  Elf_Sym *symbol = (Elf_Sym *)((char *)base + symtab_section.sh_offset);
  for (int i = 0; i < symtab_section.sh_size/symtab_section.sh_entsize; ++i) {
    struct SymbolEntry *symbol_entry =
        (struct SymbolEntry *)malloc(sizeof(struct SymbolEntry));
    const char *name = reinterpret_cast<char *>(
        strtab_section.sh_offset + (GElf_Word)base + symbol->st_name);
    symbol_entry->offset = symbol->st_value;
    symbol_entry->size = symbol->st_size;
    symbols->symbol_entries.insert(make_pair(name, symbol_entry));
    ++symbol;
  }

  return LoadAllSymbols(&stab_section, &stabstr_section, base, symbols);
}

bool WriteModuleInfo(int fd, GElf_Half arch, const std::string &obj_file) {
  const char *arch_name = NULL;
  if (arch == EM_386)
    arch_name = "x86";
  else if (arch == EM_X86_64)
    arch_name = "x86_64";
  else if (arch == EM_SPARC32PLUS)
    arch_name = "SPARC_32+";
  else {
    printf("Please add more ARCH support\n");
    return false;
  }

  unsigned char identifier[16];
  google_breakpad::FileID file_id(obj_file.c_str());
  if (file_id.ElfFileIdentifier(identifier)) {
    char identifier_str[40];
    file_id.ConvertIdentifierToString(identifier,
                                      identifier_str, sizeof(identifier_str));
    std::string filename = obj_file;
    size_t slash_pos = obj_file.find_last_of("/");
    if (slash_pos != std::string::npos)
      filename = obj_file.substr(slash_pos + 1);
    return WriteFormat(fd, "MODULE solaris %s %s %s\n", arch_name,
                       identifier_str, filename.c_str());
  }
  return false;
}

bool WriteSourceFileInfo(int fd, const struct SymbolInfo &symbols) {
  for (size_t i = 0; i < symbols.source_file_info.size(); ++i) {
    if (symbols.source_file_info[i].source_id != -1) {
      const char *name = symbols.source_file_info[i].name;
      if (!WriteFormat(fd, "FILE %d %s\n",
                       symbols.source_file_info[i].source_id, name))
        return false;
    }
  }
  return true;
}

bool WriteOneFunction(int fd, int source_id,
                      const struct FuncInfo &func_info){

  std::string func_name(func_info.name);
  std::string::size_type last_colon = func_name.find_last_of(':');
  if (last_colon != std::string::npos)
    func_name = func_name.substr(0, last_colon);
  func_name = Demangle(func_name.c_str());

  if (func_info.size <= 0)
    return true;

  if (WriteFormat(fd, "FUNC %llx %x %d %s\n",
                  (long long)func_info.rva_to_base,
                  func_info.size,
                  func_info.stack_param_size,
                  func_name.c_str())) {
    for (size_t i = 0; i < func_info.line_info.size(); ++i) {
      const struct LineInfo &line_info = func_info.line_info[i];
      if (line_info.line_num == 0)
        return true;
      if (!WriteFormat(fd, "%llx %x %d %d\n",
                       (long long)line_info.rva_to_base,
                       line_info.size,
                       line_info.line_num,
                       source_id))
        return false;
    }
    return true;
  }
  return false;
}

bool WriteFunctionInfo(int fd, const struct SymbolInfo &symbols) {
  for (size_t i = 0; i < symbols.source_file_info.size(); ++i) {
    const struct SourceFileInfo &file_info = symbols.source_file_info[i];
    for (size_t j = 0; j < file_info.func_info.size(); ++j) {
      const struct FuncInfo &func_info = file_info.func_info[j];
      if (!WriteOneFunction(fd, file_info.source_id, func_info))
        return false;
    }
  }
  return true;
}

bool DumpStabSymbols(int fd, const struct SymbolInfo &symbols) {
  return WriteSourceFileInfo(fd, symbols) &&
    WriteFunctionInfo(fd, symbols);
}

class FDWrapper {
 public:
  explicit FDWrapper(int fd) :
    fd_(fd) {
    }
  ~FDWrapper() {
    if (fd_ != -1)
      close(fd_);
  }
  int get() {
    return fd_;
  }
  int release() {
    int fd = fd_;
    fd_ = -1;
    return fd;
  }
 private:
  int fd_;
};

class MmapWrapper {
 public:
  MmapWrapper(void *mapped_address, size_t mapped_size) :
    base_(mapped_address), size_(mapped_size) {
  }
  ~MmapWrapper() {
    if (base_ != NULL) {
      assert(size_ > 0);
      munmap((char *)base_, size_);
    }
  }
  void release() {
    base_ = NULL;
    size_ = 0;
  }

 private:
  void *base_;
  size_t size_;
};

}

namespace google_breakpad {

class AutoElfEnder {
 public:
  AutoElfEnder(Elf *elf) : elf_(elf) {}
  ~AutoElfEnder() { if (elf_) elf_end(elf_); }
 private:
  Elf *elf_;
};

bool DumpSymbols::WriteSymbolFile(const std::string &obj_file, int sym_fd) {
  if (elf_version(EV_CURRENT) == EV_NONE) {
    fprintf(stderr, "elf_version() failed: %s\n", elf_errmsg(0));
    return false;
  }

  int obj_fd = open(obj_file.c_str(), O_RDONLY);
  if (obj_fd < 0)
    return false;
  FDWrapper obj_fd_wrapper(obj_fd);
  struct stat st;
  if (fstat(obj_fd, &st) != 0 && st.st_size <= 0)
    return false;
  void *obj_base = mmap(NULL, st.st_size,
                        PROT_READ, MAP_PRIVATE, obj_fd, 0);
  if (obj_base == MAP_FAILED)
    return false;
  MmapWrapper map_wrapper(obj_base, st.st_size);
  GElf_Ehdr elf_header;
  Elf *elf = elf_begin(obj_fd, ELF_C_READ, NULL);
  AutoElfEnder elfEnder(elf);

  if (gelf_getehdr(elf, &elf_header) == (GElf_Ehdr *)NULL) {
    fprintf(stderr, "failed to read elf header: %s\n", elf_errmsg(-1));
    return false;
  }

  if (!IsValidElf(&elf_header)) {
    fprintf(stderr, "header magic doesn't match\n");
    return false;
  }
  struct SymbolInfo symbols;
  if (!LoadSymbols(elf, &elf_header, &symbols, obj_base))
    return false;

  if (WriteModuleInfo(sym_fd, elf_header.e_machine, obj_file) &&
      DumpStabSymbols(sym_fd, symbols))
    return true;

  return false;
}

}
