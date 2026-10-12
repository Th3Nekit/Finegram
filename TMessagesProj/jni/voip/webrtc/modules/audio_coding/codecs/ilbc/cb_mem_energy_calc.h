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

 WebRtcIlbcfix_CbMemEnergyCalc.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_MEM_ENERGY_CALC_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_MEM_ENERGY_CALC_H_

#include <stddef.h>
#include <stdint.h>

void WebRtcIlbcfix_CbMemEnergyCalc(
    int32_t energy,                                    
    size_t range,                                        
    int16_t* ppi,                                   
    int16_t* ppo,                                   
    int16_t* energyW16,                                      
    int16_t* energyShifts,                                    
    int scale,                                                       
    size_t base_size                                                        
);

#endif
