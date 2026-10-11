/*
 * Copyright (c) 2012 Nicolas George
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
 * @ingroup lavu_avbprint
 * AVBPrint public header
 */

#ifndef AVUTIL_BPRINT_H
#define AVUTIL_BPRINT_H

#include <stdarg.h>

#include "attributes.h"
#include "avstring.h"

#define FF_PAD_STRUCTURE(name, size, ...) \
struct ff_pad_helper_##name { __VA_ARGS__ }; \
typedef struct name { \
    __VA_ARGS__ \
    char reserved_padding[size - sizeof(struct ff_pad_helper_##name)]; \
} name;

FF_PAD_STRUCTURE(AVBPrint, 1024,
    char *str;
    unsigned len;
    unsigned size;
    unsigned size_max;
    char reserved_internal_buffer[1];
)

#define AV_BPRINT_SIZE_UNLIMITED  ((unsigned)-1)

#define AV_BPRINT_SIZE_AUTOMATIC  1

#define AV_BPRINT_SIZE_COUNT_ONLY 0

void av_bprint_init(AVBPrint *buf, unsigned size_init, unsigned size_max);

void av_bprint_init_for_buffer(AVBPrint *buf, char *buffer, unsigned size);

void av_bprintf(AVBPrint *buf, const char *fmt, ...) av_printf_format(2, 3);

void av_vbprintf(AVBPrint *buf, const char *fmt, va_list vl_arg);

void av_bprint_chars(AVBPrint *buf, char c, unsigned n);

void av_bprint_append_data(AVBPrint *buf, const char *data, unsigned size);

struct tm;

void av_bprint_strftime(AVBPrint *buf, const char *fmt, const struct tm *tm);

void av_bprint_get_buffer(AVBPrint *buf, unsigned size,
                          unsigned char **mem, unsigned *actual_size);

void av_bprint_clear(AVBPrint *buf);

static inline int av_bprint_is_complete(const AVBPrint *buf)
{
    return buf->len < buf->size;
}

int av_bprint_finalize(AVBPrint *buf, char **ret_str);

void av_bprint_escape(AVBPrint *dstbuf, const char *src, const char *special_chars,
                      enum AVEscapeMode mode, int flags);

#endif
