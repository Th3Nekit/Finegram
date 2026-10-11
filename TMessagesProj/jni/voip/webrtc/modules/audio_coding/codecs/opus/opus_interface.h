/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_CODECS_OPUS_OPUS_INTERFACE_H_
#define MODULES_AUDIO_CODING_CODECS_OPUS_OPUS_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include "modules/audio_coding/codecs/opus/opus_inst.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WebRtcOpusEncInst OpusEncInst;
typedef struct WebRtcOpusDecInst OpusDecInst;

int16_t WebRtcOpus_EncoderCreate(OpusEncInst** inst,
                                 size_t channels,
                                 int32_t application,
                                 int sample_rate_hz);

int16_t WebRtcOpus_MultistreamEncoderCreate(
    OpusEncInst** inst,
    size_t channels,
    int32_t application,
    size_t streams,
    size_t coupled_streams,
    const unsigned char* channel_mapping);

int16_t WebRtcOpus_EncoderFree(OpusEncInst* inst);

int WebRtcOpus_Encode(OpusEncInst* inst,
                      const int16_t* audio_in,
                      size_t samples,
                      size_t length_encoded_buffer,
                      uint8_t* encoded);

int16_t WebRtcOpus_SetBitRate(OpusEncInst* inst, int32_t rate);

int16_t WebRtcOpus_SetPacketLossRate(OpusEncInst* inst, int32_t loss_rate);

int16_t WebRtcOpus_SetMaxPlaybackRate(OpusEncInst* inst, int32_t frequency_hz);

int16_t WebRtcOpus_GetMaxPlaybackRate(OpusEncInst* const inst,
                                      int32_t* result_hz);

int16_t WebRtcOpus_EnableFec(OpusEncInst* inst);

int16_t WebRtcOpus_DisableFec(OpusEncInst* inst);

int16_t WebRtcOpus_EnableDtx(OpusEncInst* inst);

int16_t WebRtcOpus_DisableDtx(OpusEncInst* inst);

int16_t WebRtcOpus_GetUseDtx(OpusEncInst* inst);

int16_t WebRtcOpus_EnableCbr(OpusEncInst* inst);

int16_t WebRtcOpus_DisableCbr(OpusEncInst* inst);

int16_t WebRtcOpus_SetComplexity(OpusEncInst* inst, int32_t complexity);

int32_t WebRtcOpus_GetBandwidth(OpusEncInst* inst);

int16_t WebRtcOpus_SetBandwidth(OpusEncInst* inst, int32_t bandwidth);

int32_t WebRtcOpus_GetInDtx(OpusEncInst* inst);

int16_t WebRtcOpus_SetForceChannels(OpusEncInst* inst, size_t num_channels);

int16_t WebRtcOpus_DecoderCreate(OpusDecInst** inst,
                                 size_t channels,
                                 int sample_rate_hz);

int16_t WebRtcOpus_MultistreamDecoderCreate(
    OpusDecInst** inst,
    size_t channels,
    size_t streams,
    size_t coupled_streams,
    const unsigned char* channel_mapping);

int16_t WebRtcOpus_DecoderFree(OpusDecInst* inst);

size_t WebRtcOpus_DecoderChannels(OpusDecInst* inst);

void WebRtcOpus_DecoderInit(OpusDecInst* inst);

int WebRtcOpus_Decode(OpusDecInst* inst,
                      const uint8_t* encoded,
                      size_t encoded_bytes,
                      int16_t* decoded,
                      int16_t* audio_type);

int WebRtcOpus_DecodeFec(OpusDecInst* inst,
                         const uint8_t* encoded,
                         size_t encoded_bytes,
                         int16_t* decoded,
                         int16_t* audio_type);

int WebRtcOpus_DurationEst(OpusDecInst* inst,
                           const uint8_t* payload,
                           size_t payload_length_bytes);

int WebRtcOpus_PlcDuration(OpusDecInst* inst);

int WebRtcOpus_FecDurationEst(const uint8_t* payload,
                              size_t payload_length_bytes,
                              int sample_rate_hz);

int WebRtcOpus_PacketHasFec(const uint8_t* payload,
                            size_t payload_length_bytes);

int WebRtcOpus_PacketHasVoiceActivity(const uint8_t* payload,
                                      size_t payload_length_bytes);

#ifdef __cplusplus
}
#endif

#endif
