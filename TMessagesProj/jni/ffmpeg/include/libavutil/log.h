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

#ifndef AVUTIL_LOG_H
#define AVUTIL_LOG_H

#include <stdarg.h>
#include "attributes.h"
#include "version.h"

typedef enum {
    AV_CLASS_CATEGORY_NA = 0,
    AV_CLASS_CATEGORY_INPUT,
    AV_CLASS_CATEGORY_OUTPUT,
    AV_CLASS_CATEGORY_MUXER,
    AV_CLASS_CATEGORY_DEMUXER,
    AV_CLASS_CATEGORY_ENCODER,
    AV_CLASS_CATEGORY_DECODER,
    AV_CLASS_CATEGORY_FILTER,
    AV_CLASS_CATEGORY_BITSTREAM_FILTER,
    AV_CLASS_CATEGORY_SWSCALER,
    AV_CLASS_CATEGORY_SWRESAMPLER,
    AV_CLASS_CATEGORY_HWDEVICE,
    AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT = 40,
    AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT,
    AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT,
    AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT,
    AV_CLASS_CATEGORY_DEVICE_OUTPUT,
    AV_CLASS_CATEGORY_DEVICE_INPUT,
    AV_CLASS_CATEGORY_NB
}AVClassCategory;

enum AVClassStateFlags {

    AV_CLASS_STATE_INITIALIZED         = (1 << 0),
};

#define AV_IS_INPUT_DEVICE(category) \
    (((category) == AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT) || \
     ((category) == AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT) || \
     ((category) == AV_CLASS_CATEGORY_DEVICE_INPUT))

#define AV_IS_OUTPUT_DEVICE(category) \
    (((category) == AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT) || \
     ((category) == AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT) || \
     ((category) == AV_CLASS_CATEGORY_DEVICE_OUTPUT))

struct AVOptionRanges;

typedef struct AVClass {

    const char* class_name;

    const char* (*item_name)(void* ctx);

    const struct AVOption *option;

    int version;

    int log_level_offset_offset;

    int parent_log_context_offset;

    AVClassCategory category;

    AVClassCategory (*get_category)(void* ctx);

    int (*query_ranges)(struct AVOptionRanges **, void *obj, const char *key, int flags);

    void* (*child_next)(void *obj, void *prev);

    const struct AVClass* (*child_class_iterate)(void **iter);

    int state_flags_offset;
} AVClass;

#define AV_LOG_QUIET    -8

#define AV_LOG_PANIC     0

#define AV_LOG_FATAL     8

#define AV_LOG_ERROR    16

#define AV_LOG_WARNING  24

#define AV_LOG_INFO     32

#define AV_LOG_VERBOSE  40

#define AV_LOG_DEBUG    48

#define AV_LOG_TRACE    56

#define AV_LOG_MAX_OFFSET (AV_LOG_TRACE - AV_LOG_QUIET)

#define AV_LOG_C(x) ((x) << 8)

void av_log(void *avcl, int level, const char *fmt, ...) av_printf_format(3, 4);

void av_log_once(void* avcl, int initial_level, int subsequent_level, int *state, const char *fmt, ...) av_printf_format(5, 6);

void av_vlog(void *avcl, int level, const char *fmt, va_list vl);

int av_log_get_level(void);

void av_log_set_level(int level);

void av_log_set_callback(void (*callback)(void*, int, const char*, va_list));

void av_log_default_callback(void *avcl, int level, const char *fmt,
                             va_list vl);

const char* av_default_item_name(void* ctx);
AVClassCategory av_default_get_category(void *ptr);

void av_log_format_line(void *ptr, int level, const char *fmt, va_list vl,
                        char *line, int line_size, int *print_prefix);

int av_log_format_line2(void *ptr, int level, const char *fmt, va_list vl,
                        char *line, int line_size, int *print_prefix);

#define AV_LOG_SKIP_REPEATED 1

#define AV_LOG_PRINT_LEVEL 2

#define AV_LOG_PRINT_TIME 4

#define AV_LOG_PRINT_DATETIME 8

void av_log_set_flags(int arg);
int av_log_get_flags(void);

#endif
