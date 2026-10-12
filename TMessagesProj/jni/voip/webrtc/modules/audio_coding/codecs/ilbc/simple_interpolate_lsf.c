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

 WebRtcIlbcfix_SimpleInterpolateLsf.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/simple_interpolate_lsf.h"

#include "modules/audio_coding/codecs/ilbc/bw_expand.h"
#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"
#include "modules/audio_coding/codecs/ilbc/lsf_interpolate_to_poly_enc.h"

void WebRtcIlbcfix_SimpleInterpolateLsf(
    int16_t *syntdenum,

    int16_t *weightdenum,

    int16_t *lsf,
    int16_t *lsfdeq,
    int16_t *lsfold,

    int16_t *lsfdeqold,

    int16_t length,
    IlbcEncoder *iLBCenc_inst

                                        ) {
  size_t i;
  int pos, lp_length;

  int16_t *lsf2, *lsfdeq2;

  int16_t lp[LPC_FILTERORDER + 1];

  lsf2 = lsf + length;
  lsfdeq2 = lsfdeq + length;
  lp_length = length + 1;

  if (iLBCenc_inst->mode==30) {

    WebRtcIlbcfix_LsfInterpolate2PloyEnc(lp, lsfdeqold, lsfdeq,
                                         WebRtcIlbcfix_kLsfWeight30ms[0],
                                         length);
    WEBRTC_SPL_MEMCPY_W16(syntdenum, lp, lp_length);

    WebRtcIlbcfix_LsfInterpolate2PloyEnc(lp, lsfold, lsf,
                                         WebRtcIlbcfix_kLsfWeight30ms[0],
                                         length);
    WebRtcIlbcfix_BwExpand(weightdenum, lp,
                           (int16_t*)WebRtcIlbcfix_kLpcChirpWeightDenum,
                           (int16_t)lp_length);

    pos = lp_length;
    for (i = 1; i < iLBCenc_inst->nsub; i++) {

      WebRtcIlbcfix_LsfInterpolate2PloyEnc(lp, lsfdeq, lsfdeq2,
                                           WebRtcIlbcfix_kLsfWeight30ms[i],
                                           length);
      WEBRTC_SPL_MEMCPY_W16(syntdenum + pos, lp, lp_length);

      WebRtcIlbcfix_LsfInterpolate2PloyEnc(lp, lsf, lsf2,
                                           WebRtcIlbcfix_kLsfWeight30ms[i],
                                           length);
      WebRtcIlbcfix_BwExpand(weightdenum + pos, lp,
                             (int16_t*)WebRtcIlbcfix_kLpcChirpWeightDenum,
                             (int16_t)lp_length);

      pos += lp_length;
    }

    WEBRTC_SPL_MEMCPY_W16(lsfold, lsf2, length);
    WEBRTC_SPL_MEMCPY_W16(lsfdeqold, lsfdeq2, length);

  } else {
    pos = 0;
    for (i = 0; i < iLBCenc_inst->nsub; i++) {

      WebRtcIlbcfix_LsfInterpolate2PloyEnc(lp, lsfdeqold, lsfdeq,
                                           WebRtcIlbcfix_kLsfWeight20ms[i],
                                           length);
      WEBRTC_SPL_MEMCPY_W16(syntdenum + pos, lp, lp_length);

      WebRtcIlbcfix_LsfInterpolate2PloyEnc(lp, lsfold, lsf,
                                           WebRtcIlbcfix_kLsfWeight20ms[i],
                                           length);
      WebRtcIlbcfix_BwExpand(weightdenum+pos, lp,
                             (int16_t*)WebRtcIlbcfix_kLpcChirpWeightDenum,
                             (int16_t)lp_length);

      pos += lp_length;
    }

    WEBRTC_SPL_MEMCPY_W16(lsfold, lsf, length);
    WEBRTC_SPL_MEMCPY_W16(lsfdeqold, lsfdeq, length);

  }

  return;
}
