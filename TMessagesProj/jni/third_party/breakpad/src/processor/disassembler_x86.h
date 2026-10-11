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

// disassembler_x86.h: Basic x86 bytecode disassembler
//
// Provides a simple disassembler which wraps libdisasm. This allows simple
// tests to be run against bytecode to test for various properties.
//
// Author: Cris Neckar

#ifndef GOOGLE_BREAKPAD_PROCESSOR_DISASSEMBLER_X86_H_
#define GOOGLE_BREAKPAD_PROCESSOR_DISASSEMBLER_X86_H_

#include <stddef.h>
#include <sys/types.h>

#include "google_breakpad/common/breakpad_types.h"

namespace libdis {
#include "third_party/libdisasm/libdis.h"
}

namespace google_breakpad {

enum {
  DISX86_NONE =                 0x0,
  DISX86_BAD_BRANCH_TARGET =    0x1,
  DISX86_BAD_ARGUMENT_PASSED =  0x2,
  DISX86_BAD_WRITE =            0x4,
  DISX86_BAD_BLOCK_WRITE =      0x8,
  DISX86_BAD_READ =             0x10,
  DISX86_BAD_BLOCK_READ =       0x20,
  DISX86_BAD_COMPARISON =       0x40
};

class DisassemblerX86 {
  public:

    DisassemblerX86(const uint8_t *bytecode, uint32_t, uint32_t);
    ~DisassemblerX86();

    uint32_t NextInstruction();

    bool currentInstructionValid() { return instr_valid_; }

    const libdis::x86_insn_t* currentInstruction() {
      return instr_valid_ ? &current_instr_ : NULL;
    }

    libdis::x86_insn_group currentInstructionGroup() {
      return current_instr_.group;
    }

    bool endOfBlock() { return end_of_block_; }

    uint16_t flags() { return flags_; }

    bool setBadRead();
    bool setBadWrite();

  protected:
    const uint8_t *bytecode_;
    uint32_t size_;
    uint32_t virtual_address_;
    uint32_t current_byte_offset_;
    uint32_t current_inst_offset_;

    bool instr_valid_;
    libdis::x86_insn_t current_instr_;

    bool register_valid_;
    libdis::x86_reg_t bad_register_;

    bool pushed_bad_value_;
    bool end_of_block_;

    uint16_t flags_;
};

}

#endif
