/*
 * Copyright (c) 2014 Vittorio Giovara <vittorio.giovara@gmail.com>
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
 * @ingroup lavu_video_display
 * Display matrix
 */

#ifndef AVUTIL_DISPLAY_H
#define AVUTIL_DISPLAY_H

#include <stdint.h>

double av_display_rotation_get(const int32_t matrix[9]);

void av_display_rotation_set(int32_t matrix[9], double angle);

void av_display_matrix_flip(int32_t matrix[9], int hflip, int vflip);

#endif
