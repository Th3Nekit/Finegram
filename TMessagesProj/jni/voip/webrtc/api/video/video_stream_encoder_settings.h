/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_
#define API_VIDEO_VIDEO_STREAM_ENCODER_SETTINGS_H_

#include <string>

#include "api/video/video_bitrate_allocator_factory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {

class EncoderSwitchRequestCallback {
 public:
  virtual ~EncoderSwitchRequestCallback() {}

  virtual void RequestEncoderFallback() = 0;

  virtual void RequestEncoderSwitch(const SdpVideoFormat& format,
                                    bool allow_default_fallback) = 0;
};

struct VideoStreamEncoderSettings {
  explicit VideoStreamEncoderSettings(
      const VideoEncoder::Capabilities& capabilities)
      : capabilities(capabilities) {}

  bool experiment_cpu_load_estimator = false;

  VideoEncoderFactory* encoder_factory = nullptr;

  EncoderSwitchRequestCallback* encoder_switch_request_callback = nullptr;

  VideoBitrateAllocatorFactory* bitrate_allocator_factory = nullptr;

  VideoEncoder::Capabilities capabilities;
};

}

#endif
