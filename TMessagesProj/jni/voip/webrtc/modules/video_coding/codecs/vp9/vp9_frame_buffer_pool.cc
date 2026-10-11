/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifdef RTC_ENABLE_VP9

#include "modules/video_coding/codecs/vp9/vp9_frame_buffer_pool.h"

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include <vpx/vpx_codec.h>
#include <vpx/vpx_decoder.h>
#include <vpx/vpx_frame_buffer.h>

namespace webrtc {

uint8_t* Vp9FrameBufferPool::Vp9FrameBuffer::GetData() {
  return data_.data<uint8_t>();
}

size_t Vp9FrameBufferPool::Vp9FrameBuffer::GetDataSize() const {
  return data_.size();
}

void Vp9FrameBufferPool::Vp9FrameBuffer::SetSize(size_t size) {
  data_.SetSize(size);
}

bool Vp9FrameBufferPool::InitializeVpxUsePool(
    vpx_codec_ctx* vpx_codec_context) {
  RTC_DCHECK(vpx_codec_context);

  if (vpx_codec_set_frame_buffer_functions(

          vpx_codec_context,

          &Vp9FrameBufferPool::VpxGetFrameBuffer,

          &Vp9FrameBufferPool::VpxReleaseFrameBuffer,

          this)) {

    return false;
  }
  return true;
}

rtc::scoped_refptr<Vp9FrameBufferPool::Vp9FrameBuffer>
Vp9FrameBufferPool::GetFrameBuffer(size_t min_size) {
  RTC_DCHECK_GT(min_size, 0);
  rtc::scoped_refptr<Vp9FrameBuffer> available_buffer = nullptr;
  {
    MutexLock lock(&buffers_lock_);

    for (const auto& buffer : allocated_buffers_) {
      if (buffer->HasOneRef()) {
        available_buffer = buffer;
        break;
      }
    }

    if (available_buffer == nullptr) {
      available_buffer = new Vp9FrameBuffer();
      allocated_buffers_.push_back(available_buffer);
      if (allocated_buffers_.size() > max_num_buffers_) {
        RTC_LOG(LS_WARNING)
            << allocated_buffers_.size()
            << " Vp9FrameBuffers have been "
               "allocated by a Vp9FrameBufferPool (exceeding what is "
               "considered reasonable, "
            << max_num_buffers_ << ").";

      }
    }
  }

  available_buffer->SetSize(min_size);
  return available_buffer;
}

int Vp9FrameBufferPool::GetNumBuffersInUse() const {
  int num_buffers_in_use = 0;
  MutexLock lock(&buffers_lock_);
  for (const auto& buffer : allocated_buffers_) {
    if (!buffer->HasOneRef())
      ++num_buffers_in_use;
  }
  return num_buffers_in_use;
}

bool Vp9FrameBufferPool::Resize(size_t max_number_of_buffers) {
  MutexLock lock(&buffers_lock_);
  size_t used_buffers_count = 0;
  for (const auto& buffer : allocated_buffers_) {

    if (!buffer->HasOneRef()) {
      used_buffers_count++;
    }
  }
  if (used_buffers_count > max_number_of_buffers) {
    return false;
  }
  max_num_buffers_ = max_number_of_buffers;

  size_t buffers_to_purge = allocated_buffers_.size() - max_num_buffers_;
  auto iter = allocated_buffers_.begin();
  while (iter != allocated_buffers_.end() && buffers_to_purge > 0) {
    if ((*iter)->HasOneRef()) {
      iter = allocated_buffers_.erase(iter);
      buffers_to_purge--;
    } else {
      ++iter;
    }
  }
  return true;
}

void Vp9FrameBufferPool::ClearPool() {
  MutexLock lock(&buffers_lock_);
  allocated_buffers_.clear();
}

int32_t Vp9FrameBufferPool::VpxGetFrameBuffer(void* user_priv,
                                              size_t min_size,
                                              vpx_codec_frame_buffer* fb) {
  RTC_DCHECK(user_priv);
  RTC_DCHECK(fb);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION

  size_t size_limit = 7680 * 4320 * 3 / 2 * 2;
  if (min_size > size_limit)
    return -1;
#endif

  Vp9FrameBufferPool* pool = static_cast<Vp9FrameBufferPool*>(user_priv);

  rtc::scoped_refptr<Vp9FrameBuffer> buffer = pool->GetFrameBuffer(min_size);
  fb->data = buffer->GetData();
  fb->size = buffer->GetDataSize();

  fb->priv = static_cast<void*>(buffer.release());
  return 0;
}

int32_t Vp9FrameBufferPool::VpxReleaseFrameBuffer(void* user_priv,
                                                  vpx_codec_frame_buffer* fb) {
  RTC_DCHECK(user_priv);
  RTC_DCHECK(fb);
  Vp9FrameBuffer* buffer = static_cast<Vp9FrameBuffer*>(fb->priv);
  if (buffer != nullptr) {
    buffer->Release();

    fb->priv = nullptr;
  }
  return 0;
}

}

#endif
