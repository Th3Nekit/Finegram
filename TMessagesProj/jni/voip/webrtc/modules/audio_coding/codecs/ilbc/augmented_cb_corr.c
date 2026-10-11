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

 WebRtcIlbcfix_AugmentedCbCorr.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/augmented_cb_corr.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_AugmentedCbCorr(
    int16_t *target,
    int16_t *buffer,
    int16_t *interpSamples,

    int32_t *crossDot,

    size_t low,

    size_t high,
    int scale)

{
  size_t lagcount;
  size_t ilow;
  int16_t *targetPtr;
  int32_t *crossDotPtr;
  int16_t *iSPtr=interpSamples;

  crossDotPtr=crossDot;
  for (lagcount=low; lagcount<=high; lagcount++) {

    ilow = lagcount - 4;

    (*crossDotPtr) = WebRtcSpl_DotProductWithScale(target, buffer-lagcount, ilow, scale);

    (*crossDotPtr) += WebRtcSpl_DotProductWithScale(target+ilow, iSPtr, 4, scale);
    targetPtr = target + lagcount;
    iSPtr += lagcount-ilow;

    (*crossDotPtr) += WebRtcSpl_DotProductWithScale(targetPtr, buffer-lagcount, SUBL-lagcount, scale);
    crossDotPtr++;
  }
}
