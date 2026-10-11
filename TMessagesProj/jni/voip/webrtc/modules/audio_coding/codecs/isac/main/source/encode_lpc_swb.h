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
 * encode_lpc_swb.h
 *
 * This file contains declaration of functions used to
 * encode LPC parameters (Shape & gain) of the upper band.
 *
 */

#ifndef MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENCODE_LPC_SWB_H_
#define MODULES_AUDIO_CODING_CODECS_ISAC_MAIN_SOURCE_ENCODE_LPC_SWB_H_

#include "modules/audio_coding/codecs/isac/main/source/settings.h"
#include "modules/audio_coding/codecs/isac/main/source/structs.h"

int16_t WebRtcIsac_RemoveLarMean(double* lar, int16_t bandwidth);

int16_t WebRtcIsac_DecorrelateIntraVec(const double* inLAR,
                                       double* out,
                                       int16_t bandwidth);

int16_t WebRtcIsac_DecorrelateInterVec(const double* data,
                                       double* out,
                                       int16_t bandwidth);

double WebRtcIsac_QuantizeUncorrLar(double* data, int* idx, int16_t bandwidth);

int16_t WebRtcIsac_CorrelateIntraVec(const double* data,
                                     double* out,
                                     int16_t bandwidth);

int16_t WebRtcIsac_CorrelateInterVec(const double* data,
                                     double* out,
                                     int16_t bandwidth);

int16_t WebRtcIsac_AddLarMean(double* data, int16_t bandwidth);

int16_t WebRtcIsac_DequantizeLpcParam(const int* idx,
                                      double* out,
                                      int16_t bandwidth);

int16_t WebRtcIsac_ToLogDomainRemoveMean(double* lpGains);

int16_t WebRtcIsac_DecorrelateLPGain(const double* data, double* out);

double WebRtcIsac_QuantizeLpcGain(double* lpGains, int* idx);

int16_t WebRtcIsac_DequantizeLpcGain(const int* idx, double* lpGains);

int16_t WebRtcIsac_CorrelateLpcGain(const double* data, double* out);

int16_t WebRtcIsac_AddMeanToLinearDomain(double* lpcGains);

#endif
