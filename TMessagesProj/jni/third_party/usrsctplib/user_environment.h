/*-
 * Copyright (c) 2009-2010 Brad Penoff
 * Copyright (c) 2009-2010 Humaira Kamal
 * Copyright (c) 2011-2012 Irene Ruengeler
 * Copyright (c) 2011-2012 Michael Tuexen
 *
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _USER_ENVIRONMENT_H_
#define _USER_ENVIRONMENT_H_

#include <sys/types.h>

#ifdef __FreeBSD__
#ifndef _SYS_MUTEX_H_
#include <sys/mutex.h>
#endif
#endif
#if defined(_WIN32)
#include "netinet/sctp_os_userspace.h"
#endif

extern int maxsockets;

extern int hz;

extern int ipport_firstauto, ipport_lastauto;

extern int nmbclusters;

#if !defined(_MSC_VER) && !defined(__MINGW32__)
#define min(a,b) (((a)>(b))?(b):(a))
#define max(a,b) (((a)>(b))?(a):(b))
#endif

void init_random(void);
void read_random(void *, size_t);
void finish_random(void);

extern u_short ip_id;

#if defined(__linux__)
#define IPV6_VERSION            0x60
#endif

#if defined(INVARIANTS)
#include <stdlib.h>

#if defined(_WIN32)
static inline void __declspec(noreturn)
#else
static inline void __attribute__((__noreturn__))
#endif
terminate_non_graceful(void) {
	abort();
}

#define panic(...)                                  \
	do {                                        \
		SCTP_PRINTF("%s(): ", __func__);    \
		SCTP_PRINTF(__VA_ARGS__);           \
		SCTP_PRINTF("\n");                  \
		terminate_non_graceful();           \
} while (0)

#define KASSERT(cond, args)          \
	do {                         \
		if (!(cond)) {       \
			panic args ; \
		}                    \
	} while (0)
#else
#define KASSERT(cond, args)
#endif

extern int ip_defttl;
#endif
