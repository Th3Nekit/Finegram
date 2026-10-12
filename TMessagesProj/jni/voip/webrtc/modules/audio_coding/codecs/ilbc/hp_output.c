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

 WebRtcIlbcfix_HpOutput.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/hp_output.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_HpOutput(
    int16_t *signal,
    int16_t *ba,

    int16_t *y,

    int16_t *x,
    size_t len)
{
  size_t i;
  int32_t tmpW32;
  int32_t tmpW32b;

  for (i=0; i<len; i++) {

    tmpW32 = y[1] * ba[3];
    tmpW32 += y[3] * ba[4];
    tmpW32 = (tmpW32>>15);
    tmpW32 += y[0] * ba[3];
    tmpW32 += y[2] * ba[4];
    tmpW32 *= 2;

    tmpW32 += signal[i] * ba[0];
    tmpW32 += x[0] * ba[1];
    tmpW32 += x[1] * ba[2];

    x[1] = x[0];
    x[0] = signal[i];

    tmpW32b = tmpW32 + 1024;

    tmpW32b = WEBRTC_SPL_SAT((int32_t)67108863, tmpW32b, (int32_t)-67108864);

    signal[i] = (int16_t)(tmpW32b >> 11);

    y[2] = y[0];
    y[3] = y[1];

    if (tmpW32>268435455) {
      tmpW32 = WEBRTC_SPL_WORD32_MAX;
    } else if (tmpW32<-268435456) {
      tmpW32 = WEBRTC_SPL_WORD32_MIN;
    } else {
      tmpW32 *= 8;
    }

    y[0] = (int16_t)(tmpW32 >> 16);
    y[1] = (int16_t)((tmpW32 & 0xffff) >> 1);

  }

  return;
}
