/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_MEMORY_FIFO_BUFFER_H_
#define RTC_BASE_MEMORY_FIFO_BUFFER_H_

#include <memory>

#include "api/task_queue/pending_task_safety_flag.h"
#include "rtc_base/stream.h"
#include "rtc_base/synchronization/mutex.h"

namespace rtc {

class FifoBuffer final : public StreamInterface {
 public:

  explicit FifoBuffer(size_t length);

  FifoBuffer(size_t length, Thread* owner);
  ~FifoBuffer() override;

  FifoBuffer(const FifoBuffer&) = delete;
  FifoBuffer& operator=(const FifoBuffer&) = delete;

  bool GetBuffered(size_t* data_len) const;

  StreamState GetState() const override;
  StreamResult Read(rtc::ArrayView<uint8_t> buffer,
                    size_t& bytes_read,
                    int& error) override;
  StreamResult Write(rtc::ArrayView<const uint8_t> buffer,
                     size_t& bytes_written,
                     int& error) override;
  void Close() override;

  bool SetPosition(size_t position);

  bool GetPosition(size_t* position) const;

  bool Rewind() { return SetPosition(0); }

  const void* GetReadData(size_t* data_len);
  void ConsumeReadData(size_t used);

  void* GetWriteBuffer(size_t* buf_len);
  void ConsumeWriteBuffer(size_t used);

 private:
  void PostEvent(int events, int err) {
    owner_->PostTask(webrtc::SafeTask(
        task_safety_.flag(),
        [this, events, err]() { SignalEvent(this, events, err); }));
  }

  StreamResult ReadLocked(void* buffer, size_t bytes, size_t* bytes_read)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  StreamResult WriteLocked(const void* buffer,
                           size_t bytes,
                           size_t* bytes_written)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  webrtc::ScopedTaskSafety task_safety_;

  StreamState state_ RTC_GUARDED_BY(mutex_);

  std::unique_ptr<char[]> buffer_ RTC_GUARDED_BY(mutex_);

  const size_t buffer_length_;

  size_t data_length_ RTC_GUARDED_BY(mutex_);

  size_t read_position_ RTC_GUARDED_BY(mutex_);

  Thread* const owner_;

  mutable webrtc::Mutex mutex_;
};

}

#endif
