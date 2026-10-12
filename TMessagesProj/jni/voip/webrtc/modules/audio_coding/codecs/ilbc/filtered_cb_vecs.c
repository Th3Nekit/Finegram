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

 WebRtcIlbcfix_FilteredCbVecs.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/filtered_cb_vecs.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_FilteredCbVecs(
    int16_t *cbvectors,
    int16_t *CBmem,

    size_t lMem,
    size_t samples
                                  ) {

  WebRtcSpl_MemSetW16(CBmem+lMem, 0, CB_HALFFILTERLEN);
  WebRtcSpl_MemSetW16(CBmem-CB_HALFFILTERLEN, 0, CB_HALFFILTERLEN);
  WebRtcSpl_MemSetW16(cbvectors, 0, lMem-samples);

  WebRtcSpl_FilterMAFastQ12(
      CBmem+CB_HALFFILTERLEN+lMem-samples, cbvectors+lMem-samples,
      (int16_t*)WebRtcIlbcfix_kCbFiltersRev, CB_FILTERLEN, samples);

  return;
}
