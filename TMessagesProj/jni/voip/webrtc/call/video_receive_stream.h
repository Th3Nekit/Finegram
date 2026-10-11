/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_VIDEO_RECEIVE_STREAM_H_
#define CALL_VIDEO_RECEIVE_STREAM_H_

#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "api/call/transport.h"
#include "api/crypto/crypto_options.h"
#include "api/rtp_headers.h"
#include "api/rtp_parameters.h"
#include "api/video/recordable_encoded_frame.h"
#include "api/video/video_content_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_timing.h"
#include "api/video_codecs/sdp_video_format.h"
#include "call/receive_stream.h"
#include "call/rtp_config.h"
#include "common_video/frame_counts.h"
#include "modules/rtp_rtcp/include/rtcp_statistics.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/checks.h"

namespace webrtc {

class RtpPacketSinkInterface;
class VideoDecoderFactory;

class VideoReceiveStreamInterface : public MediaReceiveStreamInterface {
 public:

  struct RecordingState {
    RecordingState() = default;
    explicit RecordingState(
        std::function<void(const RecordableEncodedFrame&)> callback)
        : callback(std::move(callback)) {}

    std::function<void(const RecordableEncodedFrame&)> callback;

    absl::optional<int64_t> last_keyframe_request_ms;
  };

  struct Decoder {
    Decoder(SdpVideoFormat video_format, int payload_type);
    Decoder();
    Decoder(const Decoder&);
    ~Decoder();

    bool operator==(const Decoder& other) const;

    std::string ToString() const;

    SdpVideoFormat video_format;

    int payload_type = 0;
  };

  struct Stats {
    Stats();
    ~Stats();
    std::string ToString(int64_t time_ms) const;

    int network_frame_rate = 0;
    int decode_frame_rate = 0;
    int render_frame_rate = 0;
    uint32_t frames_rendered = 0;

    absl::optional<std::string> decoder_implementation_name;
    absl::optional<bool> power_efficient_decoder;
    FrameCounts frame_counts;
    int decode_ms = 0;
    int max_decode_ms = 0;
    int current_delay_ms = 0;
    int target_delay_ms = 0;
    int jitter_buffer_ms = 0;

    TimeDelta jitter_buffer_delay = TimeDelta::Zero();

    TimeDelta jitter_buffer_target_delay = TimeDelta::Zero();

    uint64_t jitter_buffer_emitted_count = 0;

    TimeDelta jitter_buffer_minimum_delay = TimeDelta::Zero();
    int min_playout_delay_ms = 0;
    int render_delay_ms = 10;
    int64_t interframe_delay_max_ms = -1;

    uint32_t frames_dropped = 0;
    uint32_t frames_decoded = 0;

    TimeDelta total_decode_time = TimeDelta::Zero();

    TimeDelta total_processing_delay = TimeDelta::Zero();

    TimeDelta total_assembly_time = TimeDelta::Zero();
    uint32_t frames_assembled_from_multiple_packets = 0;

    double total_inter_frame_delay = 0;

    double total_squared_inter_frame_delay = 0;
    int64_t first_frame_received_to_decoded_ms = -1;
    absl::optional<uint64_t> qp_sum;

    int current_payload_type = -1;

    int total_bitrate_bps = 0;

    int width = 0;
    int height = 0;

    uint32_t freeze_count = 0;
    uint32_t pause_count = 0;
    uint32_t total_freezes_duration_ms = 0;
    uint32_t total_pauses_duration_ms = 0;

    VideoContentType content_type = VideoContentType::UNSPECIFIED;

    absl::optional<int64_t> estimated_playout_ntp_timestamp_ms;
    int sync_offset_ms = std::numeric_limits<int>::max();

    uint32_t ssrc = 0;
    std::string c_name;
    RtpReceiveStats rtp_stats;
    RtcpPacketTypeCounter rtcp_packet_type_counts;
    absl::optional<RtpReceiveStats> rtx_rtp_stats;

    absl::optional<webrtc::TimingFrameInfo> timing_frame_info;
  };

  struct Config {
   private:

    Config(const Config&);

   public:
    Config() = delete;
    Config(Config&&);
    Config(Transport* rtcp_send_transport,
           VideoDecoderFactory* decoder_factory = nullptr);
    Config& operator=(Config&&);
    Config& operator=(const Config&) = delete;
    ~Config();

    Config Copy() const { return Config(*this); }

    std::string ToString() const;

    std::vector<Decoder> decoders;

    VideoDecoderFactory* decoder_factory = nullptr;

    struct Rtp : public ReceiveStreamRtpConfig {
      Rtp();
      Rtp(const Rtp&);
      ~Rtp();
      std::string ToString() const;

      NackConfig nack;

      RtcpMode rtcp_mode = RtcpMode::kCompound;

      struct RtcpXr {

        bool receiver_reference_time_report = false;
      } rtcp_xr;

      KeyFrameReqMethod keyframe_method = KeyFrameReqMethod::kPliRtcp;

      LntfConfig lntf;

      int ulpfec_payload_type = -1;
      int red_payload_type = -1;

      uint32_t rtx_ssrc = 0;

      bool protected_by_flexfec = false;

      RtpPacketSinkInterface* packet_sink_ = nullptr;

      std::map<int, int> rtx_associated_payload_types;

      std::set<int> raw_payload_types;
    } rtp;

    Transport* rtcp_send_transport = nullptr;

    rtc::VideoSinkInterface<VideoFrame>* renderer = nullptr;

    int render_delay_ms = 10;

    bool enable_prerenderer_smoothing = true;

    std::string sync_group;

    rtc::scoped_refptr<webrtc::FrameDecryptorInterface> frame_decryptor;

    CryptoOptions crypto_options;

    rtc::scoped_refptr<webrtc::FrameTransformerInterface> frame_transformer;
  };

  virtual Stats GetStats() const = 0;

  virtual bool SetBaseMinimumPlayoutDelayMs(int delay_ms) = 0;

  virtual int GetBaseMinimumPlayoutDelayMs() const = 0;

  virtual RecordingState SetAndGetRecordingState(RecordingState state,
                                                 bool generate_key_frame) = 0;

  virtual void GenerateKeyFrame() = 0;

  virtual void SetRtcpMode(RtcpMode mode) = 0;

  virtual void SetFlexFecProtection(RtpPacketSinkInterface* flexfec_sink) = 0;

  virtual void SetLossNotificationEnabled(bool enabled) = 0;

  virtual void SetNackHistory(TimeDelta history) = 0;

  virtual void SetProtectionPayloadTypes(int red_payload_type,
                                         int ulpfec_payload_type) = 0;

  virtual void SetRtcpXr(Config::Rtp::RtcpXr rtcp_xr) = 0;

  virtual void SetAssociatedPayloadTypes(
      std::map<int, int> associated_payload_types) = 0;

  virtual void UpdateRtxSsrc(uint32_t ssrc) = 0;

 protected:
  virtual ~VideoReceiveStreamInterface() {}
};

}

#endif
