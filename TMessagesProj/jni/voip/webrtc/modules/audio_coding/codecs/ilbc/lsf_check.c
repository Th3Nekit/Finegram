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

 WebRtcIlbcfix_LsfCheck.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/lsf_check.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

int WebRtcIlbcfix_LsfCheck(
    int16_t *lsf,
    int dim,
    int NoAn)
{
  int k,n,m, Nit=2, change=0,pos;
  const int16_t eps=319;
  const int16_t eps2=160;
  const int16_t maxlsf=25723;
  const int16_t minlsf=82;

  for (n=0;n<Nit;n++) {
    for (m=0;m<NoAn;m++) {
      for (k=0;k<(dim-1);k++) {
        pos=m*dim+k;

        if ((lsf[pos+1]-lsf[pos])<eps) {

          if (lsf[pos+1]<lsf[pos]) {
            lsf[pos+1]= lsf[pos]+eps2;
            lsf[pos]= lsf[pos+1]-eps2;
          } else {
            lsf[pos]-=eps2;
            lsf[pos+1]+=eps2;
          }
          change=1;
        }

        if (lsf[pos]<minlsf) {
          lsf[pos]=minlsf;
          change=1;
        }

        if (lsf[pos]>maxlsf) {
          lsf[pos]=maxlsf;
          change=1;
        }
      }
    }
  }

  return change;
}
