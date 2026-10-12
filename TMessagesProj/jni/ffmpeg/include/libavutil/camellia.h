/*
 * An implementation of the CAMELLIA algorithm as mentioned in RFC3713
 * Copyright (c) 2014 Supraja Meedinti
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

#ifndef AVUTIL_CAMELLIA_H
#define AVUTIL_CAMELLIA_H

#include <stdint.h>

extern const int av_camellia_size;

struct AVCAMELLIA;

struct AVCAMELLIA *av_camellia_alloc(void);

int av_camellia_init(struct AVCAMELLIA *ctx, const uint8_t *key, int key_bits);

void av_camellia_crypt(struct AVCAMELLIA *ctx, uint8_t *dst, const uint8_t *src, int count, uint8_t* iv, int decrypt);

#endif
