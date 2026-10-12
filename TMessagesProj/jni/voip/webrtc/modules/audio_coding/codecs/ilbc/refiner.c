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

 WebRtcIlbcfix_Refiner.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/refiner.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/enh_upsample.h"
#include "modules/audio_coding/codecs/ilbc/my_corr.h"

void WebRtcIlbcfix_Refiner(
    size_t *updStartPos,
    int16_t *idata,
    size_t idatal,
    size_t centerStartPos,
    size_t estSegPos,
    int16_t *surround,

    int16_t gain
                           ){
  size_t estSegPosRounded, searchSegStartPos, searchSegEndPos, corrdim;
  size_t tloc, tloc2, i;

  int32_t maxtemp, scalefact;
  int16_t *filtStatePtr, *polyPtr;

  int16_t filt[7];
  int32_t corrVecUps[ENH_CORRDIM*ENH_UPS0];
  int32_t corrVecTemp[ENH_CORRDIM];
  int16_t vect[ENH_VECTL];
  int16_t corrVec[ENH_CORRDIM];

  estSegPosRounded = (estSegPos - 2) >> 2;

  searchSegStartPos =
      (estSegPosRounded < ENH_SLOP) ? 0 : (estSegPosRounded - ENH_SLOP);

  searchSegEndPos = estSegPosRounded + ENH_SLOP;
  if ((searchSegEndPos + ENH_BLOCKL) >= idatal) {
    searchSegEndPos = idatal - ENH_BLOCKL - 1;
  }

  corrdim = searchSegEndPos + 1 - searchSegStartPos;

  WebRtcIlbcfix_MyCorr(corrVecTemp, idata + searchSegStartPos,
                       corrdim + ENH_BLOCKL - 1, idata + centerStartPos,
                       ENH_BLOCKL);

  maxtemp = WebRtcSpl_MaxAbsValueW32(corrVecTemp, corrdim);

  scalefact = WebRtcSpl_GetSizeInBits(maxtemp) - 15;

  if (scalefact > 0) {
    for (i = 0; i < corrdim; i++) {
      corrVec[i] = (int16_t)(corrVecTemp[i] >> scalefact);
    }
  } else {
    for (i = 0; i < corrdim; i++) {
      corrVec[i] = (int16_t)corrVecTemp[i];
    }
  }

  for (i = corrdim; i < ENH_CORRDIM; i++) {
    corrVec[i] = 0;
  }

  WebRtcIlbcfix_EnhUpsample(corrVecUps, corrVec);

  tloc = WebRtcSpl_MaxIndexW32(corrVecUps, ENH_UPS0 * corrdim);

  *updStartPos = searchSegStartPos * 4 + tloc + 4;

  tloc2 = (tloc + 3) >> 2;

  if (ENH_FL0 > (searchSegStartPos + tloc2)) {
    const size_t st = ENH_FL0 - searchSegStartPos - tloc2;
    WebRtcSpl_MemSetW16(vect, 0, st);
    WEBRTC_SPL_MEMCPY_W16(&vect[st], idata, ENH_VECTL - st);
  } else {
    const size_t st = searchSegStartPos + tloc2 - ENH_FL0;
    if ((st + ENH_VECTL) > idatal) {
      const size_t en = st + ENH_VECTL - idatal;
      WEBRTC_SPL_MEMCPY_W16(vect, &idata[st], ENH_VECTL - en);
      WebRtcSpl_MemSetW16(&vect[ENH_VECTL - en], 0, en);
    } else {
      WEBRTC_SPL_MEMCPY_W16(vect, &idata[st], ENH_VECTL);
    }
  }

  filtStatePtr = filt + 6;
  polyPtr = (int16_t*)WebRtcIlbcfix_kEnhPolyPhaser[tloc2 * ENH_UPS0 - tloc];
  for (i = 0; i < 7; i++) {
    *filtStatePtr-- = *polyPtr++;
  }

  WebRtcSpl_FilterMAFastQ12(&vect[6], vect, filt, ENH_FLO_MULT2_PLUS1,
                            ENH_BLOCKL);

  WebRtcSpl_AddAffineVectorToVector(surround, vect, gain, 32768, 16,
                                    ENH_BLOCKL);

  return;
}
