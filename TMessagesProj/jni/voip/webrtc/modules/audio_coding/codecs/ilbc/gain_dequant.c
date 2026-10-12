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

 WebRtcIlbcfix_GainDequant.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/gain_dequant.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

int16_t WebRtcIlbcfix_GainDequant(

    int16_t index,
    int16_t maxIn,
    int16_t stage
                                                ){
  int16_t scale;
  const int16_t *gain;

  scale=WEBRTC_SPL_ABS_W16(maxIn);
  scale = WEBRTC_SPL_MAX(1638, scale);

  gain = WebRtcIlbcfix_kGain[stage];

  return (int16_t)((scale * gain[index] + 8192) >> 14);
}
