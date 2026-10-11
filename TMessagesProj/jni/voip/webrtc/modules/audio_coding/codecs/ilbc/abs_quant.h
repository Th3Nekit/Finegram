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

 WebRtcIlbcfix_AbsQuant.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_ABS_QUANT_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_ABS_QUANT_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_AbsQuant(
    IlbcEncoder* iLBCenc_inst,

    iLBC_bits* iLBC_encbits,

    int16_t* in,
    int16_t* weightDenum
);

#endif
