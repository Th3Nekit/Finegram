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

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_BANDWIDTH_ESTIMATOR_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_BANDWIDTH_ESTIMATOR_H_

#include <stddef.h>

#include "modules/audio_coding/codecs/isac/main/source/settings.h"
#include "modules/audio_coding/codecs/isac/main/source/structs.h"

#define MIN_ISAC_BW 10000
#define MIN_ISAC_BW_LB 10000
#define MIN_ISAC_BW_UB 25000

#define MAX_ISAC_BW 56000
#define MAX_ISAC_BW_UB 32000
#define MAX_ISAC_BW_LB 32000

#define MIN_ISAC_MD 5
#define MAX_ISAC_MD 25

#define HEADER_SIZE 35

#define INIT_FRAME_LEN_WB 60
#define INIT_FRAME_LEN_SWB 30

#define INIT_BN_EST_WB 20e3f
#define INIT_BN_EST_SWB 56e3f

#define INIT_HDR_RATE_WB \
  ((float)HEADER_SIZE * 8.0f * 1000.0f / (float)INIT_FRAME_LEN_WB)
#define INIT_HDR_RATE_SWB \
  ((float)HEADER_SIZE * 8.0f * 1000.0f / (float)INIT_FRAME_LEN_SWB)

#define BURST_LEN 3

#define BURST_INTERVAL 500

#define INIT_BURST_LEN 5

#define INIT_RATE_WB INIT_BN_EST_WB
#define INIT_RATE_SWB INIT_BN_EST_SWB

#if defined(__cplusplus)
extern "C" {
#endif

int32_t WebRtcIsac_InitBandwidthEstimator(
    BwEstimatorstr* bwest_str,
    enum IsacSamplingRate encoderSampRate,
    enum IsacSamplingRate decoderSampRate);

int16_t WebRtcIsac_UpdateBandwidthEstimator(BwEstimatorstr* bwest_str,
                                            uint16_t rtp_number,
                                            int32_t frame_length,
                                            uint32_t send_ts,
                                            uint32_t arr_ts,
                                            size_t pksize);

int16_t WebRtcIsac_UpdateUplinkBwImpl(
    BwEstimatorstr* bwest_str,
    int16_t Index,
    enum IsacSamplingRate encoderSamplingFreq);

void WebRtcIsac_GetDownlinkBwJitIndexImpl(
    BwEstimatorstr* bwest_str,
    int16_t* bottleneckIndex,
    int16_t* jitterInfo,
    enum IsacSamplingRate decoderSamplingFreq);

int32_t WebRtcIsac_GetDownlinkBandwidth(const BwEstimatorstr* bwest_str);

int32_t WebRtcIsac_GetDownlinkMaxDelay(const BwEstimatorstr* bwest_str);

int32_t WebRtcIsac_GetUplinkBandwidth(const BwEstimatorstr* bwest_str);

int32_t WebRtcIsac_GetUplinkMaxDelay(const BwEstimatorstr* bwest_str);

int WebRtcIsac_GetMinBytes(
    RateModel* State,
    int StreamSize,
    int FrameLen,
    double BottleNeck,
    double DelayBuildUp,
    enum ISACBandwidth bandwidth
                                            );

void WebRtcIsac_UpdateRateModel(
    RateModel* State,
    int StreamSize,
    int FrameSamples,
    double BottleNeck);

void WebRtcIsac_InitRateModel(RateModel* State);

int WebRtcIsac_GetNewFrameLength(double bottle_neck, int current_framelength);

double WebRtcIsac_GetSnr(double bottle_neck, int new_framelength);

int16_t WebRtcIsac_UpdateUplinkJitter(BwEstimatorstr* bwest_str, int32_t index);

#if defined(__cplusplus)
}
#endif

#endif
