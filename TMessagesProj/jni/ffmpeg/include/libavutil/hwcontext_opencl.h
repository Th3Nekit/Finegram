#ifndef AVUTIL_HWCONTEXT_OPENCL_H
#define AVUTIL_HWCONTEXT_OPENCL_H

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "frame.h"

typedef struct AVOpenCLFrameDescriptor {

    int nb_planes;

    cl_mem planes[AV_NUM_DATA_POINTERS];
} AVOpenCLFrameDescriptor;

typedef struct AVOpenCLDeviceContext {

    cl_device_id device_id;

    cl_context context;

    cl_command_queue command_queue;
} AVOpenCLDeviceContext;

typedef struct AVOpenCLFramesContext {

    cl_command_queue command_queue;
} AVOpenCLFramesContext;

#endif
