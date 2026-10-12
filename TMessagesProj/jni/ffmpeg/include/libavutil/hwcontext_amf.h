#ifndef AVUTIL_HWCONTEXT_AMF_H
#define AVUTIL_HWCONTEXT_AMF_H

#include "pixfmt.h"
#include "hwcontext.h"
#include <AMF/core/Factory.h>
#include <AMF/core/Context.h>
#include <AMF/core/Trace.h>
#include <AMF/core/Debug.h>

typedef struct AVAMFDeviceContext {
    void *              library;
    AMFFactory         *factory;
    void               *trace_writer;

    int64_t             version;
    AMFContext         *context;
    AMF_MEMORY_TYPE     memory_type;

    void (*lock)(void *lock_ctx);
    void (*unlock)(void *lock_ctx);
    void *lock_ctx;
} AVAMFDeviceContext;

enum AMF_SURFACE_FORMAT av_av_to_amf_format(enum AVPixelFormat fmt);
enum AVPixelFormat av_amf_to_av_format(enum AMF_SURFACE_FORMAT fmt);

#endif
