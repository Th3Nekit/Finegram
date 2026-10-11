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

 WebRtcIlbcfix_EnhUpsample.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/enh_upsample.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_EnhUpsample(
    int32_t *useq1,
    int16_t *seq1
                                ){
  int j;
  int32_t *pu1, *pu11;
  int16_t *ps, *w16tmp;
  const int16_t *pp;

  pu1=useq1;
  for (j=0;j<ENH_UPS0; j++) {
    pu11=pu1;

    pp=WebRtcIlbcfix_kEnhPolyPhaser[j]+1;
    ps=seq1+2;
    *pu11 = (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    pu11+=ENH_UPS0;

    pp=WebRtcIlbcfix_kEnhPolyPhaser[j]+1;
    ps=seq1+3;
    *pu11 = (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    pu11+=ENH_UPS0;

    pp=WebRtcIlbcfix_kEnhPolyPhaser[j]+1;
    ps=seq1+4;
    *pu11 = (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    pu1++;
  }

  pu1 = useq1 + 12;
  w16tmp = seq1+4;
  for (j=0;j<ENH_UPS0; j++) {
    pu11 = pu1;

    pp = WebRtcIlbcfix_kEnhPolyPhaser[j]+2;
    ps = w16tmp;
    *pu11 = (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    pu11+=ENH_UPS0;

    pp = WebRtcIlbcfix_kEnhPolyPhaser[j]+3;
    ps = w16tmp;
    *pu11 = (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    *pu11 += (*ps--) * *pp++;
    pu11+=ENH_UPS0;

    pu1++;
  }
}
