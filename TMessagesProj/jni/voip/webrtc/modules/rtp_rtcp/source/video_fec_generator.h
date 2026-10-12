/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_VIDEO_FEC_GENERATOR_H_
#define MODULES_RTP_RTCP_SOURCE_VIDEO_FEC_GENERATOR_H_

#include <memory>
#include <vector>

#include "api/units/data_rate.h"
#include "modules/include/module_fec_types.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

class VideoFecGenerator {
 public:
  VideoFecGenerator() = default;
  virtual ~VideoFecGenerator() = default;

  enum class FecType { kFlexFec, kUlpFec };
  virtual FecType GetFecType() const = 0;

  virtual absl::optional<uint32_t> FecSsrc() = 0;

  virtual size_t MaxPacketOverhead() const = 0;

  virtual DataRate CurrentFecRate() const = 0;

  virtual void SetProtectionParameters(
      const FecProtectionParams& delta_params,
      const FecProtectionParams& key_params) = 0;

  virtual void AddPacketAndGenerateFec(const RtpPacketToSend& packet) = 0;

  virtual std::vector<std::unique_ptr<RtpPacketToSend>> GetFecPackets() = 0;

  virtual absl::optional<RtpState> GetRtpState() = 0;
};

}
#endif
