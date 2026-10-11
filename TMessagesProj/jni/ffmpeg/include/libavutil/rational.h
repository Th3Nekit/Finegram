/*
 * rational numbers
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
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
 * @ingroup lavu_math_rational
 * Utilities for rational number calculation.
 * @author Michael Niedermayer <michaelni@gmx.at>
 */

#ifndef AVUTIL_RATIONAL_H
#define AVUTIL_RATIONAL_H

#include <stdint.h>
#include <limits.h>
#include "attributes.h"

typedef struct AVRational{
    int num;
    int den;
} AVRational;

static inline AVRational av_make_q(int num, int den)
{
    AVRational r = { num, den };
    return r;
}

static inline int av_cmp_q(AVRational a, AVRational b){
    const int64_t tmp= a.num * (int64_t)b.den - b.num * (int64_t)a.den;

    if(tmp) return (int)((tmp ^ a.den ^ b.den)>>63)|1;
    else if(b.den && a.den) return 0;
    else if(a.num && b.num) return (a.num>>31) - (b.num>>31);
    else                    return INT_MIN;
}

static inline double av_q2d(AVRational a){
    return a.num / (double) a.den;
}

int av_reduce(int *dst_num, int *dst_den, int64_t num, int64_t den, int64_t max);

AVRational av_mul_q(AVRational b, AVRational c) av_const;

AVRational av_div_q(AVRational b, AVRational c) av_const;

AVRational av_add_q(AVRational b, AVRational c) av_const;

AVRational av_sub_q(AVRational b, AVRational c) av_const;

static av_always_inline AVRational av_inv_q(AVRational q)
{
    AVRational r = { q.den, q.num };
    return r;
}

AVRational av_d2q(double d, int max) av_const;

int av_nearer_q(AVRational q, AVRational q1, AVRational q2);

int av_find_nearest_q_idx(AVRational q, const AVRational* q_list);

uint32_t av_q2intfloat(AVRational q);

AVRational av_gcd_q(AVRational a, AVRational b, int max_den, AVRational def);

#endif
