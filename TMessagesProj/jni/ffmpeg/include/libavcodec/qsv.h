#ifndef AVCODEC_QSV_H
#define AVCODEC_QSV_H

#include <mfxvideo.h>

#include "libavutil/buffer.h"

typedef struct AVQSVContext {

    mfxSession session;

    int iopattern;

    mfxExtBuffer **ext_buffers;
    int         nb_ext_buffers;

    int opaque_alloc;

    int nb_opaque_surfaces;

    AVBufferRef *opaque_surfaces;

    int opaque_alloc_type;
} AVQSVContext;

AVQSVContext *av_qsv_alloc_context(void);

#endif
