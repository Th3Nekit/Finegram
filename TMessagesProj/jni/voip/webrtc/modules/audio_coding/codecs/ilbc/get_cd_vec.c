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

 WebRtcIlbcfix_GetCbVec.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/get_cd_vec.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/create_augmented_vec.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

bool WebRtcIlbcfix_GetCbVec(
    int16_t *cbvec,
    int16_t *mem,
    size_t index,
    size_t lMem,
    size_t cbveclen
                            ){
  size_t k, base_size;
  size_t lag;

  int16_t tempbuff2[SUBL+5];

  base_size=lMem-cbveclen+1;

  if (cbveclen==SUBL) {
    base_size += cbveclen / 2;
  }

  if (index<lMem-cbveclen+1) {

    k=index+cbveclen;

    WEBRTC_SPL_MEMCPY_W16(cbvec, mem+lMem-k, cbveclen);

  } else if (index < base_size) {

    k = (2 * (index - (lMem - cbveclen + 1))) + cbveclen;

    lag = k / 2;

    WebRtcIlbcfix_CreateAugmentedVec(lag, mem+lMem, cbvec);

  }

  else {

    size_t memIndTest;

    if (index-base_size<lMem-cbveclen+1) {

      memIndTest = lMem-(index-base_size+cbveclen);

      WebRtcSpl_MemSetW16(mem-CB_HALFFILTERLEN, 0, CB_HALFFILTERLEN);
      WebRtcSpl_MemSetW16(mem+lMem, 0, CB_HALFFILTERLEN);

      WebRtcSpl_FilterMAFastQ12(
          &mem[memIndTest+4], cbvec, (int16_t*)WebRtcIlbcfix_kCbFiltersRev,
          CB_FILTERLEN, cbveclen);
    }

    else {
      if (cbveclen < SUBL) {

        return false;
      }

      memIndTest = lMem-cbveclen-CB_FILTERLEN;
      WebRtcSpl_MemSetW16(mem+lMem, 0, CB_HALFFILTERLEN);

      WebRtcSpl_FilterMAFastQ12(
          &mem[memIndTest+7], tempbuff2, (int16_t*)WebRtcIlbcfix_kCbFiltersRev,
          CB_FILTERLEN, cbveclen+5);

      lag = (cbveclen<<1)-20+index-base_size-lMem-1;

      WebRtcIlbcfix_CreateAugmentedVec(lag, tempbuff2+SUBL+5, cbvec);
    }
  }

  return true;
}
