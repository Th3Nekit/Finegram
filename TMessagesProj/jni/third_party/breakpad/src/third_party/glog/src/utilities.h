// Copyright (c) 2008, Google Inc.
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
// Author: Shinichiro Hamaji
//
// Define utilties for glog internal usage.

#ifndef UTILITIES_H__
#define UTILITIES_H__

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
# define OS_WINDOWS
#elif defined(__CYGWIN__) || defined(__CYGWIN32__)
# define OS_CYGWIN
#elif defined(linux) || defined(__linux) || defined(__linux__)
# define OS_LINUX
#elif defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
# define OS_MACOSX
#elif defined(__FreeBSD__)
# define OS_FREEBSD
#elif defined(__NetBSD__)
# define OS_NETBSD
#elif defined(__OpenBSD__)
# define OS_OPENBSD
#else

#endif

#ifdef _LP64
#define __PRIS_PREFIX "z"
#else
#define __PRIS_PREFIX
#endif

#define PRIdS __PRIS_PREFIX "d"
#define PRIxS __PRIS_PREFIX "x"
#define PRIuS __PRIS_PREFIX "u"
#define PRIXS __PRIS_PREFIX "X"
#define PRIoS __PRIS_PREFIX "o"

#include "base/mutex.h"

#include <string>

#if defined(OS_WINDOWS)
# include "port.h"
#endif

#include "config.h"
#include "glog/logging.h"

#if defined(HAVE_LIB_UNWIND)
# define STACKTRACE_H "stacktrace_libunwind-inl.h"
#elif !defined(NO_FRAME_POINTER)
# if defined(__i386__) && __GNUC__ >= 2
#  define STACKTRACE_H "stacktrace_x86-inl.h"
# elif defined(__x86_64__) && __GNUC__ >= 2
#  define STACKTRACE_H "stacktrace_x86_64-inl.h"
# elif (defined(__ppc__) || defined(__PPC__)) && __GNUC__ >= 2
#  define STACKTRACE_H "stacktrace_powerpc-inl.h"
# endif
#endif

#if !defined(STACKTRACE_H) && defined(HAVE_EXECINFO_H)
# define STACKTRACE_H "stacktrace_generic-inl.h"
#endif

#if defined(STACKTRACE_H)
# define HAVE_STACKTRACE
#endif

#if defined(__ELF__) && defined(OS_LINUX)
# define HAVE_SYMBOLIZE
#elif defined(OS_MACOSX) && defined(HAVE_DLADDR)

# define HAVE_SYMBOLIZE
#endif

#ifndef ARRAYSIZE

# define ARRAYSIZE(a) (sizeof(a) / sizeof(*(a)))
#endif

_START_GOOGLE_NAMESPACE_

namespace glog_internal_namespace_ {

#ifdef HAVE___ATTRIBUTE__
# define ATTRIBUTE_NOINLINE __attribute__ ((noinline))
# define HAVE_ATTRIBUTE_NOINLINE
#else
# define ATTRIBUTE_NOINLINE
#endif

const char* ProgramInvocationShortName();

bool IsGoogleLoggingInitialized();

bool is_default_thread();

int64 CycleClock_Now();

int64 UsecToCycles(int64 usec);

typedef double WallTime;
WallTime WallTime_Now();

int32 GetMainThreadPid();
bool PidHasChanged();

pid_t GetTID();

const std::string& MyUserName();

const char* const_basename(const char* filepath);

template<typename T>
inline T sync_val_compare_and_swap(T* ptr, T oldval, T newval) {
#if defined(HAVE___SYNC_VAL_COMPARE_AND_SWAP)
  return __sync_val_compare_and_swap(ptr, oldval, newval);
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
  T ret;
  __asm__ __volatile__("lock; cmpxchg %1, (%2);"
                       :"=a"(ret)

                       :"q"(newval), "q"(ptr), "a"(oldval)
                       :"memory", "cc");
  return ret;
#else
  T ret = *ptr;
  if (ret == oldval) {
    *ptr = newval;
  }
  return ret;
#endif
}

void DumpStackTraceToString(std::string* stacktrace);

struct CrashReason {
  CrashReason() : filename(0), line_number(0), message(0), depth(0) {}

  const char* filename;
  int line_number;
  const char* message;

  void* stack[32];
  int depth;
};

void SetCrashReason(const CrashReason* r);

void InitGoogleLoggingUtilities(const char* argv0);
void ShutdownGoogleLoggingUtilities();

}

_END_GOOGLE_NAMESPACE_

using namespace GOOGLE_NAMESPACE::glog_internal_namespace_;

#endif
