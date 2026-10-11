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

// stackwalker_arm64_unittest.cc: Unit tests for StackwalkerARM64 class.

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
#include "processor/stackwalker_arm64.h"
#include "processor/windows_frame_info.h"

using google_breakpad::BasicSourceLineResolver;
using google_breakpad::CallStack;
using google_breakpad::CodeModule;
using google_breakpad::StackFrameSymbolizer;
using google_breakpad::StackFrame;
using google_breakpad::StackFrameARM64;
using google_breakpad::Stackwalker;
using google_breakpad::StackwalkerARM64;
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

class StackwalkerARM64Fixture {
 public:
  StackwalkerARM64Fixture()
    : stack_section(kLittleEndian),

      module1(0x40000000, 0x10000, "module1", "version1"),
      module2(0x50000000, 0x10000, "module2", "version2") {

    system_info.os = "iOS";
    system_info.os_short = "ios";
    system_info.cpu = "arm64";
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

  void BrandContext(MDRawContextARM64 *raw_context) {
    uint8_t x = 173;
    for (size_t i = 0; i < sizeof(*raw_context); i++)
      reinterpret_cast<uint8_t *>(raw_context)[i] = (x += 17);
  }

  SystemInfo system_info;
  MDRawContextARM64 raw_context;
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

class SanityCheck: public StackwalkerARM64Fixture, public Test { };

TEST_F(SanityCheck, NoResolver) {

  StackFrameSymbolizer frame_symbolizer(NULL, NULL);
  StackwalkerARM64 walker(&system_info, &raw_context, &stack_region, &modules,
                          &frame_symbolizer);

  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());
  StackFrameARM64 *frame = static_cast<StackFrameARM64 *>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

class GetContextFrame: public StackwalkerARM64Fixture, public Test { };

TEST_F(GetContextFrame, NoStackMemory) {
  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM64 walker(&system_info, &raw_context, NULL, &modules,
                          &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(1U, frames->size());
  StackFrameARM64 *frame = static_cast<StackFrameARM64 *>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

class GetCallerFrame: public StackwalkerARM64Fixture, public Test { };

TEST_F(GetCallerFrame, ScanWithoutSymbols) {

  stack_section.start() = 0x80000000;
  uint64_t return_address1 = 0x50000100;
  uint64_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(16, 0)

    .D64(0x40090000)
    .D64(0x60000000)

    .D64(return_address1)

    .Mark(&frame1_sp)
    .Append(16, 0)

    .D64(0xF0000000)
    .D64(0x0000000D)

    .D64(return_address2)

    .Mark(&frame2_sp)
    .Append(64, 0);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x40005510;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM64 walker(&system_info, &raw_context, &stack_region, &modules,
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

  StackFrameARM64 *frame0 = static_cast<StackFrameARM64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM64::CONTEXT_VALID_ALL,
            frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameARM64 *frame1 = static_cast<StackFrameARM64 *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameARM64::CONTEXT_VALID_PC |
             StackFrameARM64::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address1, frame1->context.iregs[MD_CONTEXT_ARM64_REG_PC]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM64_REG_SP]);

  StackFrameARM64 *frame2 = static_cast<StackFrameARM64 *>(frames->at(2));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame2->trust);
  ASSERT_EQ((StackFrameARM64::CONTEXT_VALID_PC |
             StackFrameARM64::CONTEXT_VALID_SP),
            frame2->context_validity);
  EXPECT_EQ(return_address2, frame2->context.iregs[MD_CONTEXT_ARM64_REG_PC]);
  EXPECT_EQ(frame2_sp.Value(), frame2->context.iregs[MD_CONTEXT_ARM64_REG_SP]);
}

TEST_F(GetCallerFrame, ScanWithFunctionSymbols) {

  stack_section.start() = 0x80000000;
  uint64_t return_address = 0x50000200;
  Label frame1_sp;

  stack_section

    .Append(16, 0)

    .D64(0x40090000)
    .D64(0x60000000)

    .D64(0x40001000)
    .D64(0x5000F000)

    .D64(return_address)

    .Mark(&frame1_sp)
    .Append(64, 0);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x40000200;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_SP] = stack_section.start().Value();

  SetModuleSymbols(&module1,

                   "FUNC 100 400 10 monotreme\n");
  SetModuleSymbols(&module2,

                   "FUNC 100 400 10 marsupial\n");

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM64 walker(&system_info, &raw_context, &stack_region, &modules,
                          &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(2U, frames->size());

  StackFrameARM64 *frame0 = static_cast<StackFrameARM64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM64::CONTEXT_VALID_ALL,
            frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));
  EXPECT_EQ("monotreme", frame0->function_name);
  EXPECT_EQ(0x40000100ULL, frame0->function_base);

  StackFrameARM64 *frame1 = static_cast<StackFrameARM64 *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameARM64::CONTEXT_VALID_PC |
             StackFrameARM64::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address, frame1->context.iregs[MD_CONTEXT_ARM64_REG_PC]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM64_REG_SP]);
  EXPECT_EQ("marsupial", frame1->function_name);
  EXPECT_EQ(0x50000100ULL, frame1->function_base);
}

TEST_F(GetCallerFrame, ScanFirstFrame) {

  stack_section.start() = 0x80000000;
  uint64_t return_address1 = 0x50000100;
  uint64_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(32, 0)

    .D64(0x40090000)
    .D64(0x60000000)

    .Append(96, 0)

    .D64(return_address1)

    .Mark(&frame1_sp)
    .Append(32, 0)

    .D64(0xF0000000)
    .D64(0x0000000D)

    .Append(256, 0)

    .D64(return_address2)

    .Mark(&frame2_sp)
    .Append(64, 0);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x40005510;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM64 walker(&system_info, &raw_context, &stack_region, &modules,
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

  StackFrameARM64 *frame0 = static_cast<StackFrameARM64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM64::CONTEXT_VALID_ALL,
            frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameARM64 *frame1 = static_cast<StackFrameARM64 *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameARM64::CONTEXT_VALID_PC |
             StackFrameARM64::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address1, frame1->context.iregs[MD_CONTEXT_ARM64_REG_PC]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM64_REG_SP]);
}

TEST_F(GetCallerFrame, ScanningNotAllowed) {

  stack_section.start() = 0x80000000;
  uint64_t return_address1 = 0x50000100;
  uint64_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  stack_section

    .Append(16, 0)

    .D64(0x40090000)
    .D64(0x60000000)

    .D64(return_address1)

    .Mark(&frame1_sp)
    .Append(16, 0)

    .D64(0xF0000000)
    .D64(0x0000000D)

    .D64(return_address2)

    .Mark(&frame2_sp)
    .Append(64, 0);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x40005510;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM64 walker(&system_info, &raw_context, &stack_region, &modules,
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

  StackFrameARM64 *frame0 = static_cast<StackFrameARM64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM64::CONTEXT_VALID_ALL,
            frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));
}

class GetFramesByFramePointer: public StackwalkerARM64Fixture, public Test { };

TEST_F(GetFramesByFramePointer, OnlyFramePointer) {
  stack_section.start() = 0x80000000;
  uint64_t return_address1 = 0x50000100;
  uint64_t return_address2 = 0x50000900;
  Label frame1_sp, frame2_sp;
  Label frame1_fp, frame2_fp;
  stack_section

    .Append(64, 0)
    .D64(0x0000000D)
    .D64(0xF0000000)

    .Mark(&frame1_fp)
    .D64(frame2_fp)
    .D64(return_address2)
    .Mark(&frame1_sp)

    .Append(64, 0)
    .D64(0x0000000D)
    .D64(0xF0000000)

    .Mark(&frame2_fp)
    .D64(0)
    .D64(0)
    .Mark(&frame2_sp)

    .Append(64, 0)
    .D64(0x0000000D)
    .D64(0xF0000000);
  RegionFromSection();

  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x40005510;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_LR] = return_address1;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_FP] = frame1_fp.Value();
  raw_context.iregs[MD_CONTEXT_ARM64_REG_SP] = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM64 walker(&system_info, &raw_context,
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

  StackFrameARM64 *frame0 = static_cast<StackFrameARM64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameARM64::CONTEXT_VALID_ALL,
            frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameARM64 *frame1 = static_cast<StackFrameARM64 *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_FP, frame1->trust);
  ASSERT_EQ((StackFrameARM64::CONTEXT_VALID_PC |
             StackFrameARM64::CONTEXT_VALID_LR |
             StackFrameARM64::CONTEXT_VALID_FP |
             StackFrameARM64::CONTEXT_VALID_SP),
            frame1->context_validity);
  EXPECT_EQ(return_address1, frame1->context.iregs[MD_CONTEXT_ARM64_REG_PC]);
  EXPECT_EQ(return_address2, frame1->context.iregs[MD_CONTEXT_ARM64_REG_LR]);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.iregs[MD_CONTEXT_ARM64_REG_SP]);
  EXPECT_EQ(frame2_fp.Value(),
            frame1->context.iregs[MD_CONTEXT_ARM64_REG_FP]);

  StackFrameARM64 *frame2 = static_cast<StackFrameARM64 *>(frames->at(2));
  EXPECT_EQ(StackFrame::FRAME_TRUST_FP, frame2->trust);
  ASSERT_EQ((StackFrameARM64::CONTEXT_VALID_PC |
             StackFrameARM64::CONTEXT_VALID_LR |
             StackFrameARM64::CONTEXT_VALID_FP |
             StackFrameARM64::CONTEXT_VALID_SP),
            frame2->context_validity);
  EXPECT_EQ(return_address2, frame2->context.iregs[MD_CONTEXT_ARM64_REG_PC]);
  EXPECT_EQ(0U, frame2->context.iregs[MD_CONTEXT_ARM64_REG_LR]);
  EXPECT_EQ(frame2_sp.Value(), frame2->context.iregs[MD_CONTEXT_ARM64_REG_SP]);
  EXPECT_EQ(0U, frame2->context.iregs[MD_CONTEXT_ARM64_REG_FP]);
}

struct CFIFixture: public StackwalkerARM64Fixture {
  CFIFixture() {

    SetModuleSymbols(&module1,

                     "FUNC 4000 1000 10 enchiridion\n"

                     "STACK CFI INIT 4000 100 .cfa: sp 0 + .ra: x30\n"

                     "STACK CFI 4001 .cfa: sp 32 + .ra: .cfa -8 + ^"
                     " x19: .cfa -32 + ^ x20: .cfa -24 + ^ "
                     " x29: .cfa -16 + ^\n"

                     "STACK CFI 4002 x19: x0 x20: x1 x21: x2 x22: x3\n"

                     "STACK CFI 4003 .cfa: sp 40 + x1: .cfa 40 - ^"
                     " x19: x19 x20: x20 x21: x21 x22: x22\n"

                     "STACK CFI 4005 .cfa: sp 32 + x1: .cfa 32 - ^"
                     " x29: .cfa 8 - ^ .ra: .cfa ^ sp: .cfa 8 +\n"

                     "STACK CFI 4006 .cfa: sp 40 + pc: .cfa 40 - ^\n"

                     "FUNC 5000 1000 10 epictetus\n"

                     "STACK CFI INIT 5000 1000 .cfa: 0 .ra: 0\n"

                     "FUNC 6000 1000 20 palinal\n"
                     "STACK CFI INIT 6000 1000 .cfa: sp 8 - .ra: x30\n"

                     "FUNC 7000 1000 20 rhetorical\n"
                     "STACK CFI INIT 7000 1000 .cfa: moot .ra: ambiguous\n");

    expected.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040005510L;
    expected.iregs[MD_CONTEXT_ARM64_REG_SP] = 0x0000000080000000L;
    expected.iregs[19] = 0x5e68b5d5b5d55e68L;
    expected.iregs[20] = 0x34f3ebd1ebd134f3L;
    expected.iregs[21] = 0x74bca31ea31e74bcL;
    expected.iregs[22] = 0x16b32dcb2dcb16b3L;
    expected.iregs[23] = 0x21372ada2ada2137L;
    expected.iregs[24] = 0x557dbbbbbbbb557dL;
    expected.iregs[25] = 0x8ca748bf48bf8ca7L;
    expected.iregs[26] = 0x21f0ab46ab4621f0L;
    expected.iregs[27] = 0x146732b732b71467L;
    expected.iregs[28] = 0xa673645fa673645fL;
    expected.iregs[MD_CONTEXT_ARM64_REG_FP] = 0xe11081128112e110L;

    expected_validity = (StackFrameARM64::CONTEXT_VALID_PC  |
                         StackFrameARM64::CONTEXT_VALID_SP  |
                         StackFrameARM64::CONTEXT_VALID_X19 |
                         StackFrameARM64::CONTEXT_VALID_X20 |
                         StackFrameARM64::CONTEXT_VALID_X21 |
                         StackFrameARM64::CONTEXT_VALID_X22 |
                         StackFrameARM64::CONTEXT_VALID_X23 |
                         StackFrameARM64::CONTEXT_VALID_X24 |
                         StackFrameARM64::CONTEXT_VALID_X25 |
                         StackFrameARM64::CONTEXT_VALID_X26 |
                         StackFrameARM64::CONTEXT_VALID_X27 |
                         StackFrameARM64::CONTEXT_VALID_X28 |
                         StackFrameARM64::CONTEXT_VALID_FP);

    context_frame_validity = StackFrameARM64::CONTEXT_VALID_ALL;

    raw_context = expected;
  }

  void CheckWalk() {
    RegionFromSection();
    raw_context.iregs[MD_CONTEXT_ARM64_REG_SP] = stack_section.start().Value();

    StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
    StackwalkerARM64 walker(&system_info, &raw_context, &stack_region,
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

    StackFrameARM64 *frame0 = static_cast<StackFrameARM64 *>(frames->at(0));
    EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
    ASSERT_EQ(context_frame_validity, frame0->context_validity);
    EXPECT_EQ("enchiridion", frame0->function_name);
    EXPECT_EQ(0x0000000040004000UL, frame0->function_base);

    StackFrameARM64 *frame1 = static_cast<StackFrameARM64 *>(frames->at(1));
    EXPECT_EQ(StackFrame::FRAME_TRUST_CFI, frame1->trust);
    ASSERT_EQ(expected_validity, frame1->context_validity);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X1)
      EXPECT_EQ(expected.iregs[1], frame1->context.iregs[1]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X19)
      EXPECT_EQ(expected.iregs[19], frame1->context.iregs[19]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X20)
      EXPECT_EQ(expected.iregs[20], frame1->context.iregs[20]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X21)
      EXPECT_EQ(expected.iregs[21], frame1->context.iregs[21]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X22)
      EXPECT_EQ(expected.iregs[22], frame1->context.iregs[22]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X23)
      EXPECT_EQ(expected.iregs[23], frame1->context.iregs[23]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X24)
      EXPECT_EQ(expected.iregs[24], frame1->context.iregs[24]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X25)
      EXPECT_EQ(expected.iregs[25], frame1->context.iregs[25]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X26)
      EXPECT_EQ(expected.iregs[26], frame1->context.iregs[26]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X27)
      EXPECT_EQ(expected.iregs[27], frame1->context.iregs[27]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_X28)
      EXPECT_EQ(expected.iregs[28], frame1->context.iregs[28]);
    if (expected_validity & StackFrameARM64::CONTEXT_VALID_FP)
      EXPECT_EQ(expected.iregs[MD_CONTEXT_ARM64_REG_FP],
                frame1->context.iregs[MD_CONTEXT_ARM64_REG_FP]);

    EXPECT_EQ(expected.iregs[MD_CONTEXT_ARM64_REG_SP],
              frame1->context.iregs[MD_CONTEXT_ARM64_REG_SP]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_ARM64_REG_PC],
              frame1->context.iregs[MD_CONTEXT_ARM64_REG_PC]);
    EXPECT_EQ(expected.iregs[MD_CONTEXT_ARM64_REG_PC],
              frame1->instruction + 4);
    EXPECT_EQ("epictetus", frame1->function_name);
  }

  MDRawContextARM64 expected;

  uint64_t expected_validity;

  uint64_t context_frame_validity;
};

class CFI: public CFIFixture, public Test { };

TEST_F(CFI, At4000) {
  stack_section.start() = expected.iregs[MD_CONTEXT_ARM64_REG_SP];
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040004000L;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_LR] = 0x0000000040005510L;
  CheckWalk();
}

TEST_F(CFI, At4001) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM64_REG_SP];
  stack_section
    .D64(0x5e68b5d5b5d55e68L)
    .D64(0x34f3ebd1ebd134f3L)
    .D64(0xe11081128112e110L)
    .D64(0x0000000040005510L)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040004001L;

  raw_context.iregs[19] = 0xadc9f635a635adc9L;
  raw_context.iregs[20] = 0x623135ac35ac6231L;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_FP] = 0x5fc4be14be145fc4L;
  CheckWalk();
}

TEST_F(CFI, At4001LimitedValidity) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM64_REG_SP];
  stack_section
    .D64(0x5e68b5d5b5d55e68L)
    .D64(0x34f3ebd1ebd134f3L)
    .D64(0xe11081128112e110L)
    .D64(0x0000000040005510L)
    .Mark(&frame1_sp);
  context_frame_validity =
    StackFrameARM64::CONTEXT_VALID_PC | StackFrameARM64::CONTEXT_VALID_SP;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040004001L;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_FP] = 0x5fc4be14be145fc4L;

  expected_validity = (StackFrameARM64::CONTEXT_VALID_PC
                       | StackFrameARM64::CONTEXT_VALID_SP
                       | StackFrameARM64::CONTEXT_VALID_FP
                       | StackFrameARM64::CONTEXT_VALID_X19
                       | StackFrameARM64::CONTEXT_VALID_X20);
  CheckWalk();
}

TEST_F(CFI, At4002) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM64_REG_SP];
  stack_section
    .D64(0xff3dfb81fb81ff3dL)
    .D64(0x34f3ebd1ebd134f3L)
    .D64(0xe11081128112e110L)
    .D64(0x0000000040005510L)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040004002L;
  raw_context.iregs[0]  = 0x5e68b5d5b5d55e68L;
  raw_context.iregs[1]  = 0x34f3ebd1ebd134f3L;
  raw_context.iregs[2]  = 0x74bca31ea31e74bcL;
  raw_context.iregs[3]  = 0x16b32dcb2dcb16b3L;
  raw_context.iregs[19] = 0xadc9f635a635adc9L;
  raw_context.iregs[20] = 0x623135ac35ac6231L;
  raw_context.iregs[21] = 0xac4543564356ac45L;
  raw_context.iregs[22] = 0x2561562f562f2561L;

  raw_context.iregs[MD_CONTEXT_ARM64_REG_FP] = 0x5fc4be14be145fc4L;
  CheckWalk();
}

TEST_F(CFI, At4003) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM64_REG_SP];
  stack_section
    .D64(0xdd5a48c848c8dd5aL)
    .D64(0xff3dfb81fb81ff3dL)
    .D64(0x34f3ebd1ebd134f3L)
    .D64(0xe11081128112e110L)
    .D64(0x0000000040005510L)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040004003L;

  raw_context.iregs[1] = 0xfb756319fb756319L;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_FP] = 0x5fc4be14be145fc4L;

  expected.iregs[1] = 0xdd5a48c848c8dd5aL;
  expected_validity |= StackFrameARM64::CONTEXT_VALID_X1;
  CheckWalk();
}

TEST_F(CFI, At4004) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM64_REG_SP];
  stack_section
    .D64(0xdd5a48c848c8dd5aL)
    .D64(0xff3dfb81fb81ff3dL)
    .D64(0x34f3ebd1ebd134f3L)
    .D64(0xe11081128112e110L)
    .D64(0x0000000040005510L)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040004004L;

  raw_context.iregs[1] = 0xfb756319fb756319L;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_FP] = 0x5fc4be14be145fc4L;

  expected.iregs[1] = 0xdd5a48c848c8dd5aL;
  expected_validity |= StackFrameARM64::CONTEXT_VALID_X1;
  CheckWalk();
}

TEST_F(CFI, At4005) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM64_REG_SP];
  stack_section
    .D64(0xdd5a48c848c8dd5aL)
    .D64(0xff3dfb81fb81ff3dL)
    .D64(0x34f3ebd1ebd134f3L)
    .D64(0xe11081128112e110L)
    .D64(0x0000000040005510L)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040004005L;
  raw_context.iregs[1] = 0xfb756319fb756319L;
  expected.iregs[1] = 0xdd5a48c848c8dd5aL;
  expected_validity |= StackFrameARM64::CONTEXT_VALID_X1;
  CheckWalk();
}

TEST_F(CFI, At4006) {
  Label frame1_sp = expected.iregs[MD_CONTEXT_ARM64_REG_SP];
  stack_section
    .D64(0x0000000040005510L)
    .D64(0xdd5a48c848c8dd5aL)
    .D64(0xff3dfb81fb81ff3dL)
    .D64(0x34f3ebd1ebd134f3L)
    .D64(0xe11081128112e110L)
    .D64(0xf8d157835783f8d1L)
    .Mark(&frame1_sp);
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040004006L;
  raw_context.iregs[1] = 0xfb756319fb756319L;
  expected.iregs[1] = 0xdd5a48c848c8dd5aL;
  expected_validity |= StackFrameARM64::CONTEXT_VALID_X1;
  CheckWalk();
}

TEST_F(CFI, RejectBackwards) {
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040006000L;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_SP] = 0x0000000080000000L;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_LR] = 0x0000000040005510L;
  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM64 walker(&system_info, &raw_context, &stack_region, &modules,
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
  raw_context.iregs[MD_CONTEXT_ARM64_REG_PC] = 0x0000000040007000L;
  raw_context.iregs[MD_CONTEXT_ARM64_REG_SP] = 0x0000000080000000L;
  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerARM64 walker(&system_info, &raw_context, &stack_region, &modules,
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
