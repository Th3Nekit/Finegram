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

// stack_frame_cpu.h: CPU-specific StackFrame extensions.
//
// These types extend the StackFrame structure to carry CPU-specific register
// state.  They are defined in this header instead of stack_frame.h to
// avoid the need to include minidump_format.h when only the generic
// StackFrame type is needed.
//
// Author: Mark Mentovai

#ifndef GOOGLE_BREAKPAD_PROCESSOR_STACK_FRAME_CPU_H__
#define GOOGLE_BREAKPAD_PROCESSOR_STACK_FRAME_CPU_H__

#include "google_breakpad/common/minidump_format.h"
#include "google_breakpad/processor/stack_frame.h"

namespace google_breakpad {

struct WindowsFrameInfo;
class CFIFrameInfo;

struct StackFrameX86 : public StackFrame {

  enum ContextValidity {
    CONTEXT_VALID_NONE = 0,
    CONTEXT_VALID_EIP  = 1 << 0,
    CONTEXT_VALID_ESP  = 1 << 1,
    CONTEXT_VALID_EBP  = 1 << 2,
    CONTEXT_VALID_EAX  = 1 << 3,
    CONTEXT_VALID_EBX  = 1 << 4,
    CONTEXT_VALID_ECX  = 1 << 5,
    CONTEXT_VALID_EDX  = 1 << 6,
    CONTEXT_VALID_ESI  = 1 << 7,
    CONTEXT_VALID_EDI  = 1 << 8,
    CONTEXT_VALID_ALL  = -1
  };

  StackFrameX86()
     : context(),
       context_validity(CONTEXT_VALID_NONE),
       windows_frame_info(NULL),
       cfi_frame_info(NULL) {}
  ~StackFrameX86();

  virtual uint64_t ReturnAddress() const;

  MDRawContextX86 context;

  int context_validity;

  WindowsFrameInfo *windows_frame_info;
  CFIFrameInfo *cfi_frame_info;
};

struct StackFramePPC : public StackFrame {

  enum ContextValidity {
    CONTEXT_VALID_NONE = 0,
    CONTEXT_VALID_SRR0 = 1 << 0,
    CONTEXT_VALID_GPR1 = 1 << 1,
    CONTEXT_VALID_ALL  = -1
  };

  StackFramePPC() : context(), context_validity(CONTEXT_VALID_NONE) {}

  MDRawContextPPC context;

  int context_validity;
};

struct StackFramePPC64 : public StackFrame {

  enum ContextValidity {
    CONTEXT_VALID_NONE = 0,
    CONTEXT_VALID_SRR0 = 1 << 0,
    CONTEXT_VALID_GPR1 = 1 << 1,
    CONTEXT_VALID_ALL  = -1
  };

  StackFramePPC64() : context(), context_validity(CONTEXT_VALID_NONE) {}

  MDRawContextPPC64 context;

  int context_validity;
};

struct StackFrameAMD64 : public StackFrame {

  enum ContextValidity {
    CONTEXT_VALID_NONE  = 0,
    CONTEXT_VALID_RAX   = 1 << 0,
    CONTEXT_VALID_RDX   = 1 << 1,
    CONTEXT_VALID_RCX   = 1 << 2,
    CONTEXT_VALID_RBX   = 1 << 3,
    CONTEXT_VALID_RSI   = 1 << 4,
    CONTEXT_VALID_RDI   = 1 << 5,
    CONTEXT_VALID_RBP   = 1 << 6,
    CONTEXT_VALID_RSP   = 1 << 7,
    CONTEXT_VALID_R8    = 1 << 8,
    CONTEXT_VALID_R9    = 1 << 9,
    CONTEXT_VALID_R10   = 1 << 10,
    CONTEXT_VALID_R11   = 1 << 11,
    CONTEXT_VALID_R12   = 1 << 12,
    CONTEXT_VALID_R13   = 1 << 13,
    CONTEXT_VALID_R14   = 1 << 14,
    CONTEXT_VALID_R15   = 1 << 15,
    CONTEXT_VALID_RIP   = 1 << 16,
    CONTEXT_VALID_ALL  = -1
  };

  StackFrameAMD64() : context(), context_validity(CONTEXT_VALID_NONE) {}

  virtual uint64_t ReturnAddress() const;

  MDRawContextAMD64 context;

  int context_validity;
};

struct StackFrameSPARC : public StackFrame {

  enum ContextValidity {
    CONTEXT_VALID_NONE = 0,
    CONTEXT_VALID_PC   = 1 << 0,
    CONTEXT_VALID_SP   = 1 << 1,
    CONTEXT_VALID_FP   = 1 << 2,
    CONTEXT_VALID_ALL  = -1
  };

  StackFrameSPARC() : context(), context_validity(CONTEXT_VALID_NONE) {}

  MDRawContextSPARC context;

  int context_validity;
};

struct StackFrameARM : public StackFrame {

  enum ContextValidity {
    CONTEXT_VALID_NONE = 0,
    CONTEXT_VALID_R0   = 1 << 0,
    CONTEXT_VALID_R1   = 1 << 1,
    CONTEXT_VALID_R2   = 1 << 2,
    CONTEXT_VALID_R3   = 1 << 3,
    CONTEXT_VALID_R4   = 1 << 4,
    CONTEXT_VALID_R5   = 1 << 5,
    CONTEXT_VALID_R6   = 1 << 6,
    CONTEXT_VALID_R7   = 1 << 7,
    CONTEXT_VALID_R8   = 1 << 8,
    CONTEXT_VALID_R9   = 1 << 9,
    CONTEXT_VALID_R10  = 1 << 10,
    CONTEXT_VALID_R11  = 1 << 11,
    CONTEXT_VALID_R12  = 1 << 12,
    CONTEXT_VALID_R13  = 1 << 13,
    CONTEXT_VALID_R14  = 1 << 14,
    CONTEXT_VALID_R15  = 1 << 15,
    CONTEXT_VALID_ALL  = ~CONTEXT_VALID_NONE,

    CONTEXT_VALID_FP   = CONTEXT_VALID_R11,
    CONTEXT_VALID_SP   = CONTEXT_VALID_R13,
    CONTEXT_VALID_LR   = CONTEXT_VALID_R14,
    CONTEXT_VALID_PC   = CONTEXT_VALID_R15
  };

  StackFrameARM() : context(), context_validity(CONTEXT_VALID_NONE) {}

  static ContextValidity RegisterValidFlag(int n) {
    return ContextValidity(1 << n);
  }

  MDRawContextARM context;

  int context_validity;
};

struct StackFrameARM64 : public StackFrame {

  static const uint64_t CONTEXT_VALID_NONE = 0;
  static const uint64_t CONTEXT_VALID_X0   = 1ULL << 0;
  static const uint64_t CONTEXT_VALID_X1   = 1ULL << 1;
  static const uint64_t CONTEXT_VALID_X2   = 1ULL << 2;
  static const uint64_t CONTEXT_VALID_X3   = 1ULL << 3;
  static const uint64_t CONTEXT_VALID_X4   = 1ULL << 4;
  static const uint64_t CONTEXT_VALID_X5   = 1ULL << 5;
  static const uint64_t CONTEXT_VALID_X6   = 1ULL << 6;
  static const uint64_t CONTEXT_VALID_X7   = 1ULL << 7;
  static const uint64_t CONTEXT_VALID_X8   = 1ULL << 8;
  static const uint64_t CONTEXT_VALID_X9   = 1ULL << 9;
  static const uint64_t CONTEXT_VALID_X10  = 1ULL << 10;
  static const uint64_t CONTEXT_VALID_X11  = 1ULL << 11;
  static const uint64_t CONTEXT_VALID_X12  = 1ULL << 12;
  static const uint64_t CONTEXT_VALID_X13  = 1ULL << 13;
  static const uint64_t CONTEXT_VALID_X14  = 1ULL << 14;
  static const uint64_t CONTEXT_VALID_X15  = 1ULL << 15;
  static const uint64_t CONTEXT_VALID_X16  = 1ULL << 16;
  static const uint64_t CONTEXT_VALID_X17  = 1ULL << 17;
  static const uint64_t CONTEXT_VALID_X18  = 1ULL << 18;
  static const uint64_t CONTEXT_VALID_X19  = 1ULL << 19;
  static const uint64_t CONTEXT_VALID_X20  = 1ULL << 20;
  static const uint64_t CONTEXT_VALID_X21  = 1ULL << 21;
  static const uint64_t CONTEXT_VALID_X22  = 1ULL << 22;
  static const uint64_t CONTEXT_VALID_X23  = 1ULL << 23;
  static const uint64_t CONTEXT_VALID_X24  = 1ULL << 24;
  static const uint64_t CONTEXT_VALID_X25  = 1ULL << 25;
  static const uint64_t CONTEXT_VALID_X26  = 1ULL << 26;
  static const uint64_t CONTEXT_VALID_X27  = 1ULL << 27;
  static const uint64_t CONTEXT_VALID_X28  = 1ULL << 28;
  static const uint64_t CONTEXT_VALID_X29  = 1ULL << 29;
  static const uint64_t CONTEXT_VALID_X30  = 1ULL << 30;
  static const uint64_t CONTEXT_VALID_X31  = 1ULL << 31;
  static const uint64_t CONTEXT_VALID_X32  = 1ULL << 32;
  static const uint64_t CONTEXT_VALID_ALL  = ~CONTEXT_VALID_NONE;

  static const uint64_t CONTEXT_VALID_FP   = CONTEXT_VALID_X29;
  static const uint64_t CONTEXT_VALID_LR   = CONTEXT_VALID_X30;
  static const uint64_t CONTEXT_VALID_SP   = CONTEXT_VALID_X31;
  static const uint64_t CONTEXT_VALID_PC   = CONTEXT_VALID_X32;

  StackFrameARM64() : context(),
                      context_validity(CONTEXT_VALID_NONE) {}

  static uint64_t RegisterValidFlag(int n) {
    return 1ULL << n;
  }

  MDRawContextARM64 context;

  uint64_t context_validity;
};

struct StackFrameMIPS : public StackFrame {

#define INDEX_MIPS_REG_S0 MD_CONTEXT_MIPS_REG_S0  // 16
#define INDEX_MIPS_REG_S7 MD_CONTEXT_MIPS_REG_S7  // 23
#define INDEX_MIPS_REG_GP MD_CONTEXT_MIPS_REG_GP  // 28
#define INDEX_MIPS_REG_RA MD_CONTEXT_MIPS_REG_RA  // 31
#define INDEX_MIPS_REG_PC 34
#define SHIFT_MIPS_REG_S0 0
#define SHIFT_MIPS_REG_GP 8
#define SHIFT_MIPS_REG_PC 12

  enum ContextValidity {
    CONTEXT_VALID_NONE = 0,
    CONTEXT_VALID_S0 = 1 << 0,
    CONTEXT_VALID_S1 = 1 << 1,
    CONTEXT_VALID_S2 = 1 << 2,
    CONTEXT_VALID_S3 = 1 << 3,
    CONTEXT_VALID_S4 = 1 << 4,
    CONTEXT_VALID_S5 = 1 << 5,
    CONTEXT_VALID_S6 = 1 << 6,
    CONTEXT_VALID_S7 = 1 << 7,

    CONTEXT_VALID_GP = 1 << 8,
    CONTEXT_VALID_SP = 1 << 9,
    CONTEXT_VALID_FP = 1 << 10,
    CONTEXT_VALID_RA = 1 << 11,
    CONTEXT_VALID_PC = 1 << 12,
    CONTEXT_VALID_ALL = ~CONTEXT_VALID_NONE
  };

  static ContextValidity RegisterValidFlag(int n) {
    if (n >= INDEX_MIPS_REG_S0 && n <= INDEX_MIPS_REG_S7)
      return ContextValidity(1 << (n - INDEX_MIPS_REG_S0 + SHIFT_MIPS_REG_S0));
    else if (n >= INDEX_MIPS_REG_GP && n <= INDEX_MIPS_REG_RA)
      return ContextValidity(1 << (n - INDEX_MIPS_REG_GP + SHIFT_MIPS_REG_GP));
    else if (n == INDEX_MIPS_REG_PC)
      return ContextValidity(1 << SHIFT_MIPS_REG_PC);

    return CONTEXT_VALID_NONE;
  }

  StackFrameMIPS() : context(), context_validity(CONTEXT_VALID_NONE) {}

  MDRawContextMIPS context;

  int context_validity;
};

}

#endif
