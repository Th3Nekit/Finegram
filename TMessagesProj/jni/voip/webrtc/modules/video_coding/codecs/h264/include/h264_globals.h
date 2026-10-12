/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains codec dependent definitions that are needed in
// order to compile the WebRTC codebase, even if this codec is not used.

#ifndef MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_GLOBALS_H_
#define MODULES_VIDEO_CODING_CODECS_H264_INCLUDE_H264_GLOBALS_H_

#include <algorithm>
#include <string>

#include "modules/video_coding/codecs/interface/common_constants.h"
#include "rtc_base/checks.h"

namespace webrtc {

enum H264PacketizationTypes {
  kH264SingleNalu,
  kH264StapA,

  kH264FuA,

};

enum class H264PacketizationMode {
  NonInterleaved = 0,
  SingleNalUnit
};

inline std::string ToString(H264PacketizationMode mode) {
  if (mode == H264PacketizationMode::NonInterleaved) {
    return "NonInterleaved";
  } else if (mode == H264PacketizationMode::SingleNalUnit) {
    return "SingleNalUnit";
  }
  RTC_DCHECK_NOTREACHED();
  return "";
}

struct NaluInfo {
  uint8_t type;
  int sps_id;
  int pps_id;

  friend bool operator==(const NaluInfo& lhs, const NaluInfo& rhs) {
    return lhs.type == rhs.type && lhs.sps_id == rhs.sps_id &&
           lhs.pps_id == rhs.pps_id;
  }

  friend bool operator!=(const NaluInfo& lhs, const NaluInfo& rhs) {
    return !(lhs == rhs);
  }
};

const size_t kMaxNalusPerPacket = 10;

struct RTPVideoHeaderH264 {

  uint8_t nalu_type;

  H264PacketizationTypes packetization_type;
  NaluInfo nalus[kMaxNalusPerPacket];
  size_t nalus_length;

  H264PacketizationMode packetization_mode;

  friend bool operator==(const RTPVideoHeaderH264& lhs,
                         const RTPVideoHeaderH264& rhs) {
    return lhs.nalu_type == rhs.nalu_type &&
           lhs.packetization_type == rhs.packetization_type &&
           std::equal(lhs.nalus, lhs.nalus + lhs.nalus_length, rhs.nalus,
                      rhs.nalus + rhs.nalus_length) &&
           lhs.packetization_mode == rhs.packetization_mode;
  }

  friend bool operator!=(const RTPVideoHeaderH264& lhs,
                         const RTPVideoHeaderH264& rhs) {
    return !(lhs == rhs);
  }
};

}

#endif
