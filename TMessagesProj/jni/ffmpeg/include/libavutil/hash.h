/*
 * Copyright (C) 2013 Reimar Döffinger <Reimar.Doeffinger@gmx.de>
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
 * @ingroup lavu_hash_generic
 * Generic hashing API
 */

#ifndef AVUTIL_HASH_H
#define AVUTIL_HASH_H

#include <stddef.h>
#include <stdint.h>

struct AVHashContext;

int av_hash_alloc(struct AVHashContext **ctx, const char *name);

const char *av_hash_names(int i);

const char *av_hash_get_name(const struct AVHashContext *ctx);

#define AV_HASH_MAX_SIZE 64

int av_hash_get_size(const struct AVHashContext *ctx);

void av_hash_init(struct AVHashContext *ctx);

void av_hash_update(struct AVHashContext *ctx, const uint8_t *src, size_t len);

void av_hash_final(struct AVHashContext *ctx, uint8_t *dst);

void av_hash_final_bin(struct AVHashContext *ctx, uint8_t *dst, int size);

void av_hash_final_hex(struct AVHashContext *ctx, uint8_t *dst, int size);

void av_hash_final_b64(struct AVHashContext *ctx, uint8_t *dst, int size);

void av_hash_freep(struct AVHashContext **ctx);

#endif
