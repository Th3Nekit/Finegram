/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_ENGINE_H_
#define API_VOIP_VOIP_ENGINE_H_

namespace webrtc {

class VoipBase;
class VoipCodec;
class VoipNetwork;
class VoipDtmf;
class VoipStatistics;
class VoipVolumeControl;

class VoipEngine {
 public:
  virtual ~VoipEngine() = default;

  virtual VoipBase& Base() = 0;

  virtual VoipNetwork& Network() = 0;

  virtual VoipCodec& Codec() = 0;

  virtual VoipDtmf& Dtmf() = 0;

  virtual VoipStatistics& Statistics() = 0;

  virtual VoipVolumeControl& VolumeControl() = 0;
};

}

#endif
