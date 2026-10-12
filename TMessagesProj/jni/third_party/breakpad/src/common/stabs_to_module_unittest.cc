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

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// dump_stabs_unittest.cc: Unit tests for StabsToModule.

#include <vector>

#include "breakpad_googletest_includes.h"
#include "common/stabs_to_module.h"

using google_breakpad::Module;
using google_breakpad::StabsToModule;
using std::vector;

TEST(StabsToModule, SimpleCU) {
  Module m("name", "os", "arch", "id");
  StabsToModule h(&m);

  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit", 0x9f4d1271e50db93bLL,
                                     "build-directory"));
  EXPECT_TRUE(h.StartFunction("function", 0xfde4abbed390c394LL));
  EXPECT_TRUE(h.Line(0xfde4abbed390c394LL, "source-file-name", 174823314));
  EXPECT_TRUE(h.EndFunction(0xfde4abbed390c3a4LL));
  EXPECT_TRUE(h.EndCompilationUnit(0xfee4abbed390c3a4LL));
  h.Finalize();

  Module::File *file = m.FindExistingFile("source-file-name");
  ASSERT_TRUE(file != NULL);

  vector<Module::Function *> functions;
  m.GetFunctions(&functions, functions.end());
  ASSERT_EQ((size_t) 1, functions.size());
  Module::Function *function = functions[0];
  EXPECT_STREQ("function", function->name.c_str());
  EXPECT_EQ(0xfde4abbed390c394LL, function->address);
  EXPECT_EQ(0x10U, function->size);
  EXPECT_EQ(0U, function->parameter_size);
  ASSERT_EQ((size_t) 1, function->lines.size());
  Module::Line *line = &function->lines[0];
  EXPECT_EQ(0xfde4abbed390c394LL, line->address);
  EXPECT_EQ(0x10U, line->size);
  EXPECT_TRUE(line->file == file);
  EXPECT_EQ(174823314, line->number);
}

#ifdef __GNUC__

TEST(StabsToModule, Externs) {
  Module m("name", "os", "arch", "id");
  StabsToModule h(&m);

  EXPECT_TRUE(h.Extern("_foo", 0xffff));
  EXPECT_TRUE(h.Extern("__Z21dyldGlobalLockAcquirev", 0xaaaa));
  EXPECT_TRUE(h.Extern("_MorphTableGetNextMorphChain", 0x1111));
  h.Finalize();

  vector<Module::Extern *> externs;
  m.GetExterns(&externs, externs.end());
  ASSERT_EQ((size_t) 3, externs.size());
  Module::Extern *extern1 = externs[0];
  EXPECT_STREQ("MorphTableGetNextMorphChain", extern1->name.c_str());
  EXPECT_EQ((Module::Address)0x1111, extern1->address);
  Module::Extern *extern2 = externs[1];
  EXPECT_STREQ("dyldGlobalLockAcquire()", extern2->name.c_str());
  EXPECT_EQ((Module::Address)0xaaaa, extern2->address);
  Module::Extern *extern3 = externs[2];
  EXPECT_STREQ("foo", extern3->name.c_str());
  EXPECT_EQ((Module::Address)0xffff, extern3->address);
}
#endif

TEST(StabsToModule, DuplicateFunctionNames) {
  Module m("name", "os", "arch", "id");
  StabsToModule h(&m);

  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit", 0xf2cfda36ecf7f46cLL,
                                     "build-directory"));
  EXPECT_TRUE(h.StartFunction("funcfoo",
                              0xf2cfda36ecf7f46dLL));
  EXPECT_TRUE(h.EndFunction(0));
  EXPECT_TRUE(h.StartFunction("funcfoo",
                              0xf2cfda36ecf7f46dLL));
  EXPECT_TRUE(h.EndFunction(0));
  EXPECT_TRUE(h.EndCompilationUnit(0));

  h.Finalize();

  Module::File *file = m.FindExistingFile("compilation-unit");
  ASSERT_TRUE(file != NULL);

  vector<Module::Function *> functions;
  m.GetFunctions(&functions, functions.end());
  ASSERT_EQ(1U, functions.size());

  Module::Function *function = functions[0];
  EXPECT_EQ(0xf2cfda36ecf7f46dLL, function->address);
  EXPECT_LT(0U, function->size);
  EXPECT_EQ(0U, function->parameter_size);
  ASSERT_EQ(0U, function->lines.size());
}

TEST(InferSizes, LineSize) {
  Module m("name", "os", "arch", "id");
  StabsToModule h(&m);

  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit", 0xb4513962eff94e92LL,
                                     "build-directory"));
  EXPECT_TRUE(h.StartFunction("function", 0xb4513962eff94e92LL));
  EXPECT_TRUE(h.Line(0xb4513962eff94e92LL, "source-file-name-1", 77396614));
  EXPECT_TRUE(h.Line(0xb4513963eff94e92LL, "source-file-name-2", 87660088));
  EXPECT_TRUE(h.EndFunction(0));
  EXPECT_TRUE(h.EndCompilationUnit(0));
  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit-2", 0xb4523963eff94e92LL,
                                     "build-directory-2"));
  EXPECT_TRUE(h.EndCompilationUnit(0));
  h.Finalize();

  Module::File *file1 = m.FindExistingFile("source-file-name-1");
  ASSERT_TRUE(file1 != NULL);
  Module::File *file2 = m.FindExistingFile("source-file-name-2");
  ASSERT_TRUE(file2 != NULL);

  vector<Module::Function *> functions;
  m.GetFunctions(&functions, functions.end());
  ASSERT_EQ((size_t) 1, functions.size());

  Module::Function *function = functions[0];
  EXPECT_STREQ("function", function->name.c_str());
  EXPECT_EQ(0xb4513962eff94e92LL, function->address);
  EXPECT_EQ(0x1000100000000ULL, function->size);
  EXPECT_EQ(0U, function->parameter_size);
  ASSERT_EQ((size_t) 2, function->lines.size());

  Module::Line *line1 = &function->lines[0];
  EXPECT_EQ(0xb4513962eff94e92LL, line1->address);
  EXPECT_EQ(0x100000000ULL, line1->size);
  EXPECT_TRUE(line1->file == file1);
  EXPECT_EQ(77396614, line1->number);

  Module::Line *line2 = &function->lines[1];
  EXPECT_EQ(0xb4513963eff94e92LL, line2->address);
  EXPECT_EQ(0x1000000000000ULL, line2->size);
  EXPECT_TRUE(line2->file == file2);
  EXPECT_EQ(87660088, line2->number);
}

#ifdef __GNUC__

TEST(FunctionNames, Mangled) {
  Module m("name", "os", "arch", "id");
  StabsToModule h(&m);

  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit", 0xf2cfda63cef7f46cLL,
                                     "build-directory"));
  EXPECT_TRUE(h.StartFunction("_ZNSt6vectorIySaIyEE9push_backERKy",
                              0xf2cfda63cef7f46dLL));
  EXPECT_TRUE(h.EndFunction(0));
  EXPECT_TRUE(h.EndCompilationUnit(0));

  h.Finalize();

  Module::File *file = m.FindExistingFile("compilation-unit");
  ASSERT_TRUE(file != NULL);

  vector<Module::Function *> functions;
  m.GetFunctions(&functions, functions.end());
  ASSERT_EQ(1U, functions.size());

  Module::Function *function = functions[0];

  EXPECT_STREQ("std::vector<unsigned long long, "
               "std::allocator<unsigned long long> >::"
               "push_back(unsigned long long const&)",
               function->name.c_str());
  EXPECT_EQ(0xf2cfda63cef7f46dLL, function->address);
  EXPECT_LT(0U, function->size);
  EXPECT_EQ(0U, function->parameter_size);
  ASSERT_EQ(0U, function->lines.size());
}
#endif

TEST(Omitted, Function) {
  Module m("name", "os", "arch", "id");
  StabsToModule h(&m);

  EXPECT_TRUE(h.StartCompilationUnit("compilation-unit", 0, "build-directory"));
  EXPECT_TRUE(h.StartFunction("function", 0x2a133596));
  EXPECT_TRUE(h.EndFunction(0));
  EXPECT_TRUE(h.EndCompilationUnit(0));
}
