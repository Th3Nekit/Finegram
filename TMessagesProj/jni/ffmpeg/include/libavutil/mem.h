/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * @ingroup lavu_mem
 * Memory handling functions
 */

#ifndef AVUTIL_MEM_H
#define AVUTIL_MEM_H

#include <stddef.h>
#include <stdint.h>

#include "attributes.h"

#if AV_GCC_VERSION_AT_LEAST(3,1)
    #define av_malloc_attrib __attribute__((__malloc__))
#else
    #define av_malloc_attrib
#endif

#if AV_GCC_VERSION_AT_LEAST(4,3)
    #define av_alloc_size(...) __attribute__((alloc_size(__VA_ARGS__)))
#else
    #define av_alloc_size(...)
#endif

void *av_malloc(size_t size) av_malloc_attrib av_alloc_size(1);

void *av_mallocz(size_t size) av_malloc_attrib av_alloc_size(1);

av_alloc_size(1, 2) void *av_malloc_array(size_t nmemb, size_t size);

void *av_calloc(size_t nmemb, size_t size) av_malloc_attrib av_alloc_size(1, 2);

void *av_realloc(void *ptr, size_t size) av_alloc_size(2);

av_warn_unused_result
int av_reallocp(void *ptr, size_t size);

void *av_realloc_f(void *ptr, size_t nelem, size_t elsize);

av_alloc_size(2, 3) void *av_realloc_array(void *ptr, size_t nmemb, size_t size);

int av_reallocp_array(void *ptr, size_t nmemb, size_t size);

void *av_fast_realloc(void *ptr, unsigned int *size, size_t min_size);

void av_fast_malloc(void *ptr, unsigned int *size, size_t min_size);

void av_fast_mallocz(void *ptr, unsigned int *size, size_t min_size);

void av_free(void *ptr);

void av_freep(void *ptr);

char *av_strdup(const char *s) av_malloc_attrib;

char *av_strndup(const char *s, size_t len) av_malloc_attrib;

void *av_memdup(const void *p, size_t size);

void av_memcpy_backptr(uint8_t *dst, int back, int cnt);

void av_dynarray_add(void *tab_ptr, int *nb_ptr, void *elem);

av_warn_unused_result
int av_dynarray_add_nofree(void *tab_ptr, int *nb_ptr, void *elem);

void *av_dynarray2_add(void **tab_ptr, int *nb_ptr, size_t elem_size,
                       const uint8_t *elem_data);

int av_size_mult(size_t a, size_t b, size_t *r);

void av_max_alloc(size_t max);

#endif
