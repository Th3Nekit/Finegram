/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_DEFINES_H_
#define MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_DEFINES_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_timing.h"
#include "api/video_codecs/video_decoder.h"

namespace webrtc {

#define VCM_FRAME_NOT_READY 3
#define VCM_MISSING_CALLBACK 1
#define VCM_OK 0
#define VCM_GENERAL_ERROR -1
#define VCM_PARAMETER_ERROR -4
#define VCM_NO_CODEC_REGISTERED -8
#define VCM_JITTER_BUFFER_ERROR -9

enum {

  kDefaultTimingFramesDelayMs = 200,
  kDefaultOutlierFrameSizePercent = 500,

  kMaxEncodeStartTimeListSize = 150,
};

enum VCMVideoProtection {
  kProtectionNack,
  kProtectionNackFEC,
};

class VCMReceiveCallback {
 public:
  virtual int32_t FrameToRender(VideoFrame& videoFrame,
                                absl::optional<uint8_t> qp,
                                TimeDelta decode_time,
                                VideoContentType content_type,
                                VideoFrameType frame_type) = 0;

  virtual void OnDroppedFrames(uint32_t frames_dropped);

  virtual void OnIncomingPayloadType(int payload_type);
  virtual void OnDecoderInfoChanged(
      const VideoDecoder::DecoderInfo& decoder_info);

 protected:
  virtual ~VCMReceiveCallback() {}
};

class VCMFrameTypeCallback {
 public:
  virtual int32_t RequestKeyFrame() = 0;

 protected:
  virtual ~VCMFrameTypeCallback() {}
};

class VCMPacketRequestCallback {
 public:
  virtual int32_t ResendPackets(const uint16_t* sequenceNumbers,
                                uint16_t length) = 0;

 protected:
  virtual ~VCMPacketRequestCallback() {}
};

}

#endif
