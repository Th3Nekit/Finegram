/*
 * Copyright © 2018, VideoLAN and dav1d authors
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

#ifndef DAV1D_COMMON_H
#define DAV1D_COMMON_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DAV1D_API
    #if defined _WIN32
      #if defined DAV1D_BUILDING_DLL
        #define DAV1D_API __declspec(dllexport)
      #else
        #define DAV1D_API
      #endif
    #elif defined __OS2__
        #define DAV1D_API __declspec(dllexport)
    #else
      #if __GNUC__ >= 4
        #define DAV1D_API __attribute__ ((visibility ("default")))
      #else
        #define DAV1D_API
      #endif
    #endif
#endif

#if EPERM > 0
#define DAV1D_ERR(e) (-(e)) ///< Negate POSIX error code.
#else
#define DAV1D_ERR(e) (e)
#endif

typedef struct Dav1dUserData {
    const uint8_t *data;
    struct Dav1dRef *ref;
} Dav1dUserData;

typedef struct Dav1dDataProps {
    int64_t timestamp;
    int64_t duration;
    int64_t offset;
    size_t size;
    struct Dav1dUserData user_data;
} Dav1dDataProps;

DAV1D_API void dav1d_data_props_unref(Dav1dDataProps *props);

#ifdef __cplusplus
}
#endif

#endif
