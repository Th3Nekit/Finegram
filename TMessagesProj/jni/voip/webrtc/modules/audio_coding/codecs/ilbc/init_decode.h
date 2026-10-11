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

 WebRtcIlbcfix_InitDecode.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_INIT_DECODE_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_INIT_DECODE_H_

#include <stdint.h>

#include "modules/audio_coding/codecs/ilbc/defines.h"

int WebRtcIlbcfix_InitDecode(
                             IlbcDecoder*
                                 iLBCdec_inst,
                             int16_t mode,
                             int use_enhancer

);

#endif
