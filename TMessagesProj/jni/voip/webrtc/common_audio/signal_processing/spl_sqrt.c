/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the function WebRtcSpl_Sqrt().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "rtc_base/checks.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"

int32_t WebRtcSpl_SqrtLocal(int32_t in);

int32_t WebRtcSpl_SqrtLocal(int32_t in)
{

    int16_t x_half, t16;
    int32_t A, B, x2;

    B = in / 2;

    B = B - ((int32_t)0x40000000);
    x_half = (int16_t)(B >> 16);
    B = B + ((int32_t)0x40000000);
    B = B + ((int32_t)0x40000000);

    x2 = ((int32_t)x_half) * ((int32_t)x_half) * 2;
    A = -x2;
    B = B + (A >> 1);

    A >>= 16;
    A = A * A * 2;
    t16 = (int16_t)(A >> 16);
    B += -20480 * t16 * 2;

    A = x_half * t16 * 2;
    t16 = (int16_t)(A >> 16);
    B += 28672 * t16 * 2;

    t16 = (int16_t)(x2 >> 16);
    A = x_half * t16 * 2;

    B = B + (A >> 1);

    B = B + ((int32_t)32768);

    return B;
}

int32_t WebRtcSpl_Sqrt(int32_t value)
{

    int16_t x_norm, nshift, t16, sh;
    int32_t A;

    int16_t k_sqrt_2 = 23170;

    A = value;

    if (A < 0) {
        if (A == WEBRTC_SPL_WORD32_MIN) {

            A = WEBRTC_SPL_WORD32_MAX;
        } else {
            A = -A;
        }
    } else if (A == 0) {
        return 0;
    }

    sh = WebRtcSpl_NormW32(A);
    A = WEBRTC_SPL_LSHIFT_W32(A, sh);
    if (A < (WEBRTC_SPL_WORD32_MAX - 32767))
    {
        A = A + ((int32_t)32768);
    } else
    {
        A = WEBRTC_SPL_WORD32_MAX;
    }

    x_norm = (int16_t)(A >> 16);

    nshift = (sh / 2);
    RTC_DCHECK_GE(nshift, 0);

    A = (int32_t)WEBRTC_SPL_LSHIFT_W32((int32_t)x_norm, 16);
    A = WEBRTC_SPL_ABS_W32(A);
    A = WebRtcSpl_SqrtLocal(A);

    if (2 * nshift == sh) {

        t16 = (int16_t)(A >> 16);

        A = k_sqrt_2 * t16 * 2;
        A = A + ((int32_t)32768);
        A = A & ((int32_t)0x7fff0000);

        A >>= 15;

    } else
    {
        A >>= 16;
    }

    A = A & ((int32_t)0x0000ffff);
    A >>= nshift;

    return A;
}
