// Copyright (c) 2013, Google Inc.
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

// Original author: Gordana Cmiljanovic <gordana.cmiljanovic@imgtec.com>

// stackwalker_mips_unittest.cc: Unit tests for StackwalkerMIPS class.

#include <string.h>
#include <string>
#include <vector>

#include "breakpad_googletest_includes.h"
#include "common/test_assembler.h"
#include "common/using_std_string.h"
#include "google_breakpad/common/minidump_format.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/source_line_resolver_interface.h"
#include "google_breakpad/processor/stack_frame_cpu.h"
#include "processor/stackwalker_unittest_utils.h"
#include "processor/stackwalker_mips.h"
#include "processor/windows_frame_info.h"

using google_breakpad::BasicSourceLineResolver;
using google_breakpad::CallStack;
using google_breakpad::CodeModule;
using google_breakpad::StackFrameSymbolizer;
using google_breakpad::StackFrame;
using google_breakpad::StackFrameMIPS;
using google_breakpad::Stackwalker;
using google_breakpad::StackwalkerMIPS;
using google_breakpad::SystemInfo;
using google_breakpad::WindowsFrameInfo;
using google_breakpad::test_assembler::kLittleEndian;
using google_breakpad::test_assembler::Label;
using google_breakpad::test_assembler::Section;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::SetArgumentPointee;
using testing::Test;

class StackwalkerMIPSFixture {
 public:
  StackwalkerMIPSFixture()
    : stack_section(kLittleEndian),

      module1(0x00400000, 0x10000, "module1", "version1"),
      module2(0x00500000, 0x10000, "module2", "version2") {

    system_info.os = "Linux";
    system_info.os_short = "linux";
    system_info.os_version = "Observant Opossum";
    system_info.cpu = "mips";
    system_info.cpu_info = "";

    BrandContext(&raw_context);

    modules.Add(&module1);
    modules.Add(&module2);

    EXPECT_CALL(supplier, GetCStringSymbolData(_, _, _, _, _))
      .WillRepeatedly(Return(MockSymbolSupplier::NOT_FOUND));

    EXPECT_CALL(supplier, FreeSymbolData(_)).Times(AnyNumber());

    Stackwalker::set_max_frames_scanned(1024);
  }

  void SetModuleSymbols(MockCodeModule* module, const string& info) {
    size_t buffer_size;
    char* buffer = supplier.CopySymbolDataAndOwnTheCopy(info, &buffer_size);
    EXPECT_CALL(supplier, GetCStringSymbolData(module, &system_info, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(buffer),
                            SetArgumentPointee<4>(buffer_size),
                            Return(MockSymbolSupplier::FOUND)));
  }

  void RegionFromSection() {
    string contents;
    ASSERT_TRUE(stack_section.GetContents(&contents));
    stack_region.Init(stack_section.start().Value(), contents);
  }

  void BrandContext(MDRawContextMIPS* raw_context) {
    uint8_t x = 173;
    for (size_t i = 0; i < sizeof(*raw_context); ++i)
      reinterpret_cast<uint8_t*>(raw_context)[i] = (x += 17);
  }

  SystemInfo system_info;
  MDRawContextMIPS raw_context;
  Section stack_section;
  MockMemoryRegion stack_region;
  MockCodeModule module1;
  MockCodeModule module2;
  MockCodeModules modules;
  MockSymbolSupplier supplier;
  BasicSourceLineResolver resolver;
  CallStack call_stack;
  const vector<StackFrame*>* frames;
};

class SanityCheck: public StackwalkerMIPSFixture, public Test { };

TEST_F(SanityCheck, NoResolver) {
  stack_section.start() = 0x80000000;
  stack_section.D32(0).D32(0x0);
  RegionFromSection();
  raw_context.epc = 0x00400020;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = 0x80000000;

  StackFrameSymbolizer frame_symbolizer(NULL, NULL);
  StackwalkerMIPS walker(&system_info, &raw_context, &stack_region, &modules,
                        &frame_symbolizer);

  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());
  StackFrameMIPS* frame = static_cast<StackFrameMIPS*>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

class GetContextFrame: public StackwalkerMIPSFixture, public Test { };

TEST_F(GetContextFrame, Simple) {
  stack_section.start() = 0x80000000;
  stack_section.D32(0).D32(0x0);
  RegionFromSection();
  raw_context.epc = 0x00400020;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = 0x80000000;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerMIPS walker(&system_info, &raw_context, &stack_region, &modules,
                         &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  StackFrameMIPS* frame = static_cast<StackFrameMIPS*>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

TEST_F(GetContextFrame, NoStackMemory) {
  raw_context.epc = 0x00400020;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = 0x80000000;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerMIPS walker(&system_info, &raw_context, NULL, &modules,
                         &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  StackFrameMIPS* frame = static_cast<StackFrameMIPS*>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

class GetCallerFrame: public StackwalkerMIPSFixture, public Test { };

TEST_F(GetCallerFrame, ScanWithoutSymbols) {

  stack_section.start() = 0x80000000;
  uint32_t return_address1 = 0x00400100;
  uint32_t return_address2 = 0x00400900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(16, 0)

    .D32(0x00490000)
    .D32(0x00600000)

    .D32(frame1_sp)
    .D32(return_address1)

    .Mark(&frame1_sp)
    .Append(16, 0)

    .D32(0xF0000000)
    .D32(0x0000000D)

    .D32(frame2_sp)
    .D32(return_address2)

    .Mark(&frame2_sp)
    .Append(32, 0);
  RegionFromSection();

  raw_context.epc = 0x00405510;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = stack_section.start().Value();
  raw_context.iregs[MD_CONTEXT_MIPS_REG_RA] = return_address1;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerMIPS walker(&system_info, &raw_context, &stack_region, &modules,
                         &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(3U, frames->size());

  StackFrameMIPS* frame0 = static_cast<StackFrameMIPS*>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameMIPS::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameMIPS* frame1 = static_cast<StackFrameMIPS*>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameMIPS::CONTEXT_VALID_PC |
             StackFrameMIPS::CONTEXT_VALID_SP |
             StackFrameMIPS::CONTEXT_VALID_FP |
             StackFrameMIPS::CONTEXT_VALID_RA),
            frame1->context_validity);
  EXPECT_EQ(return_address1 - 2 * sizeof(return_address1), frame1->context.epc);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_MIPS_REG_SP]);

  StackFrameMIPS* frame2 = static_cast<StackFrameMIPS*>(frames->at(2));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame2->trust);
  ASSERT_EQ((StackFrameMIPS::CONTEXT_VALID_PC |
             StackFrameMIPS::CONTEXT_VALID_SP |
             StackFrameMIPS::CONTEXT_VALID_FP |
             StackFrameMIPS::CONTEXT_VALID_RA),
            frame2->context_validity);
  EXPECT_EQ(return_address2 - 2 * sizeof(return_address2), frame2->context.epc);
  EXPECT_EQ(frame2_sp.Value(), frame2->context.iregs[MD_CONTEXT_MIPS_REG_SP]);
}

TEST_F(GetCallerFrame, ScanWithFunctionSymbols) {

  stack_section.start() = 0x80000000;
  uint32_t return_address = 0x00500200;
  Label frame1_sp;
  stack_section

    .Append(16, 0)

    .D32(0x00490000)
    .D32(0x00600000)

    .D32(0x00401000)
    .D32(0x0050F000)

    .D32(frame1_sp)
    .D32(return_address)

    .Mark(&frame1_sp)
    .Append(32, 0);
  RegionFromSection();

  raw_context.epc = 0x00400200;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = stack_section.start().Value();
  raw_context.iregs[MD_CONTEXT_MIPS_REG_RA] = return_address;

  SetModuleSymbols(&module1,

                   "FUNC 100 400 10 monotreme\n");
  SetModuleSymbols(&module2,

                   "FUNC 100 400 10 marsupial\n");

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerMIPS walker(&system_info, &raw_context, &stack_region, &modules,
                         &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(2U, frames->size());

  StackFrameMIPS* frame0 = static_cast<StackFrameMIPS*>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameMIPS::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));
  EXPECT_EQ("monotreme", frame0->function_name);
  EXPECT_EQ(0x00400100U, frame0->function_base);

  StackFrameMIPS* frame1 = static_cast<StackFrameMIPS*>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameMIPS::CONTEXT_VALID_PC |
             StackFrameMIPS::CONTEXT_VALID_SP |
             StackFrameMIPS::CONTEXT_VALID_FP |
             StackFrameMIPS::CONTEXT_VALID_RA),
            frame1->context_validity);
  EXPECT_EQ(return_address - 2 * sizeof(return_address), frame1->context.epc);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_MIPS_REG_SP]);
  EXPECT_EQ("marsupial", frame1->function_name);
  EXPECT_EQ(0x00500100U, frame1->function_base);
}

TEST_F(GetCallerFrame, CheckStackFrameSizeLimit) {

  stack_section.start() = 0x80000000;
  uint32_t return_address1 = 0x00500100;
  uint32_t return_address2 = 0x00500900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(32, 0)

    .D32(0x00490000)
    .D32(0x00600000)

    .Append(96, 0)

    .D32(frame1_sp)
    .D32(return_address1)

    .Mark(&frame1_sp)
    .Append(128 * 4, 0)

    .D32(0x00F00000)
    .D32(0x0000000D)

    .Append(128 * 4, 0)

    .D32(frame2_sp)
    .D32(return_address2)

    .Mark(&frame2_sp)
    .Append(32, 0);
  RegionFromSection();

  raw_context.epc = 0x00405510;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = stack_section.start().Value();
  raw_context.iregs[MD_CONTEXT_MIPS_REG_RA] = return_address1;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerMIPS walker(&system_info, &raw_context, &stack_region, &modules,
                         &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(2U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ("module2", modules_without_symbols[1]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(2U, frames->size());

  StackFrameMIPS* frame0 = static_cast<StackFrameMIPS*>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameMIPS::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameMIPS* frame1 = static_cast<StackFrameMIPS*>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameMIPS::CONTEXT_VALID_PC |
             StackFrameMIPS::CONTEXT_VALID_SP |
             StackFrameMIPS::CONTEXT_VALID_FP |
             StackFrameMIPS::CONTEXT_VALID_RA),
            frame1->context_validity);
  EXPECT_EQ(return_address1 - 2 * sizeof(return_address1), frame1->context.epc);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_MIPS_REG_SP]);
}

TEST_F(GetCallerFrame, ScanningNotAllowed) {

  stack_section.start() = 0x80000000;
  uint32_t return_address1 = 0x00500100;
  uint32_t return_address2 = 0x00500900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(32, 0)

    .D32(0x00490000)
    .D32(0x00600000)

    .Append(96, 0)

    .D32(frame1_sp)
    .D32(return_address1)

    .Mark(&frame1_sp)
    .Append(128 * 4, 0)

    .D32(0x00F00000)
    .D32(0x0000000D)

    .Append(128 * 4, 0)

    .D32(frame2_sp)
    .D32(return_address2)

    .Mark(&frame2_sp)
    .Append(32, 0);
  RegionFromSection();

  raw_context.epc = 0x00405510;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = stack_section.start().Value();
  raw_context.iregs[MD_CONTEXT_MIPS_REG_RA] = return_address1;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerMIPS walker(&system_info, &raw_context, &stack_region, &modules,
                         &frame_symbolizer);
  Stackwalker::set_max_frames_scanned(0);

  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());

  StackFrameMIPS* frame0 = static_cast<StackFrameMIPS*>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameMIPS::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));
}

struct CFIFixture: public StackwalkerMIPSFixture {
  CFIFixture() {

    SetModuleSymbols(&module1,

                     "FUNC 4000 1000 0 enchiridion\n"

                     "STACK CFI INIT 4000 1000 .cfa: $sp 0 + .ra: $ra\n"

                     "STACK CFI 4004 .cfa: $sp 32 +\n"

                     "STACK CFI 4008 $fp: .cfa -8 + ^ .ra: .cfa -4 + ^\n"

                     "STACK CFI 400c .cfa: $fp 32 +\n"

                     "STACK CFI 4018 .cfa: $sp 32 +\n"

                     "STACK CFI 4020 $fp: $fp .cfa: $sp 0 + .ra: .ra\n"

                     "FUNC 5000 1000 0 epictetus\n"

                     "STACK CFI INIT 5000 1000 .cfa: $sp .ra: $ra\n"

                     "STACK CFI INIT 5000 8 .cfa: $sp 0 + .ra: $ra\n"

                     "FUNC 6000 1000 20 palinal\n"
                     "STACK CFI INIT 6000 1000 .cfa: $sp 4 - .ra: $ra\n"

                     "FUNC 7000 1000 20 rhetorical\n"
                     "STACK CFI INIT 7000 1000 .cfa: moot .ra: ambiguous\n"
                   );

    expected.epc = 0x00405508;
    expected.iregs[MD_CONTEXT_MIPS_REG_S0] = 0x0;
    expected.iregs[MD_CONTEXT_MIPS_REG_S1] = 0x1;
    expected.iregs[MD_CONTEXT_MIPS_REG_S2] = 0x2;
    expected.iregs[MD_CONTEXT_MIPS_REG_S3] = 0x3;
    expected.iregs[MD_CONTEXT_MIPS_REG_S4] = 0x4;
    expected.iregs[MD_CONTEXT_MIPS_REG_S5] = 0x5;
    expected.iregs[MD_CONTEXT_MIPS_REG_S6] = 0x6;
    expected.iregs[MD_CONTEXT_MIPS_REG_S7] = 0x7;
    expected.iregs[MD_CONTEXT_MIPS_REG_SP] = 0x80000000;
    expected.iregs[MD_CONTEXT_MIPS_REG_FP] = 0x80000000;
    expected.iregs[MD_CONTEXT_MIPS_REG_RA] = 0x00405510;

    expected_validity = (StackFrameMIPS::CONTEXT_VALID_PC |
                         StackFrameMIPS::CONTEXT_VALID_S0 |
                         StackFrameMIPS::CONTEXT_VALID_S1 |
                         StackFrameMIPS::CONTEXT_VALID_S2 |
                         StackFrameMIPS::CONTEXT_VALID_S3 |
                         StackFrameMIPS::CONTEXT_VALID_S4 |
                         StackFrameMIPS::CONTEXT_VALID_S5 |
                         StackFrameMIPS::CONTEXT_VALID_S6 |
                         StackFrameMIPS::CONTEXT_VALID_S7 |
                         StackFrameMIPS::CONTEXT_VALID_SP |
                         StackFrameMIPS::CONTEXT_VALID_FP |
                         StackFrameMIPS::CONTEXT_VALID_RA);

    context_frame_validity = StackFrameMIPS::CONTEXT_VALID_ALL;

    raw_context = expected;
  }

  void CheckWalk() {
    RegionFromSection();
    raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = stack_section.start().Value();

    StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
    StackwalkerMIPS walker(&system_info, &raw_context, &stack_region,
                           &modules, &frame_symbolizer);
    vector<const CodeModule*> modules_without_symbols;
    vector<const CodeModule*> modules_with_corrupt_symbols;
    ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                            &modules_with_corrupt_symbols));
    ASSERT_EQ(0U, modules_without_symbols.size());
    ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
    frames = call_stack.frames();
    ASSERT_EQ(2U, frames->size());

    StackFrameMIPS* frame0 = static_cast<StackFrameMIPS*>(frames->at(0));
    EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
    ASSERT_EQ(StackFrameMIPS::CONTEXT_VALID_ALL, frame0->context_validity);
    EXPECT_EQ("enchiridion", frame0->function_name);
    EXPECT_EQ(0x00404000U, frame0->function_base);

    StackFrameMIPS* frame1 = static_cast<StackFrameMIPS*>(frames->at(1));
    EXPECT_EQ(StackFrame::FRAME_TRUST_CFI, frame1->trust);
    ASSERT_EQ(expected_validity, frame1->context_validity);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_S0],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_S0]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_S1],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_S1]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_S2],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_S2]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_S3],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_S3]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_S4],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_S4]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_S5],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_S5]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_S6],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_S6]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_S7],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_S7]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_FP],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_FP]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_RA],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_RA]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_MIPS_REG_SP],
              frame1->context.iregs[MD_CONTEXT_MIPS_REG_SP]);
    EXPECT_EQ(expected.epc, frame1->context.epc);
    EXPECT_EQ(expected.epc, frame1->instruction);
    EXPECT_EQ("epictetus", frame1->function_name);
    EXPECT_EQ(0x00405000U, frame1->function_base);
  }

  MDRawContextMIPS expected;

  int expected_validity;

  int context_frame_validity;
};

class CFI: public CFIFixture, public Test { };

TEST_F(CFI, At4004) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_MIPS_REG_SP];
  stack_section

    .Append(24, 0)
    .D32(frame1_sp)
    .D32(0x00405510)
    .Mark(&frame1_sp);
  raw_context.epc = 0x00404004;
  CheckWalk();
}

TEST_F(CFI, RejectBackwards) {
  raw_context.epc = 0x40005000;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = 0x80000000;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_RA] = 0x00405510;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerMIPS walker(&system_info, &raw_context, &stack_region, &modules,
                         &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());
}

TEST_F(CFI, RejectBadExpressions) {
  raw_context.epc = 0x00407000;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_SP] = 0x80000000;
  raw_context.iregs[MD_CONTEXT_MIPS_REG_RA] = 0x00405510;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerMIPS walker(&system_info, &raw_context, &stack_region, &modules,
                         &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());
}
