/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Performs echo control (suppression) with fft routines in fixed-point.

#ifndef MODULES_AUDIO_PROCESSING_AECM_AECM_CORE_H_
#define MODULES_AUDIO_PROCESSING_AECM_AECM_CORE_H_

extern "C" {
#include "common_audio/ring_buffer.h"
#include "common_audio/signal_processing/include/signal_processing_library.h"
}
#include "modules/audio_processing/aecm/aecm_defines.h"

struct RealFFT;

namespace webrtc {

#ifdef _MSC_VER
#define ALIGN8_BEG __declspec(align(8))
#define ALIGN8_END
#else
#define ALIGN8_BEG
#define ALIGN8_END __attribute__((aligned(8)))
#endif

typedef struct {
  int16_t real;
  int16_t imag;
} ComplexInt16;

typedef struct {
  int farBufWritePos;
  int farBufReadPos;
  int knownDelay;
  int lastKnownDelay;
  int firstVAD;

  RingBuffer* farFrameBuf;
  RingBuffer* nearNoisyFrameBuf;
  RingBuffer* nearCleanFrameBuf;
  RingBuffer* outFrameBuf;

  int16_t farBuf[FAR_BUF_LEN];

  int16_t mult;
  uint32_t seed;

  void* delay_estimator_farend;
  void* delay_estimator;
  uint16_t currentDelay;

  uint16_t far_history[PART_LEN1 * MAX_DELAY];
  int far_history_pos;
  int far_q_domains[MAX_DELAY];

  int16_t nlpFlag;
  int16_t fixedDelay;

  uint32_t totCount;

  int16_t dfaCleanQDomain;
  int16_t dfaCleanQDomainOld;
  int16_t dfaNoisyQDomain;
  int16_t dfaNoisyQDomainOld;

  int16_t nearLogEnergy[MAX_BUF_LEN];
  int16_t farLogEnergy;
  int16_t echoAdaptLogEnergy[MAX_BUF_LEN];
  int16_t echoStoredLogEnergy[MAX_BUF_LEN];

  int16_t channelStored_buf[PART_LEN1 + 8];
  int16_t channelAdapt16_buf[PART_LEN1 + 8];
  int32_t channelAdapt32_buf[PART_LEN1 + 8];
  int16_t xBuf_buf[PART_LEN2 + 16];
  int16_t dBufClean_buf[PART_LEN2 + 16];
  int16_t dBufNoisy_buf[PART_LEN2 + 16];
  int16_t outBuf_buf[PART_LEN + 8];

  int16_t* channelStored;
  int16_t* channelAdapt16;
  int32_t* channelAdapt32;
  int16_t* xBuf;
  int16_t* dBufClean;
  int16_t* dBufNoisy;
  int16_t* outBuf;

  int32_t echoFilt[PART_LEN1];
  int16_t nearFilt[PART_LEN1];
  int32_t noiseEst[PART_LEN1];
  int noiseEstTooLowCtr[PART_LEN1];
  int noiseEstTooHighCtr[PART_LEN1];
  int16_t noiseEstCtr;
  int16_t cngMode;

  int32_t mseAdaptOld;
  int32_t mseStoredOld;
  int32_t mseThreshold;

  int16_t farEnergyMin;
  int16_t farEnergyMax;
  int16_t farEnergyMaxMin;
  int16_t farEnergyVAD;
  int16_t farEnergyMSE;
  int currentVADValue;
  int16_t vadUpdateCount;

  int16_t startupState;
  int16_t mseChannelCount;
  int16_t supGain;
  int16_t supGainOld;

  int16_t supGainErrParamA;
  int16_t supGainErrParamD;
  int16_t supGainErrParamDiffAB;
  int16_t supGainErrParamDiffBD;

  struct RealFFT* real_fft;

#ifdef AEC_DEBUG
  FILE* farFile;
  FILE* nearFile;
  FILE* outFile;
#endif
} AecmCore;

AecmCore* WebRtcAecm_CreateCore();

int WebRtcAecm_InitCore(AecmCore* const aecm, int samplingFreq);

void WebRtcAecm_FreeCore(AecmCore* aecm);

int WebRtcAecm_Control(AecmCore* aecm, int delay, int nlpFlag);

void WebRtcAecm_InitEchoPathCore(AecmCore* aecm, const int16_t* echo_path);

int WebRtcAecm_ProcessFrame(AecmCore* aecm,
                            const int16_t* farend,
                            const int16_t* nearendNoisy,
                            const int16_t* nearendClean,
                            int16_t* out);

int WebRtcAecm_ProcessBlock(AecmCore* aecm,
                            const int16_t* farend,
                            const int16_t* nearendNoisy,
                            const int16_t* noisyClean,
                            int16_t* out);

void WebRtcAecm_BufferFarFrame(AecmCore* const aecm,
                               const int16_t* const farend,
                               int farLen);

void WebRtcAecm_FetchFarFrame(AecmCore* const aecm,
                              int16_t* const farend,
                              int farLen,
                              int knownDelay);

void WebRtcAecm_UpdateFarHistory(AecmCore* self,
                                 uint16_t* far_spectrum,
                                 int far_q);

const uint16_t* WebRtcAecm_AlignedFarend(AecmCore* self, int* far_q, int delay);

int16_t WebRtcAecm_CalcSuppressionGain(AecmCore* const aecm);

void WebRtcAecm_CalcEnergies(AecmCore* aecm,
                             const uint16_t* far_spectrum,
                             int16_t far_q,
                             uint32_t nearEner,
                             int32_t* echoEst);

int16_t WebRtcAecm_CalcStepSize(AecmCore* const aecm);

void WebRtcAecm_UpdateChannel(AecmCore* aecm,
                              const uint16_t* far_spectrum,
                              int16_t far_q,
                              const uint16_t* const dfa,
                              int16_t mu,
                              int32_t* echoEst);

extern const int16_t WebRtcAecm_kCosTable[];
extern const int16_t WebRtcAecm_kSinTable[];

typedef void (*CalcLinearEnergies)(AecmCore* aecm,
                                   const uint16_t* far_spectrum,
                                   int32_t* echoEst,
                                   uint32_t* far_energy,
                                   uint32_t* echo_energy_adapt,
                                   uint32_t* echo_energy_stored);
extern CalcLinearEnergies WebRtcAecm_CalcLinearEnergies;

typedef void (*StoreAdaptiveChannel)(AecmCore* aecm,
                                     const uint16_t* far_spectrum,
                                     int32_t* echo_est);
extern StoreAdaptiveChannel WebRtcAecm_StoreAdaptiveChannel;

typedef void (*ResetAdaptiveChannel)(AecmCore* aecm);
extern ResetAdaptiveChannel WebRtcAecm_ResetAdaptiveChannel;

#if defined(WEBRTC_HAS_NEON)
void WebRtcAecm_CalcLinearEnergiesNeon(AecmCore* aecm,
                                       const uint16_t* far_spectrum,
                                       int32_t* echo_est,
                                       uint32_t* far_energy,
                                       uint32_t* echo_energy_adapt,
                                       uint32_t* echo_energy_stored);

void WebRtcAecm_StoreAdaptiveChannelNeon(AecmCore* aecm,
                                         const uint16_t* far_spectrum,
                                         int32_t* echo_est);

void WebRtcAecm_ResetAdaptiveChannelNeon(AecmCore* aecm);
#endif

#if defined(MIPS32_LE)
void WebRtcAecm_CalcLinearEnergies_mips(AecmCore* aecm,
                                        const uint16_t* far_spectrum,
                                        int32_t* echo_est,
                                        uint32_t* far_energy,
                                        uint32_t* echo_energy_adapt,
                                        uint32_t* echo_energy_stored);
#if defined(MIPS_DSP_R1_LE)
void WebRtcAecm_StoreAdaptiveChannel_mips(AecmCore* aecm,
                                          const uint16_t* far_spectrum,
                                          int32_t* echo_est);

void WebRtcAecm_ResetAdaptiveChannel_mips(AecmCore* aecm);
#endif
#endif

}

#endif
