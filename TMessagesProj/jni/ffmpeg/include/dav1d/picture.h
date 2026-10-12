/*
 * Copyright © 2018-2020, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DAV1D_PICTURE_H
#define DAV1D_PICTURE_H

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "headers.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAV1D_PICTURE_ALIGNMENT 64

typedef struct Dav1dPictureParameters {
    int w;
    int h;
    enum Dav1dPixelLayout layout;
    int bpc;
} Dav1dPictureParameters;

typedef struct Dav1dPicture {
    Dav1dSequenceHeader *seq_hdr;
    Dav1dFrameHeader *frame_hdr;

    void *data[3];

    ptrdiff_t stride[2];

    Dav1dPictureParameters p;
    Dav1dDataProps m;

    Dav1dContentLightLevel *content_light;

    Dav1dMasteringDisplay *mastering_display;

    Dav1dITUTT35 *itut_t35;

    size_t n_itut_t35;

    uintptr_t reserved[4];

    struct Dav1dRef *frame_hdr_ref;
    struct Dav1dRef *seq_hdr_ref;
    struct Dav1dRef *content_light_ref;
    struct Dav1dRef *mastering_display_ref;
    struct Dav1dRef *itut_t35_ref;
    uintptr_t reserved_ref[4];
    struct Dav1dRef *ref;

    void *allocator_data;
} Dav1dPicture;

typedef struct Dav1dPicAllocator {
    void *cookie;

    int (*alloc_picture_callback)(Dav1dPicture *pic, void *cookie);

    void (*release_picture_callback)(Dav1dPicture *pic, void *cookie);
} Dav1dPicAllocator;

DAV1D_API void dav1d_picture_unref(Dav1dPicture *p);

#ifdef __cplusplus
}
#endif

#endif
