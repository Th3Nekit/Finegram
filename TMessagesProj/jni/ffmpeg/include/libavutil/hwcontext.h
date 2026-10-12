#ifndef AVUTIL_HWCONTEXT_H
#define AVUTIL_HWCONTEXT_H

#include "buffer.h"
#include "frame.h"
#include "log.h"
#include "pixfmt.h"

enum AVHWDeviceType {
    AV_HWDEVICE_TYPE_NONE,
    AV_HWDEVICE_TYPE_VDPAU,
    AV_HWDEVICE_TYPE_CUDA,
    AV_HWDEVICE_TYPE_VAAPI,
    AV_HWDEVICE_TYPE_DXVA2,
    AV_HWDEVICE_TYPE_QSV,
    AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
    AV_HWDEVICE_TYPE_D3D11VA,
    AV_HWDEVICE_TYPE_DRM,
    AV_HWDEVICE_TYPE_OPENCL,
    AV_HWDEVICE_TYPE_MEDIACODEC,
    AV_HWDEVICE_TYPE_VULKAN,
    AV_HWDEVICE_TYPE_D3D12VA,
    AV_HWDEVICE_TYPE_AMF,

    AV_HWDEVICE_TYPE_OHCODEC,
};

typedef struct AVHWDeviceContext {

    const AVClass *av_class;

    enum AVHWDeviceType type;

    void *hwctx;

    void (*free)(struct AVHWDeviceContext *ctx);

    void *user_opaque;
} AVHWDeviceContext;

typedef struct AVHWFramesContext {

    const AVClass *av_class;

    AVBufferRef *device_ref;

    AVHWDeviceContext *device_ctx;

    void *hwctx;

    void (*free)(struct AVHWFramesContext *ctx);

    void *user_opaque;

    AVBufferPool *pool;

    int initial_pool_size;

    enum AVPixelFormat format;

    enum AVPixelFormat sw_format;

    int width, height;
} AVHWFramesContext;

enum AVHWDeviceType av_hwdevice_find_type_by_name(const char *name);

const char *av_hwdevice_get_type_name(enum AVHWDeviceType type);

enum AVHWDeviceType av_hwdevice_iterate_types(enum AVHWDeviceType prev);

AVBufferRef *av_hwdevice_ctx_alloc(enum AVHWDeviceType type);

int av_hwdevice_ctx_init(AVBufferRef *ref);

int av_hwdevice_ctx_create(AVBufferRef **device_ctx, enum AVHWDeviceType type,
                           const char *device, AVDictionary *opts, int flags);

int av_hwdevice_ctx_create_derived(AVBufferRef **dst_ctx,
                                   enum AVHWDeviceType type,
                                   AVBufferRef *src_ctx, int flags);

int av_hwdevice_ctx_create_derived_opts(AVBufferRef **dst_ctx,
                                        enum AVHWDeviceType type,
                                        AVBufferRef *src_ctx,
                                        AVDictionary *options, int flags);

AVBufferRef *av_hwframe_ctx_alloc(AVBufferRef *device_ctx);

int av_hwframe_ctx_init(AVBufferRef *ref);

int av_hwframe_get_buffer(AVBufferRef *hwframe_ctx, AVFrame *frame, int flags);

int av_hwframe_transfer_data(AVFrame *dst, const AVFrame *src, int flags);

enum AVHWFrameTransferDirection {

    AV_HWFRAME_TRANSFER_DIRECTION_FROM,

    AV_HWFRAME_TRANSFER_DIRECTION_TO,
};

int av_hwframe_transfer_get_formats(AVBufferRef *hwframe_ctx,
                                    enum AVHWFrameTransferDirection dir,
                                    enum AVPixelFormat **formats, int flags);

typedef struct AVHWFramesConstraints {

    enum AVPixelFormat *valid_hw_formats;

    enum AVPixelFormat *valid_sw_formats;

    int min_width;
    int min_height;

    int max_width;
    int max_height;
} AVHWFramesConstraints;

void *av_hwdevice_hwconfig_alloc(AVBufferRef *device_ctx);

AVHWFramesConstraints *av_hwdevice_get_hwframe_constraints(AVBufferRef *ref,
                                                           const void *hwconfig);

void av_hwframe_constraints_free(AVHWFramesConstraints **constraints);

enum {

    AV_HWFRAME_MAP_READ      = 1 << 0,

    AV_HWFRAME_MAP_WRITE     = 1 << 1,

    AV_HWFRAME_MAP_OVERWRITE = 1 << 2,

    AV_HWFRAME_MAP_DIRECT    = 1 << 3,
};

int av_hwframe_map(AVFrame *dst, const AVFrame *src, int flags);

int av_hwframe_ctx_create_derived(AVBufferRef **derived_frame_ctx,
                                  enum AVPixelFormat format,
                                  AVBufferRef *derived_device_ctx,
                                  AVBufferRef *source_frame_ctx,
                                  int flags);

#endif
