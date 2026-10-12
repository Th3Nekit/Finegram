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
 * entropy_coding.h
 *
 * This header file declares all of the functions used to arithmetically
 * encode the iSAC bistream
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENTROPY_CODING_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENTROPY_CODING_H_

#include "modules/audio_coding/codecs/isac/main/source/settings.h"
#include "modules/audio_coding/codecs/isac/main/source/structs.h"

int WebRtcIsac_DecodeSpec(Bitstr* streamdata,
                          int16_t AvgPitchGain_Q12,
                          enum ISACBand band,
                          double* fr,
                          double* fi);

int WebRtcIsac_EncodeSpec(const int16_t* fr,
                          const int16_t* fi,
                          int16_t AvgPitchGain_Q12,
                          enum ISACBand band,
                          Bitstr* streamdata);

int WebRtcIsac_DecodeLpcCoef(Bitstr* streamdata, double* LPCCoef);
int WebRtcIsac_DecodeLpcCoefUB(Bitstr* streamdata,
                               double* lpcVecs,
                               double* percepFilterGains,
                               int16_t bandwidth);

int WebRtcIsac_DecodeLpc(Bitstr* streamdata,
                         double* LPCCoef_lo,
                         double* LPCCoef_hi);

void WebRtcIsac_EncodeLpcLb(double* LPCCoef_lo,
                            double* LPCCoef_hi,
                            Bitstr* streamdata,
                            IsacSaveEncoderData* encData);

void WebRtcIsac_EncodeLpcGainLb(double* LPCCoef_lo,
                                double* LPCCoef_hi,
                                Bitstr* streamdata,
                                IsacSaveEncoderData* encData);

int16_t WebRtcIsac_EncodeLpcUB(double* lpcCoeff,
                               Bitstr* streamdata,
                               double* interpolLPCCoeff,
                               int16_t bandwidth,
                               ISACUBSaveEncDataStruct* encData);

int16_t WebRtcIsac_DecodeInterpolLpcUb(Bitstr* streamdata,
                                       double* percepFilterParam,
                                       int16_t bandwidth);

int WebRtcIsac_DecodeRc(Bitstr* streamdata, int16_t* RCQ15);

void WebRtcIsac_EncodeRc(int16_t* RCQ15, Bitstr* streamdata);

int WebRtcIsac_DecodeGain2(Bitstr* streamdata, int32_t* Gain2);

int WebRtcIsac_EncodeGain2(int32_t* gain2, Bitstr* streamdata);

void WebRtcIsac_EncodePitchGain(int16_t* PitchGains_Q12,
                                Bitstr* streamdata,
                                IsacSaveEncoderData* encData);

void WebRtcIsac_EncodePitchLag(double* PitchLags,
                               int16_t* PitchGain_Q12,
                               Bitstr* streamdata,
                               IsacSaveEncoderData* encData);

int WebRtcIsac_DecodePitchGain(Bitstr* streamdata, int16_t* PitchGain_Q12);
int WebRtcIsac_DecodePitchLag(Bitstr* streamdata,
                              int16_t* PitchGain_Q12,
                              double* PitchLag);

int WebRtcIsac_DecodeFrameLen(Bitstr* streamdata, int16_t* framelength);
int WebRtcIsac_EncodeFrameLen(int16_t framelength, Bitstr* streamdata);
int WebRtcIsac_DecodeSendBW(Bitstr* streamdata, int16_t* BWno);
void WebRtcIsac_EncodeReceiveBw(int* BWno, Bitstr* streamdata);

void WebRtcIsac_Poly2Rc(double* a, int N, double* RC);

void WebRtcIsac_Rc2Poly(double* RC, int N, double* a);

void WebRtcIsac_TranscodeLPCCoef(double* LPCCoef_lo,
                                 double* LPCCoef_hi,
                                 int* index_g);

void WebRtcIsac_EncodeLpcGainUb(double* lpGains,
                                Bitstr* streamdata,
                                int* lpcGainIndex);

void WebRtcIsac_StoreLpcGainUb(double* lpGains, Bitstr* streamdata);

int16_t WebRtcIsac_DecodeLpcGainUb(double* lpGains, Bitstr* streamdata);

int16_t WebRtcIsac_EncodeBandwidth(enum ISACBandwidth bandwidth,
                                   Bitstr* streamData);

int16_t WebRtcIsac_DecodeBandwidth(Bitstr* streamData,
                                   enum ISACBandwidth* bandwidth);

int16_t WebRtcIsac_EncodeJitterInfo(int32_t jitterIndex, Bitstr* streamData);

int16_t WebRtcIsac_DecodeJitterInfo(Bitstr* streamData, int32_t* jitterInfo);

#endif
