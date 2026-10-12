/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/alignment_adjuster.h"

#include <algorithm>
#include <limits>

#include "absl/algorithm/container.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

double RoundToMultiple(int alignment,
                       int requested_alignment,
                       VideoEncoderConfig* config,
                       bool update_config) {
  double diff = 0.0;
  for (auto& layer : config->simulcast_layers) {
    double min_dist = std::numeric_limits<double>::max();
    double new_scale = 1.0;
    for (int i = requested_alignment; i <= alignment;
         i += requested_alignment) {
      double dist = std::abs(layer.scale_resolution_down_by -
                             alignment / static_cast<double>(i));
      if (dist <= min_dist) {
        min_dist = dist;
        new_scale = alignment / static_cast<double>(i);
      }
    }
    diff += std::abs(layer.scale_resolution_down_by - new_scale);
    if (update_config) {
      RTC_LOG(LS_INFO) << "scale_resolution_down_by "
                       << layer.scale_resolution_down_by << " -> " << new_scale;
      layer.scale_resolution_down_by = new_scale;
    }
  }
  return diff;
}
}

int AlignmentAdjuster::GetAlignmentAndMaybeAdjustScaleFactors(
    const VideoEncoder::EncoderInfo& encoder_info,
    VideoEncoderConfig* config,
    absl::optional<size_t> max_layers) {
  const int requested_alignment = encoder_info.requested_resolution_alignment;
  if (!encoder_info.apply_alignment_to_all_simulcast_layers) {
    return requested_alignment;
  }

  if (requested_alignment < 1 || config->number_of_streams <= 1 ||
      config->simulcast_layers.size() <= 1) {
    return requested_alignment;
  }

  const bool has_scale_resolution_down_by = absl::c_any_of(
      config->simulcast_layers, [](const webrtc::VideoStream& layer) {
        return layer.scale_resolution_down_by >= 1.0;
      });

  if (!has_scale_resolution_down_by) {

    size_t size = config->simulcast_layers.size();
    if (max_layers && *max_layers > 0 && *max_layers < size) {
      size = *max_layers;
    }
    return requested_alignment * (1 << (size - 1));
  }

  const int kMaxAlignment = 16;

  for (auto& layer : config->simulcast_layers) {
    layer.scale_resolution_down_by =
        std::max(layer.scale_resolution_down_by, 1.0);
    layer.scale_resolution_down_by =
        std::min(layer.scale_resolution_down_by, 10000.0);
  }

  double min_diff = std::numeric_limits<double>::max();
  int best_alignment = 1;
  for (int alignment = requested_alignment; alignment <= kMaxAlignment;
       ++alignment) {
    double diff = RoundToMultiple(alignment, requested_alignment, config,
                                                    false);
    if (diff < min_diff) {
      min_diff = diff;
      best_alignment = alignment;
    }
  }
  RoundToMultiple(best_alignment, requested_alignment, config,
                                    true);

  return std::max(best_alignment, requested_alignment);
}
}
