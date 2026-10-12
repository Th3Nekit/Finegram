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

 WebRtcIlbcfix_Enhancer.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/enhancer.h"

#include "modules/audio_coding/codecs/ilbc/constants.h"
#include "modules/audio_coding/codecs/ilbc/defines.h"
#include "modules/audio_coding/codecs/ilbc/get_sync_seq.h"
#include "modules/audio_coding/codecs/ilbc/smooth.h"

void WebRtcIlbcfix_Enhancer(
    int16_t *odata,
    int16_t *idata,
    size_t idatal,
    size_t centerStartPos,
    size_t *period,
    const size_t *plocs,
    size_t periodl
                            ){

  int16_t surround[ENH_BLOCKL];

  WebRtcSpl_MemSetW16(surround, 0, ENH_BLOCKL);

  WebRtcIlbcfix_GetSyncSeq(idata, idatal, centerStartPos, period, plocs,
                           periodl, ENH_HL, surround);

  WebRtcIlbcfix_Smooth(odata, idata + centerStartPos, surround);
}
