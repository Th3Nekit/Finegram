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

#ifndef _USER_UMA_H_
#define _USER_UMA_H_

#define UMA_ZFLAG_FULL		0x40000000
#define UMA_ALIGN_PTR	(sizeof(void *) - 1)

typedef int (*uma_ctor)(void *mem, int size, void *arg, int flags);
typedef void (*uma_dtor)(void *mem, int size, void *arg);
typedef int (*uma_init)(void *mem, int size, int flags);
typedef void (*uma_fini)(void *mem, int size);
typedef struct uma_zone * uma_zone_t;
typedef struct uma_keg * uma_keg_t;

struct uma_cache {
	int stub;
};

struct uma_keg {
	int stub;
};

struct uma_zone {
	char		*uz_name;
	struct mtx	*uz_lock;
	uma_keg_t	uz_keg;

	LIST_ENTRY(uma_zone)	uz_link;
	LIST_HEAD(,uma_bucket)	uz_full_bucket;
	LIST_HEAD(,uma_bucket)	uz_free_bucket;

	uma_ctor	uz_ctor;
	uma_dtor	uz_dtor;
	uma_init	uz_init;
	uma_fini	uz_fini;

	u_int64_t	uz_allocs;
	u_int64_t	uz_frees;
	u_int64_t	uz_fails;
	uint16_t	uz_fills;
	uint16_t	uz_count;

	struct uma_cache	uz_cpu[1];
};

uma_zone_t
uma_zcreate(char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
	    uma_init uminit, uma_fini fini, int align, uint32_t flags);

#define uma_zone_set_max(zone, number)

uma_zone_t
uma_zcreate(char *name, size_t size, uma_ctor ctor, uma_dtor dtor,
	    uma_init uminit, uma_fini fini, int align, uint32_t flags) {
	return NULL;
}
#endif
