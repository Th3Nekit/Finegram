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

 WebRtcIlbcfix_CbSearch.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/cb_search.h"

#include "modules/audio_coding/codecs/ilbc/augmented_cb_corr.h"
#include "modules/audio_coding/codecs/ilbc/cb_mem_energy.h"
#include "modules/audio_coding/codecs/ilbc/cb_mem_energy_augmentation.h"
#include "modules/audio_coding/codecs/ilbc/cb_search_core.h"
#include "modules/audio_coding/codecs/ilbc/cb_update_best_index.h"
#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/create_augmented_vec.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"
#include "modules/audio_coding/codecs/ilbc/energy_inverse.h"
#include "modules/audio_coding/codecs/ilbc/filtered_cb_vecs.h"
#include "modules/audio_coding/codecs/ilbc/gain_quant.h"
#include "modules/audio_coding/codecs/ilbc/interpolate_samples.h"

void WebRtcIlbcfix_CbSearch(
    IlbcEncoder *iLBCenc_inst,

    int16_t *index,
    int16_t *gain_index,
    int16_t *intarget,
    int16_t *decResidual,
    size_t lMem,
    size_t lTarget,
    int16_t *weightDenum,
    size_t block
                            ) {
  size_t i, range;
  int16_t ii, j, stage;
  int16_t *pp;
  int16_t tmp;
  int scale;
  int16_t bits, temp1, temp2;
  size_t base_size;
  int32_t codedEner, targetEner;
  int16_t gains[CB_NSTAGES+1];
  int16_t *cb_vecPtr;
  size_t indexOffset, sInd, eInd;
  int32_t CritMax=0;
  int16_t shTotMax=WEBRTC_SPL_WORD16_MIN;
  size_t bestIndex=0;
  int16_t bestGain=0;
  size_t indexNew;
  int16_t CritNewSh;
  int32_t CritNew;
  int32_t *cDotPtr;
  size_t noOfZeros;
  int16_t *gainPtr;
  int32_t t32, tmpW32;
  int16_t *WebRtcIlbcfix_kGainSq5_ptr;

  int16_t CBbuf[CB_MEML+LPC_FILTERORDER+CB_HALFFILTERLEN];
  int32_t cDot[128];
  int32_t Crit[128];
  int16_t targetVec[SUBL+LPC_FILTERORDER];
  int16_t cbvectors[CB_MEML + 1];

  int16_t codedVec[SUBL];
  int16_t interpSamples[20*4];
  int16_t interpSamplesFilt[20*4];
  int16_t energyW16[CB_EXPAND*128];
  int16_t energyShifts[CB_EXPAND*128];
  int16_t *inverseEnergy=energyW16;
  int16_t *inverseEnergyShifts=energyShifts;
  int16_t *buf = &CBbuf[LPC_FILTERORDER];
  int16_t *target = &targetVec[LPC_FILTERORDER];
  int16_t *aug_vec = (int16_t*)cDot;

  base_size=lMem-lTarget+1;
  if (lTarget==SUBL) {
    base_size=lMem-19;
  }

  noOfZeros=lMem-WebRtcIlbcfix_kFilterRange[block];
  WebRtcSpl_MemSetW16(&buf[-LPC_FILTERORDER], 0, noOfZeros+LPC_FILTERORDER);
  WebRtcSpl_FilterARFastQ12(
      decResidual+noOfZeros, buf+noOfZeros,
      weightDenum, LPC_FILTERORDER+1, WebRtcIlbcfix_kFilterRange[block]);

  WEBRTC_SPL_MEMCPY_W16(&target[-LPC_FILTERORDER], buf+noOfZeros+WebRtcIlbcfix_kFilterRange[block]-LPC_FILTERORDER, LPC_FILTERORDER);
  WebRtcSpl_FilterARFastQ12(
      intarget, target,
      weightDenum, LPC_FILTERORDER+1, lTarget);

  WEBRTC_SPL_MEMCPY_W16(codedVec, target, lTarget);

  temp1 = WebRtcSpl_MaxAbsValueW16(buf, lMem);
  temp2 = WebRtcSpl_MaxAbsValueW16(target, lTarget);

  if ((temp1>0)&&(temp2>0)) {
    temp1 = WEBRTC_SPL_MAX(temp1, temp2);
    scale = WebRtcSpl_GetSizeInBits((uint32_t)(temp1 * temp1));
  } else {

    scale = 30;
  }

  scale = scale - 25;
  scale = WEBRTC_SPL_MAX(0, scale);

  targetEner = WebRtcSpl_DotProductWithScale(target, target, lTarget, scale);

  WebRtcIlbcfix_FilteredCbVecs(cbvectors, buf, lMem, WebRtcIlbcfix_kFilterRange[block]);

  range = WebRtcIlbcfix_kSearchRange[block][0];

  if(lTarget == SUBL) {

    WebRtcIlbcfix_InterpolateSamples(interpSamples, buf, lMem);

    WebRtcIlbcfix_InterpolateSamples(interpSamplesFilt, cbvectors, lMem);

    WebRtcIlbcfix_CbMemEnergyAugmentation(interpSamples, buf,
                                          scale, 20, energyW16, energyShifts);

    WebRtcIlbcfix_CbMemEnergyAugmentation(interpSamplesFilt, cbvectors, scale,
                                          base_size + 20, energyW16,
                                          energyShifts);

    WebRtcIlbcfix_CbMemEnergy(range, buf, cbvectors, lMem,
                              lTarget, energyW16+20, energyShifts+20, scale, base_size);

  } else {

    WebRtcIlbcfix_CbMemEnergy(range, buf, cbvectors, lMem,
                              lTarget, energyW16, energyShifts, scale, base_size);

    WebRtcSpl_MemSetW16(energyW16+range, 0, (base_size-range));
    WebRtcSpl_MemSetW16(energyW16+range+base_size, 0, (base_size-range));
  }

  WebRtcIlbcfix_EnergyInverse(energyW16, base_size*CB_EXPAND);

  gains[0] = 16384;

  for (stage=0; stage<CB_NSTAGES; stage++) {

    range = WebRtcIlbcfix_kSearchRange[block][stage];

    CritMax=0;
    shTotMax=-100;
    bestIndex=0;
    bestGain=0;

    cb_vecPtr = buf+lMem-lTarget;

    if (lTarget==SUBL) {
      WebRtcIlbcfix_AugmentedCbCorr(target, buf+lMem,
                                    interpSamples, cDot,
                                    20, 39, scale);
      cDotPtr=&cDot[20];
    } else {
      cDotPtr=cDot;
    }

    WebRtcSpl_CrossCorrelation(cDotPtr, target, cb_vecPtr, lTarget, range, scale, -1);

    if (lTarget==SUBL) {
      range=WebRtcIlbcfix_kSearchRange[block][stage]+20;
    } else {
      range=WebRtcIlbcfix_kSearchRange[block][stage];
    }

    indexOffset=0;

    WebRtcIlbcfix_CbSearchCore(
        cDot, range, stage, inverseEnergy,
        inverseEnergyShifts, Crit,
        &indexNew, &CritNew, &CritNewSh);

    WebRtcIlbcfix_CbUpdateBestIndex(
        CritNew, CritNewSh, indexNew+indexOffset, cDot[indexNew+indexOffset],
        inverseEnergy[indexNew+indexOffset], inverseEnergyShifts[indexNew+indexOffset],
        &CritMax, &shTotMax, &bestIndex, &bestGain);

    sInd = ((CB_RESRANGE >> 1) > bestIndex) ?
        0 : (bestIndex - (CB_RESRANGE >> 1));
    eInd=sInd+CB_RESRANGE;
    if (eInd>=range) {
      eInd=range-1;
      sInd=eInd-CB_RESRANGE;
    }

    range = WebRtcIlbcfix_kSearchRange[block][stage];

    if (lTarget==SUBL) {
      i=sInd;
      if (sInd<20) {
        WebRtcIlbcfix_AugmentedCbCorr(target, cbvectors + lMem,
                                      interpSamplesFilt, cDot, sInd + 20,
                                      WEBRTC_SPL_MIN(39, (eInd + 20)), scale);
        i=20;
        cDotPtr = &cDot[20 - sInd];
      } else {
        cDotPtr = cDot;
      }

      cb_vecPtr = cbvectors+lMem-20-i;

      WebRtcSpl_CrossCorrelation(cDotPtr, target, cb_vecPtr, lTarget,
                                 eInd - i + 1, scale, -1);

    } else {
      cDotPtr = cDot;
      cb_vecPtr = cbvectors+lMem-lTarget-sInd;

      WebRtcSpl_CrossCorrelation(cDotPtr, target, cb_vecPtr, lTarget,
                                 eInd - sInd + 1, scale, -1);

    }

    indexOffset=base_size+sInd;

    WebRtcIlbcfix_CbSearchCore(
        cDot, eInd-sInd+1, stage, inverseEnergy+indexOffset,
        inverseEnergyShifts+indexOffset, Crit,
        &indexNew, &CritNew, &CritNewSh);

    WebRtcIlbcfix_CbUpdateBestIndex(
        CritNew, CritNewSh, indexNew+indexOffset, cDot[indexNew],
        inverseEnergy[indexNew+indexOffset], inverseEnergyShifts[indexNew+indexOffset],
        &CritMax, &shTotMax, &bestIndex, &bestGain);

    index[stage] = (int16_t)bestIndex;

    bestGain = WebRtcIlbcfix_GainQuant(bestGain,
                                       (int16_t)WEBRTC_SPL_ABS_W16(gains[stage]), stage, &gain_index[stage]);

    if(lTarget==(STATE_LEN-iLBCenc_inst->state_short_len)) {

      if((size_t)index[stage]<base_size) {
        pp=buf+lMem-lTarget-index[stage];
      } else {
        pp=cbvectors+lMem-lTarget-
            index[stage]+base_size;
      }

    } else {

      if ((size_t)index[stage]<base_size) {
        if (index[stage]>=20) {

          index[stage]-=20;
          pp=buf+lMem-lTarget-index[stage];
        } else {

          index[stage]+=(int16_t)(base_size-20);

          WebRtcIlbcfix_CreateAugmentedVec(index[stage]-base_size+40,
                                           buf+lMem, aug_vec);
          pp = aug_vec;

        }
      } else {

        if ((index[stage] - base_size) >= 20) {

          index[stage]-=20;
          pp=cbvectors+lMem-lTarget-
              index[stage]+base_size;
        } else {

          index[stage]+=(int16_t)(base_size-20);
          WebRtcIlbcfix_CreateAugmentedVec(index[stage]-2*base_size+40,
                                           cbvectors+lMem, aug_vec);
          pp = aug_vec;
        }
      }
    }

    WebRtcSpl_AddAffineVectorToVector(target, pp, (int16_t)(-bestGain),
                                      (int32_t)8192, (int16_t)14, lTarget);

    gains[stage+1] = bestGain;

  }

  for (i=0;i<lTarget;i++) {
    codedVec[i]-=target[i];
  }

  codedEner = WebRtcSpl_DotProductWithScale(codedVec, codedVec, lTarget, scale);

  j=gain_index[0];

  temp1 = (int16_t)WebRtcSpl_NormW32(codedEner);
  temp2 = (int16_t)WebRtcSpl_NormW32(targetEner);

  if(temp1 < temp2) {
    bits = 16 - temp1;
  } else {
    bits = 16 - temp2;
  }

  tmp = (int16_t)((gains[1] * gains[1]) >> 14);

  targetEner = (int16_t)WEBRTC_SPL_SHIFT_W32(targetEner, -bits) * tmp;

  tmpW32 = ((int32_t)(gains[1]-1))<<1;

  gainPtr=(int16_t*)WebRtcIlbcfix_kGainSq5Sq+gain_index[0];
  temp1 = (int16_t)WEBRTC_SPL_SHIFT_W32(codedEner, -bits);

  WebRtcIlbcfix_kGainSq5_ptr = (int16_t*)&WebRtcIlbcfix_kGainSq5[j];

  for (ii=gain_index[0];ii<32;ii++) {

    t32 = temp1 * *gainPtr;
    t32 = t32 - targetEner;
    if (t32 < 0) {
      if ((*WebRtcIlbcfix_kGainSq5_ptr) < tmpW32) {
        j=ii;
        WebRtcIlbcfix_kGainSq5_ptr = (int16_t*)&WebRtcIlbcfix_kGainSq5[ii];
      }
    }
    gainPtr++;
  }
  gain_index[0]=j;

  return;
}
