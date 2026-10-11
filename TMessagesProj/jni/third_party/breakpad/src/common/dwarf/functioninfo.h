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

// This file contains the definitions for a DWARF2/3 information
// collector that uses the DWARF2/3 reader interface to build a mapping
// of addresses to files, lines, and functions.

#ifndef COMMON_DWARF_FUNCTIONINFO_H__
#define COMMON_DWARF_FUNCTIONINFO_H__

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "common/dwarf/dwarf2reader.h"
#include "common/using_std_string.h"

namespace dwarf2reader {

struct FunctionInfo {

  string name;

  string mangled_name;

  string file;

  uint32 line;

  uint64 lowpc;

  uint64 highpc;
};

struct SourceFileInfo {

  string name;

  uint64 lowpc;
};

typedef std::map<uint64, FunctionInfo*> FunctionMap;
typedef std::map<uint64, std::pair<string, uint32> > LineMap;

class CULineInfoHandler: public LineInfoHandler {
 public:

  CULineInfoHandler(std::vector<SourceFileInfo>* files,
                    std::vector<string>* dirs,
                    LineMap* linemap);
  virtual ~CULineInfoHandler() { }

  virtual void DefineDir(const string& name, uint32 dir_num);

  virtual void DefineFile(const string& name, int32 file_num,
                          uint32 dir_num, uint64 mod_time, uint64 length);

  virtual void AddLine(uint64 address, uint64 length,
                       uint32 file_num, uint32 line_num, uint32 column_num);

 private:
  LineMap* linemap_;
  std::vector<SourceFileInfo>* files_;
  std::vector<string>* dirs_;
};

class CUFunctionInfoHandler: public Dwarf2Handler {
 public:
  CUFunctionInfoHandler(std::vector<SourceFileInfo>* files,
                        std::vector<string>* dirs,
                        LineMap* linemap,
                        FunctionMap* offset_to_funcinfo,
                        FunctionMap* address_to_funcinfo,
                        CULineInfoHandler* linehandler,
                        const SectionMap& sections,
                        ByteReader* reader)
      : files_(files), dirs_(dirs), linemap_(linemap),
        offset_to_funcinfo_(offset_to_funcinfo),
        address_to_funcinfo_(address_to_funcinfo),
        linehandler_(linehandler), sections_(sections),
        reader_(reader), current_function_info_(NULL) { }

  virtual ~CUFunctionInfoHandler() { }

  virtual bool StartCompilationUnit(uint64 offset, uint8 address_size,
                                    uint8 offset_size, uint64 cu_length,
                                    uint8 dwarf_version);

  virtual bool StartDIE(uint64 offset, enum DwarfTag tag);

  virtual void ProcessAttributeUnsigned(uint64 offset,
                                        enum DwarfAttribute attr,
                                        enum DwarfForm form,
                                        uint64 data);

  virtual void ProcessAttributeReference(uint64 offset,
                                         enum DwarfAttribute attr,
                                         enum DwarfForm form,
                                         uint64 data);

  virtual void ProcessAttributeString(uint64 offset,
                                      enum DwarfAttribute attr,
                                      enum DwarfForm form,
                                      const string& data);

  virtual void EndDIE(uint64 offset);

 private:
  std::vector<SourceFileInfo>* files_;
  std::vector<string>* dirs_;
  LineMap* linemap_;
  FunctionMap* offset_to_funcinfo_;
  FunctionMap* address_to_funcinfo_;
  CULineInfoHandler* linehandler_;
  const SectionMap& sections_;
  ByteReader* reader_;
  FunctionInfo* current_function_info_;
  uint64 current_compilation_unit_offset_;
};

}
#endif
