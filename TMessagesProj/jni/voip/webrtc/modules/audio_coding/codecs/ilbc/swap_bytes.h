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

 WebRtcIlbcfix_SwapBytes.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SWAP_BYTES_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SWAP_BYTES_H_

#include <stddef.h>
#include <stdint.h>

void WebRtcIlbcfix_SwapBytes(
    const uint16_t* input,
    size_t wordLength,
    uint16_t* output
);

#endif
