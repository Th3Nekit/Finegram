// Copyright (c) 2000 - 2007, Google Inc.
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
// Produce stack trace

#include <stdint.h>

#include "utilities.h"

#if !defined(OS_WINDOWS)
#include <unistd.h>
#include <sys/mman.h>
#endif

#include <stdio.h>
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
#ifdef __i386__

  if ((uintptr_t)new_sp >= 0xffffe000) return NULL;
#endif
#if !defined(OS_WINDOWS)
  if (!STRICT_UNWINDING) {

    static int page_size = getpagesize();
    void *new_sp_aligned = (void *)((uintptr_t)new_sp & ~(page_size - 1));
    if (msync(new_sp_aligned, page_size, MS_ASYNC) == -1)
      return NULL;
  }
#endif
  return new_sp;
}

int GetStackTrace(void** result, int max_depth, int skip_count) {
  void **sp;
#ifdef __i386__

  sp = (void **)&result - 2;
#endif

#ifdef __x86_64__

  unsigned long rbp;

  __asm__ volatile ("mov %%rbp, %0" : "=r" (rbp));

  sp = (void **) rbp;
#endif

  int n = 0;
  while (sp && n < max_depth) {
    if (*(sp+1) == (void *)0) {

      break;
    }
    if (skip_count > 0) {
      skip_count--;
    } else {
      result[n++] = *(sp+1);
    }

    sp = NextStackFrame<true>(sp);
  }
  return n;
}

_END_GOOGLE_NAMESPACE_
