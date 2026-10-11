/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains codec dependent definitions that are needed in
// order to compile the WebRTC codebase, even if this codec is not used.

#ifndef MODULES_VIDEO_CODING_CODECS_H265_INCLUDE_H265_GLOBALS_H_
#define MODULES_VIDEO_CODING_CODECS_H265_INCLUDE_H265_GLOBALS_H_

#ifndef DISABLE_H265

#include "modules/video_coding/codecs/h264/include/h264_globals.h"

namespace webrtc {

enum H265PacketizationTypes {
  kH265SingleNalu,
  kH265AP,

  kH265FU,

};

struct H265NaluInfo {
  uint8_t type;
  int vps_id;
  int sps_id;
  int pps_id;
};

enum class H265PacketizationMode {
  NonInterleaved = 0,
  SingleNalUnit
};

struct RTPVideoHeaderH265 {

  uint8_t nalu_type;
  H265PacketizationTypes packetization_type;
  H265NaluInfo nalus[kMaxNalusPerPacket];
  size_t nalus_length;

  H265PacketizationMode packetization_mode;
};

}

#endif

#endif
