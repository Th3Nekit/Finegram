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

 WebRtcIlbcfix_BwExpand.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/bw_expand.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_BwExpand(
    int16_t *out,
    int16_t *in,

    int16_t *coef,
    int16_t length
                            ) {
  int i;

  out[0] = in[0];
  for (i = 1; i < length; i++) {

    out[i] = (int16_t)((coef[i] * in[i] + 16384) >> 15);
  }
}
