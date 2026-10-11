/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_STREAM_ENCODER_INTERFACE_H_
#define VIDEO_VIDEO_STREAM_ENCODER_INTERFACE_H_

#include <vector>

#include "api/adaptation/resource.h"
#include "api/fec_controller_override.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/units/data_rate.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_layers_allocation.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video_codecs/video_encoder.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {

class VideoStreamEncoderInterface {
 public:

  class EncoderSink : public EncodedImageCallback {
   public:
    virtual void OnEncoderConfigurationChanged(
        std::vector<VideoStream> streams,
        bool is_svc,
        VideoEncoderConfig::ContentType content_type,
        int min_transmit_bitrate_bps) = 0;

    virtual void OnBitrateAllocationUpdated(
        const VideoBitrateAllocation& allocation) = 0;

    virtual void OnVideoLayersAllocationUpdated(
        VideoLayersAllocation allocation) = 0;
  };

  virtual ~VideoStreamEncoderInterface() = default;

  virtual void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) = 0;
  virtual std::vector<rtc::scoped_refptr<Resource>>
  GetAdaptationResources() = 0;

  virtual void SetSource(
      rtc::VideoSourceInterface<VideoFrame>* source,
      const DegradationPreference& degradation_preference) = 0;

  virtual void SetSink(EncoderSink* sink, bool rotation_applied) = 0;

  virtual void SetStartBitrate(int start_bitrate_bps) = 0;

  virtual void SendKeyFrame(const std::vector<VideoFrameType>& layers = {}) = 0;

  virtual void OnLossNotification(
      const VideoEncoder::LossNotification& loss_notification) = 0;

  virtual void OnBitrateUpdated(DataRate target_bitrate,
                                DataRate stable_target_bitrate,
                                DataRate link_allocation,
                                uint8_t fraction_lost,
                                int64_t round_trip_time_ms,
                                double cwnd_reduce_ratio) = 0;

  virtual void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) = 0;

  virtual void ConfigureEncoder(VideoEncoderConfig config,
                                size_t max_data_payload_length) = 0;
  virtual void ConfigureEncoder(VideoEncoderConfig config,
                                size_t max_data_payload_length,
                                SetParametersCallback callback) = 0;

  virtual void Stop() = 0;
};

}

#endif
