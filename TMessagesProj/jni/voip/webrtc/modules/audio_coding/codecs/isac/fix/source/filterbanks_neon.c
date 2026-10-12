/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Contains a function for WebRtcIsacfix_AllpassFilter2FixDec16Neon()
// in iSAC codec, optimized for ARM Neon platform. Bit exact with function
// WebRtcIsacfix_AllpassFilter2FixDec16C() in filterbanks.c. Prototype
// C code is at end of this file.

#include <arm_neon.h>

#include "modules/audio_coding/codecs/isac/fix/source/filterbank_internal.h"
#include "rtc_base/checks.h"

void WebRtcIsacfix_AllpassFilter2FixDec16Neon(
    int16_t* data_ch1,
    int16_t* data_ch2,
    const int16_t* factor_ch1,
    const int16_t* factor_ch2,
    const int length,
    int32_t* filter_state_ch1,
    int32_t* filter_state_ch2) {
  RTC_DCHECK_EQ(0, length % 2);
  int n = 0;
  int16x4_t factorv;
  int16x4_t datav;
  int32x4_t statev;

  factorv = vld1_dup_s16(factor_ch1);
  factorv = vld1_lane_s16(factor_ch1 + 1, factorv, 1);
  factorv = vld1_lane_s16(factor_ch2, factorv, 2);
  factorv = vld1_lane_s16(factor_ch2 + 1, factorv, 3);

  statev = vld1q_dup_s32(filter_state_ch1);
  statev = vld1q_lane_s32(filter_state_ch2, statev, 2);

  int32x4_t a;
  int16x4_t tmp1, tmp2;

  datav = vld1_dup_s16(data_ch1);
  datav = vld1_lane_s16(data_ch2, datav, 2);

  a = vqdmlal_s16(statev, datav, factorv);
  tmp1 = vshrn_n_s32(a, 16);

  statev = vqdmlsl_s16(vshll_n_s16(datav, 16), tmp1, factorv);

  statev = vld1q_lane_s32(filter_state_ch1 + 1, statev, 1);
  statev = vld1q_lane_s32(filter_state_ch2 + 1, statev, 3);

  tmp1 = vld1_lane_s16(data_ch1 + 1, tmp1, 1);
  tmp1 = vld1_lane_s16(data_ch2 + 1, tmp1, 3);
  datav = vrev32_s16(tmp1);

  for (n = 0; n < length - 2; n += 2) {
    a = vqdmlal_s16(statev, datav, factorv);
    tmp1 = vshrn_n_s32(a, 16);

    vst1_lane_s16(data_ch1 + n, tmp1, 1);
    vst1_lane_s16(data_ch2 + n, tmp1, 3);

    statev = vqdmlsl_s16(vshll_n_s16(datav, 16), tmp1, factorv);

    tmp1 = vld1_lane_s16(data_ch1 + n + 2, tmp1, 1);
    tmp1 = vld1_lane_s16(data_ch2 + n + 2, tmp1, 3);
    datav = vrev32_s16(tmp1);

    a = vqdmlal_s16(statev, datav, factorv);
    tmp2 = vshrn_n_s32(a, 16);

    vst1_lane_s16(data_ch1 + n + 1, tmp2, 1);
    vst1_lane_s16(data_ch2 + n + 1, tmp2, 3);

    statev = vqdmlsl_s16(vshll_n_s16(datav, 16), tmp2, factorv);

    tmp2 = vld1_lane_s16(data_ch1 + n + 3, tmp2, 1);
    tmp2 = vld1_lane_s16(data_ch2 + n + 3, tmp2, 3);
    datav = vrev32_s16(tmp2);
  }

  a = vqdmlal_s16(statev, datav, factorv);
  tmp1 = vshrn_n_s32(a, 16);

  vst1_lane_s16(data_ch1 + n, tmp1, 1);
  vst1_lane_s16(data_ch2 + n, tmp1, 3);

  statev = vqdmlsl_s16(vshll_n_s16(datav, 16), tmp1, factorv);

  vst1q_lane_s32(filter_state_ch1, statev, 0);
  vst1q_lane_s32(filter_state_ch2, statev, 2);

  datav = vrev32_s16(tmp1);
  a = vqdmlal_s16(statev, datav, factorv);
  tmp2 = vshrn_n_s32(a, 16);

  vst1_lane_s16(data_ch1 + n + 1, tmp2, 1);
  vst1_lane_s16(data_ch2 + n + 1, tmp2, 3);

  statev = vqdmlsl_s16(vshll_n_s16(datav, 16), tmp2, factorv);

  vst1q_lane_s32(filter_state_ch1 + 1, statev, 1);
  vst1q_lane_s32(filter_state_ch2 + 1, statev, 3);
}
