#ifndef AVUTIL_HWCONTEXT_MEDIACODEC_H
#define AVUTIL_HWCONTEXT_MEDIACODEC_H

typedef struct AVMediaCodecDeviceContext {

    void *surface;

    void *native_window;

    int create_window;
} AVMediaCodecDeviceContext;

#endif
