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

 WebRtcIlbcfix_StateConstruct.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/state_construct.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_StateConstruct(
    size_t idxForMax,

    int16_t *idxVec,
    int16_t *syntDenum,
    int16_t *Out_fix,
    size_t len
                                  ) {
  size_t k;
  int16_t maxVal;
  int16_t *tmp1, *tmp2, *tmp3;

  int16_t numerator[1+LPC_FILTERORDER];
  int16_t sampleValVec[2*STATE_SHORT_LEN_30MS+LPC_FILTERORDER];
  int16_t sampleMaVec[2*STATE_SHORT_LEN_30MS+LPC_FILTERORDER];
  int16_t *sampleVal = &sampleValVec[LPC_FILTERORDER];
  int16_t *sampleMa = &sampleMaVec[LPC_FILTERORDER];
  int16_t *sampleAr = &sampleValVec[LPC_FILTERORDER];

  for (k=0; k<LPC_FILTERORDER+1; k++){
    numerator[k] = syntDenum[LPC_FILTERORDER-k];
  }

  maxVal = WebRtcIlbcfix_kFrgQuantMod[idxForMax];

  tmp1 = sampleVal;
  tmp2 = &idxVec[len-1];

  if (idxForMax<37) {
    for(k=0; k<len; k++){

      *tmp1 = (int16_t)((maxVal * WebRtcIlbcfix_kStateSq3[*tmp2] + 2097152) >>
          22);
      tmp1++;
      tmp2--;
    }
  } else if (idxForMax<59) {
    for(k=0; k<len; k++){

      *tmp1 = (int16_t)((maxVal * WebRtcIlbcfix_kStateSq3[*tmp2] + 262144) >>
          19);
      tmp1++;
      tmp2--;
    }
  } else {
    for(k=0; k<len; k++){

      *tmp1 = (int16_t)((maxVal * WebRtcIlbcfix_kStateSq3[*tmp2] + 65536) >>
          17);
      tmp1++;
      tmp2--;
    }
  }

  WebRtcSpl_MemSetW16(&sampleVal[len], 0, len);

  WebRtcSpl_MemSetW16(sampleValVec, 0, (LPC_FILTERORDER));

  WebRtcSpl_FilterMAFastQ12(
      sampleVal, sampleMa,
      numerator, LPC_FILTERORDER+1, len + LPC_FILTERORDER);
  WebRtcSpl_MemSetW16(&sampleMa[len + LPC_FILTERORDER], 0, (len - LPC_FILTERORDER));
  WebRtcSpl_FilterARFastQ12(
      sampleMa, sampleAr,
      syntDenum, LPC_FILTERORDER+1, 2 * len);

  tmp1 = &sampleAr[len-1];
  tmp2 = &sampleAr[2*len-1];
  tmp3 = Out_fix;
  for(k=0;k<len;k++){
    (*tmp3) = (*tmp1) + (*tmp2);
    tmp1--;
    tmp2--;
    tmp3++;
  }
}
