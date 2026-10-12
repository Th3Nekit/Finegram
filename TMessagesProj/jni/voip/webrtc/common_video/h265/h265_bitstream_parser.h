/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H265_H265_BITSTREAM_PARSER_H_
#define COMMON_VIDEO_H265_H265_BITSTREAM_PARSER_H_
#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/video_codecs/bitstream_parser.h"
#include "common_video/h265/h265_pps_parser.h"
#include "common_video/h265/h265_sps_parser.h"
#include "common_video/h265/h265_vps_parser.h"

namespace webrtc {

class H265BitstreamParser : public BitstreamParser {
 public:
  H265BitstreamParser();
  ~H265BitstreamParser() override;

  void ParseBitstream(const uint8_t* bitstream, size_t length);
  bool GetLastSliceQp(int* qp) const;

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

  uint32_t CalcNumPocTotalCurr(uint32_t num_long_term_sps,
                               uint32_t num_long_term_pics,
                               const std::vector<uint32_t> lt_idx_sps,
                               const std::vector<uint32_t> used_by_curr_pic_lt_flag,
                               uint32_t short_term_ref_pic_set_sps_flag,
                               uint32_t short_term_ref_pic_set_idx,
                               const H265SpsParser::ShortTermRefPicSet& short_term_ref_pic_set);

  absl::optional<H265SpsParser::SpsState> sps_;
  absl::optional<H265PpsParser::PpsState> pps_;

  absl::optional<int32_t> last_slice_qp_delta_;
};

}

#endif
