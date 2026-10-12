/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * codec.h
 *
 * This header file contains the calls to the internal encoder
 * and decoder functions.
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_CODEC_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_CODEC_H_

#include <stddef.h>

#include "modules/audio_coding/codecs/isac/main/source/structs.h"
#include "modules/third_party/fft/fft.h"

void WebRtcIsac_ResetBitstream(Bitstr* bit_stream);

int WebRtcIsac_EstimateBandwidth(BwEstimatorstr* bwest_str,
                                 Bitstr* streamdata,
                                 size_t packet_size,
                                 uint16_t rtp_seq_number,
                                 uint32_t send_ts,
                                 uint32_t arr_ts,
                                 enum IsacSamplingRate encoderSampRate,
                                 enum IsacSamplingRate decoderSampRate);

int WebRtcIsac_DecodeLb(const TransformTables* transform_tables,
                        float* signal_out,
                        ISACLBDecStruct* ISACdec_obj,
                        int16_t* current_framesamples,
                        int16_t isRCUPayload);

int WebRtcIsac_DecodeRcuLb(float* signal_out,
                           ISACLBDecStruct* ISACdec_obj,
                           int16_t* current_framesamples);

int WebRtcIsac_EncodeLb(const TransformTables* transform_tables,
                        float* in,
                        ISACLBEncStruct* ISACencLB_obj,
                        int16_t codingMode,
                        int16_t bottleneckIndex);

int WebRtcIsac_EncodeStoredDataLb(const IsacSaveEncoderData* ISACSavedEnc_obj,
                                  Bitstr* ISACBitStr_obj,
                                  int BWnumber,
                                  float scale);

int WebRtcIsac_EncodeStoredDataUb(
    const ISACUBSaveEncDataStruct* ISACSavedEnc_obj,
    Bitstr* bitStream,
    int32_t jitterInfo,
    float scale,
    enum ISACBandwidth bandwidth);

int16_t WebRtcIsac_GetRedPayloadUb(
    const ISACUBSaveEncDataStruct* ISACSavedEncObj,
    Bitstr* bitStreamObj,
    enum ISACBandwidth bandwidth);

int16_t WebRtcIsac_RateAllocation(int32_t inRateBitPerSec,
                                  double* rateLBBitPerSec,
                                  double* rateUBBitPerSec,
                                  enum ISACBandwidth* bandwidthKHz);

int WebRtcIsac_DecodeUb16(const TransformTables* transform_tables,
                          float* signal_out,
                          ISACUBDecStruct* ISACdec_obj,
                          int16_t isRCUPayload);

int WebRtcIsac_DecodeUb12(const TransformTables* transform_tables,
                          float* signal_out,
                          ISACUBDecStruct* ISACdec_obj,
                          int16_t isRCUPayload);

int WebRtcIsac_EncodeUb16(const TransformTables* transform_tables,
                          float* in,
                          ISACUBEncStruct* ISACenc_obj,
                          int32_t jitterInfo);

int WebRtcIsac_EncodeUb12(const TransformTables* transform_tables,
                          float* in,
                          ISACUBEncStruct* ISACenc_obj,
                          int32_t jitterInfo);

void WebRtcIsac_InitMasking(MaskFiltstr* maskdata);

void WebRtcIsac_InitPostFilterbank(PostFiltBankstr* postfiltdata);

void WebRtcIsac_InitTransform(TransformTables* tables);

void WebRtcIsac_Time2Spec(const TransformTables* tables,
                          double* inre1,
                          double* inre2,
                          int16_t* outre,
                          int16_t* outim,
                          FFTstr* fftstr_obj);

void WebRtcIsac_Spec2time(const TransformTables* tables,
                          double* inre,
                          double* inim,
                          double* outre1,
                          double* outre2,
                          FFTstr* fftstr_obj);

void WebRtcIsac_FilterAndCombineFloat(float* InLP,
                                      float* InHP,
                                      float* Out,
                                      PostFiltBankstr* postfiltdata);

void WebRtcIsac_NormLatticeFilterMa(int orderCoef,
                                    float* stateF,
                                    float* stateG,
                                    float* lat_in,
                                    double* filtcoeflo,
                                    double* lat_out);

void WebRtcIsac_NormLatticeFilterAr(int orderCoef,
                                    float* stateF,
                                    float* stateG,
                                    double* lat_in,
                                    double* lo_filt_coef,
                                    float* lat_out);

void WebRtcIsac_Dir2Lat(double* a, int orderCoef, float* sth, float* cth);

#endif
