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

 WebRtcIlbcfix_DecodeResidual.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DECODE_RESIDUAL_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_DECODE_RESIDUAL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "absl/base/attributes.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

ABSL_MUST_USE_RESULT
bool WebRtcIlbcfix_DecodeResidual(
    IlbcDecoder* iLBCdec_inst,
    iLBC_bits* iLBC_encbits,

    int16_t* decresidual,
    int16_t* syntdenum

);

#endif
