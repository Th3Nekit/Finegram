#ifndef AVUTIL_MOTION_VECTOR_H
#define AVUTIL_MOTION_VECTOR_H

#include <stdint.h>

typedef struct AVMotionVector {

    int32_t source;

    uint8_t w, h;

    int16_t src_x, src_y;

    int16_t dst_x, dst_y;

    uint64_t flags;

    int32_t motion_x, motion_y;
    uint16_t motion_scale;
} AVMotionVector;

#endif
