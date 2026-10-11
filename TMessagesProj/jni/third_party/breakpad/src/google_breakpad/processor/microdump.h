// Copyright (c) 2014 Google Inc.
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

// microdump.h: A microdump reader.  Microdump is a minified variant of a
// minidump (see minidump.h for documentation) which contains the minimum
// amount of information required to get a stack trace for the crashing thread.
// The information contained in a microdump is:
// - the crashing thread stack
// - system information (os type / version)
// - cpu context (state of the registers)
// - list of mmaps

#ifndef GOOGLE_BREAKPAD_PROCESSOR_MICRODUMP_H__
#define GOOGLE_BREAKPAD_PROCESSOR_MICRODUMP_H__

#include <string>
#include <vector>

#include "common/scoped_ptr.h"
#include "common/using_std_string.h"
#include "google_breakpad/processor/dump_context.h"
#include "google_breakpad/processor/memory_region.h"
#include "google_breakpad/processor/system_info.h"
#include "processor/basic_code_modules.h"

namespace google_breakpad {

class MicrodumpModules : public BasicCodeModules {
 public:

  void Add(const CodeModule* module);
};

class MicrodumpContext : public DumpContext {
 public:
  virtual void SetContextARM(MDRawContextARM* arm);
  virtual void SetContextARM64(MDRawContextARM64* arm64);
};

class MicrodumpMemoryRegion : public MemoryRegion {
 public:
  MicrodumpMemoryRegion();
  virtual ~MicrodumpMemoryRegion() {}

  void Init(uint64_t base_address, const std::vector<uint8_t>& contents);

  virtual uint64_t GetBase() const;
  virtual uint32_t GetSize() const;

  virtual bool GetMemoryAtAddress(uint64_t address, uint8_t* value) const;
  virtual bool GetMemoryAtAddress(uint64_t address, uint16_t* value) const;
  virtual bool GetMemoryAtAddress(uint64_t address, uint32_t* value) const;
  virtual bool GetMemoryAtAddress(uint64_t address, uint64_t* value) const;

  virtual void Print() const;

 private:

  template<typename ValueType>
  bool GetMemoryLittleEndian(uint64_t address, ValueType* value) const;

  uint64_t base_address_;
  std::vector<uint8_t> contents_;
};

class Microdump {
 public:
  explicit Microdump(const string& contents);
  virtual ~Microdump() {}

  DumpContext* GetContext() { return context_.get(); }
  MicrodumpMemoryRegion* GetMemory() { return stack_region_.get(); }
  MicrodumpModules* GetModules() { return modules_.get(); }
  SystemInfo* GetSystemInfo() { return system_info_.get(); }

 private:
  scoped_ptr<MicrodumpContext> context_;
  scoped_ptr<MicrodumpMemoryRegion> stack_region_;
  scoped_ptr<MicrodumpModules> modules_;
  scoped_ptr<SystemInfo> system_info_;
};

}

#endif
