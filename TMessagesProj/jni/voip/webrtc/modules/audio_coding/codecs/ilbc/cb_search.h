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

 WebRtcIlbcfix_CbSearch.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_SEARCH_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_SEARCH_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_CbSearch(
    IlbcEncoder* iLBCenc_inst,

    int16_t* index,
    int16_t* gain_index,
    int16_t* intarget,
    int16_t* decResidual,
    size_t lMem,
    size_t lTarget,
    int16_t* weightDenum,
    size_t block
);

#endif
