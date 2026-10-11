/*
 * A 32-bit implementation of the XTEA algorithm
 * Copyright (c) 2012 Samuel Pitoiset
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

#ifndef AVUTIL_XTEA_H
#define AVUTIL_XTEA_H

#include <stdint.h>

typedef struct AVXTEA {
    uint32_t key[16];
} AVXTEA;

AVXTEA *av_xtea_alloc(void);

void av_xtea_init(struct AVXTEA *ctx, const uint8_t key[16]);

void av_xtea_le_init(struct AVXTEA *ctx, const uint8_t key[16]);

void av_xtea_crypt(struct AVXTEA *ctx, uint8_t *dst, const uint8_t *src,
                   int count, uint8_t *iv, int decrypt);

void av_xtea_le_crypt(struct AVXTEA *ctx, uint8_t *dst, const uint8_t *src,
                      int count, uint8_t *iv, int decrypt);

#endif
