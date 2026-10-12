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

// Add DWARF debugging information to a Breakpad symbol file. This
// file defines the DwarfCUToModule class, which accepts parsed DWARF
// data and populates a google_breakpad::Module with the results; the
// Module can then write its contents as a Breakpad symbol file.

#ifndef COMMON_LINUX_DWARF_CU_TO_MODULE_H__
#define COMMON_LINUX_DWARF_CU_TO_MODULE_H__

#include <string>

#include "common/language.h"
#include "common/module.h"
#include "common/dwarf/bytereader.h"
#include "common/dwarf/dwarf2diehandler.h"
#include "common/dwarf/dwarf2reader.h"
#include "common/scoped_ptr.h"
#include "common/using_std_string.h"

namespace google_breakpad {

using dwarf2reader::DwarfAttribute;
using dwarf2reader::DwarfForm;
using dwarf2reader::DwarfLanguage;
using dwarf2reader::DwarfTag;

class DwarfCUToModule: public dwarf2reader::RootDIEHandler {
  struct FilePrivate;
 public:

  class FileContext {
   public:
    FileContext(const string &filename,
                Module *module,
                bool handle_inter_cu_refs);
    ~FileContext();

    void AddSectionToSectionMap(const string& name,
                                const char* contents,
                                uint64 length);

    void ClearSectionMapForTest();

    const dwarf2reader::SectionMap& section_map() const;

   private:
    friend class DwarfCUToModule;

    void ClearSpecifications();

    bool IsUnhandledInterCUReference(uint64 offset,
                                     uint64 compilation_unit_start) const;

    const string filename_;

    dwarf2reader::SectionMap section_map_;

    Module *module_;

    const bool handle_inter_cu_refs_;

    scoped_ptr<FilePrivate> file_private_;
  };

  class LineToModuleHandler {
   public:
    LineToModuleHandler() { }
    virtual ~LineToModuleHandler() { }

    virtual void StartCompilationUnit(const string& compilation_dir) = 0;

    virtual void ReadProgram(const char *program, uint64 length,
                             Module *module, vector<Module::Line> *lines) = 0;
  };

  class WarningReporter {
   public:

    WarningReporter(const string &filename, uint64 cu_offset)
        : filename_(filename), cu_offset_(cu_offset), printed_cu_header_(false),
          printed_unpaired_header_(false),
          uncovered_warnings_enabled_(false) { }
    virtual ~WarningReporter() { }

    virtual void SetCUName(const string &name) { cu_name_ = name; }

    virtual bool uncovered_warnings_enabled() const {
      return uncovered_warnings_enabled_;
    }
    virtual void set_uncovered_warnings_enabled(bool value) {
      uncovered_warnings_enabled_ = value;
    }

    virtual void UnknownSpecification(uint64 offset, uint64 target);

    virtual void UnknownAbstractOrigin(uint64 offset, uint64 target);

    virtual void MissingSection(const string &section_name);

    virtual void BadLineInfoOffset(uint64 offset);

    virtual void UncoveredFunction(const Module::Function &function);

    virtual void UncoveredLine(const Module::Line &line);

    virtual void UnnamedFunction(uint64 offset);

    virtual void DemangleError(const string &input, int error);

    virtual void UnhandledInterCUReference(uint64 offset, uint64 target);

    uint64 cu_offset() const {
      return cu_offset_;
    }

   protected:
    const string filename_;
    const uint64 cu_offset_;
    string cu_name_;
    bool printed_cu_header_;
    bool printed_unpaired_header_;
    bool uncovered_warnings_enabled_;

   private:

    void CUHeading();

    void UncoveredHeading();
  };

  DwarfCUToModule(FileContext *file_context,
                  LineToModuleHandler *line_reader,
                  WarningReporter *reporter);
  ~DwarfCUToModule();

  void ProcessAttributeSigned(enum DwarfAttribute attr,
                              enum DwarfForm form,
                              int64 data);
  void ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                enum DwarfForm form,
                                uint64 data);
  void ProcessAttributeString(enum DwarfAttribute attr,
                              enum DwarfForm form,
                              const string &data);
  bool EndAttributes();
  DIEHandler *FindChildHandler(uint64 offset, enum DwarfTag tag);

  void Finish();

  bool StartCompilationUnit(uint64 offset, uint8 address_size,
                            uint8 offset_size, uint64 cu_length,
                            uint8 dwarf_version);
  bool StartRootDIE(uint64 offset, enum DwarfTag tag);

 private:

  struct CUContext;
  struct DIEContext;
  struct Specification;
  class GenericDIEHandler;
  class FuncHandler;
  class NamedScopeHandler;

  typedef map<uint64, Specification> SpecificationByOffset;

  void SetLanguage(DwarfLanguage language);

  void ReadSourceLines(uint64 offset);

  void AssignLinesToFunctions();

  LineToModuleHandler *line_reader_;

  scoped_ptr<CUContext> cu_context_;

  scoped_ptr<DIEContext> child_context_;

  bool has_source_line_info_;

  uint64 source_line_offset_;

  vector<Module::Line> lines_;
};

}

#endif
