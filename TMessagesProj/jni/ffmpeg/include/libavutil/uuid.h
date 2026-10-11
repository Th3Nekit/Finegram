/*
 * Copyright (c) 2022 Pierre-Anthony Lemieux <pal@palemieux.com>
 *                    Zane van Iperen <zane@zanevaniperen.com>
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
 * UUID parsing and serialization utilities.
 * The library treats the UUID as an opaque sequence of 16 unsigned bytes,
 * i.e. ignoring the internal layout of the UUID, which depends on the type
 * of the UUID.
 *
 * @author Pierre-Anthony Lemieux <pal@palemieux.com>
 * @author Zane van Iperen <zane@zanevaniperen.com>
 */

#ifndef AVUTIL_UUID_H
#define AVUTIL_UUID_H

#include <stdint.h>
#include <string.h>

#define AV_PRI_UUID                          \
    "%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-" \
    "%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"

#define AV_PRI_URN_UUID                               \
    "urn:uuid:%02hhx%02hhx%02hhx%02hhx-%02hhx%02hhx-" \
    "%02hhx%02hhx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"

#define AV_UUID_ARG(x)                  \
    (x)[ 0], (x)[ 1], (x)[ 2], (x)[ 3], \
    (x)[ 4], (x)[ 5], (x)[ 6], (x)[ 7], \
    (x)[ 8], (x)[ 9], (x)[10], (x)[11], \
    (x)[12], (x)[13], (x)[14], (x)[15]

#define AV_UUID_LEN 16

typedef uint8_t AVUUID[AV_UUID_LEN];

int av_uuid_parse(const char *in, AVUUID uu);

int av_uuid_urn_parse(const char *in, AVUUID uu);

int av_uuid_parse_range(const char *in_start, const char *in_end, AVUUID uu);

void av_uuid_unparse(const AVUUID uu, char *out);

static inline int av_uuid_equal(const AVUUID uu1, const AVUUID uu2)
{
    return memcmp(uu1, uu2, AV_UUID_LEN) == 0;
}

static inline void av_uuid_copy(AVUUID dest, const AVUUID src)
{
    memcpy(dest, src, AV_UUID_LEN);
}

static inline void av_uuid_nil(AVUUID uu)
{
    memset(uu, 0, AV_UUID_LEN);
}

#endif
