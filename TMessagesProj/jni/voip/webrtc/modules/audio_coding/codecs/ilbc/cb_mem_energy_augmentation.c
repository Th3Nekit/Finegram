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

 WebRtcIlbcfix_CbMemEnergyAugmentation.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/cb_mem_energy_augmentation.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_CbMemEnergyAugmentation(
    int16_t *interpSamples,
    int16_t *CBmem,
    int scale,
    size_t base_size,
    int16_t *energyW16,
    int16_t *energyShifts
                                           ){
  int32_t energy, tmp32;
  int16_t *ppe, *pp, *interpSamplesPtr;
  int16_t *CBmemPtr;
  size_t lagcount;
  int16_t *enPtr=&energyW16[base_size-20];
  int16_t *enShPtr=&energyShifts[base_size-20];
  int32_t nrjRecursive;

  CBmemPtr = CBmem+147;
  interpSamplesPtr = interpSamples;

  nrjRecursive = WebRtcSpl_DotProductWithScale( CBmemPtr-19, CBmemPtr-19, 15, scale);
  ppe = CBmemPtr - 20;

  for (lagcount=20; lagcount<=39; lagcount++) {

    nrjRecursive += (*ppe * *ppe) >> scale;
    ppe--;
    energy = nrjRecursive;

    energy += WebRtcSpl_DotProductWithScale(interpSamplesPtr, interpSamplesPtr, 4, scale);
    interpSamplesPtr += 4;

    pp = CBmemPtr - lagcount;
    energy += WebRtcSpl_DotProductWithScale(pp, pp, SUBL-lagcount, scale);

    (*enShPtr) = (int16_t)WebRtcSpl_NormW32(energy);
    tmp32 = energy << *enShPtr;
    *enPtr = (int16_t)(tmp32 >> 16);
    enShPtr++;
    enPtr++;
  }
}
