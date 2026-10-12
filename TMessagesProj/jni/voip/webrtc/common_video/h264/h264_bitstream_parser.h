/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H264_H264_BITSTREAM_PARSER_H_
#define COMMON_VIDEO_H264_H264_BITSTREAM_PARSER_H_
#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/video_codecs/bitstream_parser.h"
#include "common_video/h264/pps_parser.h"
#include "common_video/h264/sps_parser.h"

namespace webrtc {

class H264BitstreamParser : public BitstreamParser {
 public:
  H264BitstreamParser();
  ~H264BitstreamParser() override;

  void ParseBitstream(rtc::ArrayView<const uint8_t> bitstream) override;
  absl::optional<int> GetLastSliceQp() const override;

 protected:
  enum Result {
    kOk,
    kInvalidStream,
    kUnsupportedStream,
  };
  void ParseSlice(const uint8_t* slice, size_t length);
  Result ParseNonParameterSetNalu(const uint8_t* source,
                                  size_t source_length,
                                  uint8_t nalu_type);

  absl::optional<SpsParser::SpsState> sps_;
  absl::optional<PpsParser::PpsState> pps_;

  absl::optional<int32_t> last_slice_qp_delta_;
};

}

#endif
