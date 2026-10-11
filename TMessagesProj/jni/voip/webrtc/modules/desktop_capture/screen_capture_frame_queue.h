/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURE_FRAME_QUEUE_H_
#define MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURE_FRAME_QUEUE_H_

#include <memory>

namespace webrtc {

template <typename FrameType>
class ScreenCaptureFrameQueue {
 public:
  ScreenCaptureFrameQueue() = default;
  ~ScreenCaptureFrameQueue() = default;

  ScreenCaptureFrameQueue(const ScreenCaptureFrameQueue&) = delete;
  ScreenCaptureFrameQueue& operator=(const ScreenCaptureFrameQueue&) = delete;

  void MoveToNextFrame() { current_ = (current_ + 1) % kQueueLength; }

  void ReplaceCurrentFrame(std::unique_ptr<FrameType> frame) {
    frames_[current_] = std::move(frame);
  }

  void Reset() {
    for (int i = 0; i < kQueueLength; i++) {
      frames_[i].reset();
    }
    current_ = 0;
  }

  FrameType* current_frame() const { return frames_[current_].get(); }

  FrameType* previous_frame() const {
    return frames_[(current_ + kQueueLength - 1) % kQueueLength].get();
  }

 private:

  int current_ = 0;

  static const int kQueueLength = 2;
  std::unique_ptr<FrameType> frames_[kQueueLength];
};

}

#endif
