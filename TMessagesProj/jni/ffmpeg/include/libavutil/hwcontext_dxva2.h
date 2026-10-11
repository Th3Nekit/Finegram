#ifndef AVUTIL_HWCONTEXT_DXVA2_H
#define AVUTIL_HWCONTEXT_DXVA2_H

#include <d3d9.h>
#include <dxva2api.h>

typedef struct AVDXVA2DeviceContext {
    IDirect3DDeviceManager9 *devmgr;
} AVDXVA2DeviceContext;

typedef struct AVDXVA2FramesContext {

    DWORD               surface_type;

    IDirect3DSurface9 **surfaces;
    int              nb_surfaces;

    IDirectXVideoDecoder *decoder_to_release;
} AVDXVA2FramesContext;

#endif
