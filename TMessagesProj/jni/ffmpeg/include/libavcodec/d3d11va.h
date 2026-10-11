/*
 * Direct3D11 HW acceleration
 *
 * copyright (c) 2009 Laurent Aimar
 * copyright (c) 2015 Steve Lhomme
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

#ifndef AVCODEC_D3D11VA_H
#define AVCODEC_D3D11VA_H

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0602
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#include <stdint.h>
#include <d3d11.h>

typedef struct AVD3D11VAContext {

    ID3D11VideoDecoder *decoder;

    ID3D11VideoContext *video_context;

    D3D11_VIDEO_DECODER_CONFIG *cfg;

    unsigned surface_count;

    ID3D11VideoDecoderOutputView **surface;

    uint64_t workaround;

    unsigned report_id;

    HANDLE  context_mutex;
} AVD3D11VAContext;

AVD3D11VAContext *av_d3d11va_alloc_context(void);

#endif
