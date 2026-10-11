#ifndef AVUTIL_HWCONTEXT_VDPAU_H
#define AVUTIL_HWCONTEXT_VDPAU_H

#include <vdpau/vdpau.h>

typedef struct AVVDPAUDeviceContext {
    VdpDevice          device;
    VdpGetProcAddress *get_proc_address;
} AVVDPAUDeviceContext;

#endif
