/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/media_opt_util.h"

#include <math.h>

#include <algorithm>

#include "modules/video_coding/fec_rate_table.h"
#include "modules/video_coding/internal_defines.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

static const int kPacketLossMax = 129;

namespace media_optimization {

VCMProtectionParameters::VCMProtectionParameters()
    : rtt(0),
      lossPr(0.0f),
      bitRate(0.0f),
      packetsPerFrame(0.0f),
      packetsPerFrameKey(0.0f),
      frameRate(0.0f),
      keyFrameSize(0.0f),
      fecRateDelta(0),
      fecRateKey(0),
      codecWidth(0),
      codecHeight(0),
      numLayers(1) {}

VCMProtectionMethod::VCMProtectionMethod()
    : _effectivePacketLoss(0),
      _protectionFactorK(0),
      _protectionFactorD(0),
      _scaleProtKey(2.0f),
      _maxPayloadSize(1460),
      _corrFecCost(1.0),
      _type(kNone) {}

VCMProtectionMethod::~VCMProtectionMethod() {}

enum VCMProtectionMethodEnum VCMProtectionMethod::Type() const {
  return _type;
}

uint8_t VCMProtectionMethod::RequiredPacketLossER() {
  return _effectivePacketLoss;
}

uint8_t VCMProtectionMethod::RequiredProtectionFactorK() {
  return _protectionFactorK;
}

uint8_t VCMProtectionMethod::RequiredProtectionFactorD() {
  return _protectionFactorD;
}

bool VCMProtectionMethod::RequiredUepProtectionK() {
  return _useUepProtectionK;
}

bool VCMProtectionMethod::RequiredUepProtectionD() {
  return _useUepProtectionD;
}

int VCMProtectionMethod::MaxFramesFec() const {
  return 1;
}

VCMNackFecMethod::VCMNackFecMethod(int64_t lowRttNackThresholdMs,
                                   int64_t highRttNackThresholdMs)
    : VCMFecMethod(),
      _lowRttNackMs(lowRttNackThresholdMs),
      _highRttNackMs(highRttNackThresholdMs),
      _maxFramesFec(1) {
  RTC_DCHECK(lowRttNackThresholdMs >= -1 && highRttNackThresholdMs >= -1);
  RTC_DCHECK(highRttNackThresholdMs == -1 ||
             lowRttNackThresholdMs <= highRttNackThresholdMs);
  RTC_DCHECK(lowRttNackThresholdMs > -1 || highRttNackThresholdMs == -1);
  _type = kNackFec;
}

VCMNackFecMethod::~VCMNackFecMethod() {

}
bool VCMNackFecMethod::ProtectionFactor(
    const VCMProtectionParameters* parameters) {

  VCMFecMethod::ProtectionFactor(parameters);
  if (_lowRttNackMs == -1 || parameters->rtt < _lowRttNackMs) {
    _protectionFactorD = 0;
    VCMFecMethod::UpdateProtectionFactorD(_protectionFactorD);

  } else if (_highRttNackMs == -1 || parameters->rtt < _highRttNackMs) {

    float adjustRtt = 1.0f;

    _protectionFactorD = rtc::saturated_cast<uint8_t>(
        adjustRtt * rtc::saturated_cast<float>(_protectionFactorD));

    VCMFecMethod::UpdateProtectionFactorD(_protectionFactorD);
  }

  return true;
}

int VCMNackFecMethod::ComputeMaxFramesFec(
    const VCMProtectionParameters* parameters) {
  if (parameters->numLayers > 2) {

    return 1;
  }

  float base_layer_framerate =
      parameters->frameRate /
      rtc::saturated_cast<float>(1 << (parameters->numLayers - 1));
  int max_frames_fec = std::max(
      rtc::saturated_cast<int>(
          2.0f * base_layer_framerate * parameters->rtt / 1000.0f + 0.5f),
      1);

  if (max_frames_fec > kUpperLimitFramesFec) {
    max_frames_fec = kUpperLimitFramesFec;
  }
  return max_frames_fec;
}

int VCMNackFecMethod::MaxFramesFec() const {
  return _maxFramesFec;
}

bool VCMNackFecMethod::BitRateTooLowForFec(
    const VCMProtectionParameters* parameters) {

  int estimate_bytes_per_frame = 1000 * BitsPerFrame(parameters) / 8;
  int max_bytes_per_frame = kMaxBytesPerFrameForFec;
  int num_pixels = parameters->codecWidth * parameters->codecHeight;
  if (num_pixels <= 352 * 288) {
    max_bytes_per_frame = kMaxBytesPerFrameForFecLow;
  } else if (num_pixels > 640 * 480) {
    max_bytes_per_frame = kMaxBytesPerFrameForFecHigh;
  }

  const int64_t kMaxRttTurnOffFec = 200;
  if (estimate_bytes_per_frame < max_bytes_per_frame &&
      parameters->numLayers < 3 && parameters->rtt < kMaxRttTurnOffFec) {
    return true;
  }
  return false;
}

bool VCMNackFecMethod::EffectivePacketLoss(
    const VCMProtectionParameters* parameters) {

  VCMFecMethod::EffectivePacketLoss(parameters);
  return true;
}

bool VCMNackFecMethod::UpdateParameters(
    const VCMProtectionParameters* parameters) {
  ProtectionFactor(parameters);
  EffectivePacketLoss(parameters);
  _maxFramesFec = ComputeMaxFramesFec(parameters);
  if (BitRateTooLowForFec(parameters)) {
    _protectionFactorK = 0;
    _protectionFactorD = 0;
  }

  _protectionFactorK = VCMFecMethod::ConvertFECRate(_protectionFactorK);
  _protectionFactorD = VCMFecMethod::ConvertFECRate(_protectionFactorD);

  return true;
}

VCMNackMethod::VCMNackMethod() : VCMProtectionMethod() {
  _type = kNack;
}

VCMNackMethod::~VCMNackMethod() {

}

bool VCMNackMethod::EffectivePacketLoss(
    const VCMProtectionParameters* parameter) {

  _effectivePacketLoss = 0;
  return true;
}

bool VCMNackMethod::UpdateParameters(
    const VCMProtectionParameters* parameters) {

  EffectivePacketLoss(parameters);

  return true;
}

VCMFecMethod::VCMFecMethod()
    : VCMProtectionMethod(),
      rate_control_settings_(RateControlSettings::ParseFromFieldTrials()) {
  _type = kFec;
}

VCMFecMethod::~VCMFecMethod() = default;

uint8_t VCMFecMethod::BoostCodeRateKey(uint8_t packetFrameDelta,
                                       uint8_t packetFrameKey) const {
  uint8_t boostRateKey = 2;

  uint8_t ratio = 1;

  if (packetFrameDelta > 0) {
    ratio = (int8_t)(packetFrameKey / packetFrameDelta);
  }
  ratio = VCM_MAX(boostRateKey, ratio);

  return ratio;
}

uint8_t VCMFecMethod::ConvertFECRate(uint8_t codeRateRTP) const {
  return rtc::saturated_cast<uint8_t>(
      VCM_MIN(255, (0.5 + 255.0 * codeRateRTP /
                              rtc::saturated_cast<float>(255 - codeRateRTP))));
}

void VCMFecMethod::UpdateProtectionFactorD(uint8_t protectionFactorD) {
  _protectionFactorD = protectionFactorD;
}

void VCMFecMethod::UpdateProtectionFactorK(uint8_t protectionFactorK) {
  _protectionFactorK = protectionFactorK;
}

bool VCMFecMethod::ProtectionFactor(const VCMProtectionParameters* parameters) {

  uint8_t packetLoss = rtc::saturated_cast<uint8_t>(255 * parameters->lossPr);
  if (packetLoss == 0) {
    _protectionFactorK = 0;
    _protectionFactorD = 0;
    return true;
  }

  uint8_t firstPartitionProt = rtc::saturated_cast<uint8_t>(255 * 0.20);

  uint8_t minProtLevelFec = 85;

  uint8_t lossThr = 0;
  uint8_t packetNumThr = 1;

  const uint8_t ratePar1 = 5;
  const uint8_t ratePar2 = 49;

  float spatialSizeToRef = rtc::saturated_cast<float>(parameters->codecWidth *
                                                      parameters->codecHeight) /
                           (rtc::saturated_cast<float>(704 * 576));

  const float resolnFac = 1.0 / powf(spatialSizeToRef, 0.3f);

  const int bitRatePerFrame = BitsPerFrame(parameters);

  const uint8_t avgTotPackets = rtc::saturated_cast<uint8_t>(
      1.5f + rtc::saturated_cast<float>(bitRatePerFrame) * 1000.0f /
                 rtc::saturated_cast<float>(8.0 * _maxPayloadSize));

  uint8_t codeRateDelta = 0;
  uint8_t codeRateKey = 0;

  const uint16_t effRateFecTable =
      rtc::saturated_cast<uint16_t>(resolnFac * bitRatePerFrame);
  uint8_t rateIndexTable = rtc::saturated_cast<uint8_t>(
      VCM_MAX(VCM_MIN((effRateFecTable - ratePar1) / ratePar1, ratePar2), 0));

  if (packetLoss >= kPacketLossMax) {
    packetLoss = kPacketLossMax - 1;
  }
  uint16_t indexTable = rateIndexTable * kPacketLossMax + packetLoss;

  RTC_DCHECK_LT(indexTable, kFecRateTableSize);

  codeRateDelta = kFecRateTable[indexTable];

  if (packetLoss > lossThr && avgTotPackets > packetNumThr) {

    if (codeRateDelta < firstPartitionProt) {
      codeRateDelta = firstPartitionProt;
    }
  }

  if (codeRateDelta >= kPacketLossMax) {
    codeRateDelta = kPacketLossMax - 1;
  }

  const uint8_t packetFrameDelta =
      rtc::saturated_cast<uint8_t>(0.5 + parameters->packetsPerFrame);
  const uint8_t packetFrameKey =
      rtc::saturated_cast<uint8_t>(0.5 + parameters->packetsPerFrameKey);
  const uint8_t boostKey = BoostCodeRateKey(packetFrameDelta, packetFrameKey);

  rateIndexTable = rtc::saturated_cast<uint8_t>(VCM_MAX(
      VCM_MIN(1 + (boostKey * effRateFecTable - ratePar1) / ratePar1, ratePar2),
      0));
  uint16_t indexTableKey = rateIndexTable * kPacketLossMax + packetLoss;

  indexTableKey = VCM_MIN(indexTableKey, kFecRateTableSize);

  RTC_DCHECK_LT(indexTableKey, kFecRateTableSize);

  codeRateKey = kFecRateTable[indexTableKey];

  int boostKeyProt = _scaleProtKey * codeRateDelta;
  if (boostKeyProt >= kPacketLossMax) {
    boostKeyProt = kPacketLossMax - 1;
  }

  codeRateKey = rtc::saturated_cast<uint8_t>(
      VCM_MAX(packetLoss, VCM_MAX(boostKeyProt, codeRateKey)));

  if (codeRateKey >= kPacketLossMax) {
    codeRateKey = kPacketLossMax - 1;
  }

  _protectionFactorK = codeRateKey;
  _protectionFactorD = codeRateDelta;

  float numPacketsFl =
      1.0f + (rtc::saturated_cast<float>(bitRatePerFrame) * 1000.0 /
                  rtc::saturated_cast<float>(8.0 * _maxPayloadSize) +
              0.5);

  const float estNumFecGen =
      0.5f +
      rtc::saturated_cast<float>(_protectionFactorD * numPacketsFl / 255.0f);

  _corrFecCost = 1.0f;
  if (estNumFecGen < 1.1f && _protectionFactorD < minProtLevelFec) {
    _corrFecCost = 0.5f;
  }
  if (estNumFecGen < 0.9f && _protectionFactorD < minProtLevelFec) {
    _corrFecCost = 0.0f;
  }

  return true;
}

int VCMFecMethod::BitsPerFrame(const VCMProtectionParameters* parameters) {

  const float bitRateRatio =
      webrtc::SimulcastRateAllocator::GetTemporalRateAllocation(
          parameters->numLayers, 0,
          rate_control_settings_.Vp8BaseHeavyTl3RateAllocation());
  float frameRateRatio = powf(1 / 2.0, parameters->numLayers - 1);
  float bitRate = parameters->bitRate * bitRateRatio;
  float frameRate = parameters->frameRate * frameRateRatio;

  float adjustmentFactor = 1;

  if (frameRate < 1.0f)
    frameRate = 1.0f;

  return rtc::saturated_cast<int>(adjustmentFactor * bitRate / frameRate);
}

bool VCMFecMethod::EffectivePacketLoss(
    const VCMProtectionParameters* parameters) {

  _effectivePacketLoss = 0;

  return true;
}

bool VCMFecMethod::UpdateParameters(const VCMProtectionParameters* parameters) {

  ProtectionFactor(parameters);

  EffectivePacketLoss(parameters);

  _protectionFactorK = ConvertFECRate(_protectionFactorK);
  _protectionFactorD = ConvertFECRate(_protectionFactorD);

  return true;
}
VCMLossProtectionLogic::VCMLossProtectionLogic(int64_t nowMs)
    : _currentParameters(),
      _rtt(0),
      _lossPr(0.0f),
      _bitRate(0.0f),
      _frameRate(0.0f),
      _keyFrameSize(0.0f),
      _fecRateKey(0),
      _fecRateDelta(0),
      _lastPrUpdateT(0),
      _lossPr255(0.9999f),
      _lossPrHistory(),
      _shortMaxLossPr255(0),
      _packetsPerFrame(0.9999f),
      _packetsPerFrameKey(0.9999f),
      _codecWidth(704),
      _codecHeight(576),
      _numLayers(1) {
  Reset(nowMs);
}

VCMLossProtectionLogic::~VCMLossProtectionLogic() {
  Release();
}

void VCMLossProtectionLogic::SetMethod(
    enum VCMProtectionMethodEnum newMethodType) {
  if (_selectedMethod && _selectedMethod->Type() == newMethodType)
    return;

  switch (newMethodType) {
    case kNack:
      _selectedMethod.reset(new VCMNackMethod());
      break;
    case kFec:
      _selectedMethod.reset(new VCMFecMethod());
      break;
    case kNackFec:
      _selectedMethod.reset(new VCMNackFecMethod(kLowRttNackMs, -1));
      break;
    case kNone:
      _selectedMethod.reset();
      break;
  }
  UpdateMethod();
}

void VCMLossProtectionLogic::UpdateRtt(int64_t rtt) {
  _rtt = rtt;
}

void VCMLossProtectionLogic::UpdateMaxLossHistory(uint8_t lossPr255,
                                                  int64_t now) {
  if (_lossPrHistory[0].timeMs >= 0 &&
      now - _lossPrHistory[0].timeMs < kLossPrShortFilterWinMs) {
    if (lossPr255 > _shortMaxLossPr255) {
      _shortMaxLossPr255 = lossPr255;
    }
  } else {

    if (_lossPrHistory[0].timeMs == -1) {

      _shortMaxLossPr255 = lossPr255;
    } else {

      for (int32_t i = (kLossPrHistorySize - 2); i >= 0; i--) {
        _lossPrHistory[i + 1].lossPr255 = _lossPrHistory[i].lossPr255;
        _lossPrHistory[i + 1].timeMs = _lossPrHistory[i].timeMs;
      }
    }
    if (_shortMaxLossPr255 == 0) {
      _shortMaxLossPr255 = lossPr255;
    }

    _lossPrHistory[0].lossPr255 = _shortMaxLossPr255;
    _lossPrHistory[0].timeMs = now;
    _shortMaxLossPr255 = 0;
  }
}

uint8_t VCMLossProtectionLogic::MaxFilteredLossPr(int64_t nowMs) const {
  uint8_t maxFound = _shortMaxLossPr255;
  if (_lossPrHistory[0].timeMs == -1) {
    return maxFound;
  }
  for (int32_t i = 0; i < kLossPrHistorySize; i++) {
    if (_lossPrHistory[i].timeMs == -1) {
      break;
    }
    if (nowMs - _lossPrHistory[i].timeMs >
        kLossPrHistorySize * kLossPrShortFilterWinMs) {

      break;
    }
    if (_lossPrHistory[i].lossPr255 > maxFound) {

      maxFound = _lossPrHistory[i].lossPr255;
    }
  }
  return maxFound;
}

uint8_t VCMLossProtectionLogic::FilteredLoss(int64_t nowMs,
                                             FilterPacketLossMode filter_mode,
                                             uint8_t lossPr255) {

  UpdateMaxLossHistory(lossPr255, nowMs);

  _lossPr255.Apply(rtc::saturated_cast<float>(nowMs - _lastPrUpdateT),
                   rtc::saturated_cast<float>(lossPr255));
  _lastPrUpdateT = nowMs;

  uint8_t filtered_loss = lossPr255;

  switch (filter_mode) {
    case kNoFilter:
      break;
    case kAvgFilter:
      filtered_loss = rtc::saturated_cast<uint8_t>(_lossPr255.filtered() + 0.5);
      break;
    case kMaxFilter:
      filtered_loss = MaxFilteredLossPr(nowMs);
      break;
  }

  return filtered_loss;
}

void VCMLossProtectionLogic::UpdateFilteredLossPr(uint8_t packetLossEnc) {
  _lossPr = rtc::saturated_cast<float>(packetLossEnc) / 255.0;
}

void VCMLossProtectionLogic::UpdateBitRate(float bitRate) {
  _bitRate = bitRate;
}

void VCMLossProtectionLogic::UpdatePacketsPerFrame(float nPackets,
                                                   int64_t nowMs) {
  _packetsPerFrame.Apply(
      rtc::saturated_cast<float>(nowMs - _lastPacketPerFrameUpdateT), nPackets);
  _lastPacketPerFrameUpdateT = nowMs;
}

void VCMLossProtectionLogic::UpdatePacketsPerFrameKey(float nPackets,
                                                      int64_t nowMs) {
  _packetsPerFrameKey.Apply(
      rtc::saturated_cast<float>(nowMs - _lastPacketPerFrameUpdateTKey),
      nPackets);
  _lastPacketPerFrameUpdateTKey = nowMs;
}

void VCMLossProtectionLogic::UpdateKeyFrameSize(float keyFrameSize) {
  _keyFrameSize = keyFrameSize;
}

void VCMLossProtectionLogic::UpdateFrameSize(size_t width, size_t height) {
  _codecWidth = width;
  _codecHeight = height;
}

void VCMLossProtectionLogic::UpdateNumLayers(int numLayers) {
  _numLayers = (numLayers == 0) ? 1 : numLayers;
}

bool VCMLossProtectionLogic::UpdateMethod() {
  if (!_selectedMethod)
    return false;
  _currentParameters.rtt = _rtt;
  _currentParameters.lossPr = _lossPr;
  _currentParameters.bitRate = _bitRate;
  _currentParameters.frameRate = _frameRate;
  _currentParameters.keyFrameSize = _keyFrameSize;
  _currentParameters.fecRateDelta = _fecRateDelta;
  _currentParameters.fecRateKey = _fecRateKey;
  _currentParameters.packetsPerFrame = _packetsPerFrame.filtered();
  _currentParameters.packetsPerFrameKey = _packetsPerFrameKey.filtered();
  _currentParameters.codecWidth = _codecWidth;
  _currentParameters.codecHeight = _codecHeight;
  _currentParameters.numLayers = _numLayers;
  return _selectedMethod->UpdateParameters(&_currentParameters);
}

VCMProtectionMethod* VCMLossProtectionLogic::SelectedMethod() const {
  return _selectedMethod.get();
}

VCMProtectionMethodEnum VCMLossProtectionLogic::SelectedType() const {
  return _selectedMethod ? _selectedMethod->Type() : kNone;
}

void VCMLossProtectionLogic::Reset(int64_t nowMs) {
  _lastPrUpdateT = nowMs;
  _lastPacketPerFrameUpdateT = nowMs;
  _lastPacketPerFrameUpdateTKey = nowMs;
  _lossPr255.Reset(0.9999f);
  _packetsPerFrame.Reset(0.9999f);
  _fecRateDelta = _fecRateKey = 0;
  for (int32_t i = 0; i < kLossPrHistorySize; i++) {
    _lossPrHistory[i].lossPr255 = 0;
    _lossPrHistory[i].timeMs = -1;
  }
  _shortMaxLossPr255 = 0;
  Release();
}

void VCMLossProtectionLogic::Release() {
  _selectedMethod.reset();
}

}
}
