/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_VP9_UNCOMPRESSED_HEADER_PARSER_H_
#define MODULES_VIDEO_CODING_UTILITY_VP9_UNCOMPRESSED_HEADER_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <bitset>
#include <string>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/video_coding/utility/vp9_constants.h"

namespace webrtc {

namespace vp9 {

bool GetQp(const uint8_t* buf, size_t length, int* qp);

}

enum class Vp9BitDept : uint8_t {
  k8Bit = 8,
  k10Bit = 10,
  k12Bit = 12,
};

enum class Vp9ColorSpace : uint8_t {
  CS_UNKNOWN = 0,

  CS_BT_601 = 1,
  CS_BT_709 = 2,
  CS_SMPTE_170 = 3,
  CS_SMPTE_240 = 4,
  CS_BT_2020 = 5,
  CS_RESERVED = 6,
  CS_RGB = 7,
};

enum class Vp9ColorRange {
  kStudio,

  kFull
};

enum class Vp9YuvSubsampling {
  k444,
  k440,
  k422,
  k420,
};

enum Vp9ReferenceFrame : int {
  kNone = -1,
  kIntra = 0,
  kLast = 1,
  kGolden = 2,
  kAltref = 3,
};

enum class Vp9InterpolationFilter : uint8_t {
  kEightTap = 0,
  kEightTapSmooth = 1,
  kEightTapSharp = 2,
  kBilinear = 3,
  kSwitchable = 4
};

struct Vp9UncompressedHeader {
  int profile = 0;
  absl::optional<uint8_t> show_existing_frame;
  bool is_keyframe = false;
  bool show_frame = false;
  bool error_resilient = false;
  Vp9BitDept bit_detph = Vp9BitDept::k8Bit;
  absl::optional<Vp9ColorSpace> color_space;
  absl::optional<Vp9ColorRange> color_range;
  absl::optional<Vp9YuvSubsampling> sub_sampling;
  int frame_width = 0;
  int frame_height = 0;
  int render_width = 0;
  int render_height = 0;

  size_t tile_cols_log2 = 0;
  size_t tile_rows_log2 = 0;
  absl::optional<size_t> render_size_offset_bits;
  Vp9InterpolationFilter interpolation_filter =
      Vp9InterpolationFilter::kEightTap;
  bool allow_high_precision_mv = false;
  int base_qp = 0;
  bool is_lossless = false;
  uint8_t frame_context_idx = 0;

  bool segmentation_enabled = false;
  absl::optional<std::array<uint8_t, 7>> segmentation_tree_probs;
  absl::optional<std::array<uint8_t, 3>> segmentation_pred_prob;
  bool segmentation_is_delta = false;
  std::array<std::array<absl::optional<int>, kVp9SegLvlMax>, kVp9MaxSegments>
      segmentation_features;

  std::array<int, kVp9RefsPerFrame> reference_buffers = {-1, -1, -1};

  std::bitset<kVp9MaxRefFrames> reference_buffers_sign_bias = 0;

  absl::optional<int> infer_size_from_reference;

  std::bitset<kVp9NumRefFrames> updated_buffers = 0;

  uint32_t uncompressed_header_size = 0;
  uint32_t compressed_header_size = 0;

  bool is_intra_only() const {
    return reference_buffers[0] == -1 && reference_buffers[1] == -1 &&
           reference_buffers[2] == -1;
  }

  std::string ToString() const;
};

absl::optional<Vp9UncompressedHeader> ParseUncompressedVp9Header(
    rtc::ArrayView<const uint8_t> buf);

}

#endif
