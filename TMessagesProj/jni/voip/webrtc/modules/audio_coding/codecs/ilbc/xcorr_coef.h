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

 WebRtcIlbcfix_XcorrCoef.h

******************************************************************/

#ifndef MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_XCORR_COEF_H_
#define MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_XCORR_COEF_H_

#include <stddef.h>
#include <stdint.h>

size_t WebRtcIlbcfix_XcorrCoef(
    int16_t* target,
    int16_t* regressor,
    size_t subl,
    size_t searchLen,
    size_t offset,
    int16_t step
);

#endif
