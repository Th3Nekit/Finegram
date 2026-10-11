/*
 * Copyright (c) 2015 Kevin Wheatley <kevin.j.wheatley@gmail.com>
 * Copyright (c) 2016 Ronald S. Bultje <rsbultje@gmail.com>
 * Copyright (c) 2023 Leo Izen <leo.izen@gmail.com>
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

#ifndef AVUTIL_CSP_H
#define AVUTIL_CSP_H

#include "pixfmt.h"
#include "rational.h"

typedef struct AVLumaCoefficients {
    AVRational cr, cg, cb;
} AVLumaCoefficients;

typedef struct AVCIExy {
    AVRational x, y;
} AVCIExy;

typedef struct AVPrimaryCoefficients {
    AVCIExy r, g, b;
} AVPrimaryCoefficients;

typedef AVCIExy AVWhitepointCoefficients;

typedef struct AVColorPrimariesDesc {
    AVWhitepointCoefficients wp;
    AVPrimaryCoefficients prim;
} AVColorPrimariesDesc;

typedef double (*av_csp_trc_function)(double);

const AVLumaCoefficients *av_csp_luma_coeffs_from_avcsp(enum AVColorSpace csp);

const AVColorPrimariesDesc *av_csp_primaries_desc_from_id(enum AVColorPrimaries prm);

enum AVColorPrimaries av_csp_primaries_id_from_desc(const AVColorPrimariesDesc *prm);

double av_csp_approximate_trc_gamma(enum AVColorTransferCharacteristic trc);

double av_csp_approximate_eotf_gamma(enum AVColorTransferCharacteristic trc);

av_csp_trc_function av_csp_trc_func_from_id(enum AVColorTransferCharacteristic trc);

av_csp_trc_function av_csp_trc_func_inv_from_id(enum AVColorTransferCharacteristic trc);

typedef void (*av_csp_eotf_function)(double Lw, double Lb, double c[3]);

av_csp_eotf_function av_csp_itu_eotf(enum AVColorTransferCharacteristic trc);

av_csp_eotf_function av_csp_itu_eotf_inv(enum AVColorTransferCharacteristic trc);

#endif
