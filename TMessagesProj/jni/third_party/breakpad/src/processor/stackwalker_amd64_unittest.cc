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

// stackwalker_amd64_unittest.cc: Unit tests for StackwalkerAMD64 class.

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
#include "processor/stackwalker_amd64.h"

using google_breakpad::BasicSourceLineResolver;
using google_breakpad::CallStack;
using google_breakpad::CodeModule;
using google_breakpad::StackFrameSymbolizer;
using google_breakpad::StackFrame;
using google_breakpad::StackFrameAMD64;
using google_breakpad::Stackwalker;
using google_breakpad::StackwalkerAMD64;
using google_breakpad::SystemInfo;
using google_breakpad::test_assembler::kLittleEndian;
using google_breakpad::test_assembler::Label;
using google_breakpad::test_assembler::Section;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::Return;
using testing::SetArgumentPointee;
using testing::Test;

class StackwalkerAMD64Fixture {
 public:
  StackwalkerAMD64Fixture()
    : stack_section(kLittleEndian),

      module1(0x40000000c0000000ULL, 0x10000, "module1", "version1"),
      module2(0x50000000b0000000ULL, 0x10000, "module2", "version2") {

    system_info.os = "Linux";
    system_info.os_short = "linux";
    system_info.os_version = "Horrendous Hippo";
    system_info.cpu = "x86";
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

  void BrandContext(MDRawContextAMD64 *raw_context) {
    uint8_t x = 173;
    for (size_t i = 0; i < sizeof(*raw_context); i++)
      reinterpret_cast<uint8_t *>(raw_context)[i] = (x += 17);
  }

  SystemInfo system_info;
  MDRawContextAMD64 raw_context;
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

class GetContextFrame: public StackwalkerAMD64Fixture, public Test { };

class SanityCheck: public StackwalkerAMD64Fixture, public Test { };

TEST_F(SanityCheck, NoResolver) {

  raw_context.rip = 0x40000000c0000200ULL;
  raw_context.rbp = 0x8000000080000000ULL;

  StackFrameSymbolizer frame_symbolizer(NULL, NULL);
  StackwalkerAMD64 walker(&system_info, &raw_context, &stack_region, &modules,
                          &frame_symbolizer);

  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_GE(1U, frames->size());
  StackFrameAMD64 *frame = static_cast<StackFrameAMD64 *>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

TEST_F(GetContextFrame, Simple) {

  raw_context.rip = 0x40000000c0000200ULL;
  raw_context.rbp = 0x8000000080000000ULL;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerAMD64 walker(&system_info, &raw_context, &stack_region, &modules,
                          &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_GE(1U, frames->size());
  StackFrameAMD64 *frame = static_cast<StackFrameAMD64 *>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

TEST_F(GetContextFrame, NoStackMemory) {
  raw_context.rip = 0x40000000c0000200ULL;
  raw_context.rbp = 0x8000000080000000ULL;

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerAMD64 walker(&system_info, &raw_context, NULL, &modules,
                          &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(1U, modules_without_symbols.size());
  ASSERT_EQ("module1", modules_without_symbols[0]->debug_file());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_GE(1U, frames->size());
  StackFrameAMD64 *frame = static_cast<StackFrameAMD64 *>(frames->at(0));

  EXPECT_EQ(0, memcmp(&raw_context, &frame->context, sizeof(raw_context)));
}

class GetCallerFrame: public StackwalkerAMD64Fixture, public Test { };

TEST_F(GetCallerFrame, ScanWithoutSymbols) {

  stack_section.start() = 0x8000000080000000ULL;
  uint64_t return_address1 = 0x50000000b0000100ULL;
  uint64_t return_address2 = 0x50000000b0000900ULL;
  Label frame1_sp, frame2_sp, frame1_rbp;
  stack_section

    .Append(16, 0)

    .D64(0x40000000b0000000ULL)
    .D64(0x50000000d0000000ULL)

    .D64(return_address1)

    .Mark(&frame1_sp)
    .Append(16, 0)

    .D64(0x40000000b0000000ULL)
    .D64(0x50000000d0000000ULL)

    .Mark(&frame1_rbp)
    .D64(stack_section.start())

    .D64(return_address2)

    .Mark(&frame2_sp)
    .Append(32, 0);

  RegionFromSection();

  raw_context.rip = 0x40000000c0000200ULL;
  raw_context.rbp = frame1_rbp.Value();
  raw_context.rsp = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerAMD64 walker(&system_info, &raw_context, &stack_region, &modules,
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

  StackFrameAMD64 *frame0 = static_cast<StackFrameAMD64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameAMD64::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));

  StackFrameAMD64 *frame1 = static_cast<StackFrameAMD64 *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameAMD64::CONTEXT_VALID_RIP |
             StackFrameAMD64::CONTEXT_VALID_RSP |
             StackFrameAMD64::CONTEXT_VALID_RBP),
            frame1->context_validity);
  EXPECT_EQ(return_address1, frame1->context.rip);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.rsp);
  EXPECT_EQ(frame1_rbp.Value(), frame1->context.rbp);

  StackFrameAMD64 *frame2 = static_cast<StackFrameAMD64 *>(frames->at(2));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame2->trust);
  ASSERT_EQ((StackFrameAMD64::CONTEXT_VALID_RIP |
             StackFrameAMD64::CONTEXT_VALID_RSP),
            frame2->context_validity);
  EXPECT_EQ(return_address2, frame2->context.rip);
  EXPECT_EQ(frame2_sp.Value(), frame2->context.rsp);
}

TEST_F(GetCallerFrame, ScanWithFunctionSymbols) {

  stack_section.start() = 0x8000000080000000ULL;
  uint64_t return_address = 0x50000000b0000110ULL;
  Label frame1_sp, frame1_rbp;

  stack_section

    .Append(16, 0)

    .D64(0x40000000b0000000ULL)
    .D64(0x50000000b0000000ULL)

    .D64(0x40000000c0001000ULL)
    .D64(0x50000000b000aaaaULL)

    .D64(return_address)

    .Mark(&frame1_sp)
    .Append(32, 0)
    .Mark(&frame1_rbp);
  RegionFromSection();

  raw_context.rip = 0x40000000c0000200ULL;
  raw_context.rbp = frame1_rbp.Value();
  raw_context.rsp = stack_section.start().Value();

  SetModuleSymbols(&module1,

                   "FUNC 100 400 10 platypus\n");
  SetModuleSymbols(&module2,

                   "FUNC 100 400 10 echidna\n");

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerAMD64 walker(&system_info, &raw_context, &stack_region, &modules,
                          &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(2U, frames->size());

  StackFrameAMD64 *frame0 = static_cast<StackFrameAMD64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameAMD64::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ("platypus", frame0->function_name);
  EXPECT_EQ(0x40000000c0000100ULL, frame0->function_base);

  StackFrameAMD64 *frame1 = static_cast<StackFrameAMD64 *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_SCAN, frame1->trust);
  ASSERT_EQ((StackFrameAMD64::CONTEXT_VALID_RIP |
             StackFrameAMD64::CONTEXT_VALID_RSP |
             StackFrameAMD64::CONTEXT_VALID_RBP),
            frame1->context_validity);
  EXPECT_EQ(return_address, frame1->context.rip);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.rsp);
  EXPECT_EQ(frame1_rbp.Value(), frame1->context.rbp);
  EXPECT_EQ("echidna", frame1->function_name);
  EXPECT_EQ(0x50000000b0000100ULL, frame1->function_base);
}

TEST_F(GetCallerFrame, ScanningNotAllowed) {

  stack_section.start() = 0x8000000080000000ULL;
  uint64_t return_address1 = 0x50000000b0000100ULL;
  uint64_t return_address2 = 0x50000000b0000900ULL;
  Label frame1_sp, frame2_sp, frame1_rbp;
  stack_section

    .Append(16, 0)

    .D64(0x40000000b0000000ULL)
    .D64(0x50000000d0000000ULL)

    .D64(return_address1)

    .Mark(&frame1_sp)
    .Append(16, 0)

    .D64(0x40000000b0000000ULL)
    .D64(0x50000000d0000000ULL)

    .Mark(&frame1_rbp)
    .D64(stack_section.start())

    .D64(return_address2)

    .Mark(&frame2_sp)
    .Append(32, 0);

  RegionFromSection();

  raw_context.rip = 0x40000000c0000200ULL;
  raw_context.rbp = frame1_rbp.Value();
  raw_context.rsp = stack_section.start().Value();

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerAMD64 walker(&system_info, &raw_context, &stack_region, &modules,
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

  StackFrameAMD64 *frame0 = static_cast<StackFrameAMD64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameAMD64::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(0, memcmp(&raw_context, &frame0->context, sizeof(raw_context)));
}

TEST_F(GetCallerFrame, CallerPushedRBP) {

  stack_section.start() = 0x8000000080000000ULL;
  uint64_t return_address = 0x50000000b0000110ULL;
  Label frame0_rbp, frame1_sp, frame1_rbp;

  stack_section

    .Append(16, 0)

    .D64(0x40000000b0000000ULL)
    .D64(0x50000000b0000000ULL)

    .D64(0x40000000c0001000ULL)
    .D64(0x50000000b000aaaaULL)

    .Mark(&frame0_rbp)
    .D64(frame1_rbp)
    .D64(return_address)

    .Mark(&frame1_sp)
    .Append(32, 0)
    .Mark(&frame1_rbp);
  RegionFromSection();

  raw_context.rip = 0x40000000c0000200ULL;
  raw_context.rbp = frame0_rbp.Value();
  raw_context.rsp = stack_section.start().Value();

  SetModuleSymbols(&module1,

                   "FUNC 100 400 10 sasquatch\n");
  SetModuleSymbols(&module2,

                   "FUNC 100 400 10 yeti\n");

  StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
  StackwalkerAMD64 walker(&system_info, &raw_context, &stack_region, &modules,
                          &frame_symbolizer);
  vector<const CodeModule*> modules_without_symbols;
  vector<const CodeModule*> modules_with_corrupt_symbols;
  ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                          &modules_with_corrupt_symbols));
  ASSERT_EQ(0U, modules_without_symbols.size());
  ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
  frames = call_stack.frames();
  ASSERT_EQ(2U, frames->size());

  StackFrameAMD64 *frame0 = static_cast<StackFrameAMD64 *>(frames->at(0));
  EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
  ASSERT_EQ(StackFrameAMD64::CONTEXT_VALID_ALL, frame0->context_validity);
  EXPECT_EQ(frame0_rbp.Value(), frame0->context.rbp);
  EXPECT_EQ("sasquatch", frame0->function_name);
  EXPECT_EQ(0x40000000c0000100ULL, frame0->function_base);

  StackFrameAMD64 *frame1 = static_cast<StackFrameAMD64 *>(frames->at(1));
  EXPECT_EQ(StackFrame::FRAME_TRUST_FP, frame1->trust);
  ASSERT_EQ((StackFrameAMD64::CONTEXT_VALID_RIP |
             StackFrameAMD64::CONTEXT_VALID_RSP |
             StackFrameAMD64::CONTEXT_VALID_RBP),
            frame1->context_validity);
  EXPECT_EQ(return_address, frame1->context.rip);
  EXPECT_EQ(frame1_sp.Value(), frame1->context.rsp);
  EXPECT_EQ(frame1_rbp.Value(), frame1->context.rbp);
  EXPECT_EQ("yeti", frame1->function_name);
  EXPECT_EQ(0x50000000b0000100ULL, frame1->function_base);
}

struct CFIFixture: public StackwalkerAMD64Fixture {
  CFIFixture() {

    SetModuleSymbols(&module1,

                     "FUNC 4000 1000 10 enchiridion\n"

                     "STACK CFI INIT 4000 100 .cfa: $rsp 8 + .ra: .cfa 8 - ^\n"

                     "STACK CFI 4001 .cfa: $rsp 16 + $rbx: .cfa 16 - ^\n"

                     "STACK CFI 4002 $r12: $rbx\n"

                     "STACK CFI 4003 .cfa: $rsp 40 + $r13: .cfa 32 - ^\n"

                     "STACK CFI 4005 .ra: $r13\n"

                     "STACK CFI 4006 .cfa: $rbp 16 + $rbp: .cfa 24 - ^\n"

                     "FUNC 5000 1000 10 epictetus\n"

                     "STACK CFI INIT 5000 1000 .cfa: $rsp .ra 0\n");

    expected.rsp = 0x8000000080000000ULL;
    expected.rip = 0x40000000c0005510ULL;
    expected.rbp = 0x68995b1de4700266ULL;
    expected.rbx = 0x5a5beeb38de23be8ULL;
    expected.r12 = 0xed1b02e8cc0fc79cULL;
    expected.r13 = 0x1d20ad8acacbe930ULL;
    expected.r14 = 0xe94cffc2f7adaa28ULL;
    expected.r15 = 0xb638d17d8da413b5ULL;

    raw_context = expected;
  }

  void CheckWalk() {
    RegionFromSection();
    raw_context.rsp = stack_section.start().Value();

    StackFrameSymbolizer frame_symbolizer(&supplier, &resolver);
    StackwalkerAMD64 walker(&system_info, &raw_context, &stack_region, &modules,
                            &frame_symbolizer);
    vector<const CodeModule*> modules_without_symbols;
    vector<const CodeModule*> modules_with_corrupt_symbols;
    ASSERT_TRUE(walker.Walk(&call_stack, &modules_without_symbols,
                            &modules_with_corrupt_symbols));
    ASSERT_EQ(0U, modules_without_symbols.size());
    ASSERT_EQ(0U, modules_with_corrupt_symbols.size());
    frames = call_stack.frames();
    ASSERT_EQ(2U, frames->size());

    StackFrameAMD64 *frame0 = static_cast<StackFrameAMD64 *>(frames->at(0));
    EXPECT_EQ(StackFrame::FRAME_TRUST_CONTEXT, frame0->trust);
    ASSERT_EQ(StackFrameAMD64::CONTEXT_VALID_ALL, frame0->context_validity);
    EXPECT_EQ("enchiridion", frame0->function_name);
    EXPECT_EQ(0x40000000c0004000ULL, frame0->function_base);

    StackFrameAMD64 *frame1 = static_cast<StackFrameAMD64 *>(frames->at(1));
    EXPECT_EQ(StackFrame::FRAME_TRUST_CFI, frame1->trust);
    ASSERT_EQ((StackFrameAMD64::CONTEXT_VALID_RIP |
               StackFrameAMD64::CONTEXT_VALID_RSP |
               StackFrameAMD64::CONTEXT_VALID_RBP |
               StackFrameAMD64::CONTEXT_VALID_RBX |
               StackFrameAMD64::CONTEXT_VALID_R12 |
               StackFrameAMD64::CONTEXT_VALID_R13 |
               StackFrameAMD64::CONTEXT_VALID_R14 |
               StackFrameAMD64::CONTEXT_VALID_R15),
              frame1->context_validity);
    EXPECT_EQ(expected.rip, frame1->context.rip);
    EXPECT_EQ(expected.rsp, frame1->context.rsp);
    EXPECT_EQ(expected.rbp, frame1->context.rbp);
    EXPECT_EQ(expected.rbx, frame1->context.rbx);
    EXPECT_EQ(expected.r12, frame1->context.r12);
    EXPECT_EQ(expected.r13, frame1->context.r13);
    EXPECT_EQ(expected.r14, frame1->context.r14);
    EXPECT_EQ(expected.r15, frame1->context.r15);
    EXPECT_EQ("epictetus", frame1->function_name);
  }

  MDRawContextAMD64 expected;
};

class CFI: public CFIFixture, public Test { };

TEST_F(CFI, At4000) {
  Label frame1_rsp = expected.rsp;
  stack_section
    .D64(0x40000000c0005510ULL)
    .Mark(&frame1_rsp);
  raw_context.rip = 0x40000000c0004000ULL;
  CheckWalk();
}

TEST_F(CFI, At4001) {
  Label frame1_rsp = expected.rsp;
  stack_section
    .D64(0x5a5beeb38de23be8ULL)
    .D64(0x40000000c0005510ULL)
    .Mark(&frame1_rsp);
  raw_context.rip = 0x40000000c0004001ULL;
  raw_context.rbx = 0xbe0487d2f9eafe29ULL;
  CheckWalk();
}

TEST_F(CFI, At4002) {
  Label frame1_rsp = expected.rsp;
  stack_section
    .D64(0x5a5beeb38de23be8ULL)
    .D64(0x40000000c0005510ULL)
    .Mark(&frame1_rsp);
  raw_context.rip = 0x40000000c0004002ULL;
  raw_context.rbx = 0xed1b02e8cc0fc79cULL;
  raw_context.r12 = 0xb0118de918a4bceaULL;
  CheckWalk();
}

TEST_F(CFI, At4003) {
  Label frame1_rsp = expected.rsp;
  stack_section
    .D64(0x0e023828dffd4d81ULL)
    .D64(0x1d20ad8acacbe930ULL)
    .D64(0x319e68b49e3ace0fULL)
    .D64(0x5a5beeb38de23be8ULL)
    .D64(0x40000000c0005510ULL)
    .Mark(&frame1_rsp);
  raw_context.rip = 0x40000000c0004003ULL;
  raw_context.rbx = 0xed1b02e8cc0fc79cULL;
  raw_context.r12 = 0x89d04fa804c87a43ULL;
  raw_context.r13 = 0x5118e02cbdb24b03ULL;
  CheckWalk();
}

TEST_F(CFI, At4004) {
  Label frame1_rsp = expected.rsp;
  stack_section
    .D64(0x0e023828dffd4d81ULL)
    .D64(0x1d20ad8acacbe930ULL)
    .D64(0x319e68b49e3ace0fULL)
    .D64(0x5a5beeb38de23be8ULL)
    .D64(0x40000000c0005510ULL)
    .Mark(&frame1_rsp);
  raw_context.rip = 0x40000000c0004004ULL;
  raw_context.rbx = 0xed1b02e8cc0fc79cULL;
  raw_context.r12 = 0x89d04fa804c87a43ULL;
  raw_context.r13 = 0x5118e02cbdb24b03ULL;
  CheckWalk();
}

TEST_F(CFI, At4005) {
  Label frame1_rsp = expected.rsp;
  stack_section
    .D64(0x4b516dd035745953ULL)
    .D64(0x1d20ad8acacbe930ULL)
    .D64(0xa6d445e16ae3d872ULL)
    .D64(0x5a5beeb38de23be8ULL)
    .D64(0xaa95fa054aedfbaeULL)
    .Mark(&frame1_rsp);
  raw_context.rip = 0x40000000c0004005ULL;
  raw_context.rbx = 0xed1b02e8cc0fc79cULL;
  raw_context.r12 = 0x46b1b8868891b34aULL;
  raw_context.r13 = 0x40000000c0005510ULL;
  CheckWalk();
}

TEST_F(CFI, At4006) {
  Label frame0_rbp;
  Label frame1_rsp = expected.rsp;
  stack_section
    .D64(0x043c6dfceb91aa34ULL)
    .D64(0x1d20ad8acacbe930ULL)
    .D64(0x68995b1de4700266ULL)
    .Mark(&frame0_rbp)
    .D64(0x5a5beeb38de23be8ULL)
    .D64(0xf015ee516ad89eabULL)
    .Mark(&frame1_rsp);
  raw_context.rip = 0x40000000c0004006ULL;
  raw_context.rbp = frame0_rbp.Value();
  raw_context.rbx = 0xed1b02e8cc0fc79cULL;
  raw_context.r12 = 0x26e007b341acfebdULL;
  raw_context.r13 = 0x40000000c0005510ULL;
  CheckWalk();
}
