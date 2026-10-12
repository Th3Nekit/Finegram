/*
 * An implementation of the CAST128 algorithm as mentioned in RFC2144
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

#ifndef AVUTIL_CAST5_H
#define AVUTIL_CAST5_H

#include <stdint.h>

extern const int av_cast5_size;

struct AVCAST5;

struct AVCAST5 *av_cast5_alloc(void);

int av_cast5_init(struct AVCAST5 *ctx, const uint8_t *key, int key_bits);

void av_cast5_crypt(struct AVCAST5 *ctx, uint8_t *dst, const uint8_t *src, int count, int decrypt);

void av_cast5_crypt2(struct AVCAST5 *ctx, uint8_t *dst, const uint8_t *src, int count, uint8_t *iv, int decrypt);

#endif
