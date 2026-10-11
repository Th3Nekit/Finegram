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

 WebRtcIlbcfix_FrameClassify.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/frame_classify.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

size_t WebRtcIlbcfix_FrameClassify(

    IlbcEncoder *iLBCenc_inst,

    int16_t *residualFIX
                                                ){
  int16_t max, scale;
  int32_t ssqEn[NSUB_MAX-1];
  int16_t *ssqPtr;
  int32_t *seqEnPtr;
  int32_t maxW32;
  int16_t scale1;
  size_t pos;
  size_t n;

  max = WebRtcSpl_MaxAbsValueW16(residualFIX, iLBCenc_inst->blockl);
  scale = WebRtcSpl_GetSizeInBits((uint32_t)(max * max));

  scale = scale-24;
  scale1 = WEBRTC_SPL_MAX(0, scale);

  ssqPtr=residualFIX + 2;
  seqEnPtr=ssqEn;
  for (n=(iLBCenc_inst->nsub-1); n>0; n--) {
    (*seqEnPtr) = WebRtcSpl_DotProductWithScale(ssqPtr, ssqPtr, 76, scale1);
    ssqPtr += 40;
    seqEnPtr++;
  }

  maxW32 = WebRtcSpl_MaxValueW32(ssqEn, iLBCenc_inst->nsub - 1);
  scale = WebRtcSpl_GetSizeInBits(maxW32) - 20;
  scale1 = WEBRTC_SPL_MAX(0, scale);

  seqEnPtr=ssqEn;
  if (iLBCenc_inst->mode==20) {
    ssqPtr=(int16_t*)WebRtcIlbcfix_kStartSequenceEnrgWin+1;
  } else {
    ssqPtr=(int16_t*)WebRtcIlbcfix_kStartSequenceEnrgWin;
  }
  for (n=(iLBCenc_inst->nsub-1); n>0; n--) {
    (*seqEnPtr)=WEBRTC_SPL_MUL(((*seqEnPtr)>>scale1), (*ssqPtr));
    seqEnPtr++;
    ssqPtr++;
  }

  pos = WebRtcSpl_MaxIndexW32(ssqEn, iLBCenc_inst->nsub - 1) + 1;

  return(pos);
}
