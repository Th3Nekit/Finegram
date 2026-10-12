/*
 * copyright (c) 2005-2012 Michael Niedermayer <michaelni@gmx.at>
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
 * @addtogroup lavu_math
 * Mathematical utilities for working with timestamp and time base.
 */

#ifndef AVUTIL_MATHEMATICS_H
#define AVUTIL_MATHEMATICS_H

#include <stdint.h>
#include <math.h>
#include "attributes.h"
#include "rational.h"
#include "intfloat.h"

#ifndef M_E
#define M_E            2.7182818284590452354
#endif
#ifndef M_Ef
#define M_Ef           2.7182818284590452354f
#endif
#ifndef M_LN2
#define M_LN2          0.69314718055994530942
#endif
#ifndef M_LN2f
#define M_LN2f         0.69314718055994530942f
#endif
#ifndef M_LN10
#define M_LN10         2.30258509299404568402
#endif
#ifndef M_LN10f
#define M_LN10f        2.30258509299404568402f
#endif
#ifndef M_LOG2_10
#define M_LOG2_10      3.32192809488736234787
#endif
#ifndef M_LOG2_10f
#define M_LOG2_10f     3.32192809488736234787f
#endif
#ifndef M_PHI
#define M_PHI          1.61803398874989484820
#endif
#ifndef M_PHIf
#define M_PHIf         1.61803398874989484820f
#endif
#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif
#ifndef M_PIf
#define M_PIf          3.14159265358979323846f
#endif
#ifndef M_PI_2
#define M_PI_2         1.57079632679489661923
#endif
#ifndef M_PI_2f
#define M_PI_2f        1.57079632679489661923f
#endif
#ifndef M_PI_4
#define M_PI_4         0.78539816339744830962
#endif
#ifndef M_PI_4f
#define M_PI_4f        0.78539816339744830962f
#endif
#ifndef M_1_PI
#define M_1_PI         0.31830988618379067154
#endif
#ifndef M_1_PIf
#define M_1_PIf        0.31830988618379067154f
#endif
#ifndef M_2_PI
#define M_2_PI         0.63661977236758134308
#endif
#ifndef M_2_PIf
#define M_2_PIf        0.63661977236758134308f
#endif
#ifndef M_2_SQRTPI
#define M_2_SQRTPI     1.12837916709551257390
#endif
#ifndef M_2_SQRTPIf
#define M_2_SQRTPIf    1.12837916709551257390f
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2      0.70710678118654752440
#endif
#ifndef M_SQRT1_2f
#define M_SQRT1_2f     0.70710678118654752440f
#endif
#ifndef M_SQRT2
#define M_SQRT2        1.41421356237309504880
#endif
#ifndef M_SQRT2f
#define M_SQRT2f       1.41421356237309504880f
#endif
#ifndef NAN
#define NAN            av_int2float(0x7fc00000)
#endif
#ifndef INFINITY
#define INFINITY       av_int2float(0x7f800000)
#endif

enum AVRounding {
    AV_ROUND_ZERO     = 0,
    AV_ROUND_INF      = 1,
    AV_ROUND_DOWN     = 2,
    AV_ROUND_UP       = 3,
    AV_ROUND_NEAR_INF = 5,

    AV_ROUND_PASS_MINMAX = 8192,
};

int64_t av_const av_gcd(int64_t a, int64_t b);

int64_t av_rescale(int64_t a, int64_t b, int64_t c) av_const;

int64_t av_rescale_rnd(int64_t a, int64_t b, int64_t c, enum AVRounding rnd) av_const;

int64_t av_rescale_q(int64_t a, AVRational bq, AVRational cq) av_const;

int64_t av_rescale_q_rnd(int64_t a, AVRational bq, AVRational cq,
                         enum AVRounding rnd) av_const;

int av_compare_ts(int64_t ts_a, AVRational tb_a, int64_t ts_b, AVRational tb_b);

int64_t av_compare_mod(uint64_t a, uint64_t b, uint64_t mod);

int64_t av_rescale_delta(AVRational in_tb, int64_t in_ts,  AVRational fs_tb, int duration, int64_t *last, AVRational out_tb);

int64_t av_add_stable(AVRational ts_tb, int64_t ts, AVRational inc_tb, int64_t inc);

double av_bessel_i0(double x);

#endif
