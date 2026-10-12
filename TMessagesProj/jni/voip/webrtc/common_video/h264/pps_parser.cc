/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/pps_parser.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/numeric/bits.h"
#include "common_video/h264/h264_common.h"
#include "rtc_base/bitstream_reader.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace {
constexpr int kMaxPicInitQpDeltaValue = 25;
constexpr int kMinPicInitQpDeltaValue = -26;
}

absl::optional<PpsParser::PpsState> PpsParser::ParsePps(const uint8_t* data,
                                                        size_t length) {

  return ParseInternal(H264::ParseRbsp(data, length));
}

bool PpsParser::ParsePpsIds(const uint8_t* data,
                            size_t length,
                            uint32_t* pps_id,
                            uint32_t* sps_id) {
  RTC_DCHECK(pps_id);
  RTC_DCHECK(sps_id);

  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(data, length);
  BitstreamReader reader(unpacked_buffer);
  *pps_id = reader.ReadExponentialGolomb();
  *sps_id = reader.ReadExponentialGolomb();
  return reader.Ok();
}

absl::optional<uint32_t> PpsParser::ParsePpsIdFromSlice(const uint8_t* data,
                                                        size_t length) {
  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(data, length);
  BitstreamReader slice_reader(unpacked_buffer);

  slice_reader.ReadExponentialGolomb();

  slice_reader.ReadExponentialGolomb();

  uint32_t slice_pps_id = slice_reader.ReadExponentialGolomb();
  if (!slice_reader.Ok()) {
    return absl::nullopt;
  }
  return slice_pps_id;
}

absl::optional<PpsParser::PpsState> PpsParser::ParseInternal(
    rtc::ArrayView<const uint8_t> buffer) {
  BitstreamReader reader(buffer);
  PpsState pps;
  pps.id = reader.ReadExponentialGolomb();
  pps.sps_id = reader.ReadExponentialGolomb();

  pps.entropy_coding_mode_flag = reader.Read<bool>();

  pps.bottom_field_pic_order_in_frame_present_flag = reader.Read<bool>();

  uint32_t num_slice_groups_minus1 = reader.ReadExponentialGolomb();
  if (num_slice_groups_minus1 > 0) {

    uint32_t slice_group_map_type = reader.ReadExponentialGolomb();
    if (slice_group_map_type == 0) {
      for (uint32_t i_group = 0;
           i_group <= num_slice_groups_minus1 && reader.Ok(); ++i_group) {

        reader.ReadExponentialGolomb();
      }
    } else if (slice_group_map_type == 1) {

    } else if (slice_group_map_type == 2) {
      for (uint32_t i_group = 0;
           i_group <= num_slice_groups_minus1 && reader.Ok(); ++i_group) {

        reader.ReadExponentialGolomb();

        reader.ReadExponentialGolomb();
      }
    } else if (slice_group_map_type == 3 || slice_group_map_type == 4 ||
               slice_group_map_type == 5) {

      reader.ConsumeBits(1);

      reader.ReadExponentialGolomb();
    } else if (slice_group_map_type == 6) {

      uint32_t pic_size_in_map_units = reader.ReadExponentialGolomb() + 1;
      int slice_group_id_bits = 1 + absl::bit_width(num_slice_groups_minus1);

      int64_t bits_to_consume =
          int64_t{slice_group_id_bits} * pic_size_in_map_units;
      if (!reader.Ok() || bits_to_consume > std::numeric_limits<int>::max()) {
        return absl::nullopt;
      }
      reader.ConsumeBits(bits_to_consume);
    }
  }

  reader.ReadExponentialGolomb();

  reader.ReadExponentialGolomb();

  pps.weighted_pred_flag = reader.Read<bool>();

  pps.weighted_bipred_idc = reader.ReadBits(2);

  pps.pic_init_qp_minus26 = reader.ReadSignedExponentialGolomb();

  if (!reader.Ok() || pps.pic_init_qp_minus26 > kMaxPicInitQpDeltaValue ||
      pps.pic_init_qp_minus26 < kMinPicInitQpDeltaValue) {
    return absl::nullopt;
  }

  reader.ReadExponentialGolomb();

  reader.ReadExponentialGolomb();

  reader.ConsumeBits(2);

  pps.redundant_pic_cnt_present_flag = reader.ReadBit();
  if (!reader.Ok()) {
    return absl::nullopt;
  }

  return pps;
}

}
