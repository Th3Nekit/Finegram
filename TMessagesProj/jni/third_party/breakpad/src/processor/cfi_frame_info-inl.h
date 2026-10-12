// -*- mode: C++ -*-

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

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// cfi_frame_info-inl.h: Definitions for cfi_frame_info.h inlined functions.

#ifndef PROCESSOR_CFI_FRAME_INFO_INL_H_
#define PROCESSOR_CFI_FRAME_INFO_INL_H_

#include <string.h>

namespace google_breakpad {

template <typename RegisterType, class RawContextType>
bool SimpleCFIWalker<RegisterType, RawContextType>::FindCallerRegisters(
    const MemoryRegion &memory,
    const CFIFrameInfo &cfi_frame_info,
    const RawContextType &callee_context,
    int callee_validity,
    RawContextType *caller_context,
    int *caller_validity) const {
  typedef CFIFrameInfo::RegisterValueMap<RegisterType> ValueMap;
  ValueMap callee_registers;
  ValueMap caller_registers;

  typename ValueMap::const_iterator caller_none = caller_registers.end();

  for (size_t i = 0; i < map_size_; i++) {
    const RegisterSet &r = register_map_[i];
    if (callee_validity & r.validity_flag)
      callee_registers[r.name] = callee_context.*r.context_member;
  }

  if (!cfi_frame_info.FindCallerRegs<RegisterType>(callee_registers, memory,
                                                   &caller_registers))
    return false;

  memset(caller_context, 0xda, sizeof(*caller_context));
  *caller_validity = 0;
  for (size_t i = 0; i < map_size_; i++) {
    const RegisterSet &r = register_map_[i];
    typename ValueMap::const_iterator caller_entry;

    caller_entry = caller_registers.find(r.name);
    if (caller_entry != caller_none) {
      caller_context->*r.context_member = caller_entry->second;
      *caller_validity |= r.validity_flag;
      continue;
    }

    if (r.alternate_name) {
      caller_entry = caller_registers.find(r.alternate_name);
      if (caller_entry != caller_none) {
        caller_context->*r.context_member = caller_entry->second;
        *caller_validity |= r.validity_flag;
        continue;
      }
    }

    if (r.callee_saves && (callee_validity & r.validity_flag) != 0) {
      caller_context->*r.context_member = callee_context.*r.context_member;
      *caller_validity |= r.validity_flag;
      continue;
    }

  }

  return true;
}

}

#endif
