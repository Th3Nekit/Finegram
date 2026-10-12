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
 * arith_routines.h
 *
 * Functions for arithmetic coding.
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ARITH_ROUTINES_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ARITH_ROUTINES_H_

#include "modules/audio_coding/codecs/isac/main/source/structs.h"

int WebRtcIsac_EncLogisticMulti2(
    Bitstr* streamdata,
    int16_t* dataQ7,
    const uint16_t*
        env,
    int N,
    int16_t isSWB12kHz);

int WebRtcIsac_EncTerminate(
    Bitstr* streamdata);

int WebRtcIsac_DecLogisticMulti2(
    int16_t* data,
    Bitstr* streamdata,
    const uint16_t*
        env,
    const int16_t* dither,
    int N,
    int16_t isSWB12kHz);

void WebRtcIsac_EncHistMulti(
    Bitstr* streamdata,
    const int* data,
    const uint16_t* const* cdf,
    int N);

int WebRtcIsac_DecHistBisectMulti(
    int* data,
    Bitstr* streamdata,
    const uint16_t* const* cdf,
    const uint16_t*
        cdf_size,
    int N);

int WebRtcIsac_DecHistOneStepMulti(
    int* data,
    Bitstr* streamdata,
    const uint16_t* const* cdf,
    const uint16_t*
        init_index,
    int N);

#endif
