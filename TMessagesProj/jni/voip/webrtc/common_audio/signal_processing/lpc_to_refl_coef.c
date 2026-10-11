/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the function WebRtcSpl_LpcToReflCoef().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "common_audio/signal_processing/include/signal_processing_library.h"

#define SPL_LPC_TO_REFL_COEF_MAX_AR_MODEL_ORDER 50

void WebRtcSpl_LpcToReflCoef(int16_t* a16, int use_order, int16_t* k16)
{
    int m, k;
    int32_t tmp32[SPL_LPC_TO_REFL_COEF_MAX_AR_MODEL_ORDER];
    int32_t tmp_inv_denom32;
    int16_t tmp_inv_denom16;

    k16[use_order - 1] = a16[use_order] << 3;
    for (m = use_order - 1; m > 0; m--)
    {

        tmp_inv_denom32 = 1073741823 - k16[m] * k16[m];

        tmp_inv_denom16 = (int16_t)(tmp_inv_denom32 >> 15);

        for (k = 1; k <= m; k++)
        {

            tmp32[k] = (a16[k] << 16) - (k16[m] * a16[m - k + 1] << 1);

            tmp32[k] = WebRtcSpl_DivW32W16(tmp32[k], tmp_inv_denom16);
        }

        for (k = 1; k < m; k++)
        {
            a16[k] = (int16_t)(tmp32[k] >> 1);
        }

        tmp32[m] = WEBRTC_SPL_SAT(8191, tmp32[m], -8191);
        k16[m - 1] = (int16_t)WEBRTC_SPL_LSHIFT_W32(tmp32[m], 2);
    }
    return;
}
