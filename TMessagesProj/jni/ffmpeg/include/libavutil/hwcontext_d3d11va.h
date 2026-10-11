#ifndef AVUTIL_HWCONTEXT_D3D11VA_H
#define AVUTIL_HWCONTEXT_D3D11VA_H

#include <d3d11.h>
#include <stdint.h>

typedef struct AVD3D11VADeviceContext {

    ID3D11Device        *device;

    ID3D11DeviceContext *device_context;

    ID3D11VideoDevice   *video_device;

    ID3D11VideoContext  *video_context;

    void (*lock)(void *lock_ctx);
    void (*unlock)(void *lock_ctx);
    void *lock_ctx;

    UINT BindFlags;

    UINT MiscFlags;
} AVD3D11VADeviceContext;

typedef struct AVD3D11FrameDescriptor {

    ID3D11Texture2D *texture;

    intptr_t index;
} AVD3D11FrameDescriptor;

typedef struct AVD3D11VAFramesContext {

    ID3D11Texture2D *texture;

    UINT BindFlags;

    UINT MiscFlags;

    AVD3D11FrameDescriptor *texture_infos;
} AVD3D11VAFramesContext;

#endif
