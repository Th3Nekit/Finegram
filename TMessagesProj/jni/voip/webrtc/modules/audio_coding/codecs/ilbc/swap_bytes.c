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

 WebRtcIlbcfix_SwapBytes.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/swap_bytes.h"

void WebRtcIlbcfix_SwapBytes(
    const uint16_t* input,
    size_t wordLength,
    uint16_t* output
                              ) {
  size_t k;
  for (k = wordLength; k > 0; k--) {
    *output++ = (*input >> 8)|(*input << 8);
    input++;
  }
}
