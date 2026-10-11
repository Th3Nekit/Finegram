/*
 * Copyright (c) 2016 Neil Birkbeck <neil.birkbeck@gmail.com>
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

#ifndef AVUTIL_MASTERING_DISPLAY_METADATA_H
#define AVUTIL_MASTERING_DISPLAY_METADATA_H

#include "frame.h"
#include "rational.h"

typedef struct AVMasteringDisplayMetadata {

    AVRational display_primaries[3][2];

    AVRational white_point[2];

    AVRational min_luminance;

    AVRational max_luminance;

    int has_primaries;

    int has_luminance;

} AVMasteringDisplayMetadata;

AVMasteringDisplayMetadata *av_mastering_display_metadata_alloc(void);

AVMasteringDisplayMetadata *av_mastering_display_metadata_alloc_size(size_t *size);

AVMasteringDisplayMetadata *av_mastering_display_metadata_create_side_data(AVFrame *frame);

typedef struct AVContentLightMetadata {

    unsigned MaxCLL;

    unsigned MaxFALL;
} AVContentLightMetadata;

AVContentLightMetadata *av_content_light_metadata_alloc(size_t *size);

AVContentLightMetadata *av_content_light_metadata_create_side_data(AVFrame *frame);

#endif
