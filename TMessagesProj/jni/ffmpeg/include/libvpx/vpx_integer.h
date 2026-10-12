/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VPX_VPX_VPX_INTEGER_H_
#define VPX_VPX_VPX_INTEGER_H_

#include <stddef.h>

#if defined(_MSC_VER)
#define VPX_FORCE_INLINE __forceinline
#define VPX_INLINE __inline
#else
#define VPX_FORCE_INLINE __inline__ __attribute__((always_inline))

#define VPX_INLINE inline
#endif

#if defined(__cplusplus)
#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif
#if !defined(__STDC_LIMIT_MACROS)
#define __STDC_LIMIT_MACROS
#endif
#endif

#include <inttypes.h>
#include <stdint.h>

#endif
