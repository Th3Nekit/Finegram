/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_RECORDABLE_ENCODED_FRAME_H_
#define API_VIDEO_RECORDABLE_ENCODED_FRAME_H_

#include "api/array_view.h"
#include "api/scoped_refptr.h"
#include "api/units/timestamp.h"
#include "api/video/color_space.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"

namespace webrtc {

class RecordableEncodedFrame {
 public:

  struct EncodedResolution {
    bool empty() const { return width == 0 && height == 0; }

    unsigned width = 0;
    unsigned height = 0;
  };

  virtual ~RecordableEncodedFrame() = default;

  virtual rtc::scoped_refptr<const EncodedImageBufferInterface> encoded_buffer()
      const = 0;

  virtual absl::optional<webrtc::ColorSpace> color_space() const = 0;

  virtual VideoCodecType codec() const = 0;

  virtual bool is_key_frame() const = 0;

  virtual EncodedResolution resolution() const = 0;

  virtual Timestamp render_time() const = 0;
};

}

#endif
