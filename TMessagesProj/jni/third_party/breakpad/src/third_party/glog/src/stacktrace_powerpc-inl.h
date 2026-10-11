// Copyright (c) 2007, Google Inc.
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
//
// Author: Craig Silverstein
//
// Produce stack trace.  I'm guessing (hoping!) the code is much like
// for x86.  For apple machines, at least, it seems to be; see
//    http://developer.apple.com/documentation/mac/runtimehtml/RTArch-59.html
//    http://www.linux-foundation.org/spec/ELF/ppc64/PPC-elf64abi-1.9.html#STACK
// Linux has similar code: http://patchwork.ozlabs.org/linuxppc/patch?id=8882

#include <stdio.h>
#include <stdint.h>
#include "stacktrace.h"

_START_GOOGLE_NAMESPACE_

template<bool STRICT_UNWINDING>
static void **NextStackFrame(void **old_sp) {
  void **new_sp = (void **) *old_sp;

  if (STRICT_UNWINDING) {

    if (new_sp <= old_sp) return NULL;

    if ((uintptr_t)new_sp - (uintptr_t)old_sp > 100000) return NULL;
  } else {

    if (new_sp == old_sp) return NULL;

    if ((new_sp > old_sp)
        && ((uintptr_t)new_sp - (uintptr_t)old_sp > 1000000)) return NULL;
  }
  if ((uintptr_t)new_sp & (sizeof(void *) - 1)) return NULL;
  return new_sp;
}

void StacktracePowerPCDummyFunction() __attribute__((noinline));
void StacktracePowerPCDummyFunction() { __asm__ volatile(""); }

int GetStackTrace(void** result, int max_depth, int skip_count) {
  void **sp;

#ifdef __APPLE__
  __asm__ volatile ("mr %0,r1" : "=r" (sp));
#else
  __asm__ volatile ("mr %0,1" : "=r" (sp));
#endif

  StacktracePowerPCDummyFunction();

  skip_count++;

  int n = 0;
  while (sp && n < max_depth) {
    if (skip_count > 0) {
      skip_count--;
    } else {

#if defined(_CALL_AIX) || defined(_CALL_DARWIN)
      result[n++] = *(sp+2);
#elif defined(_CALL_SYSV)
      result[n++] = *(sp+1);
#elif defined(__APPLE__) || (defined(__linux) && defined(__PPC64__))

      result[n++] = *(sp+2);
#elif defined(__linux)

      result[n++] = *(sp+1);
#else
#error Need to specify the PPC ABI for your archiecture.
#endif
    }

    sp = NextStackFrame<true>(sp);
  }
  return n;
}

_END_GOOGLE_NAMESPACE_
