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

 WebRtcIlbcfix_DoThePlc.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DO_PLC_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DO_PLC_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_DoThePlc(
    int16_t* PLCresidual,
    int16_t* PLClpc,
    int16_t PLI,

    int16_t* decresidual,
    int16_t* lpc,
    size_t inlag,
    IlbcDecoder* iLBCdec_inst

);

#endif
