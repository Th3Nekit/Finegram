/*
 * pixel format descriptor
 * Copyright (c) 2009 Michael Niedermayer <michaelni@gmx.at>
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

#ifndef AVUTIL_PIXDESC_H
#define AVUTIL_PIXDESC_H

#include <inttypes.h>

#include "attributes.h"
#include "pixfmt.h"

typedef struct AVComponentDescriptor {

    int plane;

    int step;

    int offset;

    int shift;

    int depth;
} AVComponentDescriptor;

typedef struct AVPixFmtDescriptor {
    const char *name;
    uint8_t nb_components;

    uint8_t log2_chroma_w;

    uint8_t log2_chroma_h;

    uint64_t flags;

    AVComponentDescriptor comp[4];

    const char *alias;
} AVPixFmtDescriptor;

#define AV_PIX_FMT_FLAG_BE           (1 << 0)

#define AV_PIX_FMT_FLAG_PAL          (1 << 1)

#define AV_PIX_FMT_FLAG_BITSTREAM    (1 << 2)

#define AV_PIX_FMT_FLAG_HWACCEL      (1 << 3)

#define AV_PIX_FMT_FLAG_PLANAR       (1 << 4)

#define AV_PIX_FMT_FLAG_RGB          (1 << 5)

#define AV_PIX_FMT_FLAG_ALPHA        (1 << 7)

#define AV_PIX_FMT_FLAG_BAYER        (1 << 8)

#define AV_PIX_FMT_FLAG_FLOAT        (1 << 9)

#define AV_PIX_FMT_FLAG_XYZ          (1 << 10)

int av_get_bits_per_pixel(const AVPixFmtDescriptor *pixdesc);

int av_get_padded_bits_per_pixel(const AVPixFmtDescriptor *pixdesc);

const AVPixFmtDescriptor *av_pix_fmt_desc_get(enum AVPixelFormat pix_fmt);

const AVPixFmtDescriptor *av_pix_fmt_desc_next(const AVPixFmtDescriptor *prev);

enum AVPixelFormat av_pix_fmt_desc_get_id(const AVPixFmtDescriptor *desc);

int av_pix_fmt_get_chroma_sub_sample(enum AVPixelFormat pix_fmt,
                                     int *h_shift, int *v_shift);

int av_pix_fmt_count_planes(enum AVPixelFormat pix_fmt);

const char *av_color_range_name(enum AVColorRange range);

int av_color_range_from_name(const char *name);

const char *av_color_primaries_name(enum AVColorPrimaries primaries);

int av_color_primaries_from_name(const char *name);

const char *av_color_transfer_name(enum AVColorTransferCharacteristic transfer);

int av_color_transfer_from_name(const char *name);

const char *av_color_space_name(enum AVColorSpace space);

int av_color_space_from_name(const char *name);

const char *av_chroma_location_name(enum AVChromaLocation location);

int av_chroma_location_from_name(const char *name);

int av_chroma_location_enum_to_pos(int *xpos, int *ypos, enum AVChromaLocation pos);

enum AVChromaLocation av_chroma_location_pos_to_enum(int xpos, int ypos);

const char *av_alpha_mode_name(enum AVAlphaMode mode);

enum AVAlphaMode av_alpha_mode_from_name(const char *name);

enum AVPixelFormat av_get_pix_fmt(const char *name);

const char *av_get_pix_fmt_name(enum AVPixelFormat pix_fmt);

char *av_get_pix_fmt_string(char *buf, int buf_size,
                            enum AVPixelFormat pix_fmt);

void av_read_image_line2(void *dst, const uint8_t *data[4],
                        const int linesize[4], const AVPixFmtDescriptor *desc,
                        int x, int y, int c, int w, int read_pal_component,
                        int dst_element_size);

void av_read_image_line(uint16_t *dst, const uint8_t *data[4],
                        const int linesize[4], const AVPixFmtDescriptor *desc,
                        int x, int y, int c, int w, int read_pal_component);

void av_write_image_line2(const void *src, uint8_t *data[4],
                         const int linesize[4], const AVPixFmtDescriptor *desc,
                         int x, int y, int c, int w, int src_element_size);

void av_write_image_line(const uint16_t *src, uint8_t *data[4],
                         const int linesize[4], const AVPixFmtDescriptor *desc,
                         int x, int y, int c, int w);

enum AVPixelFormat av_pix_fmt_swap_endianness(enum AVPixelFormat pix_fmt);

#define FF_LOSS_RESOLUTION        0x0001
#define FF_LOSS_DEPTH             0x0002
#define FF_LOSS_COLORSPACE        0x0004
#define FF_LOSS_ALPHA             0x0008
#define FF_LOSS_COLORQUANT        0x0010
#define FF_LOSS_CHROMA            0x0020
#define FF_LOSS_EXCESS_RESOLUTION 0x0040
#define FF_LOSS_EXCESS_DEPTH      0x0080

int av_get_pix_fmt_loss(enum AVPixelFormat dst_pix_fmt,
                        enum AVPixelFormat src_pix_fmt,
                        int has_alpha);

enum AVPixelFormat av_find_best_pix_fmt_of_2(enum AVPixelFormat dst_pix_fmt1, enum AVPixelFormat dst_pix_fmt2,
                                             enum AVPixelFormat src_pix_fmt, int has_alpha, int *loss_ptr);

#endif
