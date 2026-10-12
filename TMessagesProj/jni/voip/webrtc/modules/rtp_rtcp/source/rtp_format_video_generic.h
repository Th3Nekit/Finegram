/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_

#include <stdint.h>

#include <vector>

#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_format.h"

namespace webrtc {

class RtpPacketToSend;
struct RTPVideoHeader;

namespace RtpFormatVideoGeneric {
static const uint8_t kKeyFrameBit = 0x01;
static const uint8_t kFirstPacketBit = 0x02;

static const uint8_t kExtendedHeaderBit = 0x04;
}

class RtpPacketizerGeneric : public RtpPacketizer {
 public:

  RtpPacketizerGeneric(rtc::ArrayView<const uint8_t> payload,
                       PayloadSizeLimits limits,
                       const RTPVideoHeader& rtp_video_header);

  RtpPacketizerGeneric(rtc::ArrayView<const uint8_t> payload,
                       PayloadSizeLimits limits);

  ~RtpPacketizerGeneric() override;

  RtpPacketizerGeneric(const RtpPacketizerGeneric&) = delete;
  RtpPacketizerGeneric& operator=(const RtpPacketizerGeneric&) = delete;

  size_t NumPackets() const override;

  bool NextPacket(RtpPacketToSend* packet) override;

 private:

  void BuildHeader(const RTPVideoHeader& rtp_video_header);

  uint8_t header_[3];
  size_t header_size_;
  rtc::ArrayView<const uint8_t> remaining_payload_;
  std::vector<int> payload_sizes_;
  std::vector<int>::const_iterator current_packet_;
};
}
#endif
