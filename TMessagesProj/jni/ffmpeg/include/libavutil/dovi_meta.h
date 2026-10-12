/*
 * Copyright (c) 2020 Vacing Fang <vacingfang@tencent.com>
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
 * DOVI configuration
 */

#ifndef AVUTIL_DOVI_META_H
#define AVUTIL_DOVI_META_H

#include <stdint.h>
#include <stddef.h>

#include "rational.h"
#include "csp.h"

typedef struct AVDOVIDecoderConfigurationRecord {
    uint8_t dv_version_major;
    uint8_t dv_version_minor;
    uint8_t dv_profile;
    uint8_t dv_level;
    uint8_t rpu_present_flag;
    uint8_t el_present_flag;
    uint8_t bl_present_flag;
    uint8_t dv_bl_signal_compatibility_id;
    uint8_t dv_md_compression;
} AVDOVIDecoderConfigurationRecord;

enum AVDOVICompression {
    AV_DOVI_COMPRESSION_NONE     = 0,
    AV_DOVI_COMPRESSION_LIMITED  = 1,
    AV_DOVI_COMPRESSION_RESERVED = 2,
    AV_DOVI_COMPRESSION_EXTENDED = 3,
};

AVDOVIDecoderConfigurationRecord *av_dovi_alloc(size_t *size);

typedef struct AVDOVIRpuDataHeader {
    uint8_t rpu_type;
    uint16_t rpu_format;
    uint8_t vdr_rpu_profile;
    uint8_t vdr_rpu_level;
    uint8_t chroma_resampling_explicit_filter_flag;
    uint8_t coef_data_type;
    uint8_t coef_log2_denom;
    uint8_t vdr_rpu_normalized_idc;
    uint8_t bl_video_full_range_flag;
    uint8_t bl_bit_depth;
    uint8_t el_bit_depth;
    uint8_t vdr_bit_depth;
    uint8_t spatial_resampling_filter_flag;
    uint8_t el_spatial_resampling_filter_flag;
    uint8_t disable_residual_flag;
    uint8_t ext_mapping_idc_0_4;
    uint8_t ext_mapping_idc_5_7;
} AVDOVIRpuDataHeader;

enum AVDOVIMappingMethod {
    AV_DOVI_MAPPING_POLYNOMIAL = 0,
    AV_DOVI_MAPPING_MMR = 1,
};

#define AV_DOVI_MAX_PIECES 8
typedef struct AVDOVIReshapingCurve {
    uint8_t num_pivots;
    uint16_t pivots[AV_DOVI_MAX_PIECES + 1];
    enum AVDOVIMappingMethod mapping_idc[AV_DOVI_MAX_PIECES];

    uint8_t poly_order[AV_DOVI_MAX_PIECES];
    int64_t poly_coef[AV_DOVI_MAX_PIECES][3];

    uint8_t mmr_order[AV_DOVI_MAX_PIECES];
    int64_t mmr_constant[AV_DOVI_MAX_PIECES];
    int64_t mmr_coef[AV_DOVI_MAX_PIECES][3               ][7];
} AVDOVIReshapingCurve;

enum AVDOVINLQMethod {
    AV_DOVI_NLQ_NONE = -1,
    AV_DOVI_NLQ_LINEAR_DZ = 0,
};

typedef struct AVDOVINLQParams {
    uint16_t nlq_offset;
    uint64_t vdr_in_max;

    uint64_t linear_deadzone_slope;
    uint64_t linear_deadzone_threshold;
} AVDOVINLQParams;

typedef struct AVDOVIDataMapping {
    uint8_t vdr_rpu_id;
    uint8_t mapping_color_space;
    uint8_t mapping_chroma_format_idc;
    AVDOVIReshapingCurve curves[3];

    enum AVDOVINLQMethod nlq_method_idc;
    uint32_t num_x_partitions;
    uint32_t num_y_partitions;
    AVDOVINLQParams nlq[3];
    uint16_t nlq_pivots[2];
} AVDOVIDataMapping;

typedef struct AVDOVIColorMetadata {
    uint8_t dm_metadata_id;
    uint8_t scene_refresh_flag;

    AVRational ycc_to_rgb_matrix[9];
    AVRational ycc_to_rgb_offset[3];
    AVRational rgb_to_lms_matrix[9];

    uint16_t signal_eotf;
    uint16_t signal_eotf_param0;
    uint16_t signal_eotf_param1;
    uint32_t signal_eotf_param2;
    uint8_t signal_bit_depth;
    uint8_t signal_color_space;
    uint8_t signal_chroma_format;
    uint8_t signal_full_range_flag;
    uint16_t source_min_pq;
    uint16_t source_max_pq;
    uint16_t source_diagonal;
} AVDOVIColorMetadata;

typedef struct AVDOVIDmLevel1 {

    uint16_t min_pq;
    uint16_t max_pq;
    uint16_t avg_pq;
} AVDOVIDmLevel1;

typedef struct AVDOVIDmLevel2 {

    uint16_t target_max_pq;
    uint16_t trim_slope;
    uint16_t trim_offset;
    uint16_t trim_power;
    uint16_t trim_chroma_weight;
    uint16_t trim_saturation_gain;
    int16_t ms_weight;
} AVDOVIDmLevel2;

typedef struct AVDOVIDmLevel3 {
    uint16_t min_pq_offset;
    uint16_t max_pq_offset;
    uint16_t avg_pq_offset;
} AVDOVIDmLevel3;

typedef struct AVDOVIDmLevel4 {
    uint16_t anchor_pq;
    uint16_t anchor_power;
} AVDOVIDmLevel4;

typedef struct AVDOVIDmLevel5 {

    uint16_t left_offset;
    uint16_t right_offset;
    uint16_t top_offset;
    uint16_t bottom_offset;
} AVDOVIDmLevel5;

typedef struct AVDOVIDmLevel6 {

    uint16_t max_luminance;
    uint16_t min_luminance;
    uint16_t max_cll;
    uint16_t max_fall;
} AVDOVIDmLevel6;

typedef struct AVDOVIDmLevel8 {

    uint8_t target_display_index;
    uint16_t trim_slope;
    uint16_t trim_offset;
    uint16_t trim_power;
    uint16_t trim_chroma_weight;
    uint16_t trim_saturation_gain;
    uint16_t ms_weight;
    uint16_t target_mid_contrast;
    uint16_t clip_trim;
    uint8_t saturation_vector_field[6];
    uint8_t hue_vector_field[6];
} AVDOVIDmLevel8;

typedef struct AVDOVIDmLevel9 {

    uint8_t source_primary_index;
    AVColorPrimariesDesc source_display_primaries;
} AVDOVIDmLevel9;

typedef struct AVDOVIDmLevel10 {

    uint8_t target_display_index;
    uint16_t target_max_pq;
    uint16_t target_min_pq;
    uint8_t target_primary_index;
    AVColorPrimariesDesc target_display_primaries;
} AVDOVIDmLevel10;

typedef struct AVDOVIDmLevel11 {
    uint8_t content_type;
    uint8_t whitepoint;
    uint8_t reference_mode_flag;
#if FF_API_DOVI_L11_INVALID_PROPS
    attribute_deprecated
    uint8_t sharpness;
    attribute_deprecated
    uint8_t noise_reduction;
    attribute_deprecated
    uint8_t mpeg_noise_reduction;
    attribute_deprecated
    uint8_t frame_rate_conversion;
    attribute_deprecated
    uint8_t brightness;
    attribute_deprecated
    uint8_t color;
#endif
} AVDOVIDmLevel11;

typedef struct AVDOVIDmLevel254 {

    uint8_t dm_mode;
    uint8_t dm_version_index;
} AVDOVIDmLevel254;

typedef struct AVDOVIDmLevel255 {

    uint8_t dm_run_mode;
    uint8_t dm_run_version;
    uint8_t dm_debug[4];
} AVDOVIDmLevel255;

typedef struct AVDOVIDmData {
    uint8_t level;
    union {
        AVDOVIDmLevel1 l1;
        AVDOVIDmLevel2 l2;
        AVDOVIDmLevel3 l3;
        AVDOVIDmLevel4 l4;
        AVDOVIDmLevel5 l5;
        AVDOVIDmLevel6 l6;

        AVDOVIDmLevel8 l8;
        AVDOVIDmLevel9 l9;
        AVDOVIDmLevel10 l10;
        AVDOVIDmLevel11 l11;
        AVDOVIDmLevel254 l254;
        AVDOVIDmLevel255 l255;
    };
} AVDOVIDmData;

typedef struct AVDOVIMetadata {

    size_t header_offset;
    size_t mapping_offset;
    size_t color_offset;

    size_t ext_block_offset;
    size_t ext_block_size;
    int num_ext_blocks;

#define AV_DOVI_MAX_EXT_BLOCKS 32
} AVDOVIMetadata;

static av_always_inline AVDOVIRpuDataHeader *
av_dovi_get_header(const AVDOVIMetadata *data)
{
    return (AVDOVIRpuDataHeader *)((uint8_t *) data + data->header_offset);
}

static av_always_inline AVDOVIDataMapping *
av_dovi_get_mapping(const AVDOVIMetadata *data)
{
    return (AVDOVIDataMapping *)((uint8_t *) data + data->mapping_offset);
}

static av_always_inline AVDOVIColorMetadata *
av_dovi_get_color(const AVDOVIMetadata *data)
{
    return (AVDOVIColorMetadata *)((uint8_t *) data + data->color_offset);
}

static av_always_inline AVDOVIDmData *
av_dovi_get_ext(const AVDOVIMetadata *data, int index)
{
    return (AVDOVIDmData *)((uint8_t *) data + data->ext_block_offset +
                            data->ext_block_size * index);
}

AVDOVIDmData *av_dovi_find_level(const AVDOVIMetadata *data, uint8_t level);

AVDOVIMetadata *av_dovi_metadata_alloc(size_t *size);

#endif
