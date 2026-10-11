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

 WebRtcIlbcfix_CbUpdateBestIndex.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_UPDATE_BEST_INDEX_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_UPDATE_BEST_INDEX_H_

#include <stddef.h>
#include <stdint.h>

void WebRtcIlbcfix_CbUpdateBestIndex(
    int32_t CritNew,
    int16_t CritNewSh,
    size_t IndexNew,
    int32_t cDotNew,
    int16_t invEnergyNew,
    int16_t energyShiftNew,
    int32_t* CritMax,
    int16_t* shTotMax,
    size_t* bestIndex,

    int16_t* bestGain);

#endif
