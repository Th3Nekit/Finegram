/*
 * Copyright (C) 2024 Niklas Haas
 * Copyright (C) 2001-2011 Michael Niedermayer <michaelni@gmx.at>
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

#ifndef SWSCALE_SWSCALE_H
#define SWSCALE_SWSCALE_H

#include <stdint.h>

#include "libavutil/avutil.h"
#include "libavutil/frame.h"
#include "libavutil/log.h"
#include "libavutil/pixfmt.h"
#include "version_major.h"
#ifndef HAVE_AV_CONFIG_H

#include "version.h"
#endif

unsigned swscale_version(void);

const char *swscale_configuration(void);

const char *swscale_license(void);

const AVClass *sws_get_class(void);

typedef enum SwsDither {
    SWS_DITHER_NONE = 0,
    SWS_DITHER_AUTO,
    SWS_DITHER_BAYER,
    SWS_DITHER_ED,
    SWS_DITHER_A_DITHER,
    SWS_DITHER_X_DITHER,
    SWS_DITHER_NB,
    SWS_DITHER_MAX_ENUM = 0x7FFFFFFF,
} SwsDither;

typedef enum SwsAlphaBlend {
    SWS_ALPHA_BLEND_NONE = 0,
    SWS_ALPHA_BLEND_UNIFORM,
    SWS_ALPHA_BLEND_CHECKERBOARD,
    SWS_ALPHA_BLEND_NB,
    SWS_ALPHA_BLEND_MAX_ENUM = 0x7FFFFFFF,
} SwsAlphaBlend;

typedef enum SwsFlags {

    SWS_FAST_BILINEAR = 1 <<  0,
    SWS_BILINEAR      = 1 <<  1,
    SWS_BICUBIC       = 1 <<  2,
    SWS_X             = 1 <<  3,
    SWS_POINT         = 1 <<  4,
    SWS_AREA          = 1 <<  5,
    SWS_BICUBLIN      = 1 <<  6,
    SWS_GAUSS         = 1 <<  7,
    SWS_SINC          = 1 <<  8,
    SWS_LANCZOS       = 1 <<  9,
    SWS_SPLINE        = 1 << 10,

    SWS_STRICT        = 1 << 11,

    SWS_PRINT_INFO    = 1 << 12,

    SWS_FULL_CHR_H_INT = 1 << 13,

    SWS_FULL_CHR_H_INP = 1 << 14,

    SWS_ACCURATE_RND   = 1 << 18,
    SWS_BITEXACT       = 1 << 19,

    SWS_UNSTABLE = 1 << 20,

    SWS_DIRECT_BGR      = 1 << 15,
    SWS_ERROR_DIFFUSION = 1 << 23,
} SwsFlags;

typedef enum SwsIntent {
    SWS_INTENT_PERCEPTUAL = 0,
    SWS_INTENT_RELATIVE_COLORIMETRIC = 1,
    SWS_INTENT_SATURATION = 2,
    SWS_INTENT_ABSOLUTE_COLORIMETRIC = 3,
    SWS_INTENT_NB,
} SwsIntent;

typedef struct SwsContext {
    const AVClass *av_class;

    void *opaque;

    unsigned flags;

    double scaler_params[2];

    int threads;

    SwsDither dither;

    SwsAlphaBlend alpha_blend;

    int gamma_flag;

    int src_w, src_h;
    int dst_w, dst_h;
    int src_format;
    int dst_format;
    int src_range;
    int dst_range;
    int src_v_chr_pos;
    int src_h_chr_pos;
    int dst_v_chr_pos;
    int dst_h_chr_pos;

    int intent;

} SwsContext;

SwsContext *sws_alloc_context(void);

void sws_free_context(SwsContext **ctx);

int sws_test_format(enum AVPixelFormat format, int output);

int sws_test_hw_format(enum AVPixelFormat format);

int sws_test_colorspace(enum AVColorSpace colorspace, int output);

int sws_test_primaries(enum AVColorPrimaries primaries, int output);

int sws_test_transfer(enum AVColorTransferCharacteristic trc, int output);

int sws_test_frame(const AVFrame *frame, int output);

int sws_frame_setup(SwsContext *ctx, const AVFrame *dst, const AVFrame *src);

int sws_is_noop(const AVFrame *dst, const AVFrame *src);

int sws_scale_frame(SwsContext *c, AVFrame *dst, const AVFrame *src);

#define SWS_MAX_REDUCE_CUTOFF 0.002

#define SWS_SRC_V_CHR_DROP_MASK     0x30000
#define SWS_SRC_V_CHR_DROP_SHIFT    16

#define SWS_PARAM_DEFAULT           123456

#define SWS_CS_ITU709         1
#define SWS_CS_FCC            4
#define SWS_CS_ITU601         5
#define SWS_CS_ITU624         5
#define SWS_CS_SMPTE170M      5
#define SWS_CS_SMPTE240M      7
#define SWS_CS_DEFAULT        5
#define SWS_CS_BT2020         9

const int *sws_getCoefficients(int colorspace);

typedef struct SwsVector {
    double *coeff;
    int length;
} SwsVector;

typedef struct SwsFilter {
    SwsVector *lumH;
    SwsVector *lumV;
    SwsVector *chrH;
    SwsVector *chrV;
} SwsFilter;

int sws_isSupportedInput(enum AVPixelFormat pix_fmt);

int sws_isSupportedOutput(enum AVPixelFormat pix_fmt);

int sws_isSupportedEndiannessConversion(enum AVPixelFormat pix_fmt);

av_warn_unused_result
int sws_init_context(SwsContext *sws_context, SwsFilter *srcFilter, SwsFilter *dstFilter);

void sws_freeContext(SwsContext *swsContext);

SwsContext *sws_getContext(int srcW, int srcH, enum AVPixelFormat srcFormat,
                           int dstW, int dstH, enum AVPixelFormat dstFormat,
                           int flags, SwsFilter *srcFilter,
                           SwsFilter *dstFilter, const double *param);

int sws_scale(SwsContext *c, const uint8_t *const srcSlice[],
              const int srcStride[], int srcSliceY, int srcSliceH,
              uint8_t *const dst[], const int dstStride[]);

int sws_frame_start(SwsContext *c, AVFrame *dst, const AVFrame *src);

void sws_frame_end(SwsContext *c);

int sws_send_slice(SwsContext *c, unsigned int slice_start,
                   unsigned int slice_height);

int sws_receive_slice(SwsContext *c, unsigned int slice_start,
                      unsigned int slice_height);

unsigned int sws_receive_slice_alignment(const SwsContext *c);

int sws_setColorspaceDetails(SwsContext *c, const int inv_table[4],
                             int srcRange, const int table[4], int dstRange,
                             int brightness, int contrast, int saturation);

int sws_getColorspaceDetails(SwsContext *c, int **inv_table,
                             int *srcRange, int **table, int *dstRange,
                             int *brightness, int *contrast, int *saturation);

SwsVector *sws_allocVec(int length);

SwsVector *sws_getGaussianVec(double variance, double quality);

void sws_scaleVec(SwsVector *a, double scalar);

void sws_normalizeVec(SwsVector *a, double height);

void sws_freeVec(SwsVector *a);

SwsFilter *sws_getDefaultFilter(float lumaGBlur, float chromaGBlur,
                                float lumaSharpen, float chromaSharpen,
                                float chromaHShift, float chromaVShift,
                                int verbose);
void sws_freeFilter(SwsFilter *filter);

SwsContext *sws_getCachedContext(SwsContext *context, int srcW, int srcH,
                                 enum AVPixelFormat srcFormat, int dstW, int dstH,
                                 enum AVPixelFormat dstFormat, int flags,
                                 SwsFilter *srcFilter, SwsFilter *dstFilter,
                                 const double *param);

void sws_convertPalette8ToPacked32(const uint8_t *src, uint8_t *dst, int num_pixels, const uint8_t *palette);

void sws_convertPalette8ToPacked24(const uint8_t *src, uint8_t *dst, int num_pixels, const uint8_t *palette);

#endif
