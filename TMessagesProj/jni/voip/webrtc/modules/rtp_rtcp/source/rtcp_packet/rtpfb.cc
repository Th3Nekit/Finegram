/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"

#include "modules/rtp_rtcp/source/byte_io.h"

namespace webrtc {
namespace rtcp {
constexpr uint8_t Rtpfb::kPacketType;

void Rtpfb::ParseCommonFeedback(const uint8_t* payload) {
  SetSenderSsrc(ByteReader<uint32_t>::ReadBigEndian(&payload[0]));
  SetMediaSsrc(ByteReader<uint32_t>::ReadBigEndian(&payload[4]));
}

void Rtpfb::CreateCommonFeedback(uint8_t* payload) const {
  ByteWriter<uint32_t>::WriteBigEndian(&payload[0], sender_ssrc());
  ByteWriter<uint32_t>::WriteBigEndian(&payload[4], media_ssrc());
}

}
}
