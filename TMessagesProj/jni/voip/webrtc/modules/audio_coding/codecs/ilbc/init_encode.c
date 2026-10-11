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

 WebRtcIlbcfix_InitEncode.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/init_encode.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

int WebRtcIlbcfix_InitEncode(
    IlbcEncoder *iLBCenc_inst,
    int16_t mode) {
  iLBCenc_inst->mode = mode;

  if (mode==30) {
    iLBCenc_inst->blockl = BLOCKL_30MS;
    iLBCenc_inst->nsub = NSUB_30MS;
    iLBCenc_inst->nasub = NASUB_30MS;
    iLBCenc_inst->lpc_n = LPC_N_30MS;
    iLBCenc_inst->no_of_bytes = NO_OF_BYTES_30MS;
    iLBCenc_inst->no_of_words = NO_OF_WORDS_30MS;
    iLBCenc_inst->state_short_len=STATE_SHORT_LEN_30MS;
  }
  else if (mode==20) {
    iLBCenc_inst->blockl = BLOCKL_20MS;
    iLBCenc_inst->nsub = NSUB_20MS;
    iLBCenc_inst->nasub = NASUB_20MS;
    iLBCenc_inst->lpc_n = LPC_N_20MS;
    iLBCenc_inst->no_of_bytes = NO_OF_BYTES_20MS;
    iLBCenc_inst->no_of_words = NO_OF_WORDS_20MS;
    iLBCenc_inst->state_short_len=STATE_SHORT_LEN_20MS;
  }
  else {
    return(-1);
  }

  WebRtcSpl_MemSetW16(iLBCenc_inst->anaMem, 0, LPC_FILTERORDER);
  WEBRTC_SPL_MEMCPY_W16(iLBCenc_inst->lsfold, WebRtcIlbcfix_kLsfMean, LPC_FILTERORDER);
  WEBRTC_SPL_MEMCPY_W16(iLBCenc_inst->lsfdeqold, WebRtcIlbcfix_kLsfMean, LPC_FILTERORDER);
  WebRtcSpl_MemSetW16(iLBCenc_inst->lpc_buffer, 0, LPC_LOOKBACK + BLOCKL_MAX);

  WebRtcSpl_MemSetW16(iLBCenc_inst->hpimemx, 0, 2);
  WebRtcSpl_MemSetW16(iLBCenc_inst->hpimemy, 0, 4);

#ifdef SPLIT_10MS

  WebRtcSpl_MemSetW16(iLBCenc_inst->past_samples,0,160);
  iLBCenc_inst->section = 0;
#endif

  return (int)(iLBCenc_inst->no_of_bytes);
}
