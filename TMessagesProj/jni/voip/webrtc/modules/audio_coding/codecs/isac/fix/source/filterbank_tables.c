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
 * filterbank_tables.c
 *
 * This file contains variables that are used in
 * filterbanks.c
 *
 */

#include "modules/audio_coding/codecs/isac/fix/source/filterbank_tables.h"

const int16_t WebRtcIsacfix_kHpStCoeffInQ30[8] = {
  16189, -31932,
  17243, 15562,
  -17186, -26748,
  -27476, 26296
};

const int16_t WebRtcIsacfix_kHPStCoeffOut1Q30[8] = {
  -1306, -32719,
  11486, 16337,
  26078, 8918,
  3956, -8935
};

const int16_t WebRtcIsacfix_kHPStCoeffOut2Q30[8] = {
  -2953, -32546,
  32233, 16166,
  13217, 3383,
  -4597, -3473
};

const int16_t WebRtcIsacfix_kUpperApFactorsQ15[2] = {
  1137, 12537
};

const int16_t WebRtcIsacfix_kLowerApFactorsQ15[2] = {
  5059, 24379
};
