/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_H_

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/optional.h"
#include "api/fec_controller_override.h"
#include "api/units/data_rate.h"
#include "api/video/encoded_image.h"
#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_codec_constants.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_codec.h"
#include "rtc_base/checks.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct CodecSpecificInfo;

constexpr int kDefaultMinPixelsPerFrame = 320 * 180;

class RTC_EXPORT EncodedImageCallback {
 public:
  virtual ~EncodedImageCallback() {}

  struct Result {
    enum Error {
      OK,

      ERROR_SEND_FAILED,
    };

    explicit Result(Error error) : error(error) {}
    Result(Error error, uint32_t frame_id) : error(error), frame_id(frame_id) {}

    Error error;

    uint32_t frame_id = 0;

    bool drop_next_frame = false;
  };

  enum class DropReason : uint8_t {
    kDroppedByMediaOptimizations,
    kDroppedByEncoder
  };

  virtual Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info) = 0;

  virtual void OnDroppedFrame(DropReason reason) {}
};

class RTC_EXPORT VideoEncoder {
 public:
  struct QpThresholds {
    QpThresholds(int l, int h) : low(l), high(h) {}
    QpThresholds() : low(-1), high(-1) {}
    int low;
    int high;
  };

  struct RTC_EXPORT ScalingSettings {
   private:

    struct KOff {};

   public:

    static constexpr KOff kOff = {};

    ScalingSettings(int low, int high);
    ScalingSettings(int low, int high, int min_pixels);
    ScalingSettings(const ScalingSettings&);
    ScalingSettings(KOff);
    ~ScalingSettings();

    absl::optional<QpThresholds> thresholds;

    int min_pixels_per_frame = kDefaultMinPixelsPerFrame;

   private:

    ScalingSettings();
  };

  struct ResolutionBitrateLimits {
    ResolutionBitrateLimits(int frame_size_pixels,
                            int min_start_bitrate_bps,
                            int min_bitrate_bps,
                            int max_bitrate_bps)
        : frame_size_pixels(frame_size_pixels),
          min_start_bitrate_bps(min_start_bitrate_bps),
          min_bitrate_bps(min_bitrate_bps),
          max_bitrate_bps(max_bitrate_bps) {}

    int frame_size_pixels = 0;

    int min_start_bitrate_bps = 0;

    int min_bitrate_bps = 0;

    int max_bitrate_bps = 0;

    bool operator==(const ResolutionBitrateLimits& rhs) const;
    bool operator!=(const ResolutionBitrateLimits& rhs) const {
      return !(*this == rhs);
    }
  };

  struct RTC_EXPORT EncoderInfo {
    static constexpr uint8_t kMaxFramerateFraction =
        std::numeric_limits<uint8_t>::max();

    EncoderInfo();
    EncoderInfo(const EncoderInfo&);

    ~EncoderInfo();

    std::string ToString() const;
    bool operator==(const EncoderInfo& rhs) const;
    bool operator!=(const EncoderInfo& rhs) const { return !(*this == rhs); }

    ScalingSettings scaling_settings;

    uint32_t requested_resolution_alignment;

    bool apply_alignment_to_all_simulcast_layers;

    bool supports_native_handle;

    std::string implementation_name;

    bool has_trusted_rate_controller;

    bool is_hardware_accelerated;

    absl::InlinedVector<uint8_t, kMaxTemporalStreams>
        fps_allocation[kMaxSpatialLayers];

    std::vector<ResolutionBitrateLimits> resolution_bitrate_limits;

    absl::optional<ResolutionBitrateLimits>
    GetEncoderBitrateLimitsForResolution(int frame_size_pixels) const;

    bool supports_simulcast;

    absl::InlinedVector<VideoFrameBuffer::Type, kMaxPreferredPixelFormats>
        preferred_pixel_formats;

    absl::optional<bool> is_qp_trusted;
  };

  struct RTC_EXPORT RateControlParameters {
    RateControlParameters();
    RateControlParameters(const VideoBitrateAllocation& bitrate,
                          double framerate_fps);
    RateControlParameters(const VideoBitrateAllocation& bitrate,
                          double framerate_fps,
                          DataRate bandwidth_allocation);
    virtual ~RateControlParameters();

    VideoBitrateAllocation target_bitrate;

    VideoBitrateAllocation bitrate;

    double framerate_fps;

    DataRate bandwidth_allocation;

    bool operator==(const RateControlParameters& rhs) const;
    bool operator!=(const RateControlParameters& rhs) const;
  };

  struct LossNotification {

    uint32_t timestamp_of_last_decodable;

    uint32_t timestamp_of_last_received;

    absl::optional<bool> dependencies_of_last_received_decodable;

    absl::optional<bool> last_received_decodable;
  };

  struct Capabilities {
    explicit Capabilities(bool loss_notification)
        : loss_notification(loss_notification) {}
    bool loss_notification;
  };

  struct Settings {
    Settings(const Capabilities& capabilities,
             int number_of_cores,
             size_t max_payload_size)
        : capabilities(capabilities),
          number_of_cores(number_of_cores),
          max_payload_size(max_payload_size) {}

    Capabilities capabilities;
    int number_of_cores;
    size_t max_payload_size;

    absl::optional<int> encoder_thread_limit;
  };

  static VideoCodecVP8 GetDefaultVp8Settings();
  static VideoCodecVP9 GetDefaultVp9Settings();
  static VideoCodecH264 GetDefaultH264Settings();
  static VideoCodecH265 GetDefaultH265Settings();

  virtual ~VideoEncoder() {}

  virtual void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override);

                                                 virtual int32_t InitEncode(
      const VideoCodec* codec_settings,
      int32_t number_of_cores,
      size_t max_payload_size);
  virtual int InitEncode(const VideoCodec* codec_settings,
                         const VideoEncoder::Settings& settings);

  virtual int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) = 0;

  virtual int32_t Release() = 0;

  virtual int32_t Encode(const VideoFrame& frame,
                         const std::vector<VideoFrameType>* frame_types) = 0;

  virtual void SetRates(const RateControlParameters& parameters) = 0;

  virtual void OnPacketLossRateUpdate(float packet_loss_rate);

  virtual void OnRttUpdate(int64_t rtt_ms);

  virtual void OnLossNotification(const LossNotification& loss_notification);

  virtual EncoderInfo GetEncoderInfo() const = 0;
};
}
#endif
