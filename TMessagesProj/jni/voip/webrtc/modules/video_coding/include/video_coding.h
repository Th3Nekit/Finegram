/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_
#define MODULES_VIDEO_CODING_INCLUDE_VIDEO_CODING_H_

#include "api/field_trials_view.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/video_coding/include/video_coding_defines.h"

namespace webrtc {

class Clock;
class EncodedImageCallback;
class VideoDecoder;
class VideoEncoder;
struct CodecSpecificInfo;

class VideoCodingModule {
 public:

  static VideoCodingModule* Create(
      Clock* clock,
      const FieldTrialsView* field_trials = nullptr);

  virtual ~VideoCodingModule() = default;

  virtual void RegisterReceiveCodec(uint8_t payload_type,
                                    const VideoDecoder::Settings& settings) = 0;

  virtual void RegisterExternalDecoder(VideoDecoder* externalDecoder,
                                       uint8_t payloadType) = 0;

  virtual int32_t RegisterReceiveCallback(
      VCMReceiveCallback* receiveCallback) = 0;

  virtual int32_t RegisterFrameTypeCallback(
      VCMFrameTypeCallback* frameTypeCallback) = 0;

  virtual int32_t RegisterPacketRequestCallback(
      VCMPacketRequestCallback* callback) = 0;

  virtual int32_t Decode(uint16_t maxWaitTimeMs = 200) = 0;

  virtual int32_t IncomingPacket(const uint8_t* incomingPayload,
                                 size_t payloadLength,
                                 const RTPHeader& rtp_header,
                                 const RTPVideoHeader& video_header) = 0;

  virtual void SetNackSettings(size_t max_nack_list_size,
                               int max_packet_age_to_nack,
                               int max_incomplete_time_ms) = 0;

  virtual void Process() = 0;
};

}

#endif
