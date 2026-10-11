/*
 * Copyright (C) 2007 Marco Gerards <marco@gnu.org>
 * Copyright (C) 2009 David Conrad
 * Copyright (C) 2011 Jordi Ortiz
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

#ifndef AVCODEC_DIRAC_H
#define AVCODEC_DIRAC_H

#include <stddef.h>
#include <stdint.h>

#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"

#define MAX_DWT_LEVELS 5

enum DiracParseCodes {
    DIRAC_PCODE_SEQ_HEADER      = 0x00,
    DIRAC_PCODE_END_SEQ         = 0x10,
    DIRAC_PCODE_AUX             = 0x20,
    DIRAC_PCODE_PAD             = 0x30,
    DIRAC_PCODE_PICTURE_CODED   = 0x08,
    DIRAC_PCODE_PICTURE_RAW     = 0x48,
    DIRAC_PCODE_PICTURE_LOW_DEL = 0xC8,
    DIRAC_PCODE_PICTURE_HQ      = 0xE8,
    DIRAC_PCODE_INTER_NOREF_CO1 = 0x0A,
    DIRAC_PCODE_INTER_NOREF_CO2 = 0x09,
    DIRAC_PCODE_INTER_REF_CO1   = 0x0D,
    DIRAC_PCODE_INTER_REF_CO2   = 0x0E,
    DIRAC_PCODE_INTRA_REF_CO    = 0x0C,
    DIRAC_PCODE_INTRA_REF_RAW   = 0x4C,
    DIRAC_PCODE_INTRA_REF_PICT  = 0xCC,
    DIRAC_PCODE_MAGIC           = 0x42424344,
};

typedef struct DiracVersionInfo {
    int major;
    int minor;
} DiracVersionInfo;

typedef struct AVDiracSeqHeader {
    unsigned width;
    unsigned height;
    uint8_t chroma_format;

    uint8_t interlaced;
    uint8_t top_field_first;

    uint8_t frame_rate_index;
    uint8_t aspect_ratio_index;

    uint16_t clean_width;
    uint16_t clean_height;
    uint16_t clean_left_offset;
    uint16_t clean_right_offset;

    uint8_t pixel_range_index;
    uint8_t color_spec_index;

    int profile;
    int level;

    AVRational framerate;
    AVRational sample_aspect_ratio;

    enum AVPixelFormat pix_fmt;
    enum AVColorRange color_range;
    enum AVColorPrimaries color_primaries;
    enum AVColorTransferCharacteristic color_trc;
    enum AVColorSpace colorspace;

    DiracVersionInfo version;
    int bit_depth;
} AVDiracSeqHeader;

int av_dirac_parse_sequence_header(AVDiracSeqHeader **dsh,
                                   const uint8_t *buf, size_t buf_size,
                                   void *log_ctx);

#endif
