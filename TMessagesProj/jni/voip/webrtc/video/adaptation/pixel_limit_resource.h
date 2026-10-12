/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_PIXEL_LIMIT_RESOURCE_H_
#define VIDEO_ADAPTATION_PIXEL_LIMIT_RESOURCE_H_

#include <string>

#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/scoped_refptr.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class PixelLimitResource : public Resource {
 public:
  static rtc::scoped_refptr<PixelLimitResource> Create(
      TaskQueueBase* task_queue,
      VideoStreamInputStateProvider* input_state_provider);

  PixelLimitResource(TaskQueueBase* task_queue,
                     VideoStreamInputStateProvider* input_state_provider);
  ~PixelLimitResource() override;

  void SetMaxPixels(int max_pixels);

  std::string Name() const override { return "PixelLimitResource"; }
  void SetResourceListener(ResourceListener* listener) override;

 private:
  TaskQueueBase* const task_queue_;
  VideoStreamInputStateProvider* const input_state_provider_;
  absl::optional<int> max_pixels_ RTC_GUARDED_BY(task_queue_);
  webrtc::ResourceListener* listener_ RTC_GUARDED_BY(task_queue_);
  RepeatingTaskHandle repeating_task_ RTC_GUARDED_BY(task_queue_);
};

}

#endif
