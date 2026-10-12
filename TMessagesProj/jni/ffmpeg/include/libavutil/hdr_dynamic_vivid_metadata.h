/*
 * Copyright (c) 2021 Limin Wang <lance.lmwang at gmail.com>
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

#ifndef AVUTIL_HDR_DYNAMIC_VIVID_METADATA_H
#define AVUTIL_HDR_DYNAMIC_VIVID_METADATA_H

#include "frame.h"
#include "rational.h"

typedef struct AVHDRVivid3SplineParams {

    int th_mode;

    AVRational th_enable_mb;

    AVRational th_enable;

    AVRational th_delta1;

    AVRational th_delta2;

    AVRational enable_strength;
} AVHDRVivid3SplineParams;

typedef struct AVHDRVividColorToneMappingParams {

    AVRational targeted_system_display_maximum_luminance;

    int base_enable_flag;

    AVRational base_param_m_p;

    AVRational base_param_m_m;

    AVRational base_param_m_a;

    AVRational base_param_m_b;

    AVRational base_param_m_n;

    int base_param_k1;

    int base_param_k2;

    int base_param_k3;

    int base_param_Delta_enable_mode;

    AVRational base_param_Delta;

    int three_Spline_enable_flag;

    int three_Spline_num;

    AVHDRVivid3SplineParams three_spline[2];
} AVHDRVividColorToneMappingParams;

typedef struct AVHDRVividColorTransformParams {

    AVRational minimum_maxrgb;

    AVRational average_maxrgb;

    AVRational variance_maxrgb;

    AVRational maximum_maxrgb;

    int tone_mapping_mode_flag;

    int tone_mapping_param_num;

    AVHDRVividColorToneMappingParams tm_params[2];

    int color_saturation_mapping_flag;

    int color_saturation_num;

    AVRational color_saturation_gain[8];
} AVHDRVividColorTransformParams;

typedef struct AVDynamicHDRVivid {

    uint8_t system_start_code;

    uint8_t num_windows;

    AVHDRVividColorTransformParams params[3];
} AVDynamicHDRVivid;

AVDynamicHDRVivid *av_dynamic_hdr_vivid_alloc(size_t *size);

AVDynamicHDRVivid *av_dynamic_hdr_vivid_create_side_data(AVFrame *frame);

#endif
