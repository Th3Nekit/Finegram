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

 WebRtcIlbcfix_CompCorr.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/comp_corr.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_CompCorr(
    int32_t *corr,
    int32_t *ener,
    int16_t *buffer,
    size_t lag,
    size_t bLen,
    size_t sRange,
    int16_t scale
                            ){
  int16_t *w16ptr;

  w16ptr=&buffer[bLen-sRange-lag];

  (*corr)=WebRtcSpl_DotProductWithScale(&buffer[bLen-sRange], w16ptr, sRange, scale);
  (*ener)=WebRtcSpl_DotProductWithScale(w16ptr, w16ptr, sRange, scale);

  if (*ener == 0) {
    *corr = 0;
    *ener = 1;
  }
}
