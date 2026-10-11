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

 WebRtcIlbcfix_DecodeResidual.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/decode_residual.h"

#include <string.h>

#include "modules/audio_coding/codecs/ilbc/cb_construct.h"
#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"
#include "modules/audio_coding/codecs/ilbc/do_plc.h"
#include "modules/audio_coding/codecs/ilbc/enhancer_interface.h"
#include "modules/audio_coding/codecs/ilbc/index_conv_dec.h"
#include "modules/audio_coding/codecs/ilbc/lsf_check.h"
#include "modules/audio_coding/codecs/ilbc/state_construct.h"
#include "modules/audio_coding/codecs/ilbc/xcorr_coef.h"

bool WebRtcIlbcfix_DecodeResidual(
    IlbcDecoder *iLBCdec_inst,

    iLBC_bits *iLBC_encbits,

    int16_t *decresidual,
    int16_t *syntdenum

                                  ) {
  size_t meml_gotten, diff, start_pos;
  size_t subcount, subframe;
  int16_t *reverseDecresidual = iLBCdec_inst->enh_buf;
  int16_t *memVec = iLBCdec_inst->prevResidual;
  int16_t *mem = &memVec[CB_HALFFILTERLEN];

  diff = STATE_LEN - iLBCdec_inst->state_short_len;

  if (iLBC_encbits->state_first == 1) {
    start_pos = (iLBC_encbits->startIdx-1)*SUBL;
  } else {
    start_pos = (iLBC_encbits->startIdx-1)*SUBL + diff;
  }

  WebRtcIlbcfix_StateConstruct(iLBC_encbits->idxForMax,
                               iLBC_encbits->idxVec, &syntdenum[(iLBC_encbits->startIdx-1)*(LPC_FILTERORDER+1)],
                               &decresidual[start_pos], iLBCdec_inst->state_short_len
                               );

  if (iLBC_encbits->state_first) {

    WebRtcSpl_MemSetW16(mem, 0, CB_MEML - iLBCdec_inst->state_short_len);
    WEBRTC_SPL_MEMCPY_W16(mem+CB_MEML-iLBCdec_inst->state_short_len, decresidual+start_pos,
                          iLBCdec_inst->state_short_len);

    if (!WebRtcIlbcfix_CbConstruct(
            &decresidual[start_pos + iLBCdec_inst->state_short_len],
            iLBC_encbits->cb_index, iLBC_encbits->gain_index,
            mem + CB_MEML - ST_MEM_L_TBL, ST_MEM_L_TBL, diff))
      return false;

  }
  else {

    meml_gotten = iLBCdec_inst->state_short_len;
    WebRtcSpl_MemCpyReversedOrder(mem+CB_MEML-1,
                                  decresidual+start_pos, meml_gotten);
    WebRtcSpl_MemSetW16(mem, 0, CB_MEML - meml_gotten);

    if (!WebRtcIlbcfix_CbConstruct(reverseDecresidual, iLBC_encbits->cb_index,
                                   iLBC_encbits->gain_index,
                                   mem + CB_MEML - ST_MEM_L_TBL, ST_MEM_L_TBL,
                                   diff))
      return false;

    WebRtcSpl_MemCpyReversedOrder(&decresidual[start_pos-1],
                                  reverseDecresidual, diff);
  }

  subcount=1;

  if (iLBCdec_inst->nsub > iLBC_encbits->startIdx + 1) {

    WebRtcSpl_MemSetW16(mem, 0, CB_MEML-STATE_LEN);
    WEBRTC_SPL_MEMCPY_W16(mem+CB_MEML-STATE_LEN,
                          decresidual+(iLBC_encbits->startIdx-1)*SUBL, STATE_LEN);

    size_t Nfor = iLBCdec_inst->nsub - iLBC_encbits->startIdx - 1;
    for (subframe=0; subframe<Nfor; subframe++) {

      if (!WebRtcIlbcfix_CbConstruct(
              &decresidual[(iLBC_encbits->startIdx + 1 + subframe) * SUBL],
              iLBC_encbits->cb_index + subcount * CB_NSTAGES,
              iLBC_encbits->gain_index + subcount * CB_NSTAGES, mem, MEM_LF_TBL,
              SUBL))
        return false;

      memmove(mem, mem + SUBL, (CB_MEML - SUBL) * sizeof(*mem));
      WEBRTC_SPL_MEMCPY_W16(mem+CB_MEML-SUBL,
                            &decresidual[(iLBC_encbits->startIdx+1+subframe)*SUBL], SUBL);

      subcount++;
    }

  }

  if (iLBC_encbits->startIdx > 1) {

    meml_gotten = SUBL*(iLBCdec_inst->nsub+1-iLBC_encbits->startIdx);
    if( meml_gotten > CB_MEML ) {
      meml_gotten=CB_MEML;
    }

    WebRtcSpl_MemCpyReversedOrder(mem+CB_MEML-1,
                                  decresidual+(iLBC_encbits->startIdx-1)*SUBL, meml_gotten);
    WebRtcSpl_MemSetW16(mem, 0, CB_MEML - meml_gotten);

    size_t Nback = iLBC_encbits->startIdx - 1;
    for (subframe=0; subframe<Nback; subframe++) {

      if (!WebRtcIlbcfix_CbConstruct(
              &reverseDecresidual[subframe * SUBL],
              iLBC_encbits->cb_index + subcount * CB_NSTAGES,
              iLBC_encbits->gain_index + subcount * CB_NSTAGES, mem, MEM_LF_TBL,
              SUBL))
        return false;

      memmove(mem, mem + SUBL, (CB_MEML - SUBL) * sizeof(*mem));
      WEBRTC_SPL_MEMCPY_W16(mem+CB_MEML-SUBL,
                            &reverseDecresidual[subframe*SUBL], SUBL);

      subcount++;
    }

    WebRtcSpl_MemCpyReversedOrder(decresidual+SUBL*Nback-1,
                                  reverseDecresidual, SUBL*Nback);
  }

  return true;
}
