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
 * bandwidth_estimator.h
 *
 * This header file contains the API for the Bandwidth Estimator
 * designed for iSAC.
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_BANDWIDTH_ESTIMATOR_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_BANDWIDTH_ESTIMATOR_H_

#include "modules/audio_coding/codecs/isac/fix/source/structs.h"

int32_t WebRtcIsacfix_InitBandwidthEstimator(BwEstimatorstr* bwest_str);

int32_t WebRtcIsacfix_UpdateUplinkBwImpl(BwEstimatorstr* bwest_str,
                                         uint16_t rtp_number,
                                         int16_t frameSize,
                                         uint32_t send_ts,
                                         uint32_t arr_ts,
                                         size_t pksize,
                                         uint16_t Index);

int16_t WebRtcIsacfix_UpdateUplinkBwRec(BwEstimatorstr* bwest_str,
                                        int16_t Index);

uint16_t WebRtcIsacfix_GetDownlinkBwIndexImpl(BwEstimatorstr* bwest_str);

uint16_t WebRtcIsacfix_GetDownlinkBandwidth(const BwEstimatorstr* bwest_str);

int16_t WebRtcIsacfix_GetUplinkBandwidth(const BwEstimatorstr* bwest_str);

int16_t WebRtcIsacfix_GetDownlinkMaxDelay(const BwEstimatorstr* bwest_str);

int16_t WebRtcIsacfix_GetUplinkMaxDelay(const BwEstimatorstr* bwest_str);

uint16_t WebRtcIsacfix_GetMinBytes(
    RateModel* State,
    int16_t StreamSize,
    int16_t FrameLen,
    int16_t BottleNeck,
    int16_t DelayBuildUp);

void WebRtcIsacfix_UpdateRateModel(
    RateModel* State,
    int16_t StreamSize,
    int16_t FrameSamples,
    int16_t BottleNeck);

void WebRtcIsacfix_InitRateModel(RateModel* State);

int16_t WebRtcIsacfix_GetNewFrameLength(int16_t bottle_neck,
                                        int16_t current_framelength);

int16_t WebRtcIsacfix_GetSnr(int16_t bottle_neck, int16_t framesamples);

#endif
