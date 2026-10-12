/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_Decode.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DECODE_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DECODE_H_

#include <stdint.h>

#include "absl/base/attributes.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

ABSL_MUST_USE_RESULT
int WebRtcIlbcfix_DecodeImpl(
    int16_t* decblock,
    const uint16_t* bytes,
    IlbcDecoder* iLBCdec_inst,

    int16_t mode

);

#endif
