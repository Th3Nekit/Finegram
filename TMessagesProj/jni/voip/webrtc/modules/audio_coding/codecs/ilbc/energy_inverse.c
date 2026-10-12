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

 WebRtcIlbcfix_EnergyInverse.c

******************************************************************/

/* Inverses the in vector in into Q29 domain */

#include "modules/audio_coding/codecs/ilbc/energy_inverse.h"

void WebRtcIlbcfix_EnergyInverse(
    int16_t *energy,

    size_t noOfEnergies)

{
  int32_t Nom=(int32_t)0x1FFFFFFF;
  int16_t *energyPtr;
  size_t i;

  energyPtr=energy;
  for (i=0; i<noOfEnergies; i++) {
    (*energyPtr)=WEBRTC_SPL_MAX((*energyPtr),16384);
    energyPtr++;
  }

  energyPtr=energy;
  for (i=0; i<noOfEnergies; i++) {
    (*energyPtr) = (int16_t)WebRtcSpl_DivW32W16(Nom, (*energyPtr));
    energyPtr++;
  }
}
