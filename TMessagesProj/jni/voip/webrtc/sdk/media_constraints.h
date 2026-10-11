/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Implementation of the w3c constraints spec is the responsibility of the
// browser. Chrome no longer uses the constraints api declared here, and it will
// be removed from WebRTC.
// https://bugs.chromium.org/p/webrtc/issues/detail?id=9239

#ifndef SDK_MEDIA_CONSTRAINTS_H_
#define SDK_MEDIA_CONSTRAINTS_H_

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "api/audio_options.h"
#include "api/peer_connection_interface.h"

namespace webrtc {

class MediaConstraints {
 public:
  struct Constraint {
    Constraint() {}
    Constraint(const std::string& key, const std::string value)
        : key(key), value(value) {}
    std::string key;
    std::string value;
  };

  class Constraints : public std::vector<Constraint> {
   public:
    Constraints() = default;
    Constraints(std::initializer_list<Constraint> l)
        : std::vector<Constraint>(l) {}

    bool FindFirst(const std::string& key, std::string* value) const;
  };

  MediaConstraints() = default;
  MediaConstraints(Constraints mandatory, Constraints optional)
      : mandatory_(std::move(mandatory)), optional_(std::move(optional)) {}

  static const char kGoogEchoCancellation[];

  static const char kAutoGainControl[];
  static const char kNoiseSuppression[];
  static const char kHighpassFilter[];
  static const char kAudioMirroring[];
  static const char
      kAudioNetworkAdaptorConfig[];
  static const char kInitAudioRecordingOnSend[];

  static const char kOfferToReceiveVideo[];
  static const char kOfferToReceiveAudio[];
  static const char kVoiceActivityDetection[];
  static const char kIceRestart[];

  static const char kUseRtpMux[];

  static const char kValueTrue[];
  static const char kValueFalse[];

  static const char kEnableDscp[];

  static const char kEnableIPv6[];

  static const char kEnableVideoSuspendBelowMinBitrate[];
  static const char kScreencastMinBitrate[];
  static const char kCpuOveruseDetection[];

  static const char kRawPacketizationForVideoEnabled[];

  static const char kNumSimulcastLayers[];

  ~MediaConstraints() = default;

  const Constraints& GetMandatory() const { return mandatory_; }
  const Constraints& GetOptional() const { return optional_; }

 private:
  const Constraints mandatory_ = {};
  const Constraints optional_ = {};
};

void CopyConstraintsIntoRtcConfiguration(
    const MediaConstraints* constraints,
    PeerConnectionInterface::RTCConfiguration* configuration);

void CopyConstraintsIntoAudioOptions(const MediaConstraints* constraints,
                                     cricket::AudioOptions* options);

bool CopyConstraintsIntoOfferAnswerOptions(
    const MediaConstraints* constraints,
    PeerConnectionInterface::RTCOfferAnswerOptions* offer_answer_options);

}

#endif
