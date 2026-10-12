/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VOIP_VOIP_NETWORK_H_
#define API_VOIP_VOIP_NETWORK_H_

#include "api/array_view.h"
#include "api/voip/voip_base.h"

namespace webrtc {

class VoipNetwork {
 public:

  virtual VoipResult ReceivedRTPPacket(
      ChannelId channel_id,
      rtc::ArrayView<const uint8_t> rtp_packet) = 0;

  virtual VoipResult ReceivedRTCPPacket(
      ChannelId channel_id,
      rtc::ArrayView<const uint8_t> rtcp_packet) = 0;

 protected:
  virtual ~VoipNetwork() = default;
};

}

#endif
