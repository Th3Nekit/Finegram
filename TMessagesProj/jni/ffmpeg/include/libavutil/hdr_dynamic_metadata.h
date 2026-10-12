/*
 * Copyright (c) 2018 Mohammad Izadi <moh.izadi at gmail.com>
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

#ifndef AVUTIL_HDR_DYNAMIC_METADATA_H
#define AVUTIL_HDR_DYNAMIC_METADATA_H

#include "frame.h"
#include "rational.h"

enum AVHDRPlusOverlapProcessOption {
    AV_HDR_PLUS_OVERLAP_PROCESS_WEIGHTED_AVERAGING = 0,
    AV_HDR_PLUS_OVERLAP_PROCESS_LAYERING = 1,
};

typedef struct AVHDRPlusPercentile {

    uint8_t percentage;

    AVRational percentile;
} AVHDRPlusPercentile;

typedef struct AVHDRPlusColorTransformParams {

    AVRational window_upper_left_corner_x;

    AVRational window_upper_left_corner_y;

    AVRational window_lower_right_corner_x;

    AVRational window_lower_right_corner_y;

    uint16_t center_of_ellipse_x;

    uint16_t center_of_ellipse_y;

    uint8_t rotation_angle;

    uint16_t semimajor_axis_internal_ellipse;

    uint16_t semimajor_axis_external_ellipse;

    uint16_t semiminor_axis_external_ellipse;

    enum AVHDRPlusOverlapProcessOption overlap_process_option;

    AVRational maxscl[3];

    AVRational average_maxrgb;

    uint8_t num_distribution_maxrgb_percentiles;

    AVHDRPlusPercentile distribution_maxrgb[15];

    AVRational fraction_bright_pixels;

    uint8_t tone_mapping_flag;

    AVRational knee_point_x;

    AVRational knee_point_y;

    uint8_t num_bezier_curve_anchors;

    AVRational bezier_curve_anchors[15];

    uint8_t color_saturation_mapping_flag;

    AVRational color_saturation_weight;
} AVHDRPlusColorTransformParams;

typedef struct AVDynamicHDRPlus {

    uint8_t itu_t_t35_country_code;

    uint8_t application_version;

    uint8_t num_windows;

    AVHDRPlusColorTransformParams params[3];

    AVRational targeted_system_display_maximum_luminance;

    uint8_t targeted_system_display_actual_peak_luminance_flag;

    uint8_t num_rows_targeted_system_display_actual_peak_luminance;

    uint8_t num_cols_targeted_system_display_actual_peak_luminance;

    AVRational targeted_system_display_actual_peak_luminance[25][25];

    uint8_t mastering_display_actual_peak_luminance_flag;

    uint8_t num_rows_mastering_display_actual_peak_luminance;

    uint8_t num_cols_mastering_display_actual_peak_luminance;

    AVRational mastering_display_actual_peak_luminance[25][25];
} AVDynamicHDRPlus;

AVDynamicHDRPlus *av_dynamic_hdr_plus_alloc(size_t *size);

AVDynamicHDRPlus *av_dynamic_hdr_plus_create_side_data(AVFrame *frame);

int av_dynamic_hdr_plus_from_t35(AVDynamicHDRPlus *s, const uint8_t *data,
                                 size_t size);

#define AV_HDR_PLUS_MAX_PAYLOAD_SIZE 907

int av_dynamic_hdr_plus_to_t35(const AVDynamicHDRPlus *s, uint8_t **data, size_t *size);

#endif
