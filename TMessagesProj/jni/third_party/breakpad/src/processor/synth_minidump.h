// -*- mode: C++ -*-

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

// synth_minidump.h: Interface to SynthMinidump: fake minidump generator.
//
// We treat a minidump file as the concatenation of a bunch of
// test_assembler::Sections. The file header, stream directory,
// streams, memory regions, strings, and so on --- each is a Section
// that eventually gets appended to the minidump. Dump, Memory,
// Context, Thread, and so on all inherit from test_assembler::Section.
// For example:
//
//    using google_breakpad::test_assembler::kLittleEndian;
//    using google_breakpad::SynthMinidump::Context;
//    using google_breakpad::SynthMinidump::Dump;
//    using google_breakpad::SynthMinidump::Memory;
//    using google_breakpad::SynthMinidump::Thread;
//
//    Dump minidump(MD_NORMAL, kLittleEndian);
//
//    Memory stack1(minidump, 0x569eb0a9);
//    ... build contents of stack1 with test_assembler::Section functions ...
//
//    MDRawContextX86 x86_context1;
//    x86_context1.context_flags = MD_CONTEXT_X86;
//    x86_context1.eip = 0x7c90eb94;
//    x86_context1.esp = 0x569eb0a9;
//    x86_context1.ebp = x86_context1.esp + something appropriate;
//    Context context1(minidump, x86_context1);
//
//    Thread thread1(minidump, 0xe4a4821d, stack1, context1);
//
//    minidump.Add(&stack1);
//    minidump.Add(&context1);
//    minidump.Add(&thread1);
//    minidump.Finish();
//
//    string contents;
//    EXPECT_TRUE(minidump.GetContents(&contents));
//    // contents now holds the bytes of a minidump file
//
// Because the test_assembler classes let us write Label references to
// sections before the Labels' values are known, this gives us
// flexibility in how we put the dump together: minidump pieces can
// hold the file offsets of other minidump pieces before the
// referents' positions have been decided. As long as everything has
// been placed by the time we call dump.GetContents to obtain the
// bytes, all the Labels' values will be known, and everything will
// get patched up appropriately.
//
// The dump.Add(thing) functions append THINGS's contents to the
// minidump, but they also do two other things:
//
// - dump.Add(thing) invokes thing->Finish, which tells *thing the
//   offset within the file at which it was placed, and allows *thing
//   to do any final content generation.
//
// - If THING is something which should receive an entry in some sort
//   of list or directory, then dump.Add(THING) automatically creates
//   the appropriate directory or list entry. Streams must appear in
//   the stream directory; memory ranges should be listed in the
//   memory list; threads should be placed in the thread list; and so
//   on.
//
// By convention, Section subclass constructors that take references
// to other Sections do not take care of 'Add'ing their arguments to
// the dump. For example, although the Thread constructor takes
// references to a Memory and a Context, it does not add them to the
// dump on the caller's behalf. Rather, the caller is responsible for
// 'Add'ing every section they create. This allows Sections to be
// cited from more than one place; for example, Memory ranges are
// cited both from Thread objects (as their stack contents) and by the
// memory list stream.
//
// If you forget to Add some Section, the Dump::GetContents call will
// fail, as the test_assembler::Labels used to cite the Section's
// contents from elsewhere will still be undefined.
#ifndef PROCESSOR_SYNTH_MINIDUMP_H_
#define PROCESSOR_SYNTH_MINIDUMP_H_

#include <assert.h>

#include <iostream>
#include <string>

#include "common/test_assembler.h"
#include "common/using_std_string.h"
#include "google_breakpad/common/breakpad_types.h"
#include "google_breakpad/common/minidump_format.h"

namespace google_breakpad {

namespace SynthMinidump {

using test_assembler::Endianness;
using test_assembler::kBigEndian;
using test_assembler::kLittleEndian;
using test_assembler::kUnsetEndian;
using test_assembler::Label;

class Dump;
class Memory;
class String;

class Section: public test_assembler::Section {
 public:
  explicit Section(const Dump &dump);

  void CiteLocationIn(test_assembler::Section *section) const;

  virtual void Finish(const Label &offset) {
    file_offset_ = offset; size_ = Size();
  }

 protected:

  Label file_offset_, size_;
};

class Stream: public Section {
 public:

  Stream(const Dump &dump, uint32_t type) : Section(dump), type_(type) { }

  void CiteStreamIn(test_assembler::Section *section) const;

 private:

  uint32_t type_;
};

class SystemInfo: public Stream {
 public:

  SystemInfo(const Dump &dump,
             const MDRawSystemInfo &system_info,
             const String &csd_version);

  static const MDRawSystemInfo windows_x86;
  static const string windows_x86_csd_version;
};

class String: public Section {
 public:
  String(const Dump &dump, const string &value);

  void CiteStringIn(test_assembler::Section *section) const;
};

class Memory: public Section {
 public:
  Memory(const Dump &dump, uint64_t address)
      : Section(dump), address_(address) { start() = address; }

  void CiteMemoryIn(test_assembler::Section *section) const;

 private:

  uint64_t address_;
};

class Context: public Section {
 public:

  Context(const Dump &dump, const MDRawContextX86 &context);
  Context(const Dump &dump, const MDRawContextARM &context);
  Context(const Dump &dump, const MDRawContextMIPS &context);

  Context(const Dump &dump) : Section(dump) {}

};

class Thread: public Section {
 public:

  Thread(const Dump &dump,
         uint32_t thread_id,
         const Memory &stack,
         const Context &context,
         uint32_t suspend_count = 0,
         uint32_t priority_class = 0,
         uint32_t priority = 0,
         uint64_t teb = 0);
};

class Module: public Section {
 public:

  Module(const Dump &dump,
         uint64_t base_of_image,
         uint32_t size_of_image,
         const String &name,
         uint32_t time_date_stamp = 1262805309,
         uint32_t checksum = 0,
         const MDVSFixedFileInfo &version_info = Module::stock_version_info,
         const Section *cv_record = NULL,
         const Section *misc_record = NULL);

 private:

  static const MDVSFixedFileInfo stock_version_info;
};

class Exception : public Stream {
public:
  Exception(const Dump &dump,
            const Context &context,
            uint32_t thread_id = 0,
            uint32_t exception_code = 0,
            uint32_t exception_flags = 0,
            uint64_t exception_address = 0);
};

template<typename Element>
class List: public Stream {
 public:
  List(const Dump &dump, uint32_t type) : Stream(dump, type), count_(0) {
    D32(count_label_);
  }

  void Add(Element *element) {
    element->Finish(file_offset_ + Size());
    Append(*element);
    count_++;
  }

  bool Empty() { return count_ == 0; }

  virtual void Finish(const Label &offset) {
    Stream::Finish(offset);
    count_label_ = count_;
  }

 private:
  size_t count_;
  Label count_label_;
};

class Dump: public test_assembler::Section {
 public:

  Dump(uint64_t flags,
       Endianness endianness = kLittleEndian,
       uint32_t version = MD_HEADER_VERSION,
       uint32_t date_time_stamp = 1262805309);

  Dump &Add(SynthMinidump::Section *object);
  Dump &Add(Stream *object);
  Dump &Add(Memory *object);
  Dump &Add(Thread *object);
  Dump &Add(Module *object);

  void Finish();

 private:

  Label file_start_;

  SynthMinidump::Section stream_directory_;
  size_t stream_count_;
  Label stream_count_label_;
  Label stream_directory_rva_;

  List<Thread> thread_list_;

  List<Module> module_list_;

  List<SynthMinidump::Section> memory_list_;
};

}

}

#endif
