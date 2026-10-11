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

 WebRtcIlbcfix_GainQuant.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/gain_quant.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

int16_t WebRtcIlbcfix_GainQuant(
    int16_t gain,
    int16_t maxIn,
    int16_t stage,
    int16_t *index
                                        ) {

  int16_t scale, cblen;
  int32_t gainW32, measure1, measure2;
  const int16_t *cbPtr, *cb;
  int loc, noMoves, noChecks, i;

  scale = WEBRTC_SPL_MAX(1638, maxIn);

  cb = WebRtcIlbcfix_kGain[stage];
  cblen = 32>>stage;
  noChecks = 4-stage;

  gainW32 = gain << 14;

  loc = cblen>>1;
  noMoves = loc;
  cbPtr = cb + loc;

  for (i=noChecks;i>0;i--) {
    noMoves>>=1;
    measure1 = scale * *cbPtr;

    measure1 = measure1 - gainW32;

    if (0>measure1) {
      cbPtr+=noMoves;
      loc+=noMoves;
    } else {
      cbPtr-=noMoves;
      loc-=noMoves;
    }
  }

  measure1 = scale * *cbPtr;
  if (gainW32>measure1) {

    measure2 = scale * cbPtr[1];
    if ((measure2-gainW32)<(gainW32-measure1)) {
      loc+=1;
    }
  } else {

    measure2 = scale * cbPtr[-1];
    if ((gainW32-measure2)<=(measure1-gainW32)) {
      loc-=1;
    }
  }

  loc=WEBRTC_SPL_MIN(loc, (cblen-1));
  *index=loc;

  return (int16_t)((scale * cb[loc] + 8192) >> 14);
}
