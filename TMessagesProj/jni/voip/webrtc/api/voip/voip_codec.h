/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_CODEC_H_
#define API_VOIP_VOIP_CODEC_H_

#include <map>

#include "api/audio_codecs/audio_format.h"
#include "api/voip/voip_base.h"

namespace webrtc {

class VoipCodec {
 public:

  virtual VoipResult SetSendCodec(ChannelId channel_id,
                                  int payload_type,
                                  const SdpAudioFormat& encoder_spec) = 0;

  virtual VoipResult SetReceiveCodecs(
      ChannelId channel_id,
      const std::map<int, SdpAudioFormat>& decoder_specs) = 0;

 protected:
  virtual ~VoipCodec() = default;
};

}

#endif
