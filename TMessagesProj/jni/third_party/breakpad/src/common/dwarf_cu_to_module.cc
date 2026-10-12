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

// Implement the DwarfCUToModule class; see dwarf_cu_to_module.h.

// For <inttypes.h> PRI* macros, before anything else might #include it.
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "common/dwarf_cu_to_module.h"

#include <assert.h>
#if !defined(__ANDROID__)
#include <cxxabi.h>
#endif
#include <inttypes.h>
#include <stdio.h>

#include <algorithm>
#include <utility>

#include "common/dwarf_line_to_module.h"
#include "common/unordered.h"

namespace google_breakpad {

using std::map;
using std::pair;
using std::sort;
using std::vector;

struct DwarfCUToModule::Specification {

  string qualified_name;

  string enclosing_name;

  string unqualified_name;
};

struct AbstractOrigin {
  AbstractOrigin() : name() {}
  explicit AbstractOrigin(const string& name) : name(name) {}

  string name;
};

typedef map<uint64, AbstractOrigin> AbstractOriginByOffset;

struct DwarfCUToModule::FilePrivate {

  unordered_set<string> common_strings;

  SpecificationByOffset specifications;

  AbstractOriginByOffset origins;
};

DwarfCUToModule::FileContext::FileContext(const string &filename,
                                          Module *module,
                                          bool handle_inter_cu_refs)
    : filename_(filename),
      module_(module),
      handle_inter_cu_refs_(handle_inter_cu_refs),
      file_private_(new FilePrivate()) {
}

DwarfCUToModule::FileContext::~FileContext() {
}

void DwarfCUToModule::FileContext::AddSectionToSectionMap(
    const string& name, const char* contents, uint64 length) {
  section_map_[name] = std::make_pair(contents, length);
}

void DwarfCUToModule::FileContext::ClearSectionMapForTest() {
  section_map_.clear();
}

const dwarf2reader::SectionMap&
DwarfCUToModule::FileContext::section_map() const {
  return section_map_;
}

void DwarfCUToModule::FileContext::ClearSpecifications() {
  if (!handle_inter_cu_refs_)
    file_private_->specifications.clear();
}

bool DwarfCUToModule::FileContext::IsUnhandledInterCUReference(
    uint64 offset, uint64 compilation_unit_start) const {
  if (handle_inter_cu_refs_)
    return false;
  return offset < compilation_unit_start;
}

struct DwarfCUToModule::CUContext {
  CUContext(FileContext *file_context_arg, WarningReporter *reporter_arg)
      : file_context(file_context_arg),
        reporter(reporter_arg),
        language(Language::CPlusPlus) {}

  ~CUContext() {
    for (vector<Module::Function *>::iterator it = functions.begin();
         it != functions.end(); ++it) {
      delete *it;
    }
  };

  FileContext *file_context;

  WarningReporter *reporter;

  const Language *language;

  vector<Module::Function *> functions;
};

struct DwarfCUToModule::DIEContext {

  string name;
};

class DwarfCUToModule::GenericDIEHandler: public dwarf2reader::DIEHandler {
 public:

  GenericDIEHandler(CUContext *cu_context, DIEContext *parent_context,
                    uint64 offset)
      : cu_context_(cu_context),
        parent_context_(parent_context),
        offset_(offset),
        declaration_(false),
        specification_(NULL) { }

  void ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                enum DwarfForm form,
                                uint64 data);

  void ProcessAttributeReference(enum DwarfAttribute attr,
                                 enum DwarfForm form,
                                 uint64 data);

  void ProcessAttributeString(enum DwarfAttribute attr,
                              enum DwarfForm form,
                              const string &data);

 protected:

  string ComputeQualifiedName();

  CUContext *cu_context_;
  DIEContext *parent_context_;
  uint64 offset_;

  string AddStringToPool(const string &str);

  bool declaration_;

  Specification *specification_;

  string name_attribute_;

  string demangled_name_;
};

void DwarfCUToModule::GenericDIEHandler::ProcessAttributeUnsigned(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_declaration: declaration_ = (data != 0); break;
    default: break;
  }
}

void DwarfCUToModule::GenericDIEHandler::ProcessAttributeReference(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_specification: {
      FileContext *file_context = cu_context_->file_context;
      if (file_context->IsUnhandledInterCUReference(
              data, cu_context_->reporter->cu_offset())) {
        cu_context_->reporter->UnhandledInterCUReference(offset_, data);
        break;
      }

      SpecificationByOffset *specifications =
          &file_context->file_private_->specifications;
      SpecificationByOffset::iterator spec = specifications->find(data);
      if (spec != specifications->end()) {
        specification_ = &spec->second;
      } else {

        cu_context_->reporter->UnknownSpecification(offset_, data);
      }
      break;
    }
    default: break;
  }
}

string DwarfCUToModule::GenericDIEHandler::AddStringToPool(const string &str) {
  pair<unordered_set<string>::iterator, bool> result =
    cu_context_->file_context->file_private_->common_strings.insert(str);
  return *result.first;
}

void DwarfCUToModule::GenericDIEHandler::ProcessAttributeString(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    const string &data) {
  switch (attr) {
    case dwarf2reader::DW_AT_name:
      name_attribute_ = AddStringToPool(data);
      break;
    case dwarf2reader::DW_AT_MIPS_linkage_name: {
      char* demangled = NULL;
      int status = -1;
#if !defined(__ANDROID__)
      demangled = abi::__cxa_demangle(data.c_str(), NULL, NULL, &status);
#endif
      if (status != 0) {
        cu_context_->reporter->DemangleError(data, status);
        demangled_name_ = "";
        break;
      }
      if (demangled) {
        demangled_name_ = AddStringToPool(demangled);
        free(reinterpret_cast<void*>(demangled));
      }
      break;
    }
    default: break;
  }
}

string DwarfCUToModule::GenericDIEHandler::ComputeQualifiedName() {

  const string *qualified_name = NULL;
  if (!demangled_name_.empty()) {

    qualified_name = &demangled_name_;
  } else if (specification_ && !specification_->qualified_name.empty()) {

    qualified_name = &specification_->qualified_name;
  }

  const string *unqualified_name;
  const string *enclosing_name;
  if (!qualified_name) {

    if (name_attribute_.empty() && specification_)
      unqualified_name = &specification_->unqualified_name;
    else
      unqualified_name = &name_attribute_;

    if (specification_)
      enclosing_name = &specification_->enclosing_name;
    else
      enclosing_name = &parent_context_->name;
  }

  string return_value;
  if (qualified_name) {
    return_value = *qualified_name;
  } else {

    return_value = cu_context_->language->MakeQualifiedName(*enclosing_name,
                                                            *unqualified_name);
  }

  if (declaration_) {
    Specification spec;
    if (qualified_name) {
      spec.qualified_name = *qualified_name;
    } else {
      spec.enclosing_name = *enclosing_name;
      spec.unqualified_name = *unqualified_name;
    }
    cu_context_->file_context->file_private_->specifications[offset_] = spec;
  }

  return return_value;
}

class DwarfCUToModule::FuncHandler: public GenericDIEHandler {
 public:
  FuncHandler(CUContext *cu_context, DIEContext *parent_context,
              uint64 offset)
      : GenericDIEHandler(cu_context, parent_context, offset),
        low_pc_(0), high_pc_(0), high_pc_form_(dwarf2reader::DW_FORM_addr),
        abstract_origin_(NULL), inline_(false) { }
  void ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                enum DwarfForm form,
                                uint64 data);
  void ProcessAttributeSigned(enum DwarfAttribute attr,
                              enum DwarfForm form,
                              int64 data);
  void ProcessAttributeReference(enum DwarfAttribute attr,
                                 enum DwarfForm form,
                                 uint64 data);

  bool EndAttributes();
  void Finish();

 private:

  string name_;
  uint64 low_pc_, high_pc_;
  DwarfForm high_pc_form_;
  const AbstractOrigin* abstract_origin_;
  bool inline_;
};

void DwarfCUToModule::FuncHandler::ProcessAttributeUnsigned(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {

    case dwarf2reader::DW_AT_inline:      inline_  = true; break;

    case dwarf2reader::DW_AT_low_pc:      low_pc_  = data; break;
    case dwarf2reader::DW_AT_high_pc:
      high_pc_form_ = form;
      high_pc_ = data;
      break;

    default:
      GenericDIEHandler::ProcessAttributeUnsigned(attr, form, data);
      break;
  }
}

void DwarfCUToModule::FuncHandler::ProcessAttributeSigned(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    int64 data) {
  switch (attr) {

    case dwarf2reader::DW_AT_inline:      inline_  = true; break;

    default:
      break;
  }
}

void DwarfCUToModule::FuncHandler::ProcessAttributeReference(
    enum DwarfAttribute attr,
    enum DwarfForm form,
    uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_abstract_origin: {
      const AbstractOriginByOffset& origins =
          cu_context_->file_context->file_private_->origins;
      AbstractOriginByOffset::const_iterator origin = origins.find(data);
      if (origin != origins.end()) {
        abstract_origin_ = &(origin->second);
      } else {
        cu_context_->reporter->UnknownAbstractOrigin(offset_, data);
      }
      break;
    }
    default:
      GenericDIEHandler::ProcessAttributeReference(attr, form, data);
      break;
  }
}

bool DwarfCUToModule::FuncHandler::EndAttributes() {

  name_ = ComputeQualifiedName();
  if (name_.empty() && abstract_origin_) {
    name_ = abstract_origin_->name;
  }
  return true;
}

void DwarfCUToModule::FuncHandler::Finish() {

  if (high_pc_form_ != dwarf2reader::DW_FORM_addr) {
    high_pc_ += low_pc_;
  }

  if (low_pc_ < high_pc_) {

    string name;
    if (!name_.empty()) {
      name = name_;
    } else {
      cu_context_->reporter->UnnamedFunction(offset_);
      name = "<name omitted>";
    }

    scoped_ptr<Module::Function> func(new Module::Function(name, low_pc_));
    func->size = high_pc_ - low_pc_;
    func->parameter_size = 0;
    if (func->address) {

       cu_context_->functions.push_back(func.release());
     }
  } else if (inline_) {
    AbstractOrigin origin(name_);
    cu_context_->file_context->file_private_->origins[offset_] = origin;
  }
}

class DwarfCUToModule::NamedScopeHandler: public GenericDIEHandler {
 public:
  NamedScopeHandler(CUContext *cu_context, DIEContext *parent_context,
                    uint64 offset)
      : GenericDIEHandler(cu_context, parent_context, offset) { }
  bool EndAttributes();
  DIEHandler *FindChildHandler(uint64 offset, enum DwarfTag tag);

 private:
  DIEContext child_context_;
};

bool DwarfCUToModule::NamedScopeHandler::EndAttributes() {
  child_context_.name = ComputeQualifiedName();
  return true;
}

dwarf2reader::DIEHandler *DwarfCUToModule::NamedScopeHandler::FindChildHandler(
    uint64 offset,
    enum DwarfTag tag) {
  switch (tag) {
    case dwarf2reader::DW_TAG_subprogram:
      return new FuncHandler(cu_context_, &child_context_, offset);
    case dwarf2reader::DW_TAG_namespace:
    case dwarf2reader::DW_TAG_class_type:
    case dwarf2reader::DW_TAG_structure_type:
    case dwarf2reader::DW_TAG_union_type:
      return new NamedScopeHandler(cu_context_, &child_context_, offset);
    default:
      return NULL;
  }
}

void DwarfCUToModule::WarningReporter::CUHeading() {
  if (printed_cu_header_)
    return;
  fprintf(stderr, "%s: in compilation unit '%s' (offset 0x%llx):\n",
          filename_.c_str(), cu_name_.c_str(), cu_offset_);
  printed_cu_header_ = true;
}

void DwarfCUToModule::WarningReporter::UnknownSpecification(uint64 offset,
                                                            uint64 target) {
  CUHeading();
  fprintf(stderr, "%s: the DIE at offset 0x%llx has a DW_AT_specification"
          " attribute referring to the die at offset 0x%llx, which either"
          " was not marked as a declaration, or comes later in the file\n",
          filename_.c_str(), offset, target);
}

void DwarfCUToModule::WarningReporter::UnknownAbstractOrigin(uint64 offset,
                                                             uint64 target) {
  CUHeading();
  fprintf(stderr, "%s: the DIE at offset 0x%llx has a DW_AT_abstract_origin"
          " attribute referring to the die at offset 0x%llx, which either"
          " was not marked as an inline, or comes later in the file\n",
          filename_.c_str(), offset, target);
}

void DwarfCUToModule::WarningReporter::MissingSection(const string &name) {
  CUHeading();
  fprintf(stderr, "%s: warning: couldn't find DWARF '%s' section\n",
          filename_.c_str(), name.c_str());
}

void DwarfCUToModule::WarningReporter::BadLineInfoOffset(uint64 offset) {
  CUHeading();
  fprintf(stderr, "%s: warning: line number data offset beyond end"
          " of '.debug_line' section\n",
          filename_.c_str());
}

void DwarfCUToModule::WarningReporter::UncoveredHeading() {
  if (printed_unpaired_header_)
    return;
  CUHeading();
  fprintf(stderr, "%s: warning: skipping unpaired lines/functions:\n",
          filename_.c_str());
  printed_unpaired_header_ = true;
}

void DwarfCUToModule::WarningReporter::UncoveredFunction(
    const Module::Function &function) {
  if (!uncovered_warnings_enabled_)
    return;
  UncoveredHeading();
  fprintf(stderr, "    function%s: %s\n",
          function.size == 0 ? " (zero-length)" : "",
          function.name.c_str());
}

void DwarfCUToModule::WarningReporter::UncoveredLine(const Module::Line &line) {
  if (!uncovered_warnings_enabled_)
    return;
  UncoveredHeading();
  fprintf(stderr, "    line%s: %s:%d at 0x%" PRIx64 "\n",
          (line.size == 0 ? " (zero-length)" : ""),
          line.file->name.c_str(), line.number, line.address);
}

void DwarfCUToModule::WarningReporter::UnnamedFunction(uint64 offset) {
  CUHeading();
  fprintf(stderr, "%s: warning: function at offset 0x%llx has no name\n",
          filename_.c_str(), offset);
}

void DwarfCUToModule::WarningReporter::DemangleError(
    const string &input, int error) {
  CUHeading();
  fprintf(stderr, "%s: warning: failed to demangle %s with error %d\n",
          filename_.c_str(), input.c_str(), error);
}

void DwarfCUToModule::WarningReporter::UnhandledInterCUReference(
    uint64 offset, uint64 target) {
  CUHeading();
  fprintf(stderr, "%s: warning: the DIE at offset 0x%llx has a "
                  "DW_FORM_ref_addr attribute with an inter-CU reference to "
                  "0x%llx, but inter-CU reference handling is turned off.\n",
                  filename_.c_str(), offset, target);
}

DwarfCUToModule::DwarfCUToModule(FileContext *file_context,
                                 LineToModuleHandler *line_reader,
                                 WarningReporter *reporter)
    : line_reader_(line_reader),
      cu_context_(new CUContext(file_context, reporter)),
      child_context_(new DIEContext()),
      has_source_line_info_(false) {
}

DwarfCUToModule::~DwarfCUToModule() {
}

void DwarfCUToModule::ProcessAttributeSigned(enum DwarfAttribute attr,
                                             enum DwarfForm form,
                                             int64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_language:
      SetLanguage(static_cast<DwarfLanguage>(data));
      break;
    default:
      break;
  }
}

void DwarfCUToModule::ProcessAttributeUnsigned(enum DwarfAttribute attr,
                                               enum DwarfForm form,
                                               uint64 data) {
  switch (attr) {
    case dwarf2reader::DW_AT_stmt_list:
      has_source_line_info_ = true;
      source_line_offset_ = data;
      break;
    case dwarf2reader::DW_AT_language:
      SetLanguage(static_cast<DwarfLanguage>(data));
      break;
    default:
      break;
  }
}

void DwarfCUToModule::ProcessAttributeString(enum DwarfAttribute attr,
                                             enum DwarfForm form,
                                             const string &data) {
  switch (attr) {
    case dwarf2reader::DW_AT_name:
      cu_context_->reporter->SetCUName(data);
      break;
    case dwarf2reader::DW_AT_comp_dir:
      line_reader_->StartCompilationUnit(data);
      break;
    default:
      break;
  }
}

bool DwarfCUToModule::EndAttributes() {
  return true;
}

dwarf2reader::DIEHandler *DwarfCUToModule::FindChildHandler(
    uint64 offset,
    enum DwarfTag tag) {
  switch (tag) {
    case dwarf2reader::DW_TAG_subprogram:
      return new FuncHandler(cu_context_.get(), child_context_.get(), offset);
    case dwarf2reader::DW_TAG_namespace:
    case dwarf2reader::DW_TAG_class_type:
    case dwarf2reader::DW_TAG_structure_type:
    case dwarf2reader::DW_TAG_union_type:
      return new NamedScopeHandler(cu_context_.get(), child_context_.get(),
                                   offset);
    default:
      return NULL;
  }
}

void DwarfCUToModule::SetLanguage(DwarfLanguage language) {
  switch (language) {
    case dwarf2reader::DW_LANG_Java:
      cu_context_->language = Language::Java;
      break;

    case dwarf2reader::DW_LANG_Mips_Assembler:
      cu_context_->language = Language::Assembler;
      break;

    default:
    case dwarf2reader::DW_LANG_ObjC:
    case dwarf2reader::DW_LANG_ObjC_plus_plus:
    case dwarf2reader::DW_LANG_C:
    case dwarf2reader::DW_LANG_C89:
    case dwarf2reader::DW_LANG_C99:
    case dwarf2reader::DW_LANG_C_plus_plus:
      cu_context_->language = Language::CPlusPlus;
      break;
  }
}

void DwarfCUToModule::ReadSourceLines(uint64 offset) {
  const dwarf2reader::SectionMap &section_map
      = cu_context_->file_context->section_map();
  dwarf2reader::SectionMap::const_iterator map_entry
      = section_map.find(".debug_line");

  if (map_entry == section_map.end())
    map_entry = section_map.find("__debug_line");
  if (map_entry == section_map.end()) {
    cu_context_->reporter->MissingSection(".debug_line");
    return;
  }
  const char *section_start = map_entry->second.first;
  uint64 section_length = map_entry->second.second;
  if (offset >= section_length) {
    cu_context_->reporter->BadLineInfoOffset(offset);
    return;
  }
  line_reader_->ReadProgram(section_start + offset, section_length - offset,
                            cu_context_->file_context->module_, &lines_);
}

namespace {

template <class T>
inline bool within(const T &item, Module::Address address) {

  return address - item.address < item.size;
}
}

void DwarfCUToModule::AssignLinesToFunctions() {
  vector<Module::Function *> *functions = &cu_context_->functions;
  WarningReporter *reporter = cu_context_->reporter;

  std::sort(functions->begin(), functions->end(),
            Module::Function::CompareByAddress);
  std::sort(lines_.begin(), lines_.end(), Module::Line::CompareByAddress);

  const Module::Line *last_line_used = NULL;

  const Module::Function *last_function_cited = NULL;
  const Module::Line *last_line_cited = NULL;

  vector<Module::Function *>::iterator func_it = functions->begin();
  vector<Module::Line>::const_iterator line_it = lines_.begin();

  Module::Address current;

  Module::Function *func;
  const Module::Line *line;

  if (func_it != functions->end() && line_it != lines_.end()) {
    func = *func_it;
    line = &*line_it;
    current = std::min(func->address, line->address);
  } else if (line_it != lines_.end()) {
    func = NULL;
    line = &*line_it;
    current = line->address;
  } else if (func_it != functions->end()) {
    func = *func_it;
    line = NULL;
    current = (*func_it)->address;
  } else {
    return;
  }

  while (func || line) {

    assert(!func || current < func->address || within(*func, current));
    assert(!line || current < line->address || within(*line, current));

    Module::Address next_transition;

    if (func && current >= func->address) {
      if (line && current >= line->address) {

        Module::Address func_left = func->size - (current - func->address);
        Module::Address line_left = line->size - (current - line->address);

        next_transition = current + std::min(func_left, line_left);
        Module::Line l = *line;
        l.address = current;
        l.size = next_transition - current;
        func->lines.push_back(l);
        last_line_used = line;
      } else {

        if (func != last_function_cited) {
          reporter->UncoveredFunction(*func);
          last_function_cited = func;
        }
        if (line && within(*func, line->address))
          next_transition = line->address;
        else

          next_transition = func->address + func->size;
      }
    } else {
      if (line && current >= line->address) {

        if (line != last_line_cited
            && !(func
                 && line == last_line_used
                 && func->address - line->address == line->size)) {
          reporter->UncoveredLine(*line);
          last_line_cited = line;
        }
        if (func && within(*line, func->address))
          next_transition = func->address;
        else

          next_transition = line->address + line->size;
      } else {

        assert(func || line);
        if (func && line)
          next_transition = std::min(func->address, line->address);
        else if (func)
          next_transition = func->address;
        else
          next_transition = line->address;
      }
    }

    if (!next_transition)
      break;

    while (func_it != functions->end()
           && next_transition >= (*func_it)->address
           && !within(**func_it, next_transition))
      func_it++;
    func = (func_it != functions->end()) ? *func_it : NULL;
    while (line_it != lines_.end()
           && next_transition >= line_it->address
           && !within(*line_it, next_transition))
      line_it++;
    line = (line_it != lines_.end()) ? &*line_it : NULL;

    assert(next_transition > current);
    current = next_transition;
  }
}

void DwarfCUToModule::Finish() {

  if (!cu_context_->language->HasFunctions())
    return;

  if (has_source_line_info_)
    ReadSourceLines(source_line_offset_);

  vector<Module::Function *> *functions = &cu_context_->functions;

  AssignLinesToFunctions();

  cu_context_->file_context->module_->AddFunctions(functions->begin(),
                                                   functions->end());

  functions->clear();

  cu_context_->file_context->ClearSpecifications();
}

bool DwarfCUToModule::StartCompilationUnit(uint64 offset,
                                           uint8 address_size,
                                           uint8 offset_size,
                                           uint64 cu_length,
                                           uint8 dwarf_version) {
  return dwarf_version >= 2;
}

bool DwarfCUToModule::StartRootDIE(uint64 offset, enum DwarfTag tag) {

  return tag == dwarf2reader::DW_TAG_compile_unit;
}

}
