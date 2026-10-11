/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/include/video_frame_buffer_pool.h"

#include <limits>

#include "api/make_ref_counted.h"
#include "rtc_base/checks.h"

namespace webrtc {

namespace {
bool HasOneRef(const rtc::scoped_refptr<VideoFrameBuffer>& buffer) {

  switch (buffer->type()) {
    case VideoFrameBuffer::Type::kI420: {
      return static_cast<rtc::RefCountedObject<I420Buffer>*>(buffer.get())
          ->HasOneRef();
    }
    case VideoFrameBuffer::Type::kI444: {
      return static_cast<rtc::RefCountedObject<I444Buffer>*>(buffer.get())
          ->HasOneRef();
    }
    case VideoFrameBuffer::Type::kI422: {
      return static_cast<rtc::RefCountedObject<I422Buffer>*>(buffer.get())
          ->HasOneRef();
    }
    case VideoFrameBuffer::Type::kI010: {
      return static_cast<rtc::RefCountedObject<I010Buffer>*>(buffer.get())
          ->HasOneRef();
    }
    case VideoFrameBuffer::Type::kI210: {
      return static_cast<rtc::RefCountedObject<I210Buffer>*>(buffer.get())
          ->HasOneRef();
    }
    case VideoFrameBuffer::Type::kI410: {
      return static_cast<rtc::RefCountedObject<I410Buffer>*>(buffer.get())
          ->HasOneRef();
    }
    case VideoFrameBuffer::Type::kNV12: {
      return static_cast<rtc::RefCountedObject<NV12Buffer>*>(buffer.get())
          ->HasOneRef();
    }
    default:
      RTC_DCHECK_NOTREACHED();
  }
  return false;
}

}

VideoFrameBufferPool::VideoFrameBufferPool() : VideoFrameBufferPool(false) {}

VideoFrameBufferPool::VideoFrameBufferPool(bool zero_initialize)
    : VideoFrameBufferPool(zero_initialize,
                           std::numeric_limits<size_t>::max()) {}

VideoFrameBufferPool::VideoFrameBufferPool(bool zero_initialize,
                                           size_t max_number_of_buffers)
    : zero_initialize_(zero_initialize),
      max_number_of_buffers_(max_number_of_buffers) {}

VideoFrameBufferPool::~VideoFrameBufferPool() = default;

void VideoFrameBufferPool::Release() {
  buffers_.clear();
}

bool VideoFrameBufferPool::Resize(size_t max_number_of_buffers) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  size_t used_buffers_count = 0;
  for (const rtc::scoped_refptr<VideoFrameBuffer>& buffer : buffers_) {

    if (!HasOneRef(buffer)) {
      used_buffers_count++;
    }
  }
  if (used_buffers_count > max_number_of_buffers) {
    return false;
  }
  max_number_of_buffers_ = max_number_of_buffers;

  size_t buffers_to_purge = buffers_.size() - max_number_of_buffers_;
  auto iter = buffers_.begin();
  while (iter != buffers_.end() && buffers_to_purge > 0) {
    if (HasOneRef(*iter)) {
      iter = buffers_.erase(iter);
      buffers_to_purge--;
    } else {
      ++iter;
    }
  }
  return true;
}

rtc::scoped_refptr<I420Buffer> VideoFrameBufferPool::CreateI420Buffer(
    int width,
    int height) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  rtc::scoped_refptr<VideoFrameBuffer> existing_buffer =
      GetExistingBuffer(width, height, VideoFrameBuffer::Type::kI420);
  if (existing_buffer) {

    rtc::RefCountedObject<I420Buffer>* raw_buffer =
        static_cast<rtc::RefCountedObject<I420Buffer>*>(existing_buffer.get());

    return rtc::scoped_refptr<I420Buffer>(raw_buffer);
  }

  if (buffers_.size() >= max_number_of_buffers_)
    return nullptr;

  rtc::scoped_refptr<I420Buffer> buffer =
      rtc::make_ref_counted<I420Buffer>(width, height);

  if (zero_initialize_)
    buffer->InitializeData();

  buffers_.push_back(buffer);
  return buffer;
}

rtc::scoped_refptr<I444Buffer> VideoFrameBufferPool::CreateI444Buffer(
    int width,
    int height) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  rtc::scoped_refptr<VideoFrameBuffer> existing_buffer =
      GetExistingBuffer(width, height, VideoFrameBuffer::Type::kI444);
  if (existing_buffer) {

    rtc::RefCountedObject<I444Buffer>* raw_buffer =
        static_cast<rtc::RefCountedObject<I444Buffer>*>(existing_buffer.get());

    return rtc::scoped_refptr<I444Buffer>(raw_buffer);
  }

  if (buffers_.size() >= max_number_of_buffers_)
    return nullptr;

  rtc::scoped_refptr<I444Buffer> buffer =
      rtc::make_ref_counted<I444Buffer>(width, height);

  if (zero_initialize_)
    buffer->InitializeData();

  buffers_.push_back(buffer);
  return buffer;
}

rtc::scoped_refptr<I422Buffer> VideoFrameBufferPool::CreateI422Buffer(
    int width,
    int height) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  rtc::scoped_refptr<VideoFrameBuffer> existing_buffer =
      GetExistingBuffer(width, height, VideoFrameBuffer::Type::kI422);
  if (existing_buffer) {

    rtc::RefCountedObject<I422Buffer>* raw_buffer =
        static_cast<rtc::RefCountedObject<I422Buffer>*>(existing_buffer.get());

    return rtc::scoped_refptr<I422Buffer>(raw_buffer);
  }

  if (buffers_.size() >= max_number_of_buffers_)
    return nullptr;

  rtc::scoped_refptr<I422Buffer> buffer =
      rtc::make_ref_counted<I422Buffer>(width, height);

  if (zero_initialize_)
    buffer->InitializeData();

  buffers_.push_back(buffer);
  return buffer;
}

rtc::scoped_refptr<NV12Buffer> VideoFrameBufferPool::CreateNV12Buffer(
    int width,
    int height) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  rtc::scoped_refptr<VideoFrameBuffer> existing_buffer =
      GetExistingBuffer(width, height, VideoFrameBuffer::Type::kNV12);
  if (existing_buffer) {

    rtc::RefCountedObject<NV12Buffer>* raw_buffer =
        static_cast<rtc::RefCountedObject<NV12Buffer>*>(existing_buffer.get());

    return rtc::scoped_refptr<NV12Buffer>(raw_buffer);
  }

  if (buffers_.size() >= max_number_of_buffers_)
    return nullptr;

  rtc::scoped_refptr<NV12Buffer> buffer =
      rtc::make_ref_counted<NV12Buffer>(width, height);

  if (zero_initialize_)
    buffer->InitializeData();

  buffers_.push_back(buffer);
  return buffer;
}

rtc::scoped_refptr<I010Buffer> VideoFrameBufferPool::CreateI010Buffer(
    int width,
    int height) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  rtc::scoped_refptr<VideoFrameBuffer> existing_buffer =
      GetExistingBuffer(width, height, VideoFrameBuffer::Type::kI010);
  if (existing_buffer) {

    rtc::RefCountedObject<I010Buffer>* raw_buffer =
        static_cast<rtc::RefCountedObject<I010Buffer>*>(existing_buffer.get());

    return rtc::scoped_refptr<I010Buffer>(raw_buffer);
  }

  if (buffers_.size() >= max_number_of_buffers_)
    return nullptr;

  rtc::scoped_refptr<I010Buffer> buffer = I010Buffer::Create(width, height);

  buffers_.push_back(buffer);
  return buffer;
}

rtc::scoped_refptr<I210Buffer> VideoFrameBufferPool::CreateI210Buffer(
    int width,
    int height) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  rtc::scoped_refptr<VideoFrameBuffer> existing_buffer =
      GetExistingBuffer(width, height, VideoFrameBuffer::Type::kI210);
  if (existing_buffer) {

    rtc::RefCountedObject<I210Buffer>* raw_buffer =
        static_cast<rtc::RefCountedObject<I210Buffer>*>(existing_buffer.get());

    return rtc::scoped_refptr<I210Buffer>(raw_buffer);
  }

  if (buffers_.size() >= max_number_of_buffers_)
    return nullptr;

  rtc::scoped_refptr<I210Buffer> buffer = I210Buffer::Create(width, height);

  buffers_.push_back(buffer);
  return buffer;
}

rtc::scoped_refptr<I410Buffer> VideoFrameBufferPool::CreateI410Buffer(
    int width,
    int height) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  rtc::scoped_refptr<VideoFrameBuffer> existing_buffer =
      GetExistingBuffer(width, height, VideoFrameBuffer::Type::kI410);
  if (existing_buffer) {

    rtc::RefCountedObject<I410Buffer>* raw_buffer =
        static_cast<rtc::RefCountedObject<I410Buffer>*>(existing_buffer.get());

    return rtc::scoped_refptr<I410Buffer>(raw_buffer);
  }

  if (buffers_.size() >= max_number_of_buffers_)
    return nullptr;

  rtc::scoped_refptr<I410Buffer> buffer = I410Buffer::Create(width, height);

  buffers_.push_back(buffer);
  return buffer;
}

rtc::scoped_refptr<VideoFrameBuffer> VideoFrameBufferPool::GetExistingBuffer(
    int width,
    int height,
    VideoFrameBuffer::Type type) {

  for (auto it = buffers_.begin(); it != buffers_.end();) {
    const auto& buffer = *it;
    if (buffer->width() != width || buffer->height() != height ||
        buffer->type() != type) {
      it = buffers_.erase(it);
    } else {
      ++it;
    }
  }

  for (const rtc::scoped_refptr<VideoFrameBuffer>& buffer : buffers_) {

    if (HasOneRef(buffer)) {
      RTC_CHECK(buffer->type() == type);
      return buffer;
    }
  }
  return nullptr;
}

}
