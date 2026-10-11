/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_STREAM_ENCODER_H_
#define VIDEO_VIDEO_STREAM_ENCODER_H_

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "api/adaptation/resource.h"
#include "api/field_trials_view.h"
#include "api/rtp_sender_interface.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/units/data_rate.h"
#include "api/video/encoded_image.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_rotation.h"
#include "api/video/video_sink_interface.h"
#include "api/video/video_stream_encoder_settings.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/resource_adaptation_processor.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "modules/video_coding/utility/frame_dropper.h"
#include "modules/video_coding/utility/qp_parser.h"
#include "rtc_base/experiments/rate_control_settings.h"
#include "rtc_base/numerics/exp_filter.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/rate_statistics.h"
#include "rtc_base/thread_annotations.h"
#include "system_wrappers/include/clock.h"
#include "video/adaptation/video_stream_encoder_resource_manager.h"
#include "video/encoder_bitrate_adjuster.h"
#include "video/frame_cadence_adapter.h"
#include "video/frame_encode_metadata_writer.h"
#include "video/video_source_sink_controller.h"
#include "video/video_stream_encoder_interface.h"
#include "video/video_stream_encoder_observer.h"

namespace webrtc {

class VideoStreamEncoder : public VideoStreamEncoderInterface,
                           private EncodedImageCallback,
                           public VideoSourceRestrictionsListener {
 public:

  enum class BitrateAllocationCallbackType {
    kVideoBitrateAllocation,
    kVideoBitrateAllocationWhenScreenSharing,
    kVideoLayersAllocation
  };
  VideoStreamEncoder(
      Clock* clock,
      uint32_t number_of_cores,
      VideoStreamEncoderObserver* encoder_stats_observer,
      const VideoStreamEncoderSettings& settings,
      std::unique_ptr<OveruseFrameDetector> overuse_detector,
      std::unique_ptr<FrameCadenceAdapterInterface> frame_cadence_adapter,
      std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
          encoder_queue,
      BitrateAllocationCallbackType allocation_cb_type,
      const FieldTrialsView& field_trials,
      webrtc::VideoEncoderFactory::EncoderSelectorInterface* encoder_selector =
          nullptr);
  ~VideoStreamEncoder() override;

  VideoStreamEncoder(const VideoStreamEncoder&) = delete;
  VideoStreamEncoder& operator=(const VideoStreamEncoder&) = delete;

  void AddAdaptationResource(rtc::scoped_refptr<Resource> resource) override;
  std::vector<rtc::scoped_refptr<Resource>> GetAdaptationResources() override;

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source,
                 const DegradationPreference& degradation_preference) override;

  void SetSink(EncoderSink* sink, bool rotation_applied) override;

  void SetStartBitrate(int start_bitrate_bps) override;

  void SetFecControllerOverride(
      FecControllerOverride* fec_controller_override) override;

  void ConfigureEncoder(VideoEncoderConfig config,
                        size_t max_data_payload_length) override;
  void ConfigureEncoder(VideoEncoderConfig config,
                        size_t max_data_payload_length,
                        SetParametersCallback callback) override;

  void Stop() override;

  void SendKeyFrame(const std::vector<VideoFrameType>& layers = {}) override;

  void OnLossNotification(
      const VideoEncoder::LossNotification& loss_notification) override;

  void OnBitrateUpdated(DataRate target_bitrate,
                        DataRate stable_target_bitrate,
                        DataRate target_headroom,
                        uint8_t fraction_lost,
                        int64_t round_trip_time_ms,
                        double cwnd_reduce_ratio) override;

  DataRate UpdateTargetBitrate(DataRate target_bitrate,
                               double cwnd_reduce_ratio);

 protected:
  friend class VideoStreamEncoderFrameCadenceRestrictionTest;

  TaskQueueBase* encoder_queue() { return encoder_queue_.get(); }

  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      rtc::scoped_refptr<Resource> reason,
      const VideoSourceRestrictions& unfiltered_restrictions) override;

  void InjectAdaptationResource(rtc::scoped_refptr<Resource> resource,
                                VideoAdaptationReason reason);
  void InjectAdaptationConstraint(AdaptationConstraint* adaptation_constraint);

  void AddRestrictionsListenerForTesting(
      VideoSourceRestrictionsListener* restrictions_listener);
  void RemoveRestrictionsListenerForTesting(
      VideoSourceRestrictionsListener* restrictions_listener);

 private:
  class CadenceCallback : public FrameCadenceAdapterInterface::Callback {
   public:
    explicit CadenceCallback(VideoStreamEncoder& video_stream_encoder)
        : video_stream_encoder_(video_stream_encoder) {}

    void OnFrame(Timestamp post_time,
                 bool queue_overload,
                 const VideoFrame& frame) override {
      video_stream_encoder_.OnFrame(post_time, queue_overload, frame);
    }
    void OnDiscardedFrame() override {
      video_stream_encoder_.OnDiscardedFrame();
    }
    void RequestRefreshFrame() override {
      video_stream_encoder_.RequestRefreshFrame();
    }

   private:
    VideoStreamEncoder& video_stream_encoder_;
  };

  class VideoFrameInfo {
   public:
    VideoFrameInfo(int width, int height, bool is_texture)
        : width(width), height(height), is_texture(is_texture) {}
    int width;
    int height;
    bool is_texture;
    int pixel_count() const { return width * height; }
  };

  struct EncoderRateSettings {
    EncoderRateSettings();
    EncoderRateSettings(const VideoBitrateAllocation& bitrate,
                        double framerate_fps,
                        DataRate bandwidth_allocation,
                        DataRate encoder_target,
                        DataRate stable_encoder_target);
    bool operator==(const EncoderRateSettings& rhs) const;
    bool operator!=(const EncoderRateSettings& rhs) const;

    VideoEncoder::RateControlParameters rate_control;

    DataRate encoder_target;
    DataRate stable_encoder_target;
  };

  class DegradationPreferenceManager;

  void ReconfigureEncoder() RTC_RUN_ON(encoder_queue_);
  void OnEncoderSettingsChanged() RTC_RUN_ON(encoder_queue_);
  void OnFrame(Timestamp post_time,
               bool queue_overload,
               const VideoFrame& video_frame);
  void OnDiscardedFrame();
  void RequestRefreshFrame();

  void MaybeEncodeVideoFrame(const VideoFrame& frame,
                             int64_t time_when_posted_in_ms);

  void EncodeVideoFrame(const VideoFrame& frame,
                        int64_t time_when_posted_in_ms);

  bool DropDueToSize(uint32_t pixel_count) const RTC_RUN_ON(encoder_queue_);

  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info) override;

  void OnDroppedFrame(EncodedImageCallback::DropReason reason) override;

  bool EncoderPaused() const;
  void TraceFrameDropStart();
  void TraceFrameDropEnd();

  EncoderRateSettings UpdateBitrateAllocation(
      const EncoderRateSettings& rate_settings) RTC_RUN_ON(encoder_queue_);

  uint32_t GetInputFramerateFps() RTC_RUN_ON(encoder_queue_);
  void SetEncoderRates(const EncoderRateSettings& rate_settings)
      RTC_RUN_ON(encoder_queue_);

  void RunPostEncode(const EncodedImage& encoded_image,
                     int64_t time_sent_us,
                     int temporal_index,
                     DataSize frame_size);
  void ReleaseEncoder() RTC_RUN_ON(encoder_queue_);

  void ShutdownResourceAdaptationQueue();

  void CheckForAnimatedContent(const VideoFrame& frame,
                               int64_t time_when_posted_in_ms)
      RTC_RUN_ON(encoder_queue_);

  void RequestEncoderSwitch() RTC_RUN_ON(encoder_queue_);

  EncodedImage AugmentEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info);

  void ProcessDroppedFrame(const VideoFrame& frame,
                           VideoStreamEncoderObserver::DropReason reason)
      RTC_RUN_ON(encoder_queue_);
int b;
  const FieldTrialsView& field_trials_;
  TaskQueueBase* const worker_queue_;

  const int number_of_cores_;

  EncoderSink* sink_ = nullptr;
  const VideoStreamEncoderSettings settings_;
  const BitrateAllocationCallbackType allocation_cb_type_;
  const RateControlSettings rate_control_settings_;

  webrtc::VideoEncoderFactory::EncoderSelectorInterface* const
      encoder_selector_from_constructor_;
  std::unique_ptr<VideoEncoderFactory::EncoderSelectorInterface> const
      encoder_selector_from_factory_;

  VideoEncoderFactory::EncoderSelectorInterface* const encoder_selector_;

  VideoStreamEncoderObserver* const encoder_stats_observer_;

  CadenceCallback cadence_callback_{*this};

  std::unique_ptr<FrameCadenceAdapterInterface> frame_cadence_adapter_
      RTC_GUARDED_BY(encoder_queue_) RTC_PT_GUARDED_BY(encoder_queue_);

  VideoEncoderConfig encoder_config_ RTC_GUARDED_BY(encoder_queue_);
  std::unique_ptr<VideoEncoder> encoder_ RTC_GUARDED_BY(encoder_queue_)
      RTC_PT_GUARDED_BY(encoder_queue_);
  bool encoder_initialized_ = false;
  std::unique_ptr<VideoBitrateAllocator> rate_allocator_
      RTC_GUARDED_BY(encoder_queue_) RTC_PT_GUARDED_BY(encoder_queue_);
  int max_framerate_ RTC_GUARDED_BY(encoder_queue_) = -1;

  bool pending_encoder_reconfiguration_ RTC_GUARDED_BY(encoder_queue_) = false;

  bool pending_encoder_creation_ RTC_GUARDED_BY(encoder_queue_) = false;
  absl::InlinedVector<SetParametersCallback, 2> encoder_configuration_callbacks_
      RTC_GUARDED_BY(encoder_queue_);

  absl::optional<VideoFrameInfo> last_frame_info_
      RTC_GUARDED_BY(encoder_queue_);
  int crop_width_ RTC_GUARDED_BY(encoder_queue_) = 0;
  int crop_height_ RTC_GUARDED_BY(encoder_queue_) = 0;
  absl::optional<uint32_t> encoder_target_bitrate_bps_
      RTC_GUARDED_BY(encoder_queue_);
  size_t max_data_payload_length_ RTC_GUARDED_BY(encoder_queue_) = 0;
  absl::optional<EncoderRateSettings> last_encoder_rate_settings_
      RTC_GUARDED_BY(encoder_queue_);
  bool encoder_paused_and_dropped_frame_ RTC_GUARDED_BY(encoder_queue_) = false;

  bool was_encode_called_since_last_initialization_
      RTC_GUARDED_BY(encoder_queue_) = false;

  bool encoder_failed_ RTC_GUARDED_BY(encoder_queue_) = false;
  Clock* const clock_;

  int64_t last_captured_timestamp_ RTC_GUARDED_BY(encoder_queue_) = 0;

  const int64_t delta_ntp_internal_ms_ RTC_GUARDED_BY(encoder_queue_);

  int64_t last_frame_log_ms_ RTC_GUARDED_BY(encoder_queue_);
  int captured_frame_count_ RTC_GUARDED_BY(encoder_queue_) = 0;
  int dropped_frame_cwnd_pushback_count_ RTC_GUARDED_BY(encoder_queue_) = 0;
  int dropped_frame_encoder_block_count_ RTC_GUARDED_BY(encoder_queue_) = 0;
  absl::optional<VideoFrame> pending_frame_ RTC_GUARDED_BY(encoder_queue_);
  int64_t pending_frame_post_time_us_ RTC_GUARDED_BY(encoder_queue_) = 0;

  VideoFrame::UpdateRect accumulated_update_rect_
      RTC_GUARDED_BY(encoder_queue_);
  bool accumulated_update_rect_is_valid_ RTC_GUARDED_BY(encoder_queue_) = true;

  absl::optional<VideoFrame::UpdateRect> last_update_rect_
      RTC_GUARDED_BY(encoder_queue_);
  Timestamp animation_start_time_ RTC_GUARDED_BY(encoder_queue_) =
      Timestamp::PlusInfinity();
  bool cap_resolution_due_to_video_content_ RTC_GUARDED_BY(encoder_queue_) =
      false;

  enum class ExpectResizeState {
    kNoResize,
    kResize,
    kFirstFrameAfterResize
  } expect_resize_state_ RTC_GUARDED_BY(encoder_queue_) =
      ExpectResizeState::kNoResize;

  FecControllerOverride* fec_controller_override_
      RTC_GUARDED_BY(encoder_queue_) = nullptr;
  absl::optional<int64_t> last_parameters_update_ms_
      RTC_GUARDED_BY(encoder_queue_);
  absl::optional<int64_t> last_encode_info_ms_ RTC_GUARDED_BY(encoder_queue_);

  VideoEncoder::EncoderInfo encoder_info_ RTC_GUARDED_BY(encoder_queue_);
  VideoCodec send_codec_ RTC_GUARDED_BY(encoder_queue_);

  FrameDropper frame_dropper_ RTC_GUARDED_BY(encoder_queue_);

  bool force_disable_frame_dropper_ RTC_GUARDED_BY(encoder_queue_) = false;

  std::atomic<int> pending_frame_drops_{0};

  absl::optional<int> cwnd_frame_drop_interval_ RTC_GUARDED_BY(encoder_queue_);

  int cwnd_frame_counter_ RTC_GUARDED_BY(encoder_queue_) = 0;

  std::unique_ptr<EncoderBitrateAdjuster> bitrate_adjuster_
      RTC_GUARDED_BY(encoder_queue_);

  std::vector<VideoFrameType> next_frame_types_ RTC_GUARDED_BY(encoder_queue_);

  FrameEncodeMetadataWriter frame_encode_metadata_writer_{this};

  struct AutomaticAnimationDetectionExperiment {
    bool enabled = false;
    int min_duration_ms = 2000;
    double min_area_ratio = 0.8;
    int min_fps = 10;
    std::unique_ptr<StructParametersParser> Parser() {
      return StructParametersParser::Create(
          "enabled", &enabled,
          "min_duration_ms", &min_duration_ms,
          "min_area_ratio", &min_area_ratio,
          "min_fps", &min_fps);
    }
  };

  AutomaticAnimationDetectionExperiment
  ParseAutomatincAnimationDetectionFieldTrial() const;

  AutomaticAnimationDetectionExperiment
      automatic_animation_detection_experiment_ RTC_GUARDED_BY(encoder_queue_);

  VideoStreamInputStateProvider input_state_provider_;

  const std::unique_ptr<VideoStreamAdapter> video_stream_adapter_
      RTC_GUARDED_BY(encoder_queue_);

  std::unique_ptr<ResourceAdaptationProcessorInterface>
      resource_adaptation_processor_ RTC_GUARDED_BY(encoder_queue_);
  std::unique_ptr<DegradationPreferenceManager> degradation_preference_manager_
      RTC_GUARDED_BY(encoder_queue_);
  std::vector<AdaptationConstraint*> adaptation_constraints_
      RTC_GUARDED_BY(encoder_queue_);

  VideoStreamEncoderResourceManager stream_resource_manager_
      RTC_GUARDED_BY(encoder_queue_);
  std::vector<rtc::scoped_refptr<Resource>> additional_resources_
      RTC_GUARDED_BY(encoder_queue_);

  VideoSourceSinkController video_source_sink_controller_
      RTC_GUARDED_BY(worker_queue_);

  const bool default_limits_allowed_;

  QpParser qp_parser_;
  const bool qp_parsing_allowed_;

  bool switch_encoder_on_init_failures_;

  const absl::optional<int> vp9_low_tier_core_threshold_;
  const absl::optional<int> experimental_encoder_thread_limit_;

  absl::optional<VideoSourceRestrictions> latest_restrictions_
      RTC_GUARDED_BY(encoder_queue_);
  absl::optional<VideoSourceRestrictions> animate_restrictions_
      RTC_GUARDED_BY(encoder_queue_);

  ScopedTaskSafety task_safety_;

  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> encoder_queue_;
};

}

#endif
