/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_FIX_INCLUDE_ISACFIX_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_FIX_INCLUDE_ISACFIX_H_

#include <stddef.h>

#include "modules/audio_coding/codecs/isac/bandwidth_info.h"

typedef struct {
  void* dummy;
} ISACFIX_MainStruct;

#if defined(__cplusplus)
extern "C" {
#endif

int16_t WebRtcIsacfix_Create(ISACFIX_MainStruct** ISAC_main_inst);

int16_t WebRtcIsacfix_Free(ISACFIX_MainStruct* ISAC_main_inst);

int16_t WebRtcIsacfix_EncoderInit(ISACFIX_MainStruct* ISAC_main_inst,
                                  int16_t CodingMode);

int WebRtcIsacfix_Encode(ISACFIX_MainStruct* ISAC_main_inst,
                         const int16_t* speechIn,
                         uint8_t* encoded);

void WebRtcIsacfix_DecoderInit(ISACFIX_MainStruct* ISAC_main_inst);

int16_t WebRtcIsacfix_UpdateBwEstimate1(ISACFIX_MainStruct* ISAC_main_inst,
                                        const uint8_t* encoded,
                                        size_t packet_size,
                                        uint16_t rtp_seq_number,
                                        uint32_t arr_ts);

int16_t WebRtcIsacfix_UpdateBwEstimate(ISACFIX_MainStruct* ISAC_main_inst,
                                       const uint8_t* encoded,
                                       size_t packet_size,
                                       uint16_t rtp_seq_number,
                                       uint32_t send_ts,
                                       uint32_t arr_ts);

int WebRtcIsacfix_Decode(ISACFIX_MainStruct* ISAC_main_inst,
                         const uint8_t* encoded,
                         size_t len,
                         int16_t* decoded,
                         int16_t* speechType);

size_t WebRtcIsacfix_DecodePlc(ISACFIX_MainStruct* ISAC_main_inst,
                               int16_t* decoded,
                               size_t noOfLostFrames);

int16_t WebRtcIsacfix_ReadFrameLen(const uint8_t* encoded,
                                   size_t encoded_len_bytes,
                                   size_t* frameLength);

int16_t WebRtcIsacfix_Control(ISACFIX_MainStruct* ISAC_main_inst,
                              int16_t rate,
                              int framesize);

void WebRtcIsacfix_SetInitialBweBottleneck(ISACFIX_MainStruct* ISAC_main_inst,
                                           int bottleneck_bits_per_second);

int16_t WebRtcIsacfix_ControlBwe(ISACFIX_MainStruct* ISAC_main_inst,
                                 int16_t rateBPS,
                                 int frameSizeMs,
                                 int16_t enforceFrameSize);

void WebRtcIsacfix_version(char* version);

int16_t WebRtcIsacfix_GetErrorCode(ISACFIX_MainStruct* ISAC_main_inst);

int32_t WebRtcIsacfix_GetUplinkBw(ISACFIX_MainStruct* ISAC_main_inst);

int16_t WebRtcIsacfix_SetMaxPayloadSize(ISACFIX_MainStruct* ISAC_main_inst,
                                        int16_t maxPayloadBytes);

int16_t WebRtcIsacfix_SetMaxRate(ISACFIX_MainStruct* ISAC_main_inst,
                                 int32_t maxRate);

int16_t WebRtcIsacfix_CreateInternal(ISACFIX_MainStruct* ISAC_main_inst);

int16_t WebRtcIsacfix_FreeInternal(ISACFIX_MainStruct* ISAC_main_inst);

int16_t WebRtcIsacfix_GetNewBitStream(ISACFIX_MainStruct* ISAC_main_inst,
                                      int16_t bweIndex,
                                      float scale,
                                      uint8_t* encoded);

int16_t WebRtcIsacfix_GetDownLinkBwIndex(ISACFIX_MainStruct* ISAC_main_inst,
                                         int16_t* rateIndex);

int16_t WebRtcIsacfix_UpdateUplinkBw(ISACFIX_MainStruct* ISAC_main_inst,
                                     int16_t rateIndex);

int16_t WebRtcIsacfix_ReadBwIndex(const uint8_t* encoded,
                                  size_t encoded_len_bytes,
                                  int16_t* rateIndex);

int16_t WebRtcIsacfix_GetNewFrameLen(ISACFIX_MainStruct* ISAC_main_inst);

#if defined(__cplusplus)
}
#endif

#endif
