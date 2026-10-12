/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

class RtpPacketToSend;

class RtpPacketizer {
 public:
  struct PayloadSizeLimits {
    int max_payload_len = 1200;
    int first_packet_reduction_len = 0;
    int last_packet_reduction_len = 0;

    int single_packet_reduction_len = 0;
  };

  static std::unique_ptr<RtpPacketizer> Create(
      absl::optional<VideoCodecType> type,
      rtc::ArrayView<const uint8_t> payload,
      PayloadSizeLimits limits,

      const RTPVideoHeader& rtp_video_header);

  virtual ~RtpPacketizer() = default;

  virtual size_t NumPackets() const = 0;

  virtual bool NextPacket(RtpPacketToSend* packet) = 0;

  static std::vector<int> SplitAboutEqually(int payload_len,
                                            const PayloadSizeLimits& limits);
};
}
#endif
