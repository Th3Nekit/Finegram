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

#ifndef MODULES_VIDEO_CODING_CODECS_VP9_VP9_FRAME_BUFFER_POOL_H_
#define MODULES_VIDEO_CODING_CODECS_VP9_VP9_FRAME_BUFFER_POOL_H_

#ifdef RTC_ENABLE_VP9

#include <vector>

#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/buffer.h"
#include "rtc_base/synchronization/mutex.h"

struct vpx_codec_ctx;
struct vpx_codec_frame_buffer;

namespace webrtc {

constexpr size_t kDefaultMaxNumBuffers = 68;

class Vp9FrameBufferPool {
 public:
  class Vp9FrameBuffer final
      : public rtc::RefCountedNonVirtual<Vp9FrameBuffer> {
   public:
    uint8_t* GetData();
    size_t GetDataSize() const;
    void SetSize(size_t size);

    using rtc::RefCountedNonVirtual<Vp9FrameBuffer>::HasOneRef;

   private:

    rtc::Buffer data_;
  };

  bool InitializeVpxUsePool(vpx_codec_ctx* vpx_codec_context);

  rtc::scoped_refptr<Vp9FrameBuffer> GetFrameBuffer(size_t min_size);

  int GetNumBuffersInUse() const;

  bool Resize(size_t max_number_of_buffers);

  void ClearPool();

  static int32_t VpxGetFrameBuffer(void* user_priv,
                                   size_t min_size,
                                   vpx_codec_frame_buffer* fb);

  static int32_t VpxReleaseFrameBuffer(void* user_priv,
                                       vpx_codec_frame_buffer* fb);

 private:

  mutable Mutex buffers_lock_;

  std::vector<rtc::scoped_refptr<Vp9FrameBuffer>> allocated_buffers_
      RTC_GUARDED_BY(buffers_lock_);
  size_t max_num_buffers_ = kDefaultMaxNumBuffers;
};

}

#endif

#endif
