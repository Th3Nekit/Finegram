/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_VIDEO_SEND_STREAM_H_
#define CALL_VIDEO_SEND_STREAM_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/adaptation/resource.h"
#include "api/call/transport.h"
#include "api/crypto/crypto_options.h"
#include "api/frame_transformer_interface.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_stream_encoder_settings.h"
#include "api/video_codecs/scalability_mode.h"
#include "call/rtp_config.h"
#include "common_video/frame_counts.h"
#include "common_video/include/quality_limitation_reason.h"
#include "modules/rtp_rtcp/include/report_block_data.h"
#include "modules/rtp_rtcp/include/rtcp_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "video/config/video_encoder_config.h"

namespace webrtc {

class FrameEncryptorInterface;

class VideoSendStream {
 public:

  struct StreamStats {
    enum class StreamType {

      kMedia,

      kRtx,

      kFlexfec,
    };

    StreamStats();
    ~StreamStats();

    std::string ToString() const;

    StreamType type = StreamType::kMedia;

    absl::optional<uint32_t> referenced_media_ssrc;
    FrameCounts frame_counts;
    int width = 0;
    int height = 0;

    int total_bitrate_bps = 0;
    int retransmit_bitrate_bps = 0;

    int avg_delay_ms = 0;
    int max_delay_ms = 0;
    StreamDataCounters rtp_stats;
    RtcpPacketTypeCounter rtcp_packet_type_counts;

    absl::optional<ReportBlockData> report_block_data;
    double encode_frame_rate = 0.0;
    int frames_encoded = 0;
    absl::optional<uint64_t> qp_sum;
    uint64_t total_encode_time_ms = 0;
    uint64_t total_encoded_bytes_target = 0;
    uint32_t huge_frames_sent = 0;
    absl::optional<ScalabilityMode> scalability_mode;
  };

  struct Stats {
    Stats();
    ~Stats();
    std::string ToString(int64_t time_ms) const;
    absl::optional<std::string> encoder_implementation_name;
    double input_frame_rate = 0;
    int encode_frame_rate = 0;
    int avg_encode_time_ms = 0;
    int encode_usage_percent = 0;
    uint32_t frames_encoded = 0;

    uint64_t total_encode_time_ms = 0;

    uint64_t total_encoded_bytes_target = 0;
    uint32_t frames = 0;
    uint32_t frames_dropped_by_capturer = 0;
    uint32_t frames_dropped_by_bad_timestamp = 0;
    uint32_t frames_dropped_by_encoder_queue = 0;
    uint32_t frames_dropped_by_rate_limiter = 0;
    uint32_t frames_dropped_by_congestion_window = 0;
    uint32_t frames_dropped_by_encoder = 0;

    int target_media_bitrate_bps = 0;

    int media_bitrate_bps = 0;
    bool suspended = false;
    bool bw_limited_resolution = false;
    bool cpu_limited_resolution = false;
    bool bw_limited_framerate = false;
    bool cpu_limited_framerate = false;

    QualityLimitationReason quality_limitation_reason =
        QualityLimitationReason::kNone;

    std::map<QualityLimitationReason, int64_t> quality_limitation_durations_ms;

    uint32_t quality_limitation_resolution_changes = 0;

    int number_of_cpu_adapt_changes = 0;
    int number_of_quality_adapt_changes = 0;
    bool has_entered_low_resolution = false;
    std::map<uint32_t, StreamStats> substreams;
    webrtc::VideoContentType content_type =
        webrtc::VideoContentType::UNSPECIFIED;
    uint32_t frames_sent = 0;
    uint32_t huge_frames_sent = 0;
    absl::optional<bool> power_efficient_encoder;
  };

  struct Config {
   public:
    Config() = delete;
    Config(Config&&);
    explicit Config(Transport* send_transport);

    Config& operator=(Config&&);
    Config& operator=(const Config&) = delete;

    ~Config();

    Config Copy() const { return Config(*this); }

    std::string ToString() const;

    RtpConfig rtp;

    VideoStreamEncoderSettings encoder_settings;

    int rtcp_report_interval_ms = 1000;

    Transport* send_transport = nullptr;

    int render_delay_ms = 0;

    int target_delay_ms = 0;

    bool suspend_below_min_bitrate = false;

    bool periodic_alr_bandwidth_probing = false;

    rtc::scoped_refptr<webrtc::FrameEncryptorInterface> frame_encryptor;

    VideoEncoderFactory::EncoderSelectorInterface* encoder_selector = nullptr;

    CryptoOptions crypto_options;

    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer;

   private:

    Config(const Config&);
  };

  virtual void Start() = 0;

  virtual void Stop() = 0;

  virtual bool started() = 0;

  virtual void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) = 0;
  virtual std::vector<rtc::scoped_refptr<Resource>>
  GetAdaptationResources() = 0;

  virtual void SetSource(
      rtc::VideoSourceInterface<webrtc::VideoFrame>* source,
      const DegradationPreference& degradation_preference) = 0;

  virtual void ReconfigureVideoEncoder(VideoEncoderConfig config) = 0;

  virtual void ReconfigureVideoEncoder(VideoEncoderConfig config,
                                       SetParametersCallback callback) = 0;

  virtual Stats GetStats() = 0;

  virtual void GenerateKeyFrame(const std::vector<std::string>& rids) = 0;

 protected:
  virtual ~VideoSendStream() {}
};

}

#endif
