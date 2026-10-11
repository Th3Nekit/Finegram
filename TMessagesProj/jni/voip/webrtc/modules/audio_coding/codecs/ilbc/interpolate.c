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

 WebRtcIlbcfix_Interpolate.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/interpolate.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_Interpolate(
    int16_t *out,
    int16_t *in1,
    int16_t *in2,
    int16_t coef,
    int16_t length)
{
  int i;
  int16_t invcoef;

  invcoef = 16384 - coef;
  for (i = 0; i < length; i++) {
    out[i] = (int16_t)((coef * in1[i] + invcoef * in2[i] + 8192) >> 14);
  }

  return;
}
