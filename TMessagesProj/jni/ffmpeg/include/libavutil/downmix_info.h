/*
 * Copyright (c) 2014 Tim Walker <tdskywalker@gmail.com>
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

#ifndef AVUTIL_DOWNMIX_INFO_H
#define AVUTIL_DOWNMIX_INFO_H

#include "frame.h"

enum AVDownmixType {
    AV_DOWNMIX_TYPE_UNKNOWN,
    AV_DOWNMIX_TYPE_LORO,
    AV_DOWNMIX_TYPE_LTRT,
    AV_DOWNMIX_TYPE_DPLII,
    AV_DOWNMIX_TYPE_NB
};

typedef struct AVDownmixInfo {

    enum AVDownmixType preferred_downmix_type;

    double center_mix_level;

    double center_mix_level_ltrt;

    double surround_mix_level;

    double surround_mix_level_ltrt;

    double lfe_mix_level;
} AVDownmixInfo;

AVDownmixInfo *av_downmix_info_update_side_data(AVFrame *frame);

#endif
