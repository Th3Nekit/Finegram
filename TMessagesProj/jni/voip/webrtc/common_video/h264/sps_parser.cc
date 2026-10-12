/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/h264/sps_parser.h"

#include <cstdint>
#include <vector>

#include "common_video/h264/h264_common.h"
#include "rtc_base/bitstream_reader.h"

namespace {
constexpr int kScalingDeltaMin = -128;
constexpr int kScaldingDeltaMax = 127;
}

namespace webrtc {

SpsParser::SpsState::SpsState() = default;
SpsParser::SpsState::SpsState(const SpsState&) = default;
SpsParser::SpsState::~SpsState() = default;

absl::optional<SpsParser::SpsState> SpsParser::ParseSps(const uint8_t* data,
                                                        size_t length) {
  std::vector<uint8_t> unpacked_buffer = H264::ParseRbsp(data, length);
  BitstreamReader reader(unpacked_buffer);
  return ParseSpsUpToVui(reader);
}

absl::optional<SpsParser::SpsState> SpsParser::ParseSpsUpToVui(
    BitstreamReader& reader) {

  SpsState sps;

  uint32_t chroma_format_idc = 1;

  uint8_t profile_idc = reader.Read<uint8_t>();

  reader.ConsumeBits(16);

  sps.id = reader.ReadExponentialGolomb();
  sps.separate_colour_plane_flag = 0;

  if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
      profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
      profile_idc == 86 || profile_idc == 118 || profile_idc == 128 ||
      profile_idc == 138 || profile_idc == 139 || profile_idc == 134) {

    chroma_format_idc = reader.ReadExponentialGolomb();
    if (chroma_format_idc == 3) {

      sps.separate_colour_plane_flag = reader.ReadBit();
    }

    reader.ReadExponentialGolomb();

    reader.ReadExponentialGolomb();

    reader.ConsumeBits(1);

    if (reader.Read<bool>()) {

      int scaling_list_count = (chroma_format_idc == 3 ? 12 : 8);
      for (int i = 0; i < scaling_list_count; ++i) {

        if (reader.Read<bool>()) {
          int last_scale = 8;
          int next_scale = 8;
          int size_of_scaling_list = i < 6 ? 16 : 64;
          for (int j = 0; j < size_of_scaling_list; j++) {
            if (next_scale != 0) {

              int delta_scale = reader.ReadSignedExponentialGolomb();
              if (!reader.Ok() || delta_scale < kScalingDeltaMin ||
                  delta_scale > kScaldingDeltaMax) {
                return absl::nullopt;
              }
              next_scale = (last_scale + delta_scale + 256) % 256;
            }
            if (next_scale != 0)
              last_scale = next_scale;
          }
        }
      }
    }
  }

  const uint32_t kMaxLog2Minus4 = 12;

  uint32_t log2_max_frame_num_minus4 = reader.ReadExponentialGolomb();
  if (!reader.Ok() || log2_max_frame_num_minus4 > kMaxLog2Minus4) {
    return absl::nullopt;
  }
  sps.log2_max_frame_num = log2_max_frame_num_minus4 + 4;

  sps.pic_order_cnt_type = reader.ReadExponentialGolomb();
  if (sps.pic_order_cnt_type == 0) {

    uint32_t log2_max_pic_order_cnt_lsb_minus4 = reader.ReadExponentialGolomb();
    if (!reader.Ok() || log2_max_pic_order_cnt_lsb_minus4 > kMaxLog2Minus4) {
      return absl::nullopt;
    }
    sps.log2_max_pic_order_cnt_lsb = log2_max_pic_order_cnt_lsb_minus4 + 4;
  } else if (sps.pic_order_cnt_type == 1) {

    sps.delta_pic_order_always_zero_flag = reader.ReadBit();

    reader.ReadExponentialGolomb();

    reader.ReadExponentialGolomb();

    uint32_t num_ref_frames_in_pic_order_cnt_cycle =
        reader.ReadExponentialGolomb();
    for (size_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; ++i) {

      reader.ReadExponentialGolomb();
      if (!reader.Ok()) {
        return absl::nullopt;
      }
    }
  }

  sps.max_num_ref_frames = reader.ReadExponentialGolomb();

  reader.ConsumeBits(1);

  sps.width = 16 * (reader.ReadExponentialGolomb() + 1);

  uint32_t pic_height_in_map_units_minus1 = reader.ReadExponentialGolomb();

  sps.frame_mbs_only_flag = reader.ReadBit();
  if (!sps.frame_mbs_only_flag) {

    reader.ConsumeBits(1);
  }
  sps.height =
      16 * (2 - sps.frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1);

  reader.ConsumeBits(1);

  uint32_t frame_crop_left_offset = 0;
  uint32_t frame_crop_right_offset = 0;
  uint32_t frame_crop_top_offset = 0;
  uint32_t frame_crop_bottom_offset = 0;

  if (reader.Read<bool>()) {

    frame_crop_left_offset = reader.ReadExponentialGolomb();
    frame_crop_right_offset = reader.ReadExponentialGolomb();
    frame_crop_top_offset = reader.ReadExponentialGolomb();
    frame_crop_bottom_offset = reader.ReadExponentialGolomb();
  }

  sps.vui_params_present = reader.ReadBit();

  if (!reader.Ok()) {
    return absl::nullopt;
  }

  if (sps.separate_colour_plane_flag || chroma_format_idc == 0) {
    frame_crop_bottom_offset *= (2 - sps.frame_mbs_only_flag);
    frame_crop_top_offset *= (2 - sps.frame_mbs_only_flag);
  } else if (!sps.separate_colour_plane_flag && chroma_format_idc > 0) {

    if (chroma_format_idc == 1 || chroma_format_idc == 2) {
      frame_crop_left_offset *= 2;
      frame_crop_right_offset *= 2;
    }

    if (chroma_format_idc == 1) {
      frame_crop_top_offset *= 2;
      frame_crop_bottom_offset *= 2;
    }
  }

  sps.width -= (frame_crop_left_offset + frame_crop_right_offset);
  sps.height -= (frame_crop_top_offset + frame_crop_bottom_offset);

  return sps;
}

}
