#ifndef AVUTIL_HWCONTEXT_CUDA_H
#define AVUTIL_HWCONTEXT_CUDA_H

#ifndef CUDA_VERSION
#include <cuda.h>
#endif

#include "pixfmt.h"

typedef struct AVCUDADeviceContextInternal AVCUDADeviceContextInternal;

typedef struct AVCUDADeviceContext {
    CUcontext cuda_ctx;
    CUstream stream;
    AVCUDADeviceContextInternal *internal;
} AVCUDADeviceContext;

#define AV_CUDA_USE_PRIMARY_CONTEXT (1 << 0)

#define AV_CUDA_USE_CURRENT_CONTEXT (1 << 1)

#endif
