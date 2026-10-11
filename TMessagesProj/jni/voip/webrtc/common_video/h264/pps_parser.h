/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_VIDEO_H264_PPS_PARSER_H_
#define COMMON_VIDEO_H264_PPS_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/array_view.h"

namespace webrtc {

class PpsParser {
 public:

  struct PpsState {
    PpsState() = default;

    bool bottom_field_pic_order_in_frame_present_flag = false;
    bool weighted_pred_flag = false;
    bool entropy_coding_mode_flag = false;
    uint32_t weighted_bipred_idc = false;
    uint32_t redundant_pic_cnt_present_flag = 0;
    int pic_init_qp_minus26 = 0;
    uint32_t id = 0;
    uint32_t sps_id = 0;
  };

  static absl::optional<PpsState> ParsePps(const uint8_t* data, size_t length);

  static bool ParsePpsIds(const uint8_t* data,
                          size_t length,
                          uint32_t* pps_id,
                          uint32_t* sps_id);

  static absl::optional<uint32_t> ParsePpsIdFromSlice(const uint8_t* data,
                                                      size_t length);

 protected:

  static absl::optional<PpsState> ParseInternal(
      rtc::ArrayView<const uint8_t> buffer);
};

}

#endif
