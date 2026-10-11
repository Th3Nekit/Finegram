/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_RECEIVE_STREAM2_H_
#define VIDEO_VIDEO_RECEIVE_STREAM2_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/task_queue/task_queue_base.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/recordable_encoded_frame.h"
#include "call/call.h"
#include "call/rtp_packet_sink_interface.h"
#include "call/syncable.h"
#include "call/video_receive_stream.h"
#include "modules/rtp_rtcp/source/source_tracker.h"
#include "modules/video_coding/nack_requester.h"
#include "modules/video_coding/video_receiver2.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "video/receive_statistics_proxy.h"
#include "video/rtp_streams_synchronizer2.h"
#include "video/rtp_video_stream_receiver2.h"
#include "video/transport_adapter.h"
#include "video/video_stream_buffer_controller.h"
#include "video/video_stream_decoder2.h"

namespace webrtc {

class RtpStreamReceiverInterface;
class RtpStreamReceiverControllerInterface;
class RtxReceiveStream;
class VCMTiming;

constexpr TimeDelta kMaxWaitForKeyFrame = TimeDelta::Millis(200);
constexpr TimeDelta kMaxWaitForFrame = TimeDelta::Seconds(3);

namespace internal {

class CallStats;

struct VideoFrameMetaData {
  VideoFrameMetaData(const webrtc::VideoFrame& frame, Timestamp now)
      : rtp_timestamp(frame.timestamp()),
        timestamp_us(frame.timestamp_us()),
        ntp_time_ms(frame.ntp_time_ms()),
        width(frame.width()),
        height(frame.height()),
        decode_timestamp(now) {}

  int64_t render_time_ms() const {
    return timestamp_us / rtc::kNumMicrosecsPerMillisec;
  }

  const uint32_t rtp_timestamp;
  const int64_t timestamp_us;
  const int64_t ntp_time_ms;
  const int width;
  const int height;

  const Timestamp decode_timestamp;
};

class VideoReceiveStream2
    : public webrtc::VideoReceiveStreamInterface,
      public rtc::VideoSinkInterface<VideoFrame>,
      public RtpVideoStreamReceiver2::OnCompleteFrameCallback,
      public Syncable,
      public CallStatsObserver,
      public FrameSchedulingReceiver {
 public:

  static constexpr size_t kBufferedEncodedFramesMaxSize = 60;

  VideoReceiveStream2(const Environment& env,
                      Call* call,
                      int num_cpu_cores,
                      PacketRouter* packet_router,
                      VideoReceiveStreamInterface::Config config,
                      CallStats* call_stats,
                      std::unique_ptr<VCMTiming> timing,
                      NackPeriodicProcessor* nack_periodic_processor,
                      DecodeSynchronizer* decode_sync);

  ~VideoReceiveStream2() override;

  void RegisterWithTransport(
      RtpStreamReceiverControllerInterface* receiver_controller);

  void UnregisterFromTransport();

  const std::string& sync_group() const;

  uint32_t remote_ssrc() const { return config_.rtp.remote_ssrc; }

  uint32_t rtx_ssrc() const {
    RTC_DCHECK_RUN_ON(&packet_sequence_checker_);
    return updated_rtx_ssrc_.value_or(config_.rtp.rtx_ssrc);
  }

  void SignalNetworkState(NetworkState state);
  bool DeliverRtcp(const uint8_t* packet, size_t length);

  void SetSync(Syncable* audio_syncable);

  void SetLocalSsrc(uint32_t local_ssrc);

  void Start() override;
  void Stop() override;

  void SetRtcpMode(RtcpMode mode) override;
  void SetFlexFecProtection(RtpPacketSinkInterface* flexfec_sink) override;
  void SetLossNotificationEnabled(bool enabled) override;
  void SetNackHistory(TimeDelta history) override;
  void SetProtectionPayloadTypes(int red_payload_type,
                                 int ulpfec_payload_type) override;
  void SetRtcpXr(Config::Rtp::RtcpXr rtcp_xr) override;
  void SetAssociatedPayloadTypes(
      std::map<int, int> associated_payload_types) override;

  webrtc::VideoReceiveStreamInterface::Stats GetStats() const override;

  bool SetBaseMinimumPlayoutDelayMs(int delay_ms) override;
  int GetBaseMinimumPlayoutDelayMs() const override;

  void SetFrameDecryptor(
      rtc::scoped_refptr<FrameDecryptorInterface> frame_decryptor) override;
  void SetDepacketizerToDecoderFrameTransformer(
      rtc::scoped_refptr<FrameTransformerInterface> frame_transformer) override;

  void OnFrame(const VideoFrame& video_frame) override;

  void OnCompleteFrame(std::unique_ptr<EncodedFrame> frame) override;

  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;

  uint32_t id() const override;
  absl::optional<Syncable::Info> GetInfo() const override;
  bool GetPlayoutRtpTimestamp(uint32_t* rtp_timestamp,
                              int64_t* time_ms) const override;
  void SetEstimatedPlayoutNtpTimestampMs(int64_t ntp_timestamp_ms,
                                         int64_t time_ms) override;

  bool SetMinimumPlayoutDelay(int delay_ms) override;

  std::vector<webrtc::RtpSource> GetSources() const override;

  RecordingState SetAndGetRecordingState(RecordingState state,
                                         bool generate_key_frame) override;
  void GenerateKeyFrame() override;

  void UpdateRtxSsrc(uint32_t ssrc) override;

 private:

  void OnEncodedFrame(std::unique_ptr<EncodedFrame> frame) override;

  void OnDecodableFrameTimeout(TimeDelta wait) override;

  void CreateAndRegisterExternalDecoder(const Decoder& decoder);

  struct DecodeFrameResult {

    bool force_request_key_frame;

    absl::optional<int64_t> decoded_frame_picture_id;

    bool keyframe_required;
  };

  DecodeFrameResult HandleEncodedFrameOnDecodeQueue(
      std::unique_ptr<EncodedFrame> frame,
      bool keyframe_request_is_due,
      bool keyframe_required) RTC_RUN_ON(decode_sequence_checker_);
  void UpdatePlayoutDelays() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(worker_sequence_checker_);
  void RequestKeyFrame(Timestamp now) RTC_RUN_ON(packet_sequence_checker_);
  void HandleKeyFrameGeneration(bool received_frame_is_keyframe,
                                Timestamp now,
                                bool always_request_key_frame,
                                bool keyframe_request_is_due)
      RTC_RUN_ON(packet_sequence_checker_);
  bool IsReceivingKeyFrame(Timestamp timestamp) const
      RTC_RUN_ON(packet_sequence_checker_);
  int DecodeAndMaybeDispatchEncodedFrame(std::unique_ptr<EncodedFrame> frame)
      RTC_RUN_ON(decode_sequence_checker_);

  void UpdateHistograms();

  const Environment env_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker worker_sequence_checker_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker packet_sequence_checker_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker decode_sequence_checker_;

  TransportAdapter transport_adapter_;
  const VideoReceiveStreamInterface::Config config_;
  const int num_cpu_cores_;
  Call* const call_;

  CallStats* const call_stats_;

  bool decoder_running_ RTC_GUARDED_BY(worker_sequence_checker_) = false;
  bool decoder_stopped_ RTC_GUARDED_BY(decode_sequence_checker_) = true;

  SourceTracker source_tracker_;
  ReceiveStatisticsProxy stats_proxy_;

  const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;

  std::unique_ptr<VCMTiming> timing_;
  VideoReceiver2 video_receiver_;
  std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>> incoming_video_stream_;
  RtpVideoStreamReceiver2 rtp_video_stream_receiver_;
  std::unique_ptr<VideoStreamDecoder> video_stream_decoder_;
  RtpStreamsSynchronizer rtp_stream_sync_;

  std::unique_ptr<VideoStreamBufferController> buffer_;

  RtpStreamReceiverControllerInterface* receiver_controller_
      RTC_GUARDED_BY(packet_sequence_checker_) = nullptr;

  std::unique_ptr<RtpStreamReceiverInterface> media_receiver_
      RTC_GUARDED_BY(packet_sequence_checker_);
  std::unique_ptr<RtxReceiveStream> rtx_receive_stream_
      RTC_GUARDED_BY(packet_sequence_checker_);
  absl::optional<uint32_t> updated_rtx_ssrc_
      RTC_GUARDED_BY(packet_sequence_checker_);
  std::unique_ptr<RtpStreamReceiverInterface> rtx_receiver_
      RTC_GUARDED_BY(packet_sequence_checker_);

  bool keyframe_required_ RTC_GUARDED_BY(packet_sequence_checker_) = true;

  bool frame_decoded_ RTC_GUARDED_BY(decode_sequence_checker_) = false;

  absl::optional<Timestamp> last_keyframe_request_
      RTC_GUARDED_BY(packet_sequence_checker_);

  TimeDelta max_wait_for_keyframe_ RTC_GUARDED_BY(packet_sequence_checker_);
  TimeDelta max_wait_for_frame_ RTC_GUARDED_BY(packet_sequence_checker_);

  absl::optional<TimeDelta> frame_minimum_playout_delay_
      RTC_GUARDED_BY(worker_sequence_checker_);

  absl::optional<TimeDelta> base_minimum_playout_delay_
      RTC_GUARDED_BY(worker_sequence_checker_);

  absl::optional<TimeDelta> syncable_minimum_playout_delay_
      RTC_GUARDED_BY(worker_sequence_checker_);

  absl::optional<TimeDelta> frame_maximum_playout_delay_
      RTC_GUARDED_BY(worker_sequence_checker_);

  std::function<void(const RecordableEncodedFrame&)>
      encoded_frame_buffer_function_ RTC_GUARDED_BY(decode_sequence_checker_);

  bool keyframe_generation_requested_ RTC_GUARDED_BY(packet_sequence_checker_) =
      false;

  webrtc::Mutex pending_resolution_mutex_;

  absl::optional<RecordableEncodedFrame::EncodedResolution> pending_resolution_
      RTC_GUARDED_BY(pending_resolution_mutex_);

  std::vector<std::unique_ptr<EncodedFrame>> buffered_encoded_frames_
      RTC_GUARDED_BY(decode_sequence_checker_);

  ScopedTaskSafety task_safety_;

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> decode_queue_;
};

}
}

#endif
