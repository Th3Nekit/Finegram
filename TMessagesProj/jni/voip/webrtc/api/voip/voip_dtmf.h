/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_DTMF_H_
#define API_VOIP_VOIP_DTMF_H_

#include "api/voip/voip_base.h"

namespace webrtc {

enum class DtmfEvent : uint8_t {
  kDigitZero = 0,
  kDigitOne,
  kDigitTwo,
  kDigitThree,
  kDigitFour,
  kDigitFive,
  kDigitSix,
  kDigitSeven,
  kDigitEight,
  kDigitNine,
  kAsterisk,
  kHash,
  kLetterA,
  kLetterB,
  kLetterC,
  kLetterD
};

class VoipDtmf {
 public:

  virtual VoipResult RegisterTelephoneEventType(ChannelId channel_id,
                                                int rtp_payload_type,
                                                int sample_rate_hz) = 0;

  virtual VoipResult SendDtmfEvent(ChannelId channel_id,
                                   DtmfEvent dtmf_event,
                                   int duration_ms) = 0;

 protected:
  virtual ~VoipDtmf() = default;
};

}

#endif
