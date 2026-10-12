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

 WebRtcIlbcfix_CbConstruct.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_CONSTRUCT_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_CONSTRUCT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "absl/base/attributes.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

ABSL_MUST_USE_RESULT
bool WebRtcIlbcfix_CbConstruct(
    int16_t* decvector,
    const int16_t* index,
    const int16_t* gain_index,
    int16_t* mem,
    size_t lMem,
    size_t veclen
);

#endif
