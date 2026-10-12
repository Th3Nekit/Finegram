/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef COMMON_VIDEO_H264_SPS_VUI_REWRITER_H_
#define COMMON_VIDEO_H264_SPS_VUI_REWRITER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/video/color_space.h"
#include "common_video/h264/sps_parser.h"
#include "rtc_base/buffer.h"

namespace webrtc {

class SpsVuiRewriter : private SpsParser {
 public:
  enum class ParseResult { kFailure, kVuiOk, kVuiRewritten };
  enum class Direction { kIncoming, kOutgoing };

  static ParseResult ParseAndRewriteSps(
      const uint8_t* buffer,
      size_t length,
      absl::optional<SpsParser::SpsState>* sps,
      const ColorSpace* color_space,
      rtc::Buffer* destination,
      Direction Direction);

  static rtc::Buffer ParseOutgoingBitstreamAndRewrite(
      rtc::ArrayView<const uint8_t> buffer,
      const ColorSpace* color_space);

 private:
  static ParseResult ParseAndRewriteSps(
      const uint8_t* buffer,
      size_t length,
      absl::optional<SpsParser::SpsState>* sps,
      const ColorSpace* color_space,
      rtc::Buffer* destination);

  static void UpdateStats(ParseResult result, Direction direction);
};

}

#endif
