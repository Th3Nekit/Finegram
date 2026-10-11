/*
 * Copyright (c) 2023 Jan Ekström <jeebjp@gmail.com>
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

#ifndef AVUTIL_AMBIENT_VIEWING_ENVIRONMENT_H
#define AVUTIL_AMBIENT_VIEWING_ENVIRONMENT_H

#include <stddef.h>
#include "frame.h"
#include "rational.h"

typedef struct AVAmbientViewingEnvironment {

    AVRational ambient_illuminance;

    AVRational ambient_light_x;

    AVRational ambient_light_y;
} AVAmbientViewingEnvironment;

AVAmbientViewingEnvironment *av_ambient_viewing_environment_alloc(size_t *size);

AVAmbientViewingEnvironment *av_ambient_viewing_environment_create_side_data(AVFrame *frame);

#endif
