/*-
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.
 * Copyright (c) 2005 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* This file has been renamed user_malloc.h for Userspace */
#ifndef _USER_MALLOC_H_
#define	_USER_MALLOC_H_

#include <stdlib.h>
#include <sys/types.h>
#if !defined(_WIN32)
#include <strings.h>
#include <stdint.h>
#else
#if (defined(_MSC_VER) && _MSC_VER >= 1600) || (defined(__MSVCRT_VERSION__) && __MSVCRT_VERSION__ >= 1400)
#include <stdint.h>
#elif defined(SCTP_STDINT_INCLUDE)
#include SCTP_STDINT_INCLUDE
#else
#define uint32_t unsigned __int32
#define uint64_t unsigned __int64
#endif
#include <winsock2.h>
#endif

#define	MINALLOCSIZE	UMA_SMALLEST_UNIT

#define	M_NOWAIT	0x0001
#define	M_WAITOK	0x0002
#define	M_ZERO		0x0100
#define	M_NOVM		0x0200
#define	M_USE_RESERVE	0x0400

#define	M_MAGIC		877983977

struct malloc_type_stats {
	uint64_t	mts_memalloced;
	uint64_t	mts_memfreed;
	uint64_t	mts_numallocs;
	uint64_t	mts_numfrees;
	uint64_t	mts_size;
	uint64_t	_mts_reserved1;
	uint64_t	_mts_reserved2;
	uint64_t	_mts_reserved3;
};

#ifndef MAXCPU
#define MAXCPU 4
#endif

struct malloc_type_internal {
	struct malloc_type_stats	mti_stats[MAXCPU];
};

struct malloc_type {
	struct malloc_type *ks_next;
	u_long		 _ks_memuse;
	u_long		 _ks_size;
	u_long		 _ks_inuse;
	uint64_t	 _ks_calls;
	u_long		 _ks_maxused;
	u_long		 ks_magic;
	const char	*ks_shortdesc;

	void		*ks_handle;
	const char	*_lo_name;
	const char	*_lo_type;
	u_int		 _lo_flags;
	void		*_lo_list_next;
	struct witness	*_lo_witness;
	uintptr_t	 _mtx_lock;
	u_int		 _mtx_recurse;
};

#define	MALLOC_TYPE_STREAM_VERSION	0x00000001
struct malloc_type_stream_header {
	uint32_t	mtsh_version;
	uint32_t	mtsh_maxcpus;
	uint32_t	mtsh_count;
	uint32_t	_mtsh_pad;
};

#define	MALLOC_MAX_NAME	32
struct malloc_type_header {
	char				mth_name[MALLOC_MAX_NAME];
};

#define	MALLOC_DEFINE(type, shortdesc, longdesc)			\
	struct malloc_type type[1] = {					\
		{ NULL, 0, 0, 0, 0, 0, M_MAGIC, shortdesc, NULL, NULL,	\
		    NULL, 0, NULL, NULL, 0, 0 }				\
	}

#define	MALLOC_DECLARE(type) \
	extern struct malloc_type type[1]

#define	FREE(addr, type) free((addr))

#define	MALLOC(space, cast, size, type, flags)                          \
    ((space) = (cast)malloc((u_long)(size)));                           \
    do {								\
        if (flags & M_ZERO) {                                            \
	  memset(space,0,size);                                         \
	}								\
    } while (0);

#endif
