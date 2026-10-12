/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_RTP_VIDEO_STREAM_RECEIVER2_H_
#define VIDEO_RTP_VIDEO_STREAM_RECEIVER2_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/crypto/frame_decryptor_interface.h"
#include "api/sequence_checker.h"
#include "api/units/timestamp.h"
#include "api/video/color_space.h"
#include "api/video/video_codec_type.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/recovered_packet_receiver.h"
#include "modules/rtp_rtcp/include/remote_ntp_time_estimator.h"
#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/absolute_capture_time_interpolator.h"
#include "modules/rtp_rtcp/source/capture_clock_offset_updater.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl2.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_interface.h"
#include "modules/rtp_rtcp/source/rtp_video_header.h"
#include "modules/rtp_rtcp/source/rtp_video_stream_receiver_frame_transformer_delegate.h"
#include "modules/rtp_rtcp/source/video_rtp_depacketizer.h"
#include "modules/video_coding/h264_sps_pps_tracker.h"
#include "modules/video_coding/h265_vps_sps_pps_tracker.h"
#include "modules/video_coding/loss_notification_controller.h"
#include "modules/video_coding/nack_requester.h"
#include "modules/video_coding/packet_buffer.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "video/buffered_frame_decryptor.h"
#include "video/unique_timestamp_counter.h"

namespace webrtc {

class NackRequester;
class PacketRouter;
class ReceiveStatistics;
class RtcpRttStats;
class RtpPacketReceived;
class Transport;
class UlpfecReceiver;

class RtpVideoStreamReceiver2 : public LossNotificationSender,
                                public RecoveredPacketReceiver,
                                public RtpPacketSinkInterface,
                                public KeyFrameRequestSender,
                                public NackSender,
                                public OnDecryptedFrameCallback,
                                public OnDecryptionStatusChangeCallback,
                                public RtpVideoFrameReceiver {
 public:

  class OnCompleteFrameCallback {
   public:
    virtual ~OnCompleteFrameCallback() {}
    virtual void OnCompleteFrame(std::unique_ptr<EncodedFrame> frame) = 0;
  };

  RtpVideoStreamReceiver2(
      TaskQueueBase* current_queue,
      Clock* clock,
      Transport* transport,
      RtcpRttStats* rtt_stats,

      PacketRouter* packet_router,
      const VideoReceiveStreamInterface::Config* config,
      ReceiveStatistics* rtp_receive_statistics,
      RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
      RtcpCnameCallback* rtcp_cname_callback,
      NackPeriodicProcessor* nack_periodic_processor,

      OnCompleteFrameCallback* complete_frame_callback,
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor,
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer,
      const FieldTrialsView& field_trials,
      RtcEventLog* event_log);
  ~RtpVideoStreamReceiver2() override;

  void AddReceiveCodec(uint8_t payload_type,
                       VideoCodecType video_codec,
                       const webrtc::CodecParameterMap& codec_params,
                       bool raw_payload);
  void RemoveReceiveCodec(uint8_t payload_type);

  void RemoveReceiveCodecs();

  void StartReceive();
  void StopReceive();

  absl::optional<Syncable::Info> GetSyncInfo() const;

  bool DeliverRtcp(const uint8_t* rtcp_packet, size_t rtcp_packet_length);

  void FrameContinuous(int64_t seq_num);

  void FrameDecoded(int64_t seq_num);

  void SignalNetworkState(NetworkState state);

  int GetUniqueFramesSeen() const {
    RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
    return frame_counter_.GetUniqueSeen();
  }

  void OnRtpPacket(const RtpPacketReceived& packet) override;

  bool OnReceivedPayloadData(rtc::CopyOnWriteBuffer codec_payload,
                             const RtpPacketReceived& rtp_packet,
                             const RTPVideoHeader& video,
                             int times_nacked);

  void OnRecoveredPacket(const RtpPacketReceived& packet) override;

  void RequestKeyFrame() override;

  void SendNack(const std::vector<uint16_t>& sequence_numbers,
                bool buffering_allowed) override;

  void SendLossNotification(uint16_t last_decoded_seq_num,
                            uint16_t last_received_seq_num,
                            bool decodability_flag,
                            bool buffering_allowed) override;

  bool IsDecryptable() const;

  void OnDecryptedFrame(std::unique_ptr<RtpFrameObject> frame) override;

  void OnDecryptionStatusChange(
      FrameDecryptorInterface::Status status) override;

  void SetFrameDecryptor(
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor);

  void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer);

  void UpdateRtt(int64_t max_rtt_ms);

  void OnLocalSsrcChange(uint32_t local_ssrc);

  void SetRtcpMode(RtcpMode mode);

  void SetReferenceTimeReport(bool enabled);

  void SetPacketSink(RtpPacketSinkInterface* packet_sink);

  void SetLossNotificationEnabled(bool enabled);

  void SetNackHistory(TimeDelta history);

  int ulpfec_payload_type() const;
  int red_payload_type() const;
  void SetProtectionPayloadTypes(int red_payload_type, int ulpfec_payload_type);

  absl::optional<int64_t> LastReceivedPacketMs() const;
  absl::optional<uint32_t> LastReceivedFrameRtpTimestamp() const;
  absl::optional<int64_t> LastReceivedKeyframePacketMs() const;

 private:

  void ManageFrame(std::unique_ptr<RtpFrameObject> frame) override;

  void OnCompleteFrames(RtpFrameReferenceFinder::ReturnVector frame)
      RTC_RUN_ON(packet_sequence_checker_);

  class RtcpFeedbackBuffer : public KeyFrameRequestSender,
                             public NackSender,
                             public LossNotificationSender {
   public:
    RtcpFeedbackBuffer(KeyFrameRequestSender* key_frame_request_sender,
                       NackSender* nack_sender,
                       LossNotificationSender* loss_notification_sender);

    ~RtcpFeedbackBuffer() override = default;

    void RequestKeyFrame() override;

    void SendNack(const std::vector<uint16_t>& sequence_numbers,
                  bool buffering_allowed) override;

    void SendLossNotification(uint16_t last_decoded_seq_num,
                              uint16_t last_received_seq_num,
                              bool decodability_flag,
                              bool buffering_allowed) override;

    void SendBufferedRtcpFeedback();

    void ClearLossNotificationState();

   private:

    struct LossNotificationState {
      LossNotificationState(uint16_t last_decoded_seq_num,
                            uint16_t last_received_seq_num,
                            bool decodability_flag)
          : last_decoded_seq_num(last_decoded_seq_num),
            last_received_seq_num(last_received_seq_num),
            decodability_flag(decodability_flag) {}

      uint16_t last_decoded_seq_num;
      uint16_t last_received_seq_num;
      bool decodability_flag;
    };

    RTC_NO_UNIQUE_ADDRESS SequenceChecker packet_sequence_checker_;
    KeyFrameRequestSender* const key_frame_request_sender_;
    NackSender* const nack_sender_;
    LossNotificationSender* const loss_notification_sender_;

    bool request_key_frame_ RTC_GUARDED_BY(packet_sequence_checker_);

    std::vector<uint16_t> nack_sequence_numbers_
        RTC_GUARDED_BY(packet_sequence_checker_);

    absl::optional<LossNotificationState> lntf_state_
        RTC_GUARDED_BY(packet_sequence_checker_);
  };
  enum ParseGenericDependenciesResult {
    kStashPacket,
    kDropPacket,
    kHasGenericDescriptor,
    kNoGenericDescriptor
  };

  void ReceivePacket(const RtpPacketReceived& packet)
      RTC_RUN_ON(packet_sequence_checker_);

  void ParseAndHandleEncapsulatingHeader(const RtpPacketReceived& packet)
      RTC_RUN_ON(packet_sequence_checker_);
  void NotifyReceiverOfEmptyPacket(uint16_t seq_num)
      RTC_RUN_ON(packet_sequence_checker_);
  bool IsRedEnabled() const;
  void InsertSpsPpsIntoTracker(uint8_t payload_type)
      RTC_RUN_ON(packet_sequence_checker_);
  void OnInsertedPacket(video_coding::PacketBuffer::InsertResult result)
      RTC_RUN_ON(packet_sequence_checker_);
  ParseGenericDependenciesResult ParseGenericDependenciesExtension(
      const RtpPacketReceived& rtp_packet,
      RTPVideoHeader* video_header) RTC_RUN_ON(packet_sequence_checker_);
  void OnAssembledFrame(std::unique_ptr<RtpFrameObject> frame)
      RTC_RUN_ON(packet_sequence_checker_);
  void UpdatePacketReceiveTimestamps(const RtpPacketReceived& packet,
                                     bool is_keyframe)
      RTC_RUN_ON(packet_sequence_checker_);

  const FieldTrialsView& field_trials_;
  TaskQueueBase* const worker_queue_;
  Clock* const clock_;

  const VideoReceiveStreamInterface::Config& config_;
  PacketRouter* const packet_router_;

  RemoteNtpTimeEstimator ntp_estimator_;

  FieldTrialOptional<int> forced_playout_delay_max_ms_;
  FieldTrialOptional<int> forced_playout_delay_min_ms_;
  ReceiveStatistics* const rtp_receive_statistics_;
  std::unique_ptr<UlpfecReceiver> ulpfec_receiver_
      RTC_GUARDED_BY(packet_sequence_checker_);
  int red_payload_type_ RTC_GUARDED_BY(packet_sequence_checker_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_task_checker_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker packet_sequence_checker_;
  RtpPacketSinkInterface* packet_sink_ RTC_GUARDED_BY(packet_sequence_checker_);
  bool receiving_ RTC_GUARDED_BY(packet_sequence_checker_);
  int64_t last_packet_log_ms_ RTC_GUARDED_BY(packet_sequence_checker_);

  const std::unique_ptr<ModuleRtpRtcpImpl2> rtp_rtcp_;

  NackPeriodicProcessor* const nack_periodic_processor_;
  OnCompleteFrameCallback* complete_frame_callback_;
  const KeyFrameReqMethod keyframe_request_method_;

  RtcpFeedbackBuffer rtcp_feedback_buffer_;

  std::unique_ptr<NackRequester> nack_module_
      RTC_GUARDED_BY(packet_sequence_checker_);
  std::unique_ptr<LossNotificationController> loss_notification_controller_
      RTC_GUARDED_BY(packet_sequence_checker_);

  video_coding::PacketBuffer packet_buffer_
      RTC_GUARDED_BY(packet_sequence_checker_);
  UniqueTimestampCounter frame_counter_
      RTC_GUARDED_BY(packet_sequence_checker_);
  SeqNumUnwrapper<uint16_t> frame_id_unwrapper_
      RTC_GUARDED_BY(packet_sequence_checker_);

  std::unique_ptr<FrameDependencyStructure> video_structure_
      RTC_GUARDED_BY(packet_sequence_checker_);

  absl::optional<int64_t> video_structure_frame_id_
      RTC_GUARDED_BY(packet_sequence_checker_);
  Timestamp last_logged_failed_to_parse_dd_
      RTC_GUARDED_BY(packet_sequence_checker_) = Timestamp::MinusInfinity();

  std::unique_ptr<RtpFrameReferenceFinder> reference_finder_
      RTC_GUARDED_BY(packet_sequence_checker_);
  absl::optional<VideoCodecType> current_codec_
      RTC_GUARDED_BY(packet_sequence_checker_);
  uint32_t last_assembled_frame_rtp_timestamp_
      RTC_GUARDED_BY(packet_sequence_checker_);

  std::map<int64_t, uint16_t> last_seq_num_for_pic_id_
      RTC_GUARDED_BY(packet_sequence_checker_);
  video_coding::H264SpsPpsTracker tracker_
      RTC_GUARDED_BY(packet_sequence_checker_);
  video_coding::H265VpsSpsPpsTracker h265_tracker_;
      RTC_GUARDED_BY(packet_sequence_checker_);

  std::map<uint8_t, std::unique_ptr<VideoRtpDepacketizer>> payload_type_map_
      RTC_GUARDED_BY(packet_sequence_checker_);

  std::map<uint8_t, webrtc::CodecParameterMap> pt_codec_params_
      RTC_GUARDED_BY(packet_sequence_checker_);
  int16_t last_payload_type_ RTC_GUARDED_BY(packet_sequence_checker_) = -1;

  bool has_received_frame_ RTC_GUARDED_BY(packet_sequence_checker_);

  absl::optional<uint32_t> last_received_rtp_timestamp_
      RTC_GUARDED_BY(packet_sequence_checker_);
  absl::optional<uint32_t> last_received_keyframe_rtp_timestamp_
      RTC_GUARDED_BY(packet_sequence_checker_);
  absl::optional<Timestamp> last_received_rtp_system_time_
      RTC_GUARDED_BY(packet_sequence_checker_);
  absl::optional<Timestamp> last_received_keyframe_rtp_system_time_
      RTC_GUARDED_BY(packet_sequence_checker_);

  std::unique_ptr<BufferedFrameDecryptor> buffered_frame_decryptor_
      RTC_PT_GUARDED_BY(packet_sequence_checker_);
  bool frames_decryptable_ RTC_GUARDED_BY(worker_task_checker_);
  absl::optional<ColorSpace> last_color_space_;

  AbsoluteCaptureTimeInterpolator absolute_capture_time_interpolator_
      RTC_GUARDED_BY(packet_sequence_checker_);

  CaptureClockOffsetUpdater capture_clock_offset_updater_
      RTC_GUARDED_BY(packet_sequence_checker_);

  int64_t last_completed_picture_id_ = 0;

  rtc::scoped_refptr<RtpVideoStreamReceiverFrameTransformerDelegate>
      frame_transformer_delegate_;

  SeqNumUnwrapper<uint16_t> rtp_seq_num_unwrapper_
      RTC_GUARDED_BY(packet_sequence_checker_);
  std::map<int64_t, RtpPacketInfo> packet_infos_
      RTC_GUARDED_BY(packet_sequence_checker_);
  std::vector<RtpPacketReceived> stashed_packets_
      RTC_GUARDED_BY(packet_sequence_checker_);

  Timestamp next_keyframe_request_for_missing_video_structure_ =
      Timestamp::MinusInfinity();
};

}

#endif
