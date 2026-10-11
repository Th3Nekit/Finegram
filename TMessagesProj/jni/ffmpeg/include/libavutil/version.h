/*
 * copyright (c) 2003 Fabrice Bellard
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
 * @ingroup lavu
 * Libavutil version macros
 */

#ifndef AVUTIL_VERSION_H
#define AVUTIL_VERSION_H

#include "macros.h"

#define AV_VERSION_INT(a, b, c) ((a)<<16 | (b)<<8 | (c))
#define AV_VERSION_DOT(a, b, c) a ##.## b ##.## c
#define AV_VERSION(a, b, c) AV_VERSION_DOT(a, b, c)

#define AV_VERSION_MAJOR(a) ((a) >> 16)
#define AV_VERSION_MINOR(a) (((a) & 0x00FF00) >> 8)
#define AV_VERSION_MICRO(a) ((a) & 0xFF)

#define LIBAVUTIL_VERSION_MAJOR  60
#define LIBAVUTIL_VERSION_MINOR  26
#define LIBAVUTIL_VERSION_MICRO 102

#define LIBAVUTIL_VERSION_INT   AV_VERSION_INT(LIBAVUTIL_VERSION_MAJOR, \
                                               LIBAVUTIL_VERSION_MINOR, \
                                               LIBAVUTIL_VERSION_MICRO)
#define LIBAVUTIL_VERSION       AV_VERSION(LIBAVUTIL_VERSION_MAJOR,     \
                                           LIBAVUTIL_VERSION_MINOR,     \
                                           LIBAVUTIL_VERSION_MICRO)
#define LIBAVUTIL_BUILD         LIBAVUTIL_VERSION_INT

#define LIBAVUTIL_IDENT         "Lavu" AV_STRINGIFY(LIBAVUTIL_VERSION)

#define FF_API_MOD_UINTP2               (LIBAVUTIL_VERSION_MAJOR < 61)
#define FF_API_RISCV_FD_ZBA             (LIBAVUTIL_VERSION_MAJOR < 61)
#define FF_API_VULKAN_FIXED_QUEUES      (LIBAVUTIL_VERSION_MAJOR < 61)
#define FF_API_OPT_INT_LIST             (LIBAVUTIL_VERSION_MAJOR < 61)
#define FF_API_OPT_PTR                  (LIBAVUTIL_VERSION_MAJOR < 61)
#define FF_API_CPU_FLAG_FORCE           (LIBAVUTIL_VERSION_MAJOR < 61)
#define FF_API_DOVI_L11_INVALID_PROPS   (LIBAVUTIL_VERSION_MAJOR < 61)
#define FF_API_ASSERT_FPU               (LIBAVUTIL_VERSION_MAJOR < 61)
#define FF_API_VULKAN_SYNC_QUEUES       (LIBAVUTIL_VERSION_MAJOR < 62)

#endif
