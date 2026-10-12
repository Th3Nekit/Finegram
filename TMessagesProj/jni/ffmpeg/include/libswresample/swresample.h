/*
 * Copyright (C) 2011-2013 Michael Niedermayer (michaelni@gmx.at)
 *
 * This file is part of libswresample
 *
 * libswresample is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libswresample is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libswresample; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SWRESAMPLE_SWRESAMPLE_H
#define SWRESAMPLE_SWRESAMPLE_H

#include <stdint.h>
#include "libavutil/channel_layout.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"

#include "libswresample/version_major.h"
#ifndef HAVE_AV_CONFIG_H

#include "libswresample/version.h"
#endif

#define SWR_FLAG_RESAMPLE 1 ///< Force resampling even if equal sample rate

enum SwrDitherType {
    SWR_DITHER_NONE = 0,
    SWR_DITHER_RECTANGULAR,
    SWR_DITHER_TRIANGULAR,
    SWR_DITHER_TRIANGULAR_HIGHPASS,

    SWR_DITHER_NS = 64,
    SWR_DITHER_NS_LIPSHITZ,
    SWR_DITHER_NS_F_WEIGHTED,
    SWR_DITHER_NS_MODIFIED_E_WEIGHTED,
    SWR_DITHER_NS_IMPROVED_E_WEIGHTED,
    SWR_DITHER_NS_SHIBATA,
    SWR_DITHER_NS_LOW_SHIBATA,
    SWR_DITHER_NS_HIGH_SHIBATA,
    SWR_DITHER_NB,
};

enum SwrEngine {
    SWR_ENGINE_SWR,
    SWR_ENGINE_SOXR,
    SWR_ENGINE_NB,
};

enum SwrFilterType {
    SWR_FILTER_TYPE_CUBIC,
    SWR_FILTER_TYPE_BLACKMAN_NUTTALL,
    SWR_FILTER_TYPE_KAISER,
};

typedef struct SwrContext SwrContext;

const AVClass *swr_get_class(void);

struct SwrContext *swr_alloc(void);

int swr_init(struct SwrContext *s);

int swr_is_initialized(struct SwrContext *s);

int swr_alloc_set_opts2(struct SwrContext **ps,
                        const AVChannelLayout *out_ch_layout, enum AVSampleFormat out_sample_fmt, int out_sample_rate,
                        const AVChannelLayout *in_ch_layout, enum AVSampleFormat  in_sample_fmt, int  in_sample_rate,
                        int log_offset, void *log_ctx);

void swr_free(struct SwrContext **s);

void swr_close(struct SwrContext *s);

int swr_convert(struct SwrContext *s, uint8_t * const *out, int out_count,
                                const uint8_t * const *in , int in_count);

int64_t swr_next_pts(struct SwrContext *s, int64_t pts);

int swr_set_compensation(struct SwrContext *s, int sample_delta, int compensation_distance);

int swr_set_channel_mapping(struct SwrContext *s, const int *channel_map);

int swr_build_matrix2(const AVChannelLayout *in_layout, const AVChannelLayout *out_layout,
                      double center_mix_level, double surround_mix_level,
                      double lfe_mix_level, double maxval,
                      double rematrix_volume, double *matrix,
                      ptrdiff_t stride, enum AVMatrixEncoding matrix_encoding,
                      void *log_context);

int swr_set_matrix(struct SwrContext *s, const double *matrix, int stride);

int swr_drop_output(struct SwrContext *s, int count);

int swr_inject_silence(struct SwrContext *s, int count);

int64_t swr_get_delay(struct SwrContext *s, int64_t base);

int swr_get_out_samples(struct SwrContext *s, int in_samples);

unsigned swresample_version(void);

const char *swresample_configuration(void);

const char *swresample_license(void);

int swr_convert_frame(SwrContext *swr,
                      AVFrame *output, const AVFrame *input);

int swr_config_frame(SwrContext *swr, const AVFrame *out, const AVFrame *in);

#endif
