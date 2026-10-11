/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * filterbank_tables.h
 *
 * Header file for variables that are defined in
 * filterbank_tables.c.
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_FILTERBANK_TABLES_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_FILTERBANK_TABLES_H_

#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

extern const int16_t WebRtcIsacfix_kHpStCoeffInQ30[8];

extern const int16_t WebRtcIsacfix_kHPStCoeffOut1Q30[8];

extern const int16_t WebRtcIsacfix_kHPStCoeffOut2Q30[8];

extern const int16_t WebRtcIsacfix_kUpperApFactorsQ15[2];

extern const int16_t WebRtcIsacfix_kLowerApFactorsQ15[2];

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
