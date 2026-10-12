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

 WebRtcIlbcfix_Chebyshev.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/chebyshev.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

int16_t WebRtcIlbcfix_Chebyshev(

    int16_t x,
    int16_t *f
                                      ) {
  int16_t b1_high, b1_low;
  int32_t b2;
  int32_t tmp1W32;
  int32_t tmp2W32;
  int i;

  b2 = (int32_t)0x1000000;

  tmp1W32 = (x << 10) + (f[1] << 14);

  for (i = 2; i < 5; i++) {
    tmp2W32 = tmp1W32;

    b1_high = (int16_t)(tmp1W32 >> 16);
    b1_low = (int16_t)((tmp1W32 - ((int32_t)b1_high << 16)) >> 1);

    tmp1W32 = ((b1_high * x + ((b1_low * x) >> 15)) << 2) - b2 + (f[i] << 14);

    b2 = tmp2W32;
  }

  b1_high = (int16_t)(tmp1W32 >> 16);
  b1_low = (int16_t)((tmp1W32 - ((int32_t)b1_high << 16)) >> 1);

  tmp1W32 = ((b1_high * x) << 1) + (((b1_low * x) >> 15) << 1) -
      b2 + (f[i] << 13);

  if (tmp1W32>((int32_t)33553408)) {
    return(WEBRTC_SPL_WORD16_MAX);
  } else if (tmp1W32<((int32_t)-33554432)) {
    return(WEBRTC_SPL_WORD16_MIN);
  } else {
    return (int16_t)(tmp1W32 >> 10);
  }
}
