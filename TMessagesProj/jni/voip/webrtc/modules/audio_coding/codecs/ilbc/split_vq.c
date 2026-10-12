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

 WebRtcIlbcfix_SplitVq.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/split_vq.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"
#include "modules/audio_coding/codecs/ilbc/vq3.h"
#include "modules/audio_coding/codecs/ilbc/vq4.h"

void WebRtcIlbcfix_SplitVq(
    int16_t *qX,
    int16_t *index,

    int16_t *X,
    int16_t *CB,
    int16_t *dim,
    int16_t *cbsize
                           ) {

  int16_t *qXPtr, *indexPtr, *CBPtr, *XPtr;

  qXPtr=qX;
  indexPtr=index;
  CBPtr=CB;
  XPtr=X;
  WebRtcIlbcfix_Vq3(qXPtr, indexPtr, CBPtr, XPtr, cbsize[0]);

  qXPtr+=3;
  indexPtr+=1;
  CBPtr+=(dim[0]*cbsize[0]);
  XPtr+=3;
  WebRtcIlbcfix_Vq3(qXPtr, indexPtr, CBPtr, XPtr, cbsize[1]);

  qXPtr+=3;
  indexPtr+=1;
  CBPtr+=(dim[1]*cbsize[1]);
  XPtr+=3;
  WebRtcIlbcfix_Vq4(qXPtr, indexPtr, CBPtr, XPtr, cbsize[2]);

  return;
}
