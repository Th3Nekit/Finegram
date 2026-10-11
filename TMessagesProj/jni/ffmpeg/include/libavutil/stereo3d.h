/*
 * Copyright (c) 2013 Vittorio Giovara <vittorio.giovara@gmail.com>
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
 * @ingroup lavu_video_stereo3d
 * Stereoscopic video
 */

#ifndef AVUTIL_STEREO3D_H
#define AVUTIL_STEREO3D_H

#include <stdint.h>

#include "frame.h"

enum AVStereo3DType {

    AV_STEREO3D_2D,

    AV_STEREO3D_SIDEBYSIDE,

    AV_STEREO3D_TOPBOTTOM,

    AV_STEREO3D_FRAMESEQUENCE,

    AV_STEREO3D_CHECKERBOARD,

    AV_STEREO3D_SIDEBYSIDE_QUINCUNX,

    AV_STEREO3D_LINES,

    AV_STEREO3D_COLUMNS,

    AV_STEREO3D_UNSPEC,
};

enum AVStereo3DView {

    AV_STEREO3D_VIEW_PACKED,

    AV_STEREO3D_VIEW_LEFT,

    AV_STEREO3D_VIEW_RIGHT,

    AV_STEREO3D_VIEW_UNSPEC,
};

enum AVStereo3DPrimaryEye {

    AV_PRIMARY_EYE_NONE,

    AV_PRIMARY_EYE_LEFT,

    AV_PRIMARY_EYE_RIGHT,
};

#define AV_STEREO3D_FLAG_INVERT     (1 << 0)

typedef struct AVStereo3D {

    enum AVStereo3DType type;

    int flags;

    enum AVStereo3DView view;

    enum AVStereo3DPrimaryEye primary_eye;

    uint32_t baseline;

    AVRational horizontal_disparity_adjustment;

    AVRational horizontal_field_of_view;
} AVStereo3D;

AVStereo3D *av_stereo3d_alloc(void);

AVStereo3D *av_stereo3d_alloc_size(size_t *size);

AVStereo3D *av_stereo3d_create_side_data(AVFrame *frame);

const char *av_stereo3d_type_name(unsigned int type);

int av_stereo3d_from_name(const char *name);

const char *av_stereo3d_view_name(unsigned int view);

int av_stereo3d_view_from_name(const char *name);

const char *av_stereo3d_primary_eye_name(unsigned int eye);

int av_stereo3d_primary_eye_from_name(const char *name);

#endif
