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

 WebRtcIlbcfix_SortSq.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SORT_SQ_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SORT_SQ_H_

#include <stdint.h>

void WebRtcIlbcfix_SortSq(
    int16_t* xq,
    int16_t* index,
    int16_t x,
    const int16_t* cb,
    int16_t cb_size
);

#endif
