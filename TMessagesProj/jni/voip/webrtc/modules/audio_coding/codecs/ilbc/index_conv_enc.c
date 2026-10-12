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

 IiLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_IndexConvEnc.c

******************************************************************/

#include "modules/audio_coding/codecs/ilbc/index_conv_enc.h"

#include "modules/audio_coding/codecs/ilbc/defines.h"

void WebRtcIlbcfix_IndexConvEnc(
    int16_t *index
                                ){
  int k;

  for (k=4;k<6;k++) {

    if ((index[k]>=108)&&(index[k]<172)) {
      index[k]-=64;
    } else if (index[k]>=236) {
      index[k]-=128;
    } else {

    }
  }
}
