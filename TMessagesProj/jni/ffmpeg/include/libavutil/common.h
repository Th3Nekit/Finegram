/*
 * copyright (c) 2006 Michael Niedermayer <michaelni@gmx.at>
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
 * common internal and external API header
 */

#ifndef AVUTIL_COMMON_H
#define AVUTIL_COMMON_H

#if defined(__cplusplus) && !defined(__STDC_CONSTANT_MACROS) && !defined(UINT64_C)
#error missing -D__STDC_CONSTANT_MACROS / #define __STDC_CONSTANT_MACROS
#endif

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "attributes.h"
#include "error.h"
#include "macros.h"
#include "version.h"

#ifdef HAVE_AV_CONFIG_H
#   include "config.h"
#   include "intmath.h"
#   include "internal.h"
#else
#   include "mem.h"
#endif

#define RSHIFT(a,b) ((a) > 0 ? ((a) + ((1<<(b))>>1))>>(b) : ((a) + ((1<<(b))>>1)-1)>>(b))

#define ROUNDED_DIV(a,b) (((a)>=0 ? (a) + ((b)>>1) : (a) - ((b)>>1))/(b))

#define AV_CEIL_RSHIFT(a,b) (!av_builtin_constant_p(b) ? -((-(a)) >> (b)) \
                                                       : ((a) + (1<<(b)) - 1) >> (b))

#define FF_CEIL_RSHIFT AV_CEIL_RSHIFT

#define FFUDIV(a,b) (((a)>0 ?(a):(a)-(b)+1) / (b))
#define FFUMOD(a,b) ((a)-(b)*FFUDIV(a,b))

#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))
#define FFSIGN(a) ((a) > 0 ? 1 : -1)

#define FFNABS(a) ((a) <= 0 ? (a) : (-(a)))

#define FFABSU(a) ((a) <= 0 ? -(unsigned)(a) : (unsigned)(a))
#define FFABS64U(a) ((a) <= 0 ? -(uint64_t)(a) : (uint64_t)(a))

#ifndef av_ceil_log2
#   define av_ceil_log2     av_ceil_log2_c
#endif
#ifndef av_clip
#   define av_clip          av_clip_c
#endif
#ifndef av_clip64
#   define av_clip64        av_clip64_c
#endif
#ifndef av_clip_uint8
#   define av_clip_uint8    av_clip_uint8_c
#endif
#ifndef av_clip_int8
#   define av_clip_int8     av_clip_int8_c
#endif
#ifndef av_clip_uint16
#   define av_clip_uint16   av_clip_uint16_c
#endif
#ifndef av_clip_int16
#   define av_clip_int16    av_clip_int16_c
#endif
#ifndef av_clipl_int32
#   define av_clipl_int32   av_clipl_int32_c
#endif
#ifndef av_clip_intp2
#   define av_clip_intp2    av_clip_intp2_c
#endif
#ifndef av_clip_uintp2
#   define av_clip_uintp2   av_clip_uintp2_c
#endif
#ifndef av_sat_add32
#   define av_sat_add32     av_sat_add32_c
#endif
#ifndef av_sat_dadd32
#   define av_sat_dadd32    av_sat_dadd32_c
#endif
#ifndef av_sat_sub32
#   define av_sat_sub32     av_sat_sub32_c
#endif
#ifndef av_sat_dsub32
#   define av_sat_dsub32    av_sat_dsub32_c
#endif
#ifndef av_sat_add64
#   define av_sat_add64     av_sat_add64_c
#endif
#ifndef av_sat_sub64
#   define av_sat_sub64     av_sat_sub64_c
#endif
#ifndef av_clipf
#   define av_clipf         av_clipf_c
#endif
#ifndef av_clipd
#   define av_clipd         av_clipd_c
#endif
#ifndef av_zero_extend
#   define av_zero_extend   av_zero_extend_c
#endif
#ifndef av_popcount
#   define av_popcount      av_popcount_c
#endif
#ifndef av_popcount64
#   define av_popcount64    av_popcount64_c
#endif
#ifndef av_parity
#   define av_parity        av_parity_c
#endif

#ifndef av_log2
av_const int av_log2(unsigned v);
#endif

#ifndef av_log2_16bit
av_const int av_log2_16bit(unsigned v);
#endif

static av_always_inline av_const int av_clip_c(int a, int amin, int amax)
{
#if defined(HAVE_AV_CONFIG_H) && defined(ASSERT_LEVEL) && ASSERT_LEVEL >= 2
    if (amin > amax) abort();
#endif
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}

static av_always_inline av_const int64_t av_clip64_c(int64_t a, int64_t amin, int64_t amax)
{
#if defined(HAVE_AV_CONFIG_H) && defined(ASSERT_LEVEL) && ASSERT_LEVEL >= 2
    if (amin > amax) abort();
#endif
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}

static av_always_inline av_const uint8_t av_clip_uint8_c(int a)
{
    if (a&(~0xFF)) return (~a)>>31;
    else           return a;
}

static av_always_inline av_const int8_t av_clip_int8_c(int a)
{
    if ((a+0x80U) & ~0xFF) return (a>>31) ^ 0x7F;
    else                  return a;
}

static av_always_inline av_const uint16_t av_clip_uint16_c(int a)
{
    if (a&(~0xFFFF)) return (~a)>>31;
    else             return a;
}

static av_always_inline av_const int16_t av_clip_int16_c(int a)
{
    if ((a+0x8000U) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
    else                      return a;
}

static av_always_inline av_const int32_t av_clipl_int32_c(int64_t a)
{
    if ((a+UINT64_C(0x80000000)) & ~UINT64_C(0xFFFFFFFF)) return (int32_t)((a>>63) ^ 0x7FFFFFFF);
    else                                                  return (int32_t)a;
}

static av_always_inline av_const int av_clip_intp2_c(int a, int p)
{
    if (((unsigned)a + (1U << p)) & ~((2U << p) - 1))
        return (a >> 31) ^ ((1 << p) - 1);
    else
        return a;
}

static av_always_inline av_const unsigned av_clip_uintp2_c(int a, int p)
{
    if (a & ~((1U<<p) - 1)) return (~a) >> 31 & ((1U<<p) - 1);
    else                    return  a;
}

static av_always_inline av_const unsigned av_zero_extend_c(unsigned a, unsigned p)
{
#if defined(HAVE_AV_CONFIG_H) && defined(ASSERT_LEVEL) && ASSERT_LEVEL >= 2
    if (p > 31) abort();
#endif
    return a & ((1U << p) - 1);
}

#if FF_API_MOD_UINTP2
#ifndef av_mod_uintp2
#   define av_mod_uintp2 av_mod_uintp2_c
#endif
attribute_deprecated
static av_always_inline av_const unsigned av_mod_uintp2_c(unsigned a, unsigned p)
{
    return av_zero_extend_c(a, p);
}
#endif

static av_always_inline int av_sat_add32_c(int a, int b)
{
    return av_clipl_int32((int64_t)a + b);
}

static av_always_inline int av_sat_dadd32_c(int a, int b)
{
    return av_sat_add32(a, av_sat_add32(b, b));
}

static av_always_inline int av_sat_sub32_c(int a, int b)
{
    return av_clipl_int32((int64_t)a - b);
}

static av_always_inline int av_sat_dsub32_c(int a, int b)
{
    return av_sat_sub32(a, av_sat_add32(b, b));
}

static av_always_inline int64_t av_sat_add64_c(int64_t a, int64_t b) {
#if (!defined(__INTEL_COMPILER) && AV_GCC_VERSION_AT_LEAST(5,1)) || AV_HAS_BUILTIN(__builtin_add_overflow)
    int64_t tmp;
    return !__builtin_add_overflow(a, b, &tmp) ? tmp : (tmp < 0 ? INT64_MAX : INT64_MIN);
#else
    int64_t s = a+(uint64_t)b;
    if ((int64_t)(a^b | ~s^b) >= 0)
        return INT64_MAX ^ (b >> 63);
    return s;
#endif
}

static av_always_inline int64_t av_sat_sub64_c(int64_t a, int64_t b) {
#if (!defined(__INTEL_COMPILER) && AV_GCC_VERSION_AT_LEAST(5,1)) || AV_HAS_BUILTIN(__builtin_sub_overflow)
    int64_t tmp;
    return !__builtin_sub_overflow(a, b, &tmp) ? tmp : (tmp < 0 ? INT64_MAX : INT64_MIN);
#else
    if (b <= 0 && a >= INT64_MAX + b)
        return INT64_MAX;
    if (b >= 0 && a <= INT64_MIN + b)
        return INT64_MIN;
    return a - b;
#endif
}

static av_always_inline av_const float av_clipf_c(float a, float amin, float amax)
{
#if defined(HAVE_AV_CONFIG_H) && defined(ASSERT_LEVEL) && ASSERT_LEVEL >= 2
    if (amin > amax) abort();
#endif
    return FFMIN(FFMAX(a, amin), amax);
}

static av_always_inline av_const double av_clipd_c(double a, double amin, double amax)
{
#if defined(HAVE_AV_CONFIG_H) && defined(ASSERT_LEVEL) && ASSERT_LEVEL >= 2
    if (amin > amax) abort();
#endif
    return FFMIN(FFMAX(a, amin), amax);
}

static av_always_inline av_const int av_ceil_log2_c(int x)
{
    return av_log2((x - 1U) << 1);
}

static av_always_inline av_const int av_popcount_c(uint32_t x)
{
    x -= (x >> 1) & 0x55555555;
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x += x >> 8;
    return (x + (x >> 16)) & 0x3F;
}

static av_always_inline av_const int av_popcount64_c(uint64_t x)
{
    return av_popcount((uint32_t)x) + av_popcount((uint32_t)(x >> 32));
}

static av_always_inline av_const int av_parity_c(uint32_t v)
{
    return av_popcount(v) & 1;
}

#define GET_UTF8(val, GET_BYTE, ERROR)\
    val= (uint8_t)(GET_BYTE);\
    {\
        uint32_t top = (val & 128) >> 1;\
        if ((val & 0xc0) == 0x80 || val >= 0xFE)\
            {ERROR}\
        while (val & top) {\
            unsigned int tmp = (uint8_t)(GET_BYTE) - 128;\
            if(tmp>>6)\
                {ERROR}\
            val= (val<<6) + tmp;\
            top <<= 5;\
        }\
        val &= (top << 1) - 1;\
    }

#define GET_UTF16(val, GET_16BIT, ERROR)\
    val = (uint16_t)(GET_16BIT);\
    {\
        unsigned int hi = val - 0xD800;\
        if (hi < 0x800) {\
            val = (uint16_t)(GET_16BIT) - 0xDC00;\
            if (val > 0x3FFU || hi > 0x3FFU)\
                {ERROR}\
            val += (hi<<10) + 0x10000;\
        }\
    }\


#define PUT_UTF8(val, tmp, PUT_BYTE)\
    {\
        int bytes, shift;\
        uint32_t in = val;\
        if (in < 0x80) {\
            tmp = in;\
            PUT_BYTE\
        } else {\
            bytes = (av_log2(in) + 4) / 5;\
            shift = (bytes - 1) * 6;\
            tmp = (256 - (256 >> bytes)) | (in >> shift);\
            PUT_BYTE\
            while (shift >= 6) {\
                shift -= 6;\
                tmp = 0x80 | ((in >> shift) & 0x3f);\
                PUT_BYTE\
            }\
        }\
    }

#define PUT_UTF16(val, tmp, PUT_16BIT)\
    {\
        uint32_t in = val;\
        if (in < 0x10000) {\
            tmp = in;\
            PUT_16BIT\
        } else {\
            tmp = 0xD800 | ((in - 0x10000) >> 10);\
            PUT_16BIT\
            tmp = 0xDC00 | ((in - 0x10000) & 0x3FF);\
            PUT_16BIT\
        }\
    }\

#endif
