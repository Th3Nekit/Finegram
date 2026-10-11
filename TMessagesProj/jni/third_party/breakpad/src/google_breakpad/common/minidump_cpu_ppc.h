/* Copyright (c) 2006, Google Inc.
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
 * produced on ppc.  These files may be read on any platform provided
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
 * Author: Mark Mentovai 
 * Change to split into its own file: Neal Sidhwaney */

/*
 * Breakpad minidump extension for PowerPC support.  Based on Darwin/Mac OS X'
 * mach/ppc/_types.h
 */

#ifndef GOOGLE_BREAKPAD_COMMON_MINIDUMP_CPU_PPC_H__
#define GOOGLE_BREAKPAD_COMMON_MINIDUMP_CPU_PPC_H__

#define MD_FLOATINGSAVEAREA_PPC_FPR_COUNT 32

typedef struct {

  uint64_t fpregs[MD_FLOATINGSAVEAREA_PPC_FPR_COUNT];
  uint32_t fpscr_pad;
  uint32_t fpscr;
} MDFloatingSaveAreaPPC;

#define MD_VECTORSAVEAREA_PPC_VR_COUNT 32

typedef struct {

  uint128_struct save_vr[MD_VECTORSAVEAREA_PPC_VR_COUNT];
  uint128_struct save_vscr;
  uint32_t       save_pad5[4];
  uint32_t       save_vrvalid;
  uint32_t       save_pad6[7];
} MDVectorSaveAreaPPC;

#define MD_CONTEXT_PPC_GPR_COUNT 32

#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#pragma pack(4)
#else
#pragma pack(push, 4)
#endif

typedef struct {

  uint32_t              context_flags;

  uint32_t              srr0;

  uint32_t              srr1;

  uint32_t              gpr[MD_CONTEXT_PPC_GPR_COUNT];
  uint32_t              cr;
  uint32_t              xer;
  uint32_t              lr;
  uint32_t              ctr;
  uint32_t              mq;
  uint32_t              vrsave;

  MDFloatingSaveAreaPPC float_save;
  MDVectorSaveAreaPPC   vector_save;
} MDRawContextPPC;

#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#pragma pack(0)
#else
#pragma pack(pop)
#endif

#define MD_CONTEXT_PPC                0x20000000
#define MD_CONTEXT_PPC_BASE           (MD_CONTEXT_PPC | 0x00000001)
#define MD_CONTEXT_PPC_FLOATING_POINT (MD_CONTEXT_PPC | 0x00000008)
#define MD_CONTEXT_PPC_VECTOR         (MD_CONTEXT_PPC | 0x00000020)

#define MD_CONTEXT_PPC_FULL           MD_CONTEXT_PPC_BASE
#define MD_CONTEXT_PPC_ALL            (MD_CONTEXT_PPC_FULL | \
                                       MD_CONTEXT_PPC_FLOATING_POINT | \
                                       MD_CONTEXT_PPC_VECTOR)

#endif
