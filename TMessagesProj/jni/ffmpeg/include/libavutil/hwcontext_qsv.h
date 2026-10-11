#ifndef AVUTIL_HWCONTEXT_QSV_H
#define AVUTIL_HWCONTEXT_QSV_H

#include <mfxvideo.h>

typedef struct AVQSVDeviceContext {
    mfxSession session;

    void *loader;
} AVQSVDeviceContext;

typedef struct AVQSVFramesContext {

    mfxFrameSurface1 *surfaces;

    int            nb_surfaces;

    int frame_type;

    mfxFrameInfo *info;
} AVQSVFramesContext;

#endif
