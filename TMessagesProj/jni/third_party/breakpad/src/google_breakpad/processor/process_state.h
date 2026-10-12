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

// process_state.h: A snapshot of a process, in a fully-digested state.
//
// Author: Mark Mentovai

#ifndef GOOGLE_BREAKPAD_PROCESSOR_PROCESS_STATE_H__
#define GOOGLE_BREAKPAD_PROCESSOR_PROCESS_STATE_H__

#include <string>
#include <vector>

#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/processor/system_info.h"
#include "google_breakpad/processor/minidump.h"

namespace google_breakpad {

using std::vector;

class CallStack;
class CodeModules;

enum ExploitabilityRating {
  EXPLOITABILITY_HIGH,

  EXPLOITABILITY_MEDIUM,

  EXPLOITABLITY_MEDIUM = EXPLOITABILITY_MEDIUM,

  EXPLOITABILITY_LOW,

  EXPLOITABILITY_INTERESTING,

  EXPLOITABILITY_NONE,

  EXPLOITABILITY_NOT_ANALYZED,

  EXPLOITABILITY_ERR_NOENGINE,

  EXPLOITABILITY_ERR_PROCESSING

};

class ProcessState {
 public:
  ProcessState() : modules_(NULL) { Clear(); }
  ~ProcessState();

  void Clear();

  uint32_t time_date_stamp() const { return time_date_stamp_; }
  uint32_t process_create_time() const { return process_create_time_; }
  bool crashed() const { return crashed_; }
  string crash_reason() const { return crash_reason_; }
  uint64_t crash_address() const { return crash_address_; }
  string assertion() const { return assertion_; }
  int requesting_thread() const { return requesting_thread_; }
  const vector<CallStack*>* threads() const { return &threads_; }
  const vector<MemoryRegion*>* thread_memory_regions() const {
    return &thread_memory_regions_;
  }
  const SystemInfo* system_info() const { return &system_info_; }
  const CodeModules* modules() const { return modules_; }
  const vector<const CodeModule*>* modules_without_symbols() const {
    return &modules_without_symbols_;
  }
  const vector<const CodeModule*>* modules_with_corrupt_symbols() const {
    return &modules_with_corrupt_symbols_;
  }
  ExploitabilityRating exploitability() const { return exploitability_; }

 private:

  friend class MinidumpProcessor;
  friend class MicrodumpProcessor;

  uint32_t time_date_stamp_;

  uint32_t process_create_time_;

  bool crashed_;

  string crash_reason_;

  uint64_t crash_address_;

  string assertion_;

  int requesting_thread_;

  vector<CallStack*> threads_;
  vector<MemoryRegion*> thread_memory_regions_;

  SystemInfo system_info_;

  const CodeModules *modules_;

  vector<const CodeModule*> modules_without_symbols_;

  vector<const CodeModule*> modules_with_corrupt_symbols_;

  ExploitabilityRating exploitability_;
};

}

#endif
