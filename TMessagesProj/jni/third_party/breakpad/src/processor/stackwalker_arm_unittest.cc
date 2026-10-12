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

// stackwalker_arm_unittest.cc: Unit tests for StackwalkerARM class.

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
#include "processor/stackwalker_arm.h"
#include "processor/windows_frame_info.h"

using google_breakpad::BasicSourceLineResolver;
using google_breakpad::CallStack;
using google_breakpad::CodeModule;
using google_breakpad::StackFrameSymbolizer;
using google_breakpad::StackFrame;
using google_breakpad::StackFrameARM;
using google_breakpad::Stackwalker;
using google_breakpad::StackwalkerARM;
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

class StackwalkerARMFixture {
 public:
  StackwalkerARMFixture()
    : stack_section(kLittleEndian),

      module1(0x40000000, 0x10000, "module1", "version1"),
      module2(0x50000000, 0x10000, "module2", "version2") {

    system_info.os = "Linux";
    system_info.os_short = "linux";
    system_info.os_version = "Lugubrious Labrador";
    system_info.cpu = "arm";
    system_info.cpu_info = "";

    BrandContext(&raw_context);

    modules.Add(&module1);
    modules.Add(&module2);

    EXPECT_CALL(supplier, GetCStringSymbolData(_, _, _, _, _))
      .WillRepeatedly(Return(MockSymbolSupplier::NOT_FOUND));

    EXPECT_CALL(supplier, FreeSymbolData(_)).Times(AnyNumber());

    Stackwalker::set_max_frames_scanned(1024);
  }

  void SetModuleSymbols(MockCodeModule *module, const string &info) {
    size_t buffer_size;
    char *buffer = supplier.CopySymbolDataAndOwnTheCopy(info, &buffer_size);
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

  void BrandContext(MDRawContextARM *raw_context) {
    uint8_t x = 173;
    for (size_t i = 0; i < sizeof(*raw_context); i++)
      reinterpret_cast<uint8_t *>(raw_context)[i] = (x += 17);
  }

  SystemInfo system_info;
  MDRawContextARM raw_context;
  Section stack_section;
  MockMemoryRegion stack_region;
  MockCodeModule module1;
  MockCodeModule module2;
  MockCodeModules modules;
  MockSymbolSupplier supplier;
  BasicSourceLineResolver resolver;
  CallStack call_stack;
  const vector<StackFrame *> *frames;
};

class SanityCheck: public StackwalkerARMFixture, public Test { };

TEST_F(SanityCheck, NoResolver) {

  StackFrameSymbolizer frame_symbolizer(NULL, NULL);
  StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region, &modules,
                        &frame_symbolizer);

  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());
  StackFrameARM *frame = static_cast<StackFrameARM *>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

class GetContextFrame: public StackwalkerARMFixture, public Test { };

TEST_F(GetContextFrame, Simple) {

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region, &modules,
                        &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());
  StackFrameARM *frame = static_cast<StackFrameARM *>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

TEST_F(GetContextFrame, NoStackMemory) {
  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, -1, NULL, &modules,
                        &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());
  StackFrameARM *frame = static_cast<StackFrameARM *>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

class GetCallerFrame: public StackwalkerARMFixture, public Test { };

TEST_F(GetCallerFrame, ScanWithoutSymbols) {

  stack_section.start() = 0x80000000;
  uint32_t return_address1 = 0x50000100;
  uint32_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(16, 0)

    .D32(0x40090000)
    .D32(0x60000000)

    .D32(return_address1)

    .Mark(&frame1_sp)
    .Append(16, 0)

    .D32(0xF0000000)
    .D32(0x0000000D)

    .D32(return_address2)

    .Mark(&frame2_sp)
    .Append(32, 0);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40005510;
  raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region, &modules,
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
  ASSERT_EQ(3U, frames->size());

  StackFrameARM *frame0 = static_cast<StackFrameARM *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameARM *frame1 = static_cast<StackFrameARM *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameARM::CONTEXT_VALID_PC |
             StackFrameARM::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address1, frame1->context.iregs[MD_CONTEXT_ARM_REG_PC]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM_REG_SP]);

  StackFrameARM *frame2 = static_cast<StackFrameARM *>(frames->at(2));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame2->trust);
  ASSERT_EQ((StackFrameARM::CONTEXT_VALID_PC |
             StackFrameARM::CONTEXT_VALID_SP),
            frame2->context_validity);
  EXPECT_EQ(return_address2, frame2->context.iregs[MD_CONTEXT_ARM_REG_PC]);
  EXPECT_EQ(frame2_sp.Value(), frame2->context.iregs[MD_CONTEXT_ARM_REG_SP]);
}

TEST_F(GetCallerFrame, ScanWithFunctionSymbols) {

  stack_section.start() = 0x80000000;
  uint32_t return_address = 0x50000200;
  Label frame1_sp;

  stack_section

    .Append(16, 0)

    .D32(0x40090000)
    .D32(0x60000000)

    .D32(0x40001000)
    .D32(0x5000F000)

    .D32(return_address)

    .Mark(&frame1_sp)
    .Append(32, 0);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40000200;
  raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = stack_section.start().Value();

  SetModuleSymbols(&module1,

                   "FUNC 100 400 10 monotreme\n");
  SetModuleSymbols(&module2,

                   "FUNC 100 400 10 marsupial\n");

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region, &modules,
                        &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(2U, frames->size());

  StackFrameARM *frame0 = static_cast<StackFrameARM *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));
  EXPECT_EQ("monotreme", frame0->function_name);
  EXPECT_EQ(0x40000100U, frame0->function_base);

  StackFrameARM *frame1 = static_cast<StackFrameARM *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameARM::CONTEXT_VALID_PC |
             StackFrameARM::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address, frame1->context.iregs[MD_CONTEXT_ARM_REG_PC]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM_REG_SP]);
  EXPECT_EQ("marsupial", frame1->function_name);
  EXPECT_EQ(0x50000100U, frame1->function_base);
}

TEST_F(GetCallerFrame, ScanFirstFrame) {

  stack_section.start() = 0x80000000;
  uint32_t return_address1 = 0x50000100;
  uint32_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(32, 0)

    .D32(0x40090000)
    .D32(0x60000000)

    .Append(96, 0)

    .D32(return_address1)

    .Mark(&frame1_sp)
    .Append(32, 0)

    .D32(0xF0000000)
    .D32(0x0000000D)

    .Append(96, 0)

    .D32(return_address2)

    .Mark(&frame2_sp)
    .Append(32, 0);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40005510;
  raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region, &modules,
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

  StackFrameARM *frame0 = static_cast<StackFrameARM *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameARM *frame1 = static_cast<StackFrameARM *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameARM::CONTEXT_VALID_PC |
             StackFrameARM::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address1, frame1->context.iregs[MD_CONTEXT_ARM_REG_PC]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM_REG_SP]);
}

TEST_F(GetCallerFrame, ScanningNotAllowed) {

  stack_section.start() = 0x80000000;
  uint32_t return_address1 = 0x50000100;
  uint32_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(16, 0)

    .D32(0x40090000)
    .D32(0x60000000)

    .D32(return_address1)

    .Mark(&frame1_sp)
    .Append(16, 0)

    .D32(0xF0000000)
    .D32(0x0000000D)

    .D32(return_address2)

    .Mark(&frame2_sp)
    .Append(32, 0);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40005510;
  raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region, &modules,
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

  StackFrameARM *frame0 = static_cast<StackFrameARM *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));
}

struct CFIFixture: public StackwalkerARMFixture {
  CFIFixture() {

    SetModuleSymbols(&module1,

                     "FUNC 4000 1000 10 enchiridion\n"

                     "STACK CFI INIT 4000 100 .cfa: sp .ra: lr\n"

                     "STACK CFI 4001 .cfa: sp 12 + r4: .cfa 12 - ^"
                     " r11: .cfa 8 - ^ .ra: .cfa 4 - ^\n"

                     "STACK CFI 4002 r4: r0 r5: r1 r6: r2 r7: r3\n"

                     "STACK CFI 4003 .cfa: sp 16 + r1: .cfa 16 - ^"
                     " r4: r4 r5: r5 r6: r6 r7: r7\n"

                     "STACK CFI 4005 .cfa: sp 12 + r1: .cfa 12 - ^"
                     " r11: .cfa 4 - ^ .ra: .cfa ^ sp: .cfa 4 +\n"

                     "STACK CFI 4006 .cfa: sp 16 + pc: .cfa 16 - ^\n"

                     "FUNC 5000 1000 10 epictetus\n"

                     "STACK CFI INIT 5000 1000 .cfa: 0 .ra: 0\n"

                     "FUNC 6000 1000 20 palinal\n"
                     "STACK CFI INIT 6000 1000 .cfa: sp 4 - .ra: lr\n"

                     "FUNC 7000 1000 20 rhetorical\n"
                     "STACK CFI INIT 7000 1000 .cfa: moot .ra: ambiguous\n");

    expected.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40005510;
    expected.iregs[MD_CONTEXT_ARM_REG_SP] = 0x80000000;
    expected.iregs[4] = 0xb5d55e68;
    expected.iregs[5] = 0xebd134f3;
    expected.iregs[6] = 0xa31e74bc;
    expected.iregs[7] = 0x2dcb16b3;
    expected.iregs[8] = 0x2ada2137;
    expected.iregs[9] = 0xbbbb557d;
    expected.iregs[10] = 0x48bf8ca7;
    expected.iregs[MD_CONTEXT_ARM_REG_FP] = 0x8112e110;

    expected_validity = (StackFrameARM::CONTEXT_VALID_PC |
                         StackFrameARM::CONTEXT_VALID_SP |
                         StackFrameARM::CONTEXT_VALID_R4 |
                         StackFrameARM::CONTEXT_VALID_R5 |
                         StackFrameARM::CONTEXT_VALID_R6 |
                         StackFrameARM::CONTEXT_VALID_R7 |
                         StackFrameARM::CONTEXT_VALID_R8 |
                         StackFrameARM::CONTEXT_VALID_R9 |
                         StackFrameARM::CONTEXT_VALID_R10 |
                         StackFrameARM::CONTEXT_VALID_FP);

    context_frame_validity = StackFrameARM::CONTEXT_VALID_ALL;

    raw_context = expected;
  }

  void CheckWalk() {
    RegionFromSection();
    raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = stack_section.start().Value();

    StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
    StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region,
                          &modules, &frame_symbolizer);
    walker.SetContextFrameValidity(context_frame_validity);
    vector<const CodeModule*> modules_without_symbols;
    vector<const CodeModule*> modules_with_corrupt_symbols;
    ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                            &modules_with_corrupt_symbols));
    ASSERT_EQ(0U, modules_without_symbols.size());
    ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
    frames = call_stack.frames();
    ASSERT_EQ(2U, frames->size());

    StackFrameARM *frame0 = static_cast<StackFrameARM *>(frames->at(0));
    EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
    ASSERT_EQ(context_frame_validity, frame0->context_validity);
    EXPECT_EQ("enchiridion", frame0->function_name);
    EXPECT_EQ(0x40004000U, frame0->function_base);

    StackFrameARM *frame1 = static_cast<StackFrameARM *>(frames->at(1));
    EXPECT_EQ(StackFrame::FRAME_TRUST_CFI, frame1->trust);
    ASSERT_EQ(expected_validity, frame1->context_validity);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_R1)
      EXPECT_EQ(expected.iregs[1], frame1->context.iregs[1]);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_R4)
      EXPECT_EQ(expected.iregs[4], frame1->context.iregs[4]);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_R5)
      EXPECT_EQ(expected.iregs[5], frame1->context.iregs[5]);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_R6)
      EXPECT_EQ(expected.iregs[6], frame1->context.iregs[6]);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_R7)
      EXPECT_EQ(expected.iregs[7], frame1->context.iregs[7]);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_R8)
      EXPECT_EQ(expected.iregs[8], frame1->context.iregs[8]);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_R9)
      EXPECT_EQ(expected.iregs[9], frame1->context.iregs[9]);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_R10)
      EXPECT_EQ(expected.iregs[10], frame1->context.iregs[10]);
    if (expected_validity & StackFrameARM::CONTEXT_VALID_FP)
      EXPECT_EQ(expected.iregs[MD_CONTEXT_ARM_REG_FP],
                frame1->context.iregs[MD_CONTEXT_ARM_REG_FP]);

    EXPECT_EQ(expected.iregs[MD_CONTEXT_ARM_REG_SP],
              frame1->context.iregs[MD_CONTEXT_ARM_REG_SP]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_ARM_REG_PC],
              frame1->context.iregs[MD_CONTEXT_ARM_REG_PC]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_ARM_REG_PC],
              frame1->instruction + 2);
    EXPECT_EQ("epictetus", frame1->function_name);
  }

  MDRawContextARM expected;

  int expected_validity;

  int context_frame_validity;
};

class CFI: public CFIFixture, public Test { };

TEST_F(CFI, At4000) {
  stack_section.start() = expected.iregs[MD_CONTEXT_ARM_REG_SP];
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40004000;
  raw_context.iregs[MD_CONTEXT_ARM_REG_LR] = 0x40005510;
  CheckWalk();
}

TEST_F(CFI, At4001) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM_REG_SP];
  stack_section
    .D32(0xb5d55e68)
    .D32(0x8112e110)
    .D32(0x40005510)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40004001;
  raw_context.iregs[4] = 0x635adc9f;
  raw_context.iregs[MD_CONTEXT_ARM_REG_FP] = 0xbe145fc4;
  CheckWalk();
}

TEST_F(CFI, At4001LimitedValidity) {
  context_frame_validity =
    StackFrameARM::CONTEXT_VALID_PC | StackFrameARM::CONTEXT_VALID_SP;
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40004001;
  raw_context.iregs[MD_CONTEXT_ARM_REG_FP] = 0xbe145fc4;
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM_REG_SP];
  stack_section
    .D32(0xb5d55e68)
    .D32(0x8112e110)
    .D32(0x40005510)
    .Mark(&frame1_sp);
  expected_validity = (StackFrameARM::CONTEXT_VALID_PC
                       | StackFrameARM::CONTEXT_VALID_SP
                       | StackFrameARM::CONTEXT_VALID_FP
                       | StackFrameARM::CONTEXT_VALID_R4);
  CheckWalk();
}

TEST_F(CFI, At4002) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM_REG_SP];
  stack_section
    .D32(0xfb81ff3d)
    .D32(0x8112e110)
    .D32(0x40005510)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40004002;
  raw_context.iregs[0] = 0xb5d55e68;
  raw_context.iregs[1] = 0xebd134f3;
  raw_context.iregs[2] = 0xa31e74bc;
  raw_context.iregs[3] = 0x2dcb16b3;
  raw_context.iregs[4] = 0xfdd35466;
  raw_context.iregs[5] = 0xf18c946c;
  raw_context.iregs[6] = 0xac2079e8;
  raw_context.iregs[7] = 0xa449829f;
  raw_context.iregs[MD_CONTEXT_ARM_REG_FP] = 0xbe145fc4;
  CheckWalk();
}

TEST_F(CFI, At4003) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM_REG_SP];
  stack_section
    .D32(0x48c8dd5a)
    .D32(0xcb78040e)
    .D32(0x8112e110)
    .D32(0x40005510)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40004003;
  raw_context.iregs[1] = 0xfb756319;
  raw_context.iregs[MD_CONTEXT_ARM_REG_FP] = 0x0a2857ea;
  expected.iregs[1] = 0x48c8dd5a;
  expected_validity |= StackFrameARM::CONTEXT_VALID_R1;
  CheckWalk();
}

TEST_F(CFI, At4004) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM_REG_SP];
  stack_section
    .D32(0x48c8dd5a)
    .D32(0xcb78040e)
    .D32(0x8112e110)
    .D32(0x40005510)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40004004;
  raw_context.iregs[1] = 0xfb756319;
  expected.iregs[1] = 0x48c8dd5a;
  expected_validity |= StackFrameARM::CONTEXT_VALID_R1;
  CheckWalk();
}

TEST_F(CFI, At4005) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM_REG_SP];
  stack_section
    .D32(0x48c8dd5a)
    .D32(0xf013f841)
    .D32(0x8112e110)
    .D32(0x40005510)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40004005;
  raw_context.iregs[1] = 0xfb756319;
  expected.iregs[1] = 0x48c8dd5a;
  expected_validity |= StackFrameARM::CONTEXT_VALID_R1;
  CheckWalk();
}

TEST_F(CFI, At4006) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM_REG_SP];
  stack_section
    .D32(0x40005510)
    .D32(0x48c8dd5a)
    .D32(0xf013f841)
    .D32(0x8112e110)
    .D32(0xf8d15783)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40004006;
  raw_context.iregs[1] = 0xfb756319;
  expected.iregs[1] = 0x48c8dd5a;
  expected_validity |= StackFrameARM::CONTEXT_VALID_R1;
  CheckWalk();
}

TEST_F(CFI, RejectBackwards) {
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40006000;
  raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = 0x80000000;
  raw_context.iregs[MD_CONTEXT_ARM_REG_LR] = 0x40005510;
  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region, &modules,
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
  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40007000;
  raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = 0x80000000;
  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, -1, &stack_region, &modules,
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

class StackwalkerARMFixtureIOS : public StackwalkerARMFixture {
 public:
  StackwalkerARMFixtureIOS() {
    system_info.os = "iOS";
    system_info.os_short = "ios";
  }
};

class GetFramesByFramePointer: public StackwalkerARMFixtureIOS, public Test { };

TEST_F(GetFramesByFramePointer, OnlyFramePointer) {
  stack_section.start() = 0x80000000;
  uint32_t return_address1 = 0x50000100;
  uint32_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  Label frame1_fp, frame2_fp;
  stack_section

    .Append(32, 0)
    .D32(0x0000000D)
    .D32(0xF0000000)

    .Mark(&frame1_fp)
    .D32(frame2_fp)
    .D32(return_address2)
    .Mark(&frame1_sp)

    .Append(32, 0)
    .D32(0x0000000D)
    .D32(0xF0000000)

    .Mark(&frame2_fp)
    .D32(0)
    .D32(0)
    .Mark(&frame2_sp)

    .Append(32, 0)
    .D32(0x0000000D)
    .D32(0xF0000000);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x40005510;
  raw_context.iregs[MD_CONTEXT_ARM_REG_LR] = return_address1;
  raw_context.iregs[MD_CONTEXT_ARM_REG_IOS_FP] = frame1_fp.Value();
  raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, MD_CONTEXT_ARM_REG_IOS_FP,
                        &stack_region, &modules, &frame_symbolizer);

  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(2U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ("module2", modules_without_symbols[1]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(3U, frames->size());

  StackFrameARM *frame0 = static_cast<StackFrameARM *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameARM *frame1 = static_cast<StackFrameARM *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_FP, frame1->trust);
  ASSERT_EQ((StackFrameARM::CONTEXT_VALID_PC |
             StackFrameARM::CONTEXT_VALID_LR |
             StackFrameARM::RegisterValidFlag(MD_CONTEXT_ARM_REG_IOS_FP) |
             StackFrameARM::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address1, frame1->context.iregs[MD_CONTEXT_ARM_REG_PC]);
  EXPECT_EQ(return_address2, frame1->context.iregs[MD_CONTEXT_ARM_REG_LR]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM_REG_SP]);
  EXPECT_EQ(frame2_fp.Value(),
            frame1->context.iregs[MD_CONTEXT_ARM_REG_IOS_FP]);

  StackFrameARM *frame2 = static_cast<StackFrameARM *>(frames->at(2));
  EXPECT_EQ(StackFrame::FRAME_TRUST_FP, frame2->trust);
  ASSERT_EQ((StackFrameARM::CONTEXT_VALID_PC |
             StackFrameARM::CONTEXT_VALID_LR |
             StackFrameARM::RegisterValidFlag(MD_CONTEXT_ARM_REG_IOS_FP) |
             StackFrameARM::CONTEXT_VALID_SP),
            frame2->context_validity);
  EXPECT_EQ(return_address2, frame2->context.iregs[MD_CONTEXT_ARM_REG_PC]);
  EXPECT_EQ(0U, frame2->context.iregs[MD_CONTEXT_ARM_REG_LR]);
  EXPECT_EQ(frame2_sp.Value(), frame2->context.iregs[MD_CONTEXT_ARM_REG_SP]);
  EXPECT_EQ(0U, frame2->context.iregs[MD_CONTEXT_ARM_REG_IOS_FP]);
}

TEST_F(GetFramesByFramePointer, FramePointerAndCFI) {

  SetModuleSymbols(&module1,

                     "FUNC 4000 1000 10 enchiridion\n"

                     "STACK CFI INIT 4000 100 .cfa: sp 0 + .ra: lr\n"
                     "STACK CFI 4001 .cfa: sp 8 + .ra: .cfa -4 + ^"
                     " r7: .cfa -8 + ^\n"
                     "STACK CFI 4002 .cfa: r7 8 +\n"
                  );

  stack_section.start() = 0x80000000;
  uint32_t return_address1 = 0x40004010;
  uint32_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  Label frame1_fp, frame2_fp;
  stack_section

    .Append(32, 0)
    .D32(0x0000000D)
    .D32(0xF0000000)

    .Mark(&frame1_fp)
    .D32(frame2_fp)
    .D32(return_address2)
    .Mark(&frame1_sp)

    .Append(32, 0)
    .D32(0x0000000D)
    .D32(0xF0000000)

    .Mark(&frame2_fp)
    .D32(0)
    .D32(0)
    .Mark(&frame2_sp)

    .Append(32, 0)
    .D32(0x0000000D)
    .D32(0xF0000000);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM_REG_PC] = 0x50000400;
  raw_context.iregs[MD_CONTEXT_ARM_REG_LR] = return_address1;
  raw_context.iregs[MD_CONTEXT_ARM_REG_IOS_FP] = frame1_fp.Value();
  raw_context.iregs[MD_CONTEXT_ARM_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM walker(&system_info, &raw_context, MD_CONTEXT_ARM_REG_IOS_FP,
                        &stack_region, &modules, &frame_symbolizer);

  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module2", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(3U, frames->size());

  StackFrameARM *frame0 = static_cast<StackFrameARM *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameARM *frame1 = static_cast<StackFrameARM *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_FP, frame1->trust);
  ASSERT_EQ((StackFrameARM::CONTEXT_VALID_PC |
             StackFrameARM::CONTEXT_VALID_LR |
             StackFrameARM::RegisterValidFlag(MD_CONTEXT_ARM_REG_IOS_FP) |
             StackFrameARM::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address1, frame1->context.iregs[MD_CONTEXT_ARM_REG_PC]);
  EXPECT_EQ(return_address2, frame1->context.iregs[MD_CONTEXT_ARM_REG_LR]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM_REG_SP]);
  EXPECT_EQ(frame2_fp.Value(),
            frame1->context.iregs[MD_CONTEXT_ARM_REG_IOS_FP]);
  EXPECT_EQ("enchiridion", frame1->function_name);
  EXPECT_EQ(0x40004000U, frame1->function_base);

  StackFrameARM *frame2 = static_cast<StackFrameARM *>(frames->at(2));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CFI, frame2->trust);
  ASSERT_EQ((StackFrameARM::CONTEXT_VALID_PC |
             StackFrameARM::CONTEXT_VALID_LR |
             StackFrameARM::RegisterValidFlag(MD_CONTEXT_ARM_REG_IOS_FP) |
             StackFrameARM::CONTEXT_VALID_SP),
            frame2->context_validity);
  EXPECT_EQ(return_address2, frame2->context.iregs[MD_CONTEXT_ARM_REG_PC]);
  EXPECT_EQ(0U, frame2->context.iregs[MD_CONTEXT_ARM_REG_LR]);
  EXPECT_EQ(frame2_sp.Value(), frame2->context.iregs[MD_CONTEXT_ARM_REG_SP]);
  EXPECT_EQ(0U, frame2->context.iregs[MD_CONTEXT_ARM_REG_IOS_FP]);
}
