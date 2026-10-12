/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_SENDER_VIDEO_H_

#include <map>
#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/transport/rtp/dependency_descriptor.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame_type.h"
#include "api/video/video_layers_allocation.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/absolute_capture_time_sender.h"
#include "modules/rtp_rtcp/source/active_decode_targets_helper.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "modules/rtp_rtcp/source/rtp_sender_video_frame_transformer_delegate.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/rtp_rtcp/source/video_fec_generator.h"
#include "rtc_base/bitrate_tracker.h"
#include "rtc_base/frequency_tracker.h"
#include "rtc_base/one_time_event.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class FrameEncryptorInterface;
class RtpPacketizer;
class RtpPacketToSend;

enum RetransmissionMode : uint8_t {
  kRetransmitOff = 0x0,
  kRetransmitBaseLayer = 0x2,
  kRetransmitHigherLayers = 0x4,
  kRetransmitAllLayers = 0x6,
  kConditionallyRetransmitHigherLayers = 0x8
};

class RTPSenderVideo : public RTPVideoFrameSenderInterface {
 public:
  static constexpr TimeDelta kTLRateWindowSize = TimeDelta::Millis(2'500);

  struct Config {
    Config() = default;
    Config(const Config&) = delete;
    Config(Config&&) = default;

    Clock* clock = nullptr;
    RTPSender* rtp_sender = nullptr;

    absl::optional<VideoFecGenerator::FecType> fec_type;
    size_t fec_overhead_bytes = 0;
    FrameEncryptorInterface* frame_encryptor = nullptr;
    bool require_frame_encryption = false;
    bool enable_retransmit_all_layers = false;
    absl::optional<int> red_payload_type;
    const FieldTrialsView* field_trials = nullptr;
    rtc::scoped_refptr<FrameTransformerInterface> frame_transformer;
    TaskQueueFactory* task_queue_factory = nullptr;
  };

  explicit RTPSenderVideo(const Config& config);

  virtual ~RTPSenderVideo();

  bool SendVideo(int payload_type,
                 absl::optional<VideoCodecType> codec_type,
                 uint32_t rtp_timestamp,
                 Timestamp capture_time,
                 rtc::ArrayView<const uint8_t> payload,
                 size_t encoder_output_size,
                 RTPVideoHeader video_header,
                 TimeDelta expected_retransmission_time,
                 std::vector<uint32_t> csrcs) override;

  bool SendEncodedImage(int payload_type,
                        absl::optional<VideoCodecType> codec_type,
                        uint32_t rtp_timestamp,
                        const EncodedImage& encoded_image,
                        RTPVideoHeader video_header,
                        TimeDelta expected_retransmission_time);

  void SetVideoStructure(const FrameDependencyStructure* video_structure);

  void SetVideoStructureAfterTransformation(
      const FrameDependencyStructure* video_structure) override;

  void SetVideoLayersAllocation(VideoLayersAllocation allocation);

  void SetVideoLayersAllocationAfterTransformation(
      VideoLayersAllocation allocation) override;

  DataRate PostEncodeOverhead() const;

  void SetRetransmissionSetting(int32_t retransmission_settings);

 protected:
  static uint8_t GetTemporalId(const RTPVideoHeader& header);
  bool AllowRetransmission(uint8_t temporal_id,
                           int32_t retransmission_settings,
                           TimeDelta expected_retransmission_time);

 private:
  struct TemporalLayerStats {
    FrequencyTracker frame_rate{kTLRateWindowSize};
    Timestamp last_frame_time = Timestamp::Zero();
  };

  enum class SendVideoLayersAllocation {
    kSendWithResolution,
    kSendWithoutResolution,
    kDontSend
  };

  void SetVideoStructureInternal(
      const FrameDependencyStructure* video_structure);
  void SetVideoLayersAllocationInternal(VideoLayersAllocation allocation);

  void AddRtpHeaderExtensions(const RTPVideoHeader& video_header,
                              bool first_packet,
                              bool last_packet,
                              RtpPacketToSend* packet) const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(send_checker_);

  size_t FecPacketOverhead() const RTC_EXCLUSIVE_LOCKS_REQUIRED(send_checker_);

  void LogAndSendToNetwork(
      std::vector<std::unique_ptr<RtpPacketToSend>> packets,
      size_t encoder_output_size);

  bool red_enabled() const { return red_payload_type_.has_value(); }

  bool UpdateConditionalRetransmit(uint8_t temporal_id,
                                   TimeDelta expected_retransmission_time)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(stats_mutex_);

  void MaybeUpdateCurrentPlayoutDelay(const RTPVideoHeader& header)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(send_checker_);

  RTPSender* const rtp_sender_;
  Clock* const clock_;

  rtc::RaceChecker send_checker_;
  int32_t retransmission_settings_ RTC_GUARDED_BY(send_checker_);
  VideoRotation last_rotation_ RTC_GUARDED_BY(send_checker_);
  absl::optional<ColorSpace> last_color_space_ RTC_GUARDED_BY(send_checker_);
  bool transmit_color_space_next_frame_ RTC_GUARDED_BY(send_checker_);
  std::unique_ptr<FrameDependencyStructure> video_structure_
      RTC_GUARDED_BY(send_checker_);
  absl::optional<VideoLayersAllocation> allocation_
      RTC_GUARDED_BY(send_checker_);

  SendVideoLayersAllocation send_allocation_ RTC_GUARDED_BY(send_checker_);
  absl::optional<VideoLayersAllocation> last_full_sent_allocation_
      RTC_GUARDED_BY(send_checker_);

  absl::optional<VideoPlayoutDelay> current_playout_delay_
      RTC_GUARDED_BY(send_checker_);

  bool playout_delay_pending_;

  const absl::optional<VideoPlayoutDelay> forced_playout_delay_;

  Mutex mutex_;

  const absl::optional<int> red_payload_type_;
  absl::optional<VideoFecGenerator::FecType> fec_type_;
  const size_t fec_overhead_bytes_;

  mutable Mutex stats_mutex_;
  BitrateTracker post_encode_overhead_bitrate_ RTC_GUARDED_BY(stats_mutex_);

  std::map<int, TemporalLayerStats> frame_stats_by_temporal_layer_
      RTC_GUARDED_BY(stats_mutex_);

  OneTimeEvent first_frame_sent_;

  FrameEncryptorInterface* const frame_encryptor_ = nullptr;

  const bool require_frame_encryption_;

  const bool generic_descriptor_auth_experiment_;

  AbsoluteCaptureTimeSender absolute_capture_time_sender_
      RTC_GUARDED_BY(send_checker_);

  ActiveDecodeTargetsHelper active_decode_targets_tracker_;

  const rtc::scoped_refptr<RTPSenderVideoFrameTransformerDelegate>
      frame_transformer_delegate_;
};

}

#endif
