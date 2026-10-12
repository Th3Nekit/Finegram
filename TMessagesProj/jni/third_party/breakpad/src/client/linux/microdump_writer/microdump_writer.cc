// Copyright (c) 2014, Google Inc.
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

// This translation unit generates microdumps into the console (logcat on
// Android). See crbug.com/410294 for more info and design docs.

#include "client/linux/microdump_writer/microdump_writer.h"

#include <sys/utsname.h>

#include "client/linux/dump_writer_common/seccomp_unwinder.h"
#include "client/linux/dump_writer_common/thread_info.h"
#include "client/linux/dump_writer_common/ucontext_reader.h"
#include "client/linux/handler/exception_handler.h"
#include "client/linux/log/log.h"
#include "client/linux/minidump_writer/linux_ptrace_dumper.h"
#include "common/linux/linux_libc_support.h"

namespace {

using google_breakpad::ExceptionHandler;
using google_breakpad::LinuxDumper;
using google_breakpad::LinuxPtraceDumper;
using google_breakpad::MappingInfo;
using google_breakpad::MappingList;
using google_breakpad::RawContextCPU;
using google_breakpad::SeccompUnwinder;
using google_breakpad::ThreadInfo;
using google_breakpad::UContextReader;

const size_t kLineBufferSize = 2048;

class MicrodumpWriter {
 public:
  MicrodumpWriter(const ExceptionHandler::CrashContext* context,
                  const MappingList& mappings,
                  const char* build_fingerprint,
                  const char* product_info,
                  LinuxDumper* dumper)
      : ucontext_(context ? &context->context : NULL),
#if !defined(__ARM_EABI__) && !defined(__mips__)
        float_state_(context ? &context->float_state : NULL),
#endif
        dumper_(dumper),
        mapping_list_(mappings),
        build_fingerprint_(build_fingerprint),
        product_info_(product_info),
        log_line_(NULL) {
    log_line_ = reinterpret_cast<char*>(Alloc(kLineBufferSize));
    if (log_line_)
      log_line_[0] = '\0';
  }

  ~MicrodumpWriter() { dumper_->ThreadsResume(); }

  bool Init() {

    if (!dumper_->Init() || !log_line_)
      return false;
    return dumper_->ThreadsSuspend();
  }

  bool Dump() {
    bool success;
    LogLine("-----BEGIN BREAKPAD MICRODUMP-----");
    DumpProductInformation();
    DumpOSInformation();
    success = DumpCrashingThread();
    if (success)
      success = DumpMappings();
    LogLine("-----END BREAKPAD MICRODUMP-----");
    dumper_->ThreadsResume();
    return success;
  }

 private:

  void LogLine(const char* msg) {
    logger::write(msg, my_strlen(msg));
#if !defined(__ANDROID__)
    logger::write("\n", 1);
#endif
  }

  void LogAppend(const char* str) {
    my_strlcat(log_line_, str, kLineBufferSize);
  }

  void LogAppend(char* str) {
    LogAppend(const_cast<const char*>(str));
  }

  template<typename T>
  void LogAppend(T value) {

    static const char HEX[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                               'A', 'B', 'C', 'D', 'E', 'F'};
    char hexstr[sizeof(T) * 2 + 1];
    for (int i = sizeof(T) * 2 - 1; i >= 0; --i, value >>= 4)
      hexstr[i] = HEX[static_cast<uint8_t>(value) & 0x0F];
    hexstr[sizeof(T) * 2] = '\0';
    LogAppend(hexstr);
  }

  void LogAppend(const void* buf, size_t length) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buf);
    for (size_t i = 0; i < length; ++i, ++ptr)
      LogAppend(*ptr);
  }

  void LogCommitLine() {
    LogLine(log_line_);
    my_strlcpy(log_line_, "", kLineBufferSize);
  }

  void DumpProductInformation() {
    LogAppend("V ");
    if (product_info_) {
      LogAppend(product_info_);
    } else {
      LogAppend("UNKNOWN:0.0.0.0");
    }
    LogCommitLine();
  }

  void DumpOSInformation() {
    const uint8_t n_cpus = static_cast<uint8_t>(sysconf(_SC_NPROCESSORS_CONF));

#if defined(__ANDROID__)
    const char kOSId[] = "A";
#else
    const char kOSId[] = "L";
#endif

#if defined(__aarch64__)
    const char kArch[] = "arm64";
#elif defined(__ARMEL__)
    const char kArch[] = "arm";
#elif defined(__x86_64__)
    const char kArch[] = "x86_64";
#elif defined(__i386__)
    const char kArch[] = "x86";
#elif defined(__mips__)
    const char kArch[] = "mips";
#else
#error "This code has not been ported to your platform yet"
#endif

    LogAppend("O ");
    LogAppend(kOSId);
    LogAppend(" ");
    LogAppend(kArch);
    LogAppend(" ");
    LogAppend(n_cpus);
    LogAppend(" ");

    if (build_fingerprint_) {
      LogAppend(build_fingerprint_);
    } else {
      struct utsname uts;
      if (uname(&uts) == 0) {
        LogAppend(uts.machine);
        LogAppend(" ");
        LogAppend(uts.release);
        LogAppend(" ");
        LogAppend(uts.version);
      } else {
        LogAppend("no build fingerprint available");
      }
    }
    LogCommitLine();
  }

  bool DumpThreadStack(uint32_t thread_id,
                       uintptr_t stack_pointer,
                       int max_stack_len,
                       uint8_t** stack_copy) {
    *stack_copy = NULL;
    const void* stack;
    size_t stack_len;

    if (!dumper_->GetStackInfo(&stack, &stack_len, stack_pointer)) {

      return true;
    }

    LogAppend("S 0 ");
    LogAppend(stack_pointer);
    LogAppend(" ");
    LogAppend(reinterpret_cast<uintptr_t>(stack));
    LogAppend(" ");
    LogAppend(stack_len);
    LogCommitLine();

    if (max_stack_len >= 0 &&
        stack_len > static_cast<unsigned int>(max_stack_len)) {
      stack_len = max_stack_len;
    }

    *stack_copy = reinterpret_cast<uint8_t*>(Alloc(stack_len));
    dumper_->CopyFromProcess(*stack_copy, thread_id, stack, stack_len);

    const size_t STACK_DUMP_CHUNK_SIZE = 384;
    for (size_t stack_off = 0; stack_off < stack_len;
         stack_off += STACK_DUMP_CHUNK_SIZE) {
      LogAppend("S ");
      LogAppend(reinterpret_cast<uintptr_t>(stack) + stack_off);
      LogAppend(" ");
      LogAppend(*stack_copy + stack_off,
                std::min(STACK_DUMP_CHUNK_SIZE, stack_len - stack_off));
      LogCommitLine();
    }
    return true;
  }

  bool DumpCrashingThread() {
    const unsigned num_threads = dumper_->threads().size();

    for (unsigned i = 0; i < num_threads; ++i) {
      MDRawThread thread;
      my_memset(&thread, 0, sizeof(thread));
      thread.thread_id = dumper_->threads()[i];

      if (static_cast<pid_t>(thread.thread_id) != dumper_->crash_thread())
        continue;

      assert(ucontext_);
      assert(!dumper_->IsPostMortem());

      uint8_t* stack_copy;
      const uintptr_t stack_ptr = UContextReader::GetStackPointer(ucontext_);
      if (!DumpThreadStack(thread.thread_id, stack_ptr, -1, &stack_copy))
        return false;

      RawContextCPU cpu;
      my_memset(&cpu, 0, sizeof(RawContextCPU));
#if !defined(__ARM_EABI__) && !defined(__mips__)
      UContextReader::FillCPUContext(&cpu, ucontext_, float_state_);
#else
      UContextReader::FillCPUContext(&cpu, ucontext_);
#endif
      if (stack_copy)
        SeccompUnwinder::PopSeccompStackFrame(&cpu, thread, stack_copy);
      DumpCPUState(&cpu);
    }
    return true;
  }

  void DumpCPUState(RawContextCPU* cpu) {
    LogAppend("C ");
    LogAppend(cpu, sizeof(*cpu));
    LogCommitLine();
  }

  bool HaveMappingInfo(const MappingInfo& mapping) {
    for (MappingList::const_iterator iter = mapping_list_.begin();
         iter != mapping_list_.end();
         ++iter) {

      if (mapping.start_addr >= iter->first.start_addr &&
          (mapping.start_addr + mapping.size) <=
              (iter->first.start_addr + iter->first.size)) {
        return true;
      }
    }
    return false;
  }

  void DumpModule(const MappingInfo& mapping,
                  bool member,
                  unsigned int mapping_id,
                  const uint8_t* identifier) {
    MDGUID module_identifier;
    if (identifier) {

      my_memcpy(&module_identifier, identifier, sizeof(MDGUID));
    } else {
      dumper_->ElfFileIdentifierForMapping(
          mapping,
          member,
          mapping_id,
          reinterpret_cast<uint8_t*>(&module_identifier));
    }

    char file_name[NAME_MAX];
    char file_path[NAME_MAX];
    LinuxDumper::GetMappingEffectiveNameAndPath(
        mapping, file_path, sizeof(file_path), file_name, sizeof(file_name));

    LogAppend("M ");
    LogAppend(static_cast<uintptr_t>(mapping.start_addr));
    LogAppend(" ");
    LogAppend(mapping.offset);
    LogAppend(" ");
    LogAppend(mapping.size);
    LogAppend(" ");
    LogAppend(module_identifier.data1);
    LogAppend(module_identifier.data2);
    LogAppend(module_identifier.data3);
    LogAppend(module_identifier.data4[0]);
    LogAppend(module_identifier.data4[1]);
    LogAppend(module_identifier.data4[2]);
    LogAppend(module_identifier.data4[3]);
    LogAppend(module_identifier.data4[4]);
    LogAppend(module_identifier.data4[5]);
    LogAppend(module_identifier.data4[6]);
    LogAppend(module_identifier.data4[7]);
    LogAppend("0 ");
    LogAppend(file_name);
    LogCommitLine();
  }

  bool DumpMappings() {

    for (unsigned i = 0; i < dumper_->mappings().size(); ++i) {
      const MappingInfo& mapping = *dumper_->mappings()[i];
      if (mapping.name[0] == 0 ||
          !mapping.exec ||
          mapping.size < 4096 ||
          HaveMappingInfo(mapping)) {
        continue;
      }

      DumpModule(mapping, true, i, NULL);
    }

    for (MappingList::const_iterator iter = mapping_list_.begin();
         iter != mapping_list_.end();
         ++iter) {
      DumpModule(iter->first, false, 0, iter->second);
    }
    return true;
  }

  void* Alloc(unsigned bytes) { return dumper_->allocator()->Alloc(bytes); }

  const struct ucontext* const ucontext_;
#if !defined(__ARM_EABI__) && !defined(__mips__)
  const google_breakpad::fpstate_t* const float_state_;
#endif
  LinuxDumper* dumper_;
  const MappingList& mapping_list_;
  const char* const build_fingerprint_;
  const char* const product_info_;
  char* log_line_;
};
}

namespace google_breakpad {

bool WriteMicrodump(pid_t crashing_process,
                    const void* blob,
                    size_t blob_size,
                    const MappingList& mappings,
                    const char* build_fingerprint,
                    const char* product_info) {
  LinuxPtraceDumper dumper(crashing_process);
  const ExceptionHandler::CrashContext* context = NULL;
  if (blob) {
    if (blob_size != sizeof(ExceptionHandler::CrashContext))
      return false;
    context = reinterpret_cast<const ExceptionHandler::CrashContext*>(blob);
    dumper.set_crash_address(
        reinterpret_cast<uintptr_t>(context->siginfo.si_addr));
    dumper.set_crash_signal(context->siginfo.si_signo);
    dumper.set_crash_thread(context->tid);
  }
  MicrodumpWriter writer(context, mappings, build_fingerprint, product_info,
                         &dumper);
  if (!writer.Init())
    return false;
  return writer.Dump();
}

}
