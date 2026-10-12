/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_BUFFER_QUEUE_H_
#define RTC_BASE_BUFFER_QUEUE_H_

#include <stddef.h>

#include <deque>
#include <vector>

#include "api/sequence_checker.h"
#include "rtc_base/buffer.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {

class BufferQueue final {
 public:

  BufferQueue(size_t capacity, size_t default_size);
  ~BufferQueue();

  BufferQueue(const BufferQueue&) = delete;
  BufferQueue& operator=(const BufferQueue&) = delete;

  size_t size() const;

  void Clear();

  bool ReadFront(void* data, size_t bytes, size_t* bytes_read);

  bool WriteBack(const void* data, size_t bytes, size_t* bytes_written);

  bool is_writable() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return queue_.size() < capacity_;
  }

  bool is_readable() const {
    RTC_DCHECK_RUN_ON(&sequence_checker_);
    return !queue_.empty();
  }

 private:
  RTC_NO_UNIQUE_ADDRESS webrtc::SequenceChecker sequence_checker_;
  const size_t capacity_;
  const size_t default_size_;
  std::deque<Buffer*> queue_ RTC_GUARDED_BY(sequence_checker_);
  std::vector<Buffer*> free_list_ RTC_GUARDED_BY(sequence_checker_);
};

}

#endif
