/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_Window32W32.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/window32_w32.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_Window32W32(
    int32_t *z,
    int32_t *x,
    const int32_t  *y,
    size_t N
                               ) {
  size_t i;
  int16_t x_low, x_hi, y_low, y_hi;
  int16_t left_shifts;
  int32_t temp;

  left_shifts = (int16_t)WebRtcSpl_NormW32(x[0]);
  WebRtcSpl_VectorBitShiftW32(x, N, x, (int16_t)(-left_shifts));

  for (i = 0; i < N; i++) {

    x_hi = (int16_t)(x[i] >> 16);
    y_hi = (int16_t)(y[i] >> 16);

    x_low = (int16_t)((x[i] - (x_hi << 16)) >> 1);

    y_low = (int16_t)((y[i] - (y_hi << 16)) >> 1);

    temp = ((x_hi * y_hi) << 1) + ((x_hi * y_low) >> 14);

    z[i] = temp + ((x_low * y_hi) >> 14);
  }

  WebRtcSpl_VectorBitShiftW32(z, N, z, left_shifts);

  return;
}
