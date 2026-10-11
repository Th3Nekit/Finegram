/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_MEDIA_OPT_UTIL_H_
#define MODULES_VIDEO_CODING_MEDIA_OPT_UTIL_H_

#include <math.h>
#include <stdlib.h>

#include <memory>

#include "modules/video_coding/internal_defines.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/numerics/exp_filter.h"

namespace webrtc {
namespace media_optimization {

constexpr int kLossPrHistorySize = 10;

constexpr int kLossPrShortFilterWinMs = 1000;

enum FilterPacketLossMode {
  kNoFilter,
  kAvgFilter,
  kMaxFilter

};

constexpr int64_t kLowRttNackMs = 20;

constexpr int kMaxRttDelayThreshold = 500;

struct VCMProtectionParameters {
  VCMProtectionParameters();

  int64_t rtt;
  float lossPr;
  float bitRate;
  float packetsPerFrame;
  float packetsPerFrameKey;
  float frameRate;
  float keyFrameSize;
  uint8_t fecRateDelta;
  uint8_t fecRateKey;
  uint16_t codecWidth;
  uint16_t codecHeight;
  int numLayers;
};

enum VCMProtectionMethodEnum { kNack, kFec, kNackFec, kNone };

class VCMLossProbabilitySample {
 public:
  VCMLossProbabilitySample() : lossPr255(0), timeMs(-1) {}

  uint8_t lossPr255;
  int64_t timeMs;
};

class VCMProtectionMethod {
 public:
  VCMProtectionMethod();
  virtual ~VCMProtectionMethod();

  virtual bool UpdateParameters(const VCMProtectionParameters* parameters) = 0;

  VCMProtectionMethodEnum Type() const;

  virtual uint8_t RequiredPacketLossER();

  virtual uint8_t RequiredProtectionFactorK();

  virtual uint8_t RequiredProtectionFactorD();

  virtual bool RequiredUepProtectionK();

  virtual bool RequiredUepProtectionD();

  virtual int MaxFramesFec() const;

 protected:
  uint8_t _effectivePacketLoss;
  uint8_t _protectionFactorK;
  uint8_t _protectionFactorD;

  float _scaleProtKey;
  int32_t _maxPayloadSize;

  bool _useUepProtectionK;
  bool _useUepProtectionD;
  float _corrFecCost;
  VCMProtectionMethodEnum _type;
};

class VCMNackMethod : public VCMProtectionMethod {
 public:
  VCMNackMethod();
  ~VCMNackMethod() override;
  bool UpdateParameters(const VCMProtectionParameters* parameters) override;

  bool EffectivePacketLoss(const VCMProtectionParameters* parameter);
};

class VCMFecMethod : public VCMProtectionMethod {
 public:
  VCMFecMethod();
  ~VCMFecMethod() override;
  bool UpdateParameters(const VCMProtectionParameters* parameters) override;

  bool EffectivePacketLoss(const VCMProtectionParameters* parameters);

  bool ProtectionFactor(const VCMProtectionParameters* parameters);

  uint8_t BoostCodeRateKey(uint8_t packetFrameDelta,
                           uint8_t packetFrameKey) const;

  uint8_t ConvertFECRate(uint8_t codeRate) const;

  float AvgRecoveryFEC(const VCMProtectionParameters* parameters) const;

  void UpdateProtectionFactorD(uint8_t protectionFactorD);

  void UpdateProtectionFactorK(uint8_t protectionFactorK);

  int BitsPerFrame(const VCMProtectionParameters* parameters);

 protected:
  static constexpr int kUpperLimitFramesFec = 6;

  static constexpr int kMaxBytesPerFrameForFec = 700;

  static constexpr int kMaxBytesPerFrameForFecLow = 400;

  static constexpr int kMaxBytesPerFrameForFecHigh = 1000;

  const RateControlSettings rate_control_settings_;
};

class VCMNackFecMethod : public VCMFecMethod {
 public:
  VCMNackFecMethod(int64_t lowRttNackThresholdMs,
                   int64_t highRttNackThresholdMs);
  ~VCMNackFecMethod() override;
  bool UpdateParameters(const VCMProtectionParameters* parameters) override;

  bool EffectivePacketLoss(const VCMProtectionParameters* parameters);

  bool ProtectionFactor(const VCMProtectionParameters* parameters);

  int MaxFramesFec() const override;

  bool BitRateTooLowForFec(const VCMProtectionParameters* parameters);

 private:
  int ComputeMaxFramesFec(const VCMProtectionParameters* parameters);

  int64_t _lowRttNackMs;
  int64_t _highRttNackMs;
  int _maxFramesFec;
};

class VCMLossProtectionLogic {
 public:
  explicit VCMLossProtectionLogic(int64_t nowMs);
  ~VCMLossProtectionLogic();

  void SetMethod(VCMProtectionMethodEnum newMethodType);

  void UpdateRtt(int64_t rtt);

  void UpdateFilteredLossPr(uint8_t packetLossEnc);

  void UpdateBitRate(float bitRate);

  void UpdatePacketsPerFrame(float nPackets, int64_t nowMs);

  void UpdatePacketsPerFrameKey(float nPackets, int64_t nowMs);

  void UpdateKeyFrameSize(float keyFrameSize);

  void UpdateFrameRate(float frameRate) { _frameRate = frameRate; }

  void UpdateFrameSize(size_t width, size_t height);

  void UpdateNumLayers(int numLayers);

  void UpdateFECRates(uint8_t fecRateKey, uint8_t fecRateDelta) {
    _fecRateKey = fecRateKey;
    _fecRateDelta = fecRateDelta;
  }

  bool UpdateMethod();

  VCMProtectionMethod* SelectedMethod() const;

  VCMProtectionMethodEnum SelectedType() const;

  uint8_t FilteredLoss(int64_t nowMs,
                       FilterPacketLossMode filter_mode,
                       uint8_t lossPr255);

  void Reset(int64_t nowMs);

  void Release();

 private:

  void UpdateMaxLossHistory(uint8_t lossPr255, int64_t now);
  uint8_t MaxFilteredLossPr(int64_t nowMs) const;
  std::unique_ptr<VCMProtectionMethod> _selectedMethod;
  VCMProtectionParameters _currentParameters;
  int64_t _rtt;
  float _lossPr;
  float _bitRate;
  float _frameRate;
  float _keyFrameSize;
  uint8_t _fecRateKey;
  uint8_t _fecRateDelta;
  int64_t _lastPrUpdateT;
  int64_t _lastPacketPerFrameUpdateT;
  int64_t _lastPacketPerFrameUpdateTKey;
  rtc::ExpFilter _lossPr255;
  VCMLossProbabilitySample _lossPrHistory[kLossPrHistorySize];
  uint8_t _shortMaxLossPr255;
  rtc::ExpFilter _packetsPerFrame;
  rtc::ExpFilter _packetsPerFrameKey;
  size_t _codecWidth;
  size_t _codecHeight;
  int _numLayers;
};

}
}

#endif
