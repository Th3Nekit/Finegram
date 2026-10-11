/*
 * The Video Decode and Presentation API for UNIX (VDPAU) is used for
 * hardware-accelerated decoding of MPEG-1/2, H.264 and VC-1.
 *
 * Copyright (C) 2008 NVIDIA
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

#ifndef AVCODEC_VDPAU_H
#define AVCODEC_VDPAU_H

#include <vdpau/vdpau.h>

#include "libavutil/avconfig.h"
#include "libavutil/attributes.h"

#include "avcodec.h"

struct AVCodecContext;
struct AVFrame;

typedef int (*AVVDPAU_Render2)(struct AVCodecContext *, struct AVFrame *,
                               const VdpPictureInfo *, uint32_t,
                               const VdpBitstreamBuffer *);

typedef struct AVVDPAUContext {

    VdpDecoder decoder;

    VdpDecoderRender *render;

    AVVDPAU_Render2 render2;
} AVVDPAUContext;

int av_vdpau_bind_context(AVCodecContext *avctx, VdpDevice device,
                          VdpGetProcAddress *get_proc_address, unsigned flags);

int av_vdpau_get_surface_parameters(AVCodecContext *avctx, VdpChromaType *type,
                                    uint32_t *width, uint32_t *height);

#endif
