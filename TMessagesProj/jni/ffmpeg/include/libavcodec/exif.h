/*
 * EXIF metadata parser
 * Copyright (c) 2013 Thilo Borgmann <thilo.borgmann _at_ mail.de>
 * Copyright (c) 2024-2025 Leo Izen <leo.izen@gmail.com>
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
 * EXIF metadata parser
 * @author Thilo Borgmann <thilo.borgmann _at_ mail.de>
 * @author Leo Izen <leo.izen@gmail.com>
 */

#ifndef AVCODEC_EXIF_H
#define AVCODEC_EXIF_H

#include <stddef.h>
#include <stdint.h>

#include "libavutil/buffer.h"
#include "libavutil/dict.h"
#include "libavutil/rational.h"
#include "version_major.h"

enum AVTiffDataType {
    AV_TIFF_BYTE = 1,
    AV_TIFF_STRING,
    AV_TIFF_SHORT,
    AV_TIFF_LONG,
    AV_TIFF_RATIONAL,
    AV_TIFF_SBYTE,
    AV_TIFF_UNDEFINED,
    AV_TIFF_SSHORT,
    AV_TIFF_SLONG,
    AV_TIFF_SRATIONAL,
    AV_TIFF_FLOAT,
    AV_TIFF_DOUBLE,
    AV_TIFF_IFD,
};

enum AVExifHeaderMode {

    AV_EXIF_TIFF_HEADER,

    AV_EXIF_ASSUME_LE,

    AV_EXIF_ASSUME_BE,

    AV_EXIF_T_OFF,

    AV_EXIF_EXIF00,
};

typedef struct AVExifEntry AVExifEntry;

typedef struct AVExifMetadata {

    AVExifEntry *entries;

    unsigned int count;

    unsigned int size;
} AVExifMetadata;

struct AVExifEntry {
    uint16_t id;
    enum AVTiffDataType type;
    uint32_t count;

    uint32_t ifd_offset;
    uint8_t *ifd_lead;

    union {
        void *ptr;
        int64_t *sint;
        uint64_t *uint;
        double *dbl;
        char *str;
        uint8_t *ubytes;
        int8_t *sbytes;
        AVRational *rat;
        AVExifMetadata ifd;
    } value;
};

const char *av_exif_get_tag_name(uint16_t id);

int32_t av_exif_get_tag_id(const char *name);

int av_exif_set_entry(void *logctx, AVExifMetadata *ifd, uint16_t id, enum AVTiffDataType type,
                      uint32_t count, const uint8_t *ifd_lead, uint32_t ifd_offset, const void *value);

#define AV_EXIF_FLAG_RECURSIVE (1 << 0)

int av_exif_get_entry(void *logctx, AVExifMetadata *ifd, uint16_t id, int flags, AVExifEntry **value);

int av_exif_remove_entry(void *logctx, AVExifMetadata *ifd, uint16_t id, int flags);

int av_exif_parse_buffer(void *logctx, const uint8_t *data, size_t size,
                         AVExifMetadata *ifd, enum AVExifHeaderMode header_mode);

int av_exif_write(void *logctx, const AVExifMetadata *ifd, AVBufferRef **buffer, enum AVExifHeaderMode header_mode);

void av_exif_free(AVExifMetadata *ifd);

int av_exif_ifd_to_dict(void *logctx, const AVExifMetadata *ifd, AVDictionary **metadata);

AVExifMetadata *av_exif_clone_ifd(const AVExifMetadata *ifd);

int av_exif_matrix_to_orientation(const int32_t *matrix);

int av_exif_orientation_to_matrix(int32_t *matrix, int orientation);

#endif
