#ifndef AVUTIL_HWCONTEXT_DRM_H
#define AVUTIL_HWCONTEXT_DRM_H

#include <stddef.h>
#include <stdint.h>

enum {

    AV_DRM_MAX_PLANES = 4
};

typedef struct AVDRMObjectDescriptor {

    int fd;

    size_t size;

    uint64_t format_modifier;
} AVDRMObjectDescriptor;

typedef struct AVDRMPlaneDescriptor {

    int object_index;

    ptrdiff_t offset;

    ptrdiff_t pitch;
} AVDRMPlaneDescriptor;

typedef struct AVDRMLayerDescriptor {

    uint32_t format;

    int nb_planes;

    AVDRMPlaneDescriptor planes[AV_DRM_MAX_PLANES];
} AVDRMLayerDescriptor;

typedef struct AVDRMFrameDescriptor {

    int nb_objects;

    AVDRMObjectDescriptor objects[AV_DRM_MAX_PLANES];

    int nb_layers;

    AVDRMLayerDescriptor layers[AV_DRM_MAX_PLANES];
} AVDRMFrameDescriptor;

typedef struct AVDRMDeviceContext {

    int fd;
} AVDRMDeviceContext;

#endif
