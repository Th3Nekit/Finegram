/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_ENCODED_FRAME_H_
#define API_VIDEO_ENCODED_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/types/optional.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_image.h"
#include "api/video/video_codec_type.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding_defines.h"

namespace webrtc {

class EncodedFrame : public EncodedImage {
 public:
  static const uint8_t kMaxFrameReferences = 5;

  EncodedFrame() = default;
  EncodedFrame(const EncodedFrame&) = default;
  virtual ~EncodedFrame() {}

  virtual int64_t ReceivedTime() const { return -1; }

  absl::optional<webrtc::Timestamp> ReceivedTimestamp() const;

  virtual int64_t RenderTime() const { return _renderTimeMs; }

  int64_t RenderTimeMs() const { return _renderTimeMs; }

  absl::optional<webrtc::Timestamp> RenderTimestamp() const;

  virtual bool delayed_by_retransmission() const;

  bool is_keyframe() const { return num_references == 0; }

  void SetId(int64_t id) { id_ = id; }
  int64_t Id() const { return id_; }

  uint8_t PayloadType() const { return _payloadType; }

  void SetRenderTime(const int64_t renderTimeMs) {
    _renderTimeMs = renderTimeMs;
  }

  const webrtc::EncodedImage& EncodedImage() const {
    return static_cast<const webrtc::EncodedImage&>(*this);
  }

  const CodecSpecificInfo* CodecSpecific() const { return &_codecSpecificInfo; }
  void SetCodecSpecific(const CodecSpecificInfo* codec_specific) {
    _codecSpecificInfo = *codec_specific;
  }

  size_t num_references = 0;
  int64_t references[kMaxFrameReferences];

  bool is_last_spatial_layer = true;

 protected:

  void CopyCodecSpecific(const RTPVideoHeader* header);

  int64_t _renderTimeMs = -1;
  uint8_t _payloadType = 0;
  CodecSpecificInfo _codecSpecificInfo;
  VideoCodecType _codec = kVideoCodecGeneric;

 private:

  int64_t id_ = -1;
};

}

#endif
