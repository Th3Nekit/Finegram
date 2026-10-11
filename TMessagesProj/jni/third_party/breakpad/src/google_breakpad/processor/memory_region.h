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

// memory_region.h: Access to memory regions.
//
// A MemoryRegion provides virtual access to a range of memory.  It is an
// abstraction allowing the actual source of memory to be independent of
// methods which need to access a virtual memory space.
//
// Author: Mark Mentovai

#ifndef GOOGLE_BREAKPAD_PROCESSOR_MEMORY_REGION_H__
#define GOOGLE_BREAKPAD_PROCESSOR_MEMORY_REGION_H__

#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {

class MemoryRegion {
 public:
  virtual ~MemoryRegion() {}

  virtual uint64_t GetBase() const = 0;

  virtual uint32_t GetSize() const = 0;

  virtual bool GetMemoryAtAddress(uint64_t address, uint8_t*  value) const = 0;
  virtual bool GetMemoryAtAddress(uint64_t address, uint16_t* value) const = 0;
  virtual bool GetMemoryAtAddress(uint64_t address, uint32_t* value) const = 0;
  virtual bool GetMemoryAtAddress(uint64_t address, uint64_t* value) const = 0;

  virtual void Print() const = 0;
};

}

#endif
