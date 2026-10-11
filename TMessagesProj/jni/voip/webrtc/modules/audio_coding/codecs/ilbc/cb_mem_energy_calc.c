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

 WebRtcIlbcfix_CbMemEnergyCalc.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/cb_mem_energy_calc.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_CbMemEnergyCalc(
    int32_t energy,
    size_t range,
    int16_t *ppi,
    int16_t *ppo,
    int16_t *energyW16,
    int16_t *energyShifts,
    int scale,
    size_t base_size
                                   )
{
  size_t j;
  int16_t shft;
  int32_t tmp;
  int16_t *eSh_ptr;
  int16_t *eW16_ptr;

  eSh_ptr  = &energyShifts[1+base_size];
  eW16_ptr = &energyW16[1+base_size];

  for (j = 0; j + 1 < range; j++) {

    tmp = (*ppi) * (*ppi) - (*ppo) * (*ppo);
    energy += tmp >> scale;
    energy = WEBRTC_SPL_MAX(energy, 0);

    ppi--;
    ppo--;

    shft = (int16_t)WebRtcSpl_NormW32(energy);
    *eSh_ptr++ = shft;

    tmp = energy << shft;
    *eW16_ptr++ = (int16_t)(tmp >> 16);
  }
}
