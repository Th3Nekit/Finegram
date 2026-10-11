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

 WebRtcIlbcfix_AbsQuant.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/abs_quant.h"

#include "modules/audio_coding/codecs/ilbc/abs_quant_loop.h"
#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_AbsQuant(
    IlbcEncoder *iLBCenc_inst,

    iLBC_bits *iLBC_encbits,

    int16_t *in,
    int16_t *weightDenum
                            ) {
  int16_t *syntOut;
  size_t quantLen[2];

  int16_t syntOutBuf[LPC_FILTERORDER+STATE_SHORT_LEN_30MS];
  int16_t in_weightedVec[STATE_SHORT_LEN_30MS+LPC_FILTERORDER];
  int16_t *in_weighted = &in_weightedVec[LPC_FILTERORDER];

  WebRtcSpl_MemSetW16(syntOutBuf, 0, LPC_FILTERORDER+STATE_SHORT_LEN_30MS);
  syntOut = &syntOutBuf[LPC_FILTERORDER];

  WebRtcSpl_MemSetW16(in_weightedVec, 0, LPC_FILTERORDER);

  if (iLBC_encbits->state_first) {
    quantLen[0]=SUBL;
    quantLen[1]=iLBCenc_inst->state_short_len-SUBL;
  } else {
    quantLen[0]=iLBCenc_inst->state_short_len-SUBL;
    quantLen[1]=SUBL;
  }

  WebRtcSpl_FilterARFastQ12(
      in, in_weighted,
      weightDenum, LPC_FILTERORDER+1, quantLen[0]);
  WebRtcSpl_FilterARFastQ12(
      &in[quantLen[0]], &in_weighted[quantLen[0]],
      &weightDenum[LPC_FILTERORDER+1], LPC_FILTERORDER+1, quantLen[1]);

  WebRtcIlbcfix_AbsQuantLoop(
      syntOut,
      in_weighted,
      weightDenum,
      quantLen,
      iLBC_encbits->idxVec);

}
