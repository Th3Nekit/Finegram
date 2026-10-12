/* Copyright (c) 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/* minidump_format.h: A cross-platform reimplementation of minidump-related
 * portions of DbgHelp.h from the Windows Platform SDK.
 *
 * (This is C99 source, please don't corrupt it with C++.)
 *
 * This file contains the necessary definitions to read minidump files
 * produced on ppc64.  These files may be read on any platform provided
 * that the alignments of these structures on the processing system are
 * identical to the alignments of these structures on the producing system.
 * For this reason, precise-sized types are used.  The structures defined
 * by this file have been laid out to minimize alignment problems by ensuring
 * ensuring that all members are aligned on their natural boundaries.  In
 * In some cases, tail-padding may be significant when different ABIs specify
 * different tail-padding behaviors.  To avoid problems when reading or
 * writing affected structures, MD_*_SIZE macros are provided where needed,
 * containing the useful size of the structures without padding.
 *
 * Structures that are defined by Microsoft to contain a zero-length array
 * are instead defined here to contain an array with one element, as
 * zero-length arrays are forbidden by standard C and C++.  In these cases,
 * *_minsize constants are provided to be used in place of sizeof.  For a
 * cleaner interface to these sizes when using C++, see minidump_size.h.
 *
 * These structures are also sufficient to populate minidump files.
 *
 * These definitions may be extended to support handling minidump files
 * for other CPUs and other operating systems.
 *
 * Because precise data type sizes are crucial for this implementation to
 * function properly and portably in terms of interoperability with minidumps
 * produced by DbgHelp on Windows, a set of primitive types with known sizes
 * are used as the basis of each structure defined by this file.  DbgHelp
 * on Windows is assumed to be the reference implementation; this file
 * seeks to provide a cross-platform compatible implementation.  To avoid
 * collisions with the types and values defined and used by DbgHelp in the
 * event that this implementation is used on Windows, each type and value
 * defined here is given a new name, beginning with "MD".  Names of the
 * equivalent types and values in the Windows Platform SDK are given in
 * comments.
 *
 * Author: Neal Sidhwaney */

/*
 * Breakpad minidump extension for PPC64 support.  Based on Darwin/Mac OS X'
 * mach/ppc/_types.h
 */

#ifndef GOOGLE_BREAKPAD_COMMON_MINIDUMP_CPU_PPC64_H__
#define GOOGLE_BREAKPAD_COMMON_MINIDUMP_CPU_PPC64_H__

#include "minidump_cpu_ppc.h"

typedef MDFloatingSaveAreaPPC MDFloatingSaveAreaPPC64;
typedef MDVectorSaveAreaPPC MDVectorSaveAreaPPC64;

#define MD_CONTEXT_PPC64_GPR_COUNT MD_CONTEXT_PPC_GPR_COUNT

typedef struct {

  uint64_t              context_flags;

  uint64_t              srr0;

  uint64_t              srr1;

  uint64_t              gpr[MD_CONTEXT_PPC64_GPR_COUNT];
  uint64_t              cr;
  uint64_t              xer;
  uint64_t              lr;
  uint64_t              ctr;
  uint64_t              vrsave;

  MDFloatingSaveAreaPPC float_save;
  MDVectorSaveAreaPPC   vector_save;
} MDRawContextPPC64;

#define MD_CONTEXT_PPC64                0x01000000
#define MD_CONTEXT_PPC64_BASE           (MD_CONTEXT_PPC64 | 0x00000001)
#define MD_CONTEXT_PPC64_FLOATING_POINT (MD_CONTEXT_PPC64 | 0x00000008)
#define MD_CONTEXT_PPC64_VECTOR         (MD_CONTEXT_PPC64 | 0x00000020)

#define MD_CONTEXT_PPC64_FULL           MD_CONTEXT_PPC64_BASE
#define MD_CONTEXT_PPC64_ALL            (MD_CONTEXT_PPC64_FULL | \
                                         MD_CONTEXT_PPC64_FLOATING_POINT | \
                                         MD_CONTEXT_PPC64_VECTOR)

#endif
