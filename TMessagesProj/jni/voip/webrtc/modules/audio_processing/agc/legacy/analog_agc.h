/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_LEGACY_ANALOG_AGC_H_
#define MODULES_AUDIO_PROCESSING_AGC_LEGACY_ANALOG_AGC_H_

#include "modules/audio_processing/agc/legacy/digital_agc.h"
#include "modules/audio_processing/agc/legacy/gain_control.h"

namespace webrtc {

constexpr int16_t kRxxBufferLen = 10;

static const int16_t kMsecSpeechInner = 520;
static const int16_t kMsecSpeechOuter = 340;

static const int16_t kNormalVadThreshold = 400;

static const int16_t kAlphaShortTerm = 6;
static const int16_t kAlphaLongTerm = 10;

typedef struct {

  uint32_t fs;
  int16_t compressionGaindB;
  int16_t targetLevelDbfs;
  int16_t agcMode;
  uint8_t limiterEnable;
  WebRtcAgcConfig defaultConfig;
  WebRtcAgcConfig usedConfig;

  int16_t initFlag;
  int16_t lastError;

  int32_t analogTargetLevel;
  int32_t startUpperLimit;
  int32_t startLowerLimit;
  int32_t upperPrimaryLimit;
  int32_t lowerPrimaryLimit;
  int32_t upperSecondaryLimit;
  int32_t lowerSecondaryLimit;
  uint16_t targetIdx;
  int16_t analogTarget;

  int32_t filterState[8];
  int32_t upperLimit;
  int32_t lowerLimit;
  int32_t Rxx160w32;
  int32_t Rxx16_LPw32;
  int32_t Rxx160_LPw32;
  int32_t Rxx16_LPw32Max;
  int32_t Rxx16_vectorw32[kRxxBufferLen];
  int32_t Rxx16w32_array[2][5];
  int32_t env[2][10];

  int16_t Rxx16pos;
  int16_t envSum;
  int16_t vadThreshold;
  int16_t inActive;
  int16_t msTooLow;
  int16_t msTooHigh;
  int16_t changeToSlowMode;
  int16_t firstCall;
  int16_t msZero;
  int16_t msecSpeechOuterChange;
  int16_t msecSpeechInnerChange;
  int16_t activeSpeech;
  int16_t muteGuardMs;
  int16_t inQueue;

  int32_t micRef;
  uint16_t gainTableIdx;
  int32_t micGainIdx;
  int32_t micVol;
  int32_t maxLevel;
  int32_t maxAnalog;
  int32_t maxInit;
  int32_t minLevel;
  int32_t minOutput;
  int32_t zeroCtrlMax;
  int32_t lastInMicLevel;

  int16_t scale;

  AgcVad vadMic;
  DigitalAgc digitalAgc;

  int16_t lowLevelSignal;
} LegacyAgc;

}

#endif
