/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_ENCODED_FRAME_H_
#define MODULES_VIDEO_CODING_ENCODED_FRAME_H_

#include <vector>

#include "api/video/encoded_image.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

class RTC_EXPORT VCMEncodedFrame : public EncodedImage {
 public:
  VCMEncodedFrame();
  VCMEncodedFrame(const VCMEncodedFrame&);

  ~VCMEncodedFrame();

  void SetRenderTime(const int64_t renderTimeMs) {
    _renderTimeMs = renderTimeMs;
  }

  const webrtc::EncodedImage& EncodedImage() const {
    return static_cast<const webrtc::EncodedImage&>(*this);
  }

  using EncodedImage::ColorSpace;
  using EncodedImage::data;
  using EncodedImage::GetEncodedData;
  using EncodedImage::NtpTimeMs;
  using EncodedImage::PacketInfos;
  using EncodedImage::RtpTimestamp;
  using EncodedImage::set_size;
  using EncodedImage::SetColorSpace;
  using EncodedImage::SetEncodedData;
  using EncodedImage::SetPacketInfos;
  using EncodedImage::SetRtpTimestamp;
  using EncodedImage::SetSpatialIndex;
  using EncodedImage::SetSpatialLayerFrameSize;
  using EncodedImage::size;
  using EncodedImage::SpatialIndex;
  using EncodedImage::SpatialLayerFrameSize;

  int64_t RenderTimeMs() const { return _renderTimeMs; }

  bool MissingFrame() const { return _missingFrame; }

  uint8_t PayloadType() const { return _payloadType; }

  const CodecSpecificInfo* CodecSpecific() const { return &_codecSpecificInfo; }
  void SetCodecSpecific(const CodecSpecificInfo* codec_specific) {
    _codecSpecificInfo = *codec_specific;
  }

 protected:
  void Reset();

  void CopyCodecSpecific(const RTPVideoHeader* header);

  int64_t _renderTimeMs;
  uint8_t _payloadType;
  bool _missingFrame;
  CodecSpecificInfo _codecSpecificInfo;
  webrtc::VideoCodecType _codec;
};

}

#endif
