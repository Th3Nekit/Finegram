#ifndef AVUTIL_HWCONTEXT_VAAPI_H
#define AVUTIL_HWCONTEXT_VAAPI_H

#include <va/va.h>

enum {

    AV_VAAPI_DRIVER_QUIRK_USER_SET = (1 << 0),

    AV_VAAPI_DRIVER_QUIRK_RENDER_PARAM_BUFFERS = (1 << 1),

    AV_VAAPI_DRIVER_QUIRK_ATTRIB_MEMTYPE = (1 << 2),

    AV_VAAPI_DRIVER_QUIRK_SURFACE_ATTRIBUTES = (1 << 3),
};

typedef struct AVVAAPIDeviceContext {

    VADisplay display;

    unsigned int driver_quirks;
} AVVAAPIDeviceContext;

typedef struct AVVAAPIFramesContext {

    VASurfaceAttrib *attributes;
    int           nb_attributes;

    VASurfaceID     *surface_ids;
    int           nb_surfaces;
} AVVAAPIFramesContext;

typedef struct AVVAAPIHWConfig {

    VAConfigID config_id;
} AVVAAPIHWConfig;

#endif
