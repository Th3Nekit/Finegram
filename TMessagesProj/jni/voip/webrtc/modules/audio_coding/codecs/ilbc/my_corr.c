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

 WebRtcIlbcfix_MyCorr.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/my_corr.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_MyCorr(
    int32_t* corr,
    const int16_t* seq1,
    size_t dim1,
    const int16_t* seq2,
    size_t dim2
                          ){
  uint32_t max1, max2;
  size_t loops;
  int right_shift;

  max1 = WebRtcSpl_MaxAbsValueW16(seq1, dim1) + 1;
  max2 = WebRtcSpl_MaxAbsValueW16(seq2, dim2) + 1;
  right_shift =
      (64 - 31) - WebRtcSpl_CountLeadingZeros64((max1 * max2) * (uint64_t)dim2);
  if (right_shift < 0) {
    right_shift = 0;
  }

  loops=dim1-dim2+1;

  WebRtcSpl_CrossCorrelation(corr, seq2, seq1, dim2, loops, right_shift, 1);

  return;
}
