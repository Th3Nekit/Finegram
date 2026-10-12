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

 WebRtcIlbcfix_Lsf2Lsp.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/lsf_to_lsp.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_Lsf2Lsp(
    int16_t *lsf,
    int16_t *lsp,
    int16_t m
                           ) {
  int16_t i, k;
  int16_t diff;

  int16_t freq;
  int32_t tmpW32;

  for(i=0; i<m; i++)
  {
    freq = (int16_t)((lsf[i] * 20861) >> 15);

    k = freq >> 8;
    diff = (freq&0x00ff);

    if (k>63) {
      k = 63;
    }

    tmpW32 = WebRtcIlbcfix_kCosDerivative[k] * diff;
    lsp[i] = WebRtcIlbcfix_kCos[k] + (int16_t)(tmpW32 >> 12);
  }

  return;
}
