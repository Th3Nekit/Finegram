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
//
// module_serializer.h: ModuleSerializer serializes a loaded symbol,
// i.e., a loaded BasicSouceLineResolver::Module instance, into a memory
// chunk of data. The serialized data can be read and loaded by
// FastSourceLineResolver without CPU & memory-intensive parsing.
//
// Author: Siyang Xie (lambxsy@google.com)

#ifndef PROCESSOR_MODULE_SERIALIZER_H__
#define PROCESSOR_MODULE_SERIALIZER_H__

#include <map>
#include <string>

#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/fast_source_line_resolver.h"
#include "processor/basic_source_line_resolver_types.h"
#include "processor/fast_source_line_resolver_types.h"
#include "processor/linked_ptr.h"
#include "processor/map_serializers-inl.h"
#include "processor/simple_serializer-inl.h"
#include "processor/windows_frame_info.h"

namespace google_breakpad {

class ModuleSerializer {
 public:

  size_t SizeOf(const BasicSourceLineResolver::Module &module);

  char* Write(const BasicSourceLineResolver::Module &module, char *dest);

  char* Serialize(const BasicSourceLineResolver::Module &module,
                  unsigned int *size = NULL);

  char* SerializeSymbolFileData(const string &symbol_data,
                                unsigned int *size = NULL);

  bool ConvertOneModule(const string &moduleid,
                        const BasicSourceLineResolver *basic_resolver,
                        FastSourceLineResolver *fast_resolver);

  void ConvertAllModules(const BasicSourceLineResolver *basic_resolver,
                         FastSourceLineResolver *fast_resolver);

 private:

  typedef BasicSourceLineResolver::Line Line;
  typedef BasicSourceLineResolver::Function Function;
  typedef BasicSourceLineResolver::PublicSymbol PublicSymbol;

  bool SerializeModuleAndLoadIntoFastResolver(
      const BasicSourceLineResolver::ModuleMap::const_iterator &iter,
      FastSourceLineResolver *fast_resolver);

  static const int32_t kNumberMaps_ =
      FastSourceLineResolver::Module::kNumberMaps_;

  uint32_t map_sizes_[kNumberMaps_];

  StdMapSerializer<int, string> files_serializer_;
  RangeMapSerializer<MemAddr, linked_ptr<Function> > functions_serializer_;
  AddressMapSerializer<MemAddr, linked_ptr<PublicSymbol> > pubsym_serializer_;
  ContainedRangeMapSerializer<MemAddr,
                              linked_ptr<WindowsFrameInfo> > wfi_serializer_;
  RangeMapSerializer<MemAddr, string> cfi_init_rules_serializer_;
  StdMapSerializer<MemAddr, string> cfi_delta_rules_serializer_;
};

}

#endif
