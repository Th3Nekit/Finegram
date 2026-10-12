/**
 * Copyright 2023 Elias Carotti <eliascrt at amazon dot it>
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

#ifndef AVUTIL_VIDEO_HINT_H
#define AVUTIL_VIDEO_HINT_H

#include <stddef.h>
#include <stdint.h>
#include "libavutil/avassert.h"
#include "libavutil/frame.h"

typedef struct AVVideoRect {
    uint32_t x, y;
    uint32_t width, height;
} AVVideoRect;

typedef enum AVVideoHintType {

    AV_VIDEO_HINT_TYPE_CONSTANT,

    AV_VIDEO_HINT_TYPE_CHANGED,
} AVVideoHintType;

typedef struct AVVideoHint {

    size_t nb_rects;

    size_t rect_offset;

    size_t rect_size;

    AVVideoHintType type;
} AVVideoHint;

static av_always_inline AVVideoRect *
av_video_hint_rects(const AVVideoHint *hints) {
    return (AVVideoRect *)((uint8_t *)hints + hints->rect_offset);
}

static av_always_inline AVVideoRect *
av_video_hint_get_rect(const AVVideoHint *hints, size_t idx) {
    return (AVVideoRect *)((uint8_t *)hints + hints->rect_offset + idx * hints->rect_size);
}

AVVideoHint *av_video_hint_alloc(size_t nb_rects,
                                 size_t *out_size);

AVVideoHint *av_video_hint_create_side_data(AVFrame *frame,
                                            size_t nb_rects);

#endif
