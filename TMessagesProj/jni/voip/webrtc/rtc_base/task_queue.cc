/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/task_queue.h"

#include "api/task_queue/task_queue_base.h"

namespace rtc {

TaskQueue::TaskQueue(
    std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> task_queue)
    : impl_(task_queue.release()) {}

TaskQueue::~TaskQueue() {

  impl_->Delete();
}

bool TaskQueue::IsCurrent() const {
  return impl_->IsCurrent();
}

}
