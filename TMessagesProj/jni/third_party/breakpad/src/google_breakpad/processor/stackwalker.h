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

// stackwalker.h: Generic stackwalker.
//
// The Stackwalker class is an abstract base class providing common generic
// methods that apply to stacks from all systems.  Specific implementations
// will extend this class by providing GetContextFrame and GetCallerFrame
// methods to fill in system-specific data in a StackFrame structure.
// Stackwalker assembles these StackFrame strucutres into a CallStack.
//
// Author: Mark Mentovai

#ifndef GOOGLE_BREAKPAD_PROCESSOR_STACKWALKER_H__
#define GOOGLE_BREAKPAD_PROCESSOR_STACKWALKER_H__

#include <set>
#include <string>
#include <vector>

#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/code_modules.h"
#include "google_breakpad/processor/memory_region.h"
#include "google_breakpad/processor/stack_frame_symbolizer.h"

namespace google_breakpad {

class CallStack;
class DumpContext;
class StackFrameSymbolizer;

using std::set;
using std::vector;

class Stackwalker {
 public:
  virtual ~Stackwalker() {}

  bool Walk(CallStack* stack,
            vector<const CodeModule*>* modules_without_symbols,
            vector<const CodeModule*>* modules_with_corrupt_symbols);

  static Stackwalker* StackwalkerForCPU(
     const SystemInfo* system_info,
     DumpContext* context,
     MemoryRegion* memory,
     const CodeModules* modules,
     StackFrameSymbolizer* resolver_helper);

  static void set_max_frames(uint32_t max_frames) {
    max_frames_ = max_frames;
    max_frames_set_ = true;
  }
  static uint32_t max_frames() { return max_frames_; }

  static void set_max_frames_scanned(uint32_t max_frames_scanned) {
    max_frames_scanned_ = max_frames_scanned;
  }

 protected:

  Stackwalker(const SystemInfo* system_info,
              MemoryRegion* memory,
              const CodeModules* modules,
              StackFrameSymbolizer* frame_symbolizer);

  bool InstructionAddressSeemsValid(uint64_t address);

  static const int kRASearchWords;

  template<typename InstructionType>
  bool ScanForReturnAddress(InstructionType location_start,
                            InstructionType* location_found,
                            InstructionType* ip_found,
                            bool is_context_frame) {

    const int search_words = is_context_frame ?
      kRASearchWords * 4 :
      kRASearchWords;

    return ScanForReturnAddress(location_start, location_found, ip_found,
                                search_words);
  }

  template<typename InstructionType>
  bool ScanForReturnAddress(InstructionType location_start,
                            InstructionType* location_found,
                            InstructionType* ip_found,
                            int searchwords) {
    for (InstructionType location = location_start;
         location <= location_start + searchwords * sizeof(InstructionType);
         location += sizeof(InstructionType)) {
      InstructionType ip;
      if (!memory_->GetMemoryAtAddress(location, &ip))
        break;

      if (modules_ && modules_->GetModuleForAddress(ip) &&
          InstructionAddressSeemsValid(ip)) {
        *ip_found = ip;
        *location_found = location;
        return true;
      }
    }

    return false;
  }

  const SystemInfo* system_info_;

  MemoryRegion* memory_;

  const CodeModules* modules_;

 protected:

  StackFrameSymbolizer* frame_symbolizer_;

 private:

  virtual StackFrame* GetContextFrame() = 0;

  virtual StackFrame* GetCallerFrame(const CallStack* stack,
                                     bool stack_scan_allowed) = 0;

  static uint32_t max_frames_;

  static bool max_frames_set_;

  static uint32_t max_frames_scanned_;
};

}

#endif
