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

 WebRtcIlbcfix_CompCorr.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_COMP_CORR_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_COMP_CORR_H_

#include <stddef.h>
#include <stdint.h>

void WebRtcIlbcfix_CompCorr(int32_t* corr,
                            int32_t* ener,
                            int16_t* buffer,
                            size_t lag,
                            size_t bLen,
                            size_t sRange,
                            int16_t scale
);

#endif
