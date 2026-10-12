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

 WebRtcIlbcfix_GetSyncSeq.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_GET_SYNC_SEQ_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_GET_SYNC_SEQ_H_

#include <stddef.h>
#include <stdint.h>

void WebRtcIlbcfix_GetSyncSeq(
    int16_t* idata,
    size_t idatal,
    size_t centerStartPos,
    size_t* period,
    const size_t* plocs,
    size_t periodl,
    size_t hl,
    int16_t* surround

);

#endif
