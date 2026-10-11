/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_DEPRECATED_PACKET_H_
#define MODULES_VIDEO_CODING_DEPRECATED_PACKET_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/rtp_headers.h"
#include "api/rtp_packet_info.h"
#include "api/units/timestamp.h"
#include "api/video/video_frame_type.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

enum VCMNaluCompleteness {
  kNaluUnset = 0,
  kNaluComplete = 1,
  kNaluStart,
  kNaluIncomplete,
  kNaluEnd,
};

class VCMPacket {
 public:
  VCMPacket();

  VCMPacket(const uint8_t* ptr,
            size_t size,
            const RTPHeader& rtp_header,
            const RTPVideoHeader& video_header,
            int64_t ntp_time_ms,
            Timestamp receive_time);

  ~VCMPacket();

  VideoCodecType codec() const { return video_header.codec; }
  int width() const { return video_header.width; }
  int height() const { return video_header.height; }

  bool is_first_packet_in_frame() const {
    return video_header.is_first_packet_in_frame;
  }
  bool is_last_packet_in_frame() const {
    return video_header.is_last_packet_in_frame;
  }

  uint8_t payloadType;
  uint32_t timestamp;

  int64_t ntp_time_ms_;
  uint16_t seqNum;
  const uint8_t* dataPtr;
  size_t sizeBytes;
  bool markerBit;
  int timesNacked;

  VCMNaluCompleteness completeNALU;
  bool insertStartCode;

  RTPVideoHeader video_header;
  absl::optional<RtpGenericFrameDescriptor> generic_descriptor;

  RtpPacketInfo packet_info;
};

}
#endif
