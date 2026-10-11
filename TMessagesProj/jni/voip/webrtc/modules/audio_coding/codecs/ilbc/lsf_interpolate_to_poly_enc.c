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

 WebRtcIlbcfix_LsfInterpolate2PloyEnc.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/lsf_interpolate_to_poly_enc.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"
#include "modules/audio_coding/codecs/ilbc/interpolate.h"
#include "modules/audio_coding/codecs/ilbc/lsf_to_poly.h"

void WebRtcIlbcfix_LsfInterpolate2PloyEnc(
    int16_t *a,
    int16_t *lsf1,
    int16_t *lsf2,
    int16_t coef,

    int16_t length
                                          ) {

  int16_t lsftmp[LPC_FILTERORDER];

  WebRtcIlbcfix_Interpolate(lsftmp, lsf1, lsf2, coef, length);

  WebRtcIlbcfix_Lsf2Poly(a, lsftmp);

  return;
}
