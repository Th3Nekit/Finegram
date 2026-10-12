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

 WebRtcIlbcfix_CbConstruct.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/cb_construct.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"
#include "modules/audio_coding/codecs/ilbc/gain_dequant.h"
#include "modules/audio_coding/codecs/ilbc/get_cd_vec.h"
#include "rtc_base/sanitizer.h"

static inline int32_t RTC_NO_SANITIZE("signed-integer-overflow")
    OverflowingAddS32S32ToS32(int32_t a, int32_t b) {
  return a + b;
}

bool WebRtcIlbcfix_CbConstruct(
    int16_t* decvector,
    const int16_t* index,
    const int16_t* gain_index,
    int16_t* mem,
    size_t lMem,
    size_t veclen) {
  size_t j;
  int16_t gain[CB_NSTAGES];

  int16_t cbvec0[SUBL];
  int16_t cbvec1[SUBL];
  int16_t cbvec2[SUBL];
  int32_t a32;
  int16_t *gainPtr;

  gain[0] = WebRtcIlbcfix_GainDequant(gain_index[0], 16384, 0);
  gain[1] = WebRtcIlbcfix_GainDequant(gain_index[1], gain[0], 1);
  gain[2] = WebRtcIlbcfix_GainDequant(gain_index[2], gain[1], 2);

  if (!WebRtcIlbcfix_GetCbVec(cbvec0, mem, (size_t)index[0], lMem, veclen))
    return false;
  if (!WebRtcIlbcfix_GetCbVec(cbvec1, mem, (size_t)index[1], lMem, veclen))
    return false;
  if (!WebRtcIlbcfix_GetCbVec(cbvec2, mem, (size_t)index[2], lMem, veclen))
    return false;

  gainPtr = &gain[0];
  for (j=0;j<veclen;j++) {
    a32 = (*gainPtr++) * cbvec0[j];
    a32 += (*gainPtr++) * cbvec1[j];
    a32 = OverflowingAddS32S32ToS32(a32, (*gainPtr) * cbvec2[j]);
    gainPtr -= 2;
    decvector[j] = (int16_t)((a32 + 8192) >> 14);
  }

  return true;
}
