/*
 * Copyright (C) 2007 Michael Niedermayer <michaelni@gmx.at>
 * Copyright (C) 2013 James Almer <jamrial@gmail.com>
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
 * @ingroup lavu_sha512
 * Public header for SHA-512 implementation.
 */

#ifndef AVUTIL_SHA512_H
#define AVUTIL_SHA512_H

#include <stddef.h>
#include <stdint.h>

#include "attributes.h"

extern const int av_sha512_size;

struct AVSHA512;

struct AVSHA512 *av_sha512_alloc(void);

int av_sha512_init(struct AVSHA512* context, int bits);

void av_sha512_update(struct AVSHA512* context, const uint8_t* data, size_t len);

void av_sha512_final(struct AVSHA512* context, uint8_t *digest);

#endif
