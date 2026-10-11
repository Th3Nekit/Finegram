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

 WebRtcIlbcfix_CbMemEnergy.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/cb_mem_energy.h"

#include "modules/audio_coding/codecs/ilbc/cb_mem_energy_calc.h"
#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_CbMemEnergy(
    size_t range,
    int16_t *CB,
    int16_t *filteredCB,
    size_t lMem,
    size_t lTarget,
    int16_t *energyW16,
    int16_t *energyShifts,
    int scale,
    size_t base_size
                               ) {
  int16_t *ppi, *ppo, *pp;
  int32_t energy, tmp32;

  ppi = CB+lMem-lTarget-1;
  ppo = CB+lMem-1;

  pp=CB+lMem-lTarget;
  energy = WebRtcSpl_DotProductWithScale( pp, pp, lTarget, scale);

  energyShifts[0] = (int16_t)WebRtcSpl_NormW32(energy);
  tmp32 = energy << energyShifts[0];
  energyW16[0] = (int16_t)(tmp32 >> 16);

  WebRtcIlbcfix_CbMemEnergyCalc(energy, range, ppi, ppo, energyW16, energyShifts, scale, 0);

  energy=0;
  pp=filteredCB+lMem-lTarget;

  energy = WebRtcSpl_DotProductWithScale( pp, pp, lTarget, scale);

  energyShifts[base_size] = (int16_t)WebRtcSpl_NormW32(energy);
  tmp32 = energy << energyShifts[base_size];
  energyW16[base_size] = (int16_t)(tmp32 >> 16);

  ppi = filteredCB + lMem - 1 - lTarget;
  ppo = filteredCB + lMem - 1;

  WebRtcIlbcfix_CbMemEnergyCalc(energy, range, ppi, ppo, energyW16, energyShifts, scale, base_size);
}
