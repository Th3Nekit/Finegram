/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_SOURCE_INTERFACE_H_
#define API_VIDEO_VIDEO_SOURCE_INTERFACE_H_

#include <limits>
#include <vector>

#include "absl/types/optional.h"
#include "api/video/video_sink_interface.h"
#include "rtc_base/system/rtc_export.h"

namespace rtc {

struct RTC_EXPORT VideoSinkWants {
  struct FrameSize {
    FrameSize(int width, int height) : width(width), height(height) {}
    FrameSize(const FrameSize&) = default;
    ~FrameSize() = default;

    int width;
    int height;
  };

  VideoSinkWants();
  VideoSinkWants(const VideoSinkWants&);
  ~VideoSinkWants();

  bool rotation_applied = false;

  bool black_frames = false;

  int max_pixel_count = std::numeric_limits<int>::max();

  absl::optional<int> target_pixel_count;

  int max_framerate_fps = std::numeric_limits<int>::max();

  int resolution_alignment = 1;

  std::vector<FrameSize> resolutions;

  absl::optional<FrameSize> requested_resolution;

  bool is_active = false;

  struct Aggregates {

    bool any_active_without_requested_resolution = false;
  };
  absl::optional<Aggregates> aggregates;
};

inline bool operator==(const VideoSinkWants::FrameSize& a,
                       const VideoSinkWants::FrameSize& b) {
  return a.width == b.width && a.height == b.height;
}

inline bool operator!=(const VideoSinkWants::FrameSize& a,
                       const VideoSinkWants::FrameSize& b) {
  return !(a == b);
}

template <typename VideoFrameT>
class VideoSourceInterface {
 public:
  virtual ~VideoSourceInterface() = default;

  virtual void AddOrUpdateSink(VideoSinkInterface<VideoFrameT>* sink,
                               const VideoSinkWants& wants) = 0;

  virtual void RemoveSink(VideoSinkInterface<VideoFrameT>* sink) = 0;

  virtual void RequestRefreshFrame() {}
};

}
#endif
