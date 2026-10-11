/*
 * Direct3D 12 HW acceleration.
 *
 * copyright (c) 2022-2023 Wu Jianhua <toqsxw@outlook.com>
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

#ifndef AVUTIL_HWCONTEXT_D3D12VA_H
#define AVUTIL_HWCONTEXT_D3D12VA_H

#include <stdint.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3d12video.h>

typedef struct AVD3D12VADeviceContext {

    ID3D12Device *device;

    ID3D12VideoDevice *video_device;

    void (*lock)(void *lock_ctx);
    void (*unlock)(void *lock_ctx);
    void *lock_ctx;

    D3D12_RESOURCE_FLAGS resource_flags;

    D3D12_HEAP_FLAGS heap_flags;
} AVD3D12VADeviceContext;

typedef struct AVD3D12VASyncContext {

    ID3D12Fence *fence;

    HANDLE event;

    uint64_t fence_value;
} AVD3D12VASyncContext;

typedef enum AVD3D12VAFrameFlags {
    AV_D3D12VA_FRAME_FLAG_NONE = 0,

    AV_D3D12VA_FRAME_FLAG_TEXTURE_ARRAY = (1 << 1),
} AVD3D12VAFrameFlags;

typedef struct AVD3D12VAFrame {

    ID3D12Resource *texture;

    int subresource_index;

    AVD3D12VASyncContext sync_ctx;

    AVD3D12VAFrameFlags flags;
} AVD3D12VAFrame;

typedef struct AVD3D12VAFramesContext {

    DXGI_FORMAT format;

    D3D12_RESOURCE_FLAGS resource_flags;

    D3D12_HEAP_FLAGS heap_flags;

    ID3D12Resource *texture_array;

    AVD3D12VAFrameFlags flags;
} AVD3D12VAFramesContext;

#endif
