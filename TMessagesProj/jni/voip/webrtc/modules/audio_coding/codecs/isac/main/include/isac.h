/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_INCLUDE_ISAC_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_INCLUDE_ISAC_H_

#include <stddef.h>

#include "modules/audio_coding/codecs/isac/bandwidth_info.h"

typedef struct WebRtcISACStruct ISACStruct;

#if defined(__cplusplus)
extern "C" {
#endif

int16_t WebRtcIsac_Create(ISACStruct** ISAC_main_inst);

int16_t WebRtcIsac_Free(ISACStruct* ISAC_main_inst);

int16_t WebRtcIsac_EncoderInit(ISACStruct* ISAC_main_inst, int16_t CodingMode);

int WebRtcIsac_Encode(ISACStruct* ISAC_main_inst,
                      const int16_t* speechIn,
                      uint8_t* encoded);

void WebRtcIsac_DecoderInit(ISACStruct* ISAC_main_inst);

int16_t WebRtcIsac_UpdateBwEstimate(ISACStruct* ISAC_main_inst,
                                    const uint8_t* encoded,
                                    size_t packet_size,
                                    uint16_t rtp_seq_number,
                                    uint32_t send_ts,
                                    uint32_t arr_ts);

int WebRtcIsac_Decode(ISACStruct* ISAC_main_inst,
                      const uint8_t* encoded,
                      size_t len,
                      int16_t* decoded,
                      int16_t* speechType);

size_t WebRtcIsac_DecodePlc(ISACStruct* ISAC_main_inst,
                            int16_t* decoded,
                            size_t noOfLostFrames);

int16_t WebRtcIsac_Control(ISACStruct* ISAC_main_inst,
                           int32_t rate,
                           int framesize);

void WebRtcIsac_SetInitialBweBottleneck(ISACStruct* ISAC_main_inst,
                                        int bottleneck_bits_per_second);

int16_t WebRtcIsac_ControlBwe(ISACStruct* ISAC_main_inst,
                              int32_t rateBPS,
                              int frameSizeMs,
                              int16_t enforceFrameSize);

int16_t WebRtcIsac_ReadFrameLen(const ISACStruct* ISAC_main_inst,
                                const uint8_t* encoded,
                                int16_t* frameLength);

void WebRtcIsac_version(char* version);

int16_t WebRtcIsac_GetErrorCode(ISACStruct* ISAC_main_inst);

int16_t WebRtcIsac_GetUplinkBw(ISACStruct* ISAC_main_inst, int32_t* bottleneck);

int16_t WebRtcIsac_SetMaxPayloadSize(ISACStruct* ISAC_main_inst,
                                     int16_t maxPayloadBytes);

int16_t WebRtcIsac_SetMaxRate(ISACStruct* ISAC_main_inst, int32_t maxRate);

uint16_t WebRtcIsac_DecSampRate(ISACStruct* ISAC_main_inst);

uint16_t WebRtcIsac_EncSampRate(ISACStruct* ISAC_main_inst);

int16_t WebRtcIsac_SetDecSampRate(ISACStruct* ISAC_main_inst,
                                  uint16_t samp_rate_hz);

int16_t WebRtcIsac_SetEncSampRate(ISACStruct* ISAC_main_inst,
                                  uint16_t sample_rate_hz);

int16_t WebRtcIsac_GetNewBitStream(ISACStruct* ISAC_main_inst,
                                   int16_t bweIndex,
                                   int16_t jitterInfo,
                                   int32_t rate,
                                   uint8_t* encoded,
                                   int16_t isRCU);

int16_t WebRtcIsac_GetDownLinkBwIndex(ISACStruct* ISAC_main_inst,
                                      int16_t* bweIndex,
                                      int16_t* jitterInfo);

int16_t WebRtcIsac_UpdateUplinkBw(ISACStruct* ISAC_main_inst, int16_t bweIndex);

int16_t WebRtcIsac_ReadBwIndex(const uint8_t* encoded, int16_t* bweIndex);

int16_t WebRtcIsac_GetNewFrameLen(ISACStruct* ISAC_main_inst);

int16_t WebRtcIsac_GetRedPayload(ISACStruct* ISAC_main_inst, uint8_t* encoded);

int WebRtcIsac_DecodeRcu(ISACStruct* ISAC_main_inst,
                         const uint8_t* encoded,
                         size_t len,
                         int16_t* decoded,
                         int16_t* speechType);

void WebRtcIsac_SetEncSampRateInDecoder(ISACStruct* inst, int sample_rate_hz);

#if defined(__cplusplus)
}
#endif

#endif
