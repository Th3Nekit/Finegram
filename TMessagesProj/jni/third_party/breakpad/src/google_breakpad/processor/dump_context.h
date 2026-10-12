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

// dump_context.h: A (mini/micro) dump CPU-specific context.

#ifndef GOOGLE_BREAKPAD_PROCESSOR_DUMP_CONTEXT_H__
#define GOOGLE_BREAKPAD_PROCESSOR_DUMP_CONTEXT_H__

#include "google_breakpad/common/minidump_format.h"
#include "google_breakpad/processor/dump_object.h"

namespace google_breakpad {

class DumpContext : public DumpObject {
 public:
  virtual ~DumpContext();

  uint32_t GetContextCPU() const;

  uint32_t GetContextFlags() const;

  const MDRawContextAMD64* GetContextAMD64() const;
  const MDRawContextARM*   GetContextARM() const;
  const MDRawContextARM64* GetContextARM64() const;
  const MDRawContextMIPS*  GetContextMIPS() const;
  const MDRawContextPPC*   GetContextPPC() const;
  const MDRawContextPPC64* GetContextPPC64() const;
  const MDRawContextSPARC* GetContextSPARC() const;
  const MDRawContextX86*   GetContextX86() const;

  bool GetInstructionPointer(uint64_t* ip) const;

  void Print();

 protected:
  DumpContext();

  void SetContextFlags(uint32_t context_flags);
  void SetContextX86(MDRawContextX86* x86);
  void SetContextPPC(MDRawContextPPC* ppc);
  void SetContextPPC64(MDRawContextPPC64* ppc64);
  void SetContextAMD64(MDRawContextAMD64* amd64);
  void SetContextSPARC(MDRawContextSPARC* ctx_sparc);
  void SetContextARM(MDRawContextARM* arm);
  void SetContextARM64(MDRawContextARM64* arm64);
  void SetContextMIPS(MDRawContextMIPS* ctx_mips);

  void FreeContext();

 private:

  union {
    MDRawContextBase*  base;
    MDRawContextX86*   x86;
    MDRawContextPPC*   ppc;
    MDRawContextPPC64* ppc64;
    MDRawContextAMD64* amd64;

    MDRawContextSPARC* ctx_sparc;
    MDRawContextARM*   arm;
    MDRawContextARM64* arm64;
    MDRawContextMIPS*  ctx_mips;
  } context_;

  uint32_t context_flags_;
};

}

#endif
