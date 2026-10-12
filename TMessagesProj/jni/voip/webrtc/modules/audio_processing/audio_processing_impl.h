/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_
#define MODULES_AUDIO_PROCESSING_AUDIO_PROCESSING_IMPL_H_

#include <stdio.h>

#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/function_view.h"
#include "api/task_queue/task_queue_base.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"
#include "modules/audio_processing/agc/agc_manager_direct.h"
#include "modules/audio_processing/agc/gain_control.h"
#include "modules/audio_processing/agc2/input_volume_stats_reporter.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/capture_levels_adjuster/capture_levels_adjuster.h"
#include "modules/audio_processing/echo_control_mobile_impl.h"
#include "modules/audio_processing/gain_control_impl.h"
#include "modules/audio_processing/gain_controller2.h"
#include "modules/audio_processing/high_pass_filter.h"
#include "modules/audio_processing/include/aec_dump.h"
#include "modules/audio_processing/include/audio_frame_proxies.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/include/audio_processing_statistics.h"
#include "modules/audio_processing/ns/noise_suppressor.h"
#include "modules/audio_processing/optionally_built_submodule_creators.h"
#include "modules/audio_processing/render_queue_item_verifier.h"
#include "modules/audio_processing/rms_level.h"
#include "modules/audio_processing/transient/transient_suppressor.h"
#include "rtc_base/gtest_prod_util.h"
#include "rtc_base/swap_queue.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

class ApmDataDumper;
class AudioConverter;

constexpr int RuntimeSettingQueueSize() {
  return 100;
}

class AudioProcessingImpl : public AudioProcessing {
 public:

  AudioProcessingImpl();
  AudioProcessingImpl(const AudioProcessing::Config& config,
                      std::unique_ptr<CustomProcessing> capture_post_processor,
                      std::unique_ptr<CustomProcessing> render_pre_processor,
                      std::unique_ptr<EchoControlFactory> echo_control_factory,
                      rtc::scoped_refptr<EchoDetector> echo_detector,
                      std::unique_ptr<CustomAudioAnalyzer> capture_analyzer);
  ~AudioProcessingImpl() override;
  int Initialize() override;
  int Initialize(const ProcessingConfig& processing_config) override;
  void ApplyConfig(const AudioProcessing::Config& config) override;
  bool CreateAndAttachAecDump(
      absl::string_view file_name,
      int64_t max_log_size_bytes,
      absl::Nonnull<TaskQueueBase*> worker_queue) override;
  bool CreateAndAttachAecDump(
      FILE* handle,
      int64_t max_log_size_bytes,
      absl::Nonnull<TaskQueueBase*> worker_queue) override;

  void AttachAecDump(std::unique_ptr<AecDump> aec_dump) override;
  void DetachAecDump() override;
  void SetRuntimeSetting(RuntimeSetting setting) override;
  bool PostRuntimeSetting(RuntimeSetting setting) override;

  int ProcessStream(const int16_t* const src,
                    const StreamConfig& input_config,
                    const StreamConfig& output_config,
                    int16_t* const dest) override;
  int ProcessStream(const float* const* src,
                    const StreamConfig& input_config,
                    const StreamConfig& output_config,
                    float* const* dest) override;
  bool GetLinearAecOutput(
      rtc::ArrayView<std::array<float, 160>> linear_output) const override;
  void set_output_will_be_muted(bool muted) override;
  void HandleCaptureOutputUsedSetting(bool capture_output_used)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  int set_stream_delay_ms(int delay) override;
  void set_stream_key_pressed(bool key_pressed) override;
  void set_stream_analog_level(int level) override;
  int recommended_stream_analog_level() const
      RTC_LOCKS_EXCLUDED(mutex_capture_) override;

  int ProcessReverseStream(const int16_t* const src,
                           const StreamConfig& input_config,
                           const StreamConfig& output_config,
                           int16_t* const dest) override;
  int AnalyzeReverseStream(const float* const* data,
                           const StreamConfig& reverse_config) override;
  int ProcessReverseStream(const float* const* src,
                           const StreamConfig& input_config,
                           const StreamConfig& output_config,
                           float* const* dest) override;

  int proc_sample_rate_hz() const override;
  int proc_split_sample_rate_hz() const override;
  size_t num_input_channels() const override;
  size_t num_proc_channels() const override;
  size_t num_output_channels() const override;
  size_t num_reverse_channels() const override;
  int stream_delay_ms() const override;

  AudioProcessingStats GetStatistics(bool has_remote_tracks) override {
    return GetStatistics();
  }
  AudioProcessingStats GetStatistics() override {
    return stats_reporter_.GetStatistics();
  }

  AudioProcessing::Config GetConfig() const override;

 protected:

  virtual void InitializeLocked()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_, mutex_capture_);
  void AssertLockedForTest()
      RTC_ASSERT_EXCLUSIVE_LOCK(mutex_render_, mutex_capture_) {
    mutex_render_.AssertHeld();
    mutex_capture_.AssertHeld();
  }

 private:

  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, DefaultBehavior);
  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, ValidConfigBehavior);
  FRIEND_TEST_ALL_PREFIXES(ApmConfiguration, InValidConfigBehavior);
  FRIEND_TEST_ALL_PREFIXES(ApmWithSubmodulesExcludedTest,
                           ToggleTransientSuppressor);
  FRIEND_TEST_ALL_PREFIXES(ApmWithSubmodulesExcludedTest,
                           ReinitializeTransientSuppressor);
  FRIEND_TEST_ALL_PREFIXES(ApmWithSubmodulesExcludedTest,
                           BitexactWithDisabledModules);
  FRIEND_TEST_ALL_PREFIXES(
      AudioProcessingImplGainController2FieldTrialParametrizedTest,
      ConfigAdjustedWhenExperimentEnabled);

  void set_stream_analog_level_locked(int level)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void UpdateRecommendedInputVolumeLocked()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void OverrideSubmoduleCreationForTesting(
      const ApmSubmoduleCreationOverrides& overrides);

  class RuntimeSettingEnqueuer {
   public:
    explicit RuntimeSettingEnqueuer(
        SwapQueue<RuntimeSetting>* runtime_settings);
    ~RuntimeSettingEnqueuer();

    bool Enqueue(RuntimeSetting setting);

   private:
    SwapQueue<RuntimeSetting>& runtime_settings_;
  };

  const std::unique_ptr<ApmDataDumper> data_dumper_;
  static std::atomic<int> instance_count_;
  const bool use_setup_specific_default_aec3_config_;

  struct GainController2ExperimentParams {
    struct Agc2Config {
      InputVolumeController::Config input_volume_controller;
      AudioProcessing::Config::GainController2::AdaptiveDigital
          adaptive_digital_controller;
    };

    absl::optional<Agc2Config> agc2_config;

    bool disallow_transient_suppressor_usage;
  };

  const absl::optional<GainController2ExperimentParams>
      gain_controller2_experiment_params_;

  static absl::optional<GainController2ExperimentParams>
  GetGainController2ExperimentParams();

  static AudioProcessing::Config AdjustConfig(
      const AudioProcessing::Config& config,
      const absl::optional<GainController2ExperimentParams>& experiment_params);

  static bool UseApmVadSubModule(
      const AudioProcessing::Config& config,
      const absl::optional<GainController2ExperimentParams>& experiment_params);

  TransientSuppressor::VadMode transient_suppressor_vad_mode_;

  SwapQueue<RuntimeSetting> capture_runtime_settings_;
  SwapQueue<RuntimeSetting> render_runtime_settings_;

  RuntimeSettingEnqueuer capture_runtime_settings_enqueuer_;
  RuntimeSettingEnqueuer render_runtime_settings_enqueuer_;

  const std::unique_ptr<EchoControlFactory> echo_control_factory_;

  class SubmoduleStates {
   public:
    SubmoduleStates(bool capture_post_processor_enabled,
                    bool render_pre_processor_enabled,
                    bool capture_analyzer_enabled);

    bool Update(bool high_pass_filter_enabled,
                bool mobile_echo_controller_enabled,
                bool noise_suppressor_enabled,
                bool adaptive_gain_controller_enabled,
                bool gain_controller2_enabled,
                bool voice_activity_detector_enabled,
                bool gain_adjustment_enabled,
                bool echo_controller_enabled,
                bool transient_suppressor_enabled);
    bool CaptureMultiBandSubModulesActive() const;
    bool CaptureMultiBandProcessingPresent() const;
    bool CaptureMultiBandProcessingActive(bool ec_processing_active) const;
    bool CaptureFullBandProcessingActive() const;
    bool CaptureAnalyzerActive() const;
    bool RenderMultiBandSubModulesActive() const;
    bool RenderFullBandProcessingActive() const;
    bool RenderMultiBandProcessingActive() const;
    bool HighPassFilteringRequired() const;

   private:
    const bool capture_post_processor_enabled_ = false;
    const bool render_pre_processor_enabled_ = false;
    const bool capture_analyzer_enabled_ = false;
    bool high_pass_filter_enabled_ = false;
    bool mobile_echo_controller_enabled_ = false;
    bool noise_suppressor_enabled_ = false;
    bool adaptive_gain_controller_enabled_ = false;
    bool voice_activity_detector_enabled_ = false;
    bool gain_controller2_enabled_ = false;
    bool gain_adjustment_enabled_ = false;
    bool echo_controller_enabled_ = false;
    bool transient_suppressor_enabled_ = false;
    bool first_update_ = true;
  };

  void MaybeInitializeRender(const StreamConfig& input_config,
                             const StreamConfig& output_config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_);

  void MaybeInitializeCapture(const StreamConfig& input_config,
                              const StreamConfig& output_config);

  bool UpdateActiveSubmoduleStates()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void InitializeLocked(const ProcessingConfig& config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_, mutex_capture_);
  void InitializeResidualEchoDetector()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_, mutex_capture_);
  void InitializeEchoController()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_, mutex_capture_);

  void InitializeHighPassFilter(bool forced_reset)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void InitializeGainController1() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void InitializeTransientSuppressor()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void InitializeGainController2() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void InitializeVoiceActivityDetector()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void InitializeNoiseSuppressor() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void InitializeCaptureLevelsAdjuster()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void InitializePostProcessor() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void InitializeAnalyzer() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void InitializePreProcessor() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_);

  int proc_fullband_sample_rate_hz() const
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void HandleCaptureRuntimeSettings()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void HandleRenderRuntimeSettings()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_);

  void EmptyQueuedRenderAudio() RTC_LOCKS_EXCLUDED(mutex_capture_);
  void EmptyQueuedRenderAudioLocked()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);
  void AllocateRenderQueue()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_, mutex_capture_);
  void QueueBandedRenderAudio(AudioBuffer* audio)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_);
  void QueueNonbandedRenderAudio(AudioBuffer* audio)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_);

  int ProcessCaptureStreamLocked() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  int AnalyzeReverseStreamLocked(const float* const* src,
                                 const StreamConfig& input_config,
                                 const StreamConfig& output_config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_);
  int ProcessRenderStreamLocked() RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_render_);

  void WriteAecDumpConfigMessage(bool forced)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void RecordUnprocessedCaptureStream(const float* const* capture_stream)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void RecordUnprocessedCaptureStream(const int16_t* const data,
                                      const StreamConfig& config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void RecordProcessedCaptureStream(
      const float* const* processed_capture_stream)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void RecordProcessedCaptureStream(const int16_t* const data,
                                    const StreamConfig& config)
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void RecordAudioProcessingState()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  void HandleOverrunInCaptureRuntimeSettingsQueue()
      RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_capture_);

  std::unique_ptr<AecDump> aec_dump_;

  InternalAPMConfig apm_config_for_aec_dump_ RTC_GUARDED_BY(mutex_capture_);

  mutable Mutex mutex_render_ RTC_ACQUIRED_BEFORE(mutex_capture_);
  mutable Mutex mutex_capture_;

  AudioProcessing::Config config_;

  ApmSubmoduleCreationOverrides submodule_creation_overrides_
      RTC_GUARDED_BY(mutex_capture_);

  SubmoduleStates submodule_states_;

  struct Submodules {
    Submodules(std::unique_ptr<CustomProcessing> capture_post_processor,
               std::unique_ptr<CustomProcessing> render_pre_processor,
               rtc::scoped_refptr<EchoDetector> echo_detector,
               std::unique_ptr<CustomAudioAnalyzer> capture_analyzer)
        : echo_detector(std::move(echo_detector)),
          capture_post_processor(std::move(capture_post_processor)),
          render_pre_processor(std::move(render_pre_processor)),
          capture_analyzer(std::move(capture_analyzer)) {}

    const rtc::scoped_refptr<EchoDetector> echo_detector;
    const std::unique_ptr<CustomProcessing> capture_post_processor;
    const std::unique_ptr<CustomProcessing> render_pre_processor;
    const std::unique_ptr<CustomAudioAnalyzer> capture_analyzer;
    std::unique_ptr<AgcManagerDirect> agc_manager;
    std::unique_ptr<GainControlImpl> gain_control;
    std::unique_ptr<GainController2> gain_controller2;
    std::unique_ptr<VoiceActivityDetectorWrapper> voice_activity_detector;
    std::unique_ptr<HighPassFilter> high_pass_filter;
    std::unique_ptr<EchoControl> echo_controller;
    std::unique_ptr<EchoControlMobileImpl> echo_control_mobile;
    std::unique_ptr<NoiseSuppressor> noise_suppressor;
    std::unique_ptr<TransientSuppressor> transient_suppressor;
    std::unique_ptr<CaptureLevelsAdjuster> capture_levels_adjuster;
  } submodules_;

  struct ApmFormatState {
    ApmFormatState()
        :
          api_format({{{kSampleRate16kHz, 1},
                       {kSampleRate16kHz, 1},
                       {kSampleRate16kHz, 1},
                       {kSampleRate16kHz, 1}}}),
          render_processing_format(kSampleRate16kHz, 1) {}
    ProcessingConfig api_format;
    StreamConfig render_processing_format;
  } formats_;

  const struct ApmConstants {
    ApmConstants(bool multi_channel_render_support,
                 bool multi_channel_capture_support,
                 bool enforce_split_band_hpf,
                 bool minimize_processing_for_unused_output,
                 bool transient_suppressor_forced_off)
        : multi_channel_render_support(multi_channel_render_support),
          multi_channel_capture_support(multi_channel_capture_support),
          enforce_split_band_hpf(enforce_split_band_hpf),
          minimize_processing_for_unused_output(
              minimize_processing_for_unused_output),
          transient_suppressor_forced_off(transient_suppressor_forced_off) {}
    bool multi_channel_render_support;
    bool multi_channel_capture_support;
    bool enforce_split_band_hpf;
    bool minimize_processing_for_unused_output;
    bool transient_suppressor_forced_off;
  } constants_;

  struct ApmCaptureState {
    ApmCaptureState();
    ~ApmCaptureState();
    bool was_stream_delay_set;
    bool capture_output_used;
    bool capture_output_used_last_frame;
    bool key_pressed;
    std::unique_ptr<AudioBuffer> capture_audio;
    std::unique_ptr<AudioBuffer> capture_fullband_audio;
    std::unique_ptr<AudioBuffer> linear_aec_output;

    StreamConfig capture_processing_format;
    int split_rate;
    bool echo_path_gain_change;
    float prev_pre_adjustment_gain;
    int playout_volume;
    int prev_playout_volume;
    AudioProcessingStats stats;

    absl::optional<int> applied_input_volume;
    bool applied_input_volume_changed;

    absl::optional<int> recommended_input_volume;
  } capture_ RTC_GUARDED_BY(mutex_capture_);

  struct ApmCaptureNonLockedState {
    ApmCaptureNonLockedState()
        : capture_processing_format(kSampleRate16kHz),
          split_rate(kSampleRate16kHz),
          stream_delay_ms(0) {}

    StreamConfig capture_processing_format;
    int split_rate;
    int stream_delay_ms;
    bool echo_controller_enabled = false;
  } capture_nonlocked_;

  struct ApmRenderState {
    ApmRenderState();
    ~ApmRenderState();
    std::unique_ptr<AudioConverter> render_converter;
    std::unique_ptr<AudioBuffer> render_audio;
  } render_ RTC_GUARDED_BY(mutex_render_);

  class ApmStatsReporter {
   public:
    ApmStatsReporter();
    ~ApmStatsReporter();

    AudioProcessingStats GetStatistics();

    void UpdateStatistics(const AudioProcessingStats& new_stats);

   private:
    Mutex mutex_stats_;
    AudioProcessingStats cached_stats_ RTC_GUARDED_BY(mutex_stats_);
    SwapQueue<AudioProcessingStats> stats_message_queue_;
  } stats_reporter_;

  std::vector<int16_t> aecm_render_queue_buffer_ RTC_GUARDED_BY(mutex_render_);
  std::vector<int16_t> aecm_capture_queue_buffer_
      RTC_GUARDED_BY(mutex_capture_);

  size_t agc_render_queue_element_max_size_ RTC_GUARDED_BY(mutex_render_)
      RTC_GUARDED_BY(mutex_capture_) = 0;
  std::vector<int16_t> agc_render_queue_buffer_ RTC_GUARDED_BY(mutex_render_);
  std::vector<int16_t> agc_capture_queue_buffer_ RTC_GUARDED_BY(mutex_capture_);

  size_t red_render_queue_element_max_size_ RTC_GUARDED_BY(mutex_render_)
      RTC_GUARDED_BY(mutex_capture_) = 0;
  std::vector<float> red_render_queue_buffer_ RTC_GUARDED_BY(mutex_render_);
  std::vector<float> red_capture_queue_buffer_ RTC_GUARDED_BY(mutex_capture_);

  RmsLevel capture_input_rms_ RTC_GUARDED_BY(mutex_capture_);
  RmsLevel capture_output_rms_ RTC_GUARDED_BY(mutex_capture_);
  int capture_rms_interval_counter_ RTC_GUARDED_BY(mutex_capture_) = 0;

  InputVolumeStatsReporter applied_input_volume_stats_reporter_
      RTC_GUARDED_BY(mutex_capture_);
  InputVolumeStatsReporter recommended_input_volume_stats_reporter_
      RTC_GUARDED_BY(mutex_capture_);

  std::unique_ptr<
      SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>>
      aecm_render_signal_queue_;
  std::unique_ptr<
      SwapQueue<std::vector<int16_t>, RenderQueueItemVerifier<int16_t>>>
      agc_render_signal_queue_;
  std::unique_ptr<SwapQueue<std::vector<float>, RenderQueueItemVerifier<float>>>
      red_render_signal_queue_;
};

}

#endif
