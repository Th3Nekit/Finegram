/*
 * Copyright (C) 2012 Martin Storsjo
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

#ifndef AVUTIL_HMAC_H
#define AVUTIL_HMAC_H

#include <stdint.h>

enum AVHMACType {
    AV_HMAC_MD5,
    AV_HMAC_SHA1,
    AV_HMAC_SHA224,
    AV_HMAC_SHA256,
    AV_HMAC_SHA384,
    AV_HMAC_SHA512,
};

typedef struct AVHMAC AVHMAC;

AVHMAC *av_hmac_alloc(enum AVHMACType type);

void av_hmac_free(AVHMAC *ctx);

void av_hmac_init(AVHMAC *ctx, const uint8_t *key, unsigned int keylen);

void av_hmac_update(AVHMAC *ctx, const uint8_t *data, unsigned int len);

int av_hmac_final(AVHMAC *ctx, uint8_t *out, unsigned int outlen);

int av_hmac_calc(AVHMAC *ctx, const uint8_t *data, unsigned int len,
                 const uint8_t *key, unsigned int keylen,
                 uint8_t *out, unsigned int outlen);

#endif
