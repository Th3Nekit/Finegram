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

 WebRtcIlbcfix_Lsp2Lsf.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/lsp_to_lsf.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_Lsp2Lsf(
    int16_t *lsp,
    int16_t *lsf,

    int16_t m
                           )
{
  int16_t i, k;
  int16_t diff;
  int16_t freq;
  int16_t *lspPtr, *lsfPtr, *cosTblPtr;
  int16_t tmp;

  k = 63;

  lspPtr = &lsp[9];
  lsfPtr = &lsf[9];
  cosTblPtr=(int16_t*)&WebRtcIlbcfix_kCos[k];
  for(i=m-1; i>=0; i--)
  {

    while( (((int32_t)(*cosTblPtr)-(*lspPtr)) < 0)&&(k>0) )
    {
      k-=1;
      cosTblPtr--;
    }

    diff = (*lspPtr)-(*cosTblPtr);

    tmp = (int16_t)((WebRtcIlbcfix_kAcosDerivative[k] * diff) >> 11);

    freq = (k << 9) + tmp;

    (*lsfPtr) = (int16_t)(((int32_t)freq*25736)>>15);

    lsfPtr--;
    lspPtr--;
  }

  return;
}
