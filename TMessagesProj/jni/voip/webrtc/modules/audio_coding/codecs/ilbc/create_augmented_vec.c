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

 WebRtcIlbcfix_CreateAugmentedVec.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/create_augmented_vec.h"

#include "common_audio/signal_processing/include/signal_processing_library.h"
#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"
#include "rtc_base/sanitizer.h"

void WebRtcIlbcfix_CreateAugmentedVec(
    size_t index,

    const int16_t* buffer,

    int16_t* cbVec) {
  size_t ilow;
  const int16_t *ppo, *ppi;
  int16_t cbVecTmp[4];

  size_t interp_len = WEBRTC_SPL_MIN(index, 4);

  rtc_MsanCheckInitialized(buffer - index - interp_len, sizeof(buffer[0]),
                           index + interp_len);

  ilow = index - interp_len;

  ppo = buffer-index;
  WEBRTC_SPL_MEMCPY_W16(cbVec, ppo, index);

  ppo = buffer - interp_len;
  ppi = buffer - index - interp_len;

  WebRtcSpl_ElementwiseVectorMult(&cbVec[ilow], ppi, WebRtcIlbcfix_kAlpha,
                                  interp_len, 15);
  WebRtcSpl_ReverseOrderMultArrayElements(
      cbVecTmp, ppo, &WebRtcIlbcfix_kAlpha[interp_len - 1], interp_len, 15);
  WebRtcSpl_AddVectorsAndShift(&cbVec[ilow], &cbVec[ilow], cbVecTmp, interp_len,
                               0);

  ppo = buffer - index;

  WEBRTC_SPL_MEMCPY_W16(cbVec+index, ppo, WEBRTC_SPL_MIN(SUBL-index, index));
}
