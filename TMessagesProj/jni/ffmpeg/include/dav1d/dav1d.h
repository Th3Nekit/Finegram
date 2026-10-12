/*
 * Copyright © 2018-2021, VideoLAN and dav1d authors
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

#ifndef DAV1D_H
#define DAV1D_H

#include <errno.h>
#include <stdarg.h>

#include "common.h"
#include "picture.h"
#include "data.h"
#include "version.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Dav1dContext Dav1dContext;
typedef struct Dav1dRef Dav1dRef;

#define DAV1D_MAX_THREADS 256
#define DAV1D_MAX_FRAME_DELAY 256

typedef struct Dav1dLogger {
    void *cookie;

    void (*callback)(void *cookie, const char *format, va_list ap);
} Dav1dLogger;

enum Dav1dInloopFilterType {
    DAV1D_INLOOPFILTER_NONE        = 0,
    DAV1D_INLOOPFILTER_DEBLOCK     = 1 << 0,
    DAV1D_INLOOPFILTER_CDEF        = 1 << 1,
    DAV1D_INLOOPFILTER_RESTORATION = 1 << 2,
    DAV1D_INLOOPFILTER_ALL = DAV1D_INLOOPFILTER_DEBLOCK |
                             DAV1D_INLOOPFILTER_CDEF |
                             DAV1D_INLOOPFILTER_RESTORATION,
};

enum Dav1dDecodeFrameType {
    DAV1D_DECODEFRAMETYPE_ALL   = 0,
    DAV1D_DECODEFRAMETYPE_REFERENCE = 1,
    DAV1D_DECODEFRAMETYPE_INTRA = 2,
    DAV1D_DECODEFRAMETYPE_KEY   = 3,
};

typedef struct Dav1dSettings {
    int n_threads;
    int max_frame_delay;
    int apply_grain;
    int operating_point;
    int all_layers;
    unsigned frame_size_limit;
    Dav1dPicAllocator allocator;
    Dav1dLogger logger;
    int strict_std_compliance;

    int output_invisible_frames;

    enum Dav1dInloopFilterType inloop_filters;

    enum Dav1dDecodeFrameType decode_frame_type;

    uint8_t reserved[16];
} Dav1dSettings;

DAV1D_API const char *dav1d_version(void);

DAV1D_API unsigned dav1d_version_api(void);

DAV1D_API void dav1d_default_settings(Dav1dSettings *s);

DAV1D_API int dav1d_open(Dav1dContext **c_out, const Dav1dSettings *s);

DAV1D_API int dav1d_parse_sequence_header(Dav1dSequenceHeader *out,
                                          const uint8_t *buf, const size_t sz);

DAV1D_API int dav1d_send_data(Dav1dContext *c, Dav1dData *in);

DAV1D_API int dav1d_get_picture(Dav1dContext *c, Dav1dPicture *out);

DAV1D_API int dav1d_apply_grain(Dav1dContext *c, Dav1dPicture *out,
                                const Dav1dPicture *in);

DAV1D_API void dav1d_close(Dav1dContext **c_out);

DAV1D_API void dav1d_flush(Dav1dContext *c);

enum Dav1dEventFlags {

    DAV1D_EVENT_FLAG_NEW_SEQUENCE =       1 << 0,

    DAV1D_EVENT_FLAG_NEW_OP_PARAMS_INFO = 1 << 1,
};

DAV1D_API int dav1d_get_event_flags(Dav1dContext *c, enum Dav1dEventFlags *flags);

DAV1D_API int dav1d_get_decode_error_data_props(Dav1dContext *c, Dav1dDataProps *out);

DAV1D_API int dav1d_get_frame_delay(const Dav1dSettings *s);

#ifdef __cplusplus
}
#endif

#endif
