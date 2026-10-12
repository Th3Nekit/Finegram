/*
 * Copyright (C) 2007 Michael Niedermayer <michaelni@gmx.at>
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
 * @ingroup lavu_sha
 * Public header for SHA-1 & SHA-256 hash function implementations.
 */

#ifndef AVUTIL_SHA_H
#define AVUTIL_SHA_H

#include <stddef.h>
#include <stdint.h>

#include "attributes.h"

extern const int av_sha_size;

struct AVSHA;

struct AVSHA *av_sha_alloc(void);

int av_sha_init(struct AVSHA* context, int bits);

void av_sha_update(struct AVSHA *ctx, const uint8_t *data, size_t len);

void av_sha_final(struct AVSHA* context, uint8_t *digest);

#endif
