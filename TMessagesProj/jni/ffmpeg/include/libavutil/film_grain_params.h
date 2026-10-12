#ifndef AVUTIL_FILM_GRAIN_PARAMS_H
#define AVUTIL_FILM_GRAIN_PARAMS_H

#include "frame.h"

enum AVFilmGrainParamsType {
    AV_FILM_GRAIN_PARAMS_NONE = 0,

    AV_FILM_GRAIN_PARAMS_AV1,

    AV_FILM_GRAIN_PARAMS_H274,
};

typedef struct AVFilmGrainAOMParams {

    int num_y_points;
    uint8_t y_points[14][2                     ];

    int chroma_scaling_from_luma;

    int num_uv_points[2             ];
    uint8_t uv_points[2             ][10][2                     ];

    int scaling_shift;

    int ar_coeff_lag;

    int8_t ar_coeffs_y[24];

    int8_t ar_coeffs_uv[2             ][25];

    int ar_coeff_shift;

    int grain_scale_shift;

    int uv_mult[2             ];
    int uv_mult_luma[2             ];

    int uv_offset[2             ];

    int overlap_flag;

    int limit_output_range;
} AVFilmGrainAOMParams;

typedef struct AVFilmGrainH274Params {

    int model_id;

    int blending_mode_id;

    int log2_scale_factor;

    int component_model_present[3                ];

    uint16_t num_intensity_intervals[3                ];

    uint8_t num_model_values[3                ];

    uint8_t intensity_interval_lower_bound[3                ][256                         ];

    uint8_t intensity_interval_upper_bound[3                ][256                         ];

    int16_t comp_model_value[3                ][256                         ][6                  ];
} AVFilmGrainH274Params;

typedef struct AVFilmGrainParams {

    enum AVFilmGrainParamsType type;

    uint64_t seed;

    int width, height;

    int subsampling_x, subsampling_y;

    enum AVColorRange                  color_range;
    enum AVColorPrimaries              color_primaries;
    enum AVColorTransferCharacteristic color_trc;
    enum AVColorSpace                  color_space;

    int bit_depth_luma;
    int bit_depth_chroma;

    union {
        AVFilmGrainAOMParams aom;
        AVFilmGrainH274Params h274;
    } codec;
} AVFilmGrainParams;

AVFilmGrainParams *av_film_grain_params_alloc(size_t *size);

AVFilmGrainParams *av_film_grain_params_create_side_data(AVFrame *frame);

const AVFilmGrainParams *av_film_grain_params_select(const AVFrame *frame);

#endif
