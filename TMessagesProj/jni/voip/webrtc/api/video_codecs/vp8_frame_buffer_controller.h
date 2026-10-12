/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VP8_FRAME_BUFFER_CONTROLLER_H_
#define API_VIDEO_CODECS_VP8_FRAME_BUFFER_CONTROLLER_H_

#include <array>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/fec_controller_override.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp8_frame_config.h"

namespace webrtc {

struct CodecSpecificInfo;

struct Vp8EncoderConfig {
  struct TemporalLayerConfig {
    bool operator!=(const TemporalLayerConfig& other) const {
      return ts_number_layers != other.ts_number_layers ||
             ts_target_bitrate != other.ts_target_bitrate ||
             ts_rate_decimator != other.ts_rate_decimator ||
             ts_periodicity != other.ts_periodicity ||
             ts_layer_id != other.ts_layer_id;
    }

    static constexpr size_t kMaxPeriodicity = 16;
    static constexpr size_t kMaxLayers = 5;

    uint32_t ts_number_layers;

    std::array<uint32_t, kMaxLayers> ts_target_bitrate;
    std::array<uint32_t, kMaxLayers> ts_rate_decimator;

    uint32_t ts_periodicity;

    std::array<uint32_t, kMaxPeriodicity> ts_layer_id;
  };

  absl::optional<TemporalLayerConfig> temporal_layer_config;

  absl::optional<uint32_t> rc_target_bitrate;

  absl::optional<uint32_t> rc_max_quantizer;

  absl::optional<uint32_t> g_error_resilient;

  bool reset_previous_configuration_overrides = false;
};

class Vp8FrameBufferController {
 public:
  virtual ~Vp8FrameBufferController() = default;

  virtual void SetQpLimits(size_t stream_index, int min_qp, int max_qp) = 0;

  virtual size_t StreamCount() const = 0;

  virtual bool SupportsEncoderFrameDropping(size_t stream_index) const = 0;

  virtual void OnRatesUpdated(size_t stream_index,
                              const std::vector<uint32_t>& bitrates_bps,
                              int framerate_fps) = 0;

  virtual Vp8EncoderConfig UpdateConfiguration(size_t stream_index) = 0;

  virtual Vp8FrameConfig NextFrameConfig(size_t stream_index,
                                         uint32_t rtp_timestamp) = 0;

  virtual void OnEncodeDone(size_t stream_index,
                            uint32_t rtp_timestamp,
                            size_t size_bytes,
                            bool is_keyframe,
                            int qp,
                            CodecSpecificInfo* info) = 0;

  virtual void OnFrameDropped(size_t stream_index, uint32_t rtp_timestamp) = 0;

  virtual void OnPacketLossRateUpdate(float packet_loss_rate) = 0;

  virtual void OnRttUpdate(int64_t rtt_ms) = 0;

  virtual void OnLossNotification(
      const VideoEncoder::LossNotification& loss_notification) = 0;
};

class Vp8FrameBufferControllerFactory {
 public:
  virtual ~Vp8FrameBufferControllerFactory() = default;

  virtual std::unique_ptr<Vp8FrameBufferControllerFactory> Clone() const = 0;

  virtual std::unique_ptr<Vp8FrameBufferController> Create(
      const VideoCodec& codec,
      const VideoEncoder::Settings& settings,
      FecControllerOverride* fec_controller_override) = 0;
};

}

#endif
