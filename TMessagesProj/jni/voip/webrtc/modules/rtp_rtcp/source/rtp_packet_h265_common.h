/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_PACKET_H265_COMMON_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_PACKET_H265_COMMON_H_

#include <string>
#include <vector>

namespace webrtc {

constexpr size_t kH265PayloadHeaderSizeBytes = 2;

constexpr size_t kH265NalHeaderSizeBytes = 2;

constexpr size_t kH265FuHeaderSizeBytes = 1;

constexpr size_t kH265LengthFieldSizeBytes = 2;
constexpr size_t kH265ApHeaderSizeBytes =
    kH265NalHeaderSizeBytes + kH265LengthFieldSizeBytes;

enum NalHdrMasks {
  kH265FBit = 0x80,
  kH265TypeMask = 0x7E,
  kH265LayerIDHMask = 0x1,
  kH265LayerIDLMask = 0xF8,
  kH265TIDMask = 0x7,
  kH265TypeMaskN = 0x81,
  kH265TypeMaskInFuHeader = 0x3F
};

enum FuBitmasks {
  kH265SBitMask = 0x80,
  kH265EBitMask = 0x40,
  kH265FuTypeBitMask = 0x3F
};

constexpr uint8_t kStartCode[] = {0, 0, 0, 1};

}

#endif
