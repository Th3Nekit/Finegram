/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_BITRATE_ALLOCATION_H_
#define API_VIDEO_VIDEO_BITRATE_ALLOCATION_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/video_codec_constants.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT VideoBitrateAllocation {
 public:
  static constexpr uint32_t kMaxBitrateBps =
      std::numeric_limits<uint32_t>::max();
  VideoBitrateAllocation();

  bool SetBitrate(size_t spatial_index,
                  size_t temporal_index,
                  uint32_t bitrate_bps);

  bool HasBitrate(size_t spatial_index, size_t temporal_index) const;

  uint32_t GetBitrate(size_t spatial_index, size_t temporal_index) const;

  bool IsSpatialLayerUsed(size_t spatial_index) const;

  uint32_t GetSpatialLayerSum(size_t spatial_index) const;

  uint32_t GetTemporalLayerSum(size_t spatial_index,
                               size_t temporal_index) const;

  std::vector<uint32_t> GetTemporalLayerAllocation(size_t spatial_index) const;

  std::vector<absl::optional<VideoBitrateAllocation>> GetSimulcastAllocations()
      const;

  uint32_t get_sum_bps() const { return sum_; }
  uint32_t get_sum_kbps() const {

    return sum_ / 1000;
  }

  bool operator==(const VideoBitrateAllocation& other) const;
  inline bool operator!=(const VideoBitrateAllocation& other) const {
    return !(*this == other);
  }

  std::string ToString() const;

  void set_bw_limited(bool limited) { is_bw_limited_ = limited; }
  bool is_bw_limited() const { return is_bw_limited_; }

 private:
  uint32_t sum_;
  absl::optional<uint32_t> bitrates_[kMaxSpatialLayers][kMaxTemporalStreams];
  bool is_bw_limited_;
};

}

#endif
