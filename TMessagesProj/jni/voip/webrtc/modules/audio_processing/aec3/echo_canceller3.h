/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_ECHO_CANCELLER3_H_
#define MODULES_AUDIO_PROCESSING_AEC3_ECHO_CANCELLER3_H_

#include <stddef.h>

#include <atomic>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/echo_control.h"
#include "modules/audio_processing/aec3/api_call_jitter_metrics.h"
#include "modules/audio_processing/aec3/block_delay_buffer.h"
#include "modules/audio_processing/aec3/block_framer.h"
#include "modules/audio_processing/aec3/block_processor.h"
#include "modules/audio_processing/aec3/config_selector.h"
#include "modules/audio_processing/aec3/frame_blocker.h"
#include "modules/audio_processing/aec3/multi_channel_content_detector.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/race_checker.h"
#include "rtc_base/swap_queue.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

EchoCanceller3Config AdjustConfig(const EchoCanceller3Config& config);

class Aec3RenderQueueItemVerifier {
 public:
  Aec3RenderQueueItemVerifier(size_t num_bands,
                              size_t num_channels,
                              size_t frame_length)
      : num_bands_(num_bands),
        num_channels_(num_channels),
        frame_length_(frame_length) {}

  bool operator()(const std::vector<std::vector<std::vector<float>>>& v) const {
    if (v.size() != num_bands_) {
      return false;
    }
    for (const auto& band : v) {
      if (band.size() != num_channels_) {
        return false;
      }
      for (const auto& channel : band) {
        if (channel.size() != frame_length_) {
          return false;
        }
      }
    }
    return true;
  }

 private:
  const size_t num_bands_;
  const size_t num_channels_;
  const size_t frame_length_;
};

class EchoCanceller3 : public EchoControl {
 public:
  EchoCanceller3(
      const EchoCanceller3Config& config,
      const absl::optional<EchoCanceller3Config>& multichannel_config,
      int sample_rate_hz,
      size_t num_render_channels,
      size_t num_capture_channels);

  ~EchoCanceller3() override;

  EchoCanceller3(const EchoCanceller3&) = delete;
  EchoCanceller3& operator=(const EchoCanceller3&) = delete;

  void AnalyzeRender(AudioBuffer* render) override { AnalyzeRender(*render); }

  void AnalyzeCapture(AudioBuffer* capture) override {
    AnalyzeCapture(*capture);
  }

  void ProcessCapture(AudioBuffer* capture, bool level_change) override;

  void ProcessCapture(AudioBuffer* capture,
                      AudioBuffer* linear_output,
                      bool level_change) override;

  Metrics GetMetrics() const override;

  void SetAudioBufferDelay(int delay_ms) override;

  void SetCaptureOutputUsage(bool capture_output_used) override;

  bool ActiveProcessing() const override;

  void UpdateEchoLeakageStatus(bool leakage_detected) {
    RTC_DCHECK_RUNS_SERIALIZED(&capture_race_checker_);
    block_processor_->UpdateEchoLeakageStatus(leakage_detected);
  }

  static EchoCanceller3Config CreateDefaultMultichannelConfig();

 private:
  friend class EchoCanceller3Tester;
  FRIEND_TEST_ALL_PREFIXES(EchoCanceller3, DetectionOfProperStereo);
  FRIEND_TEST_ALL_PREFIXES(EchoCanceller3,
                           DetectionOfProperStereoUsingThreshold);
  FRIEND_TEST_ALL_PREFIXES(EchoCanceller3,
                           DetectionOfProperStereoUsingHysteresis);
  FRIEND_TEST_ALL_PREFIXES(EchoCanceller3,
                           StereoContentDetectionForMonoSignals);

  class RenderWriter;

  void Initialize();

  void SetBlockProcessorForTesting(
      std::unique_ptr<BlockProcessor> block_processor);

  bool StereoRenderProcessingActiveForTesting() const {
    return multichannel_content_detector_.IsProperMultiChannelContentDetected();
  }

  const EchoCanceller3Config& GetActiveConfigForTesting() const {
    return config_selector_.active_config();
  }

  void EmptyRenderQueue();

  void AnalyzeRender(const AudioBuffer& render);

  void AnalyzeCapture(const AudioBuffer& capture);

  rtc::RaceChecker capture_race_checker_;
  rtc::RaceChecker render_race_checker_;

  std::unique_ptr<RenderWriter> render_writer_
      RTC_GUARDED_BY(render_race_checker_);

  static std::atomic<int> instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const EchoCanceller3Config config_;
  const int sample_rate_hz_;
  const int num_bands_;
  const size_t num_render_input_channels_;
  size_t num_render_channels_to_aec_;
  const size_t num_capture_channels_;
  ConfigSelector config_selector_;
  MultiChannelContentDetector multichannel_content_detector_;
  std::unique_ptr<BlockFramer> linear_output_framer_
      RTC_GUARDED_BY(capture_race_checker_);
  BlockFramer output_framer_ RTC_GUARDED_BY(capture_race_checker_);
  FrameBlocker capture_blocker_ RTC_GUARDED_BY(capture_race_checker_);
  std::unique_ptr<FrameBlocker> render_blocker_
      RTC_GUARDED_BY(capture_race_checker_);
  SwapQueue<std::vector<std::vector<std::vector<float>>>,
            Aec3RenderQueueItemVerifier>
      render_transfer_queue_;
  std::unique_ptr<BlockProcessor> block_processor_
      RTC_GUARDED_BY(capture_race_checker_);
  std::vector<std::vector<std::vector<float>>> render_queue_output_frame_
      RTC_GUARDED_BY(capture_race_checker_);
  bool saturated_microphone_signal_ RTC_GUARDED_BY(capture_race_checker_) =
      false;
  Block render_block_ RTC_GUARDED_BY(capture_race_checker_);
  std::unique_ptr<Block> linear_output_block_
      RTC_GUARDED_BY(capture_race_checker_);
  Block capture_block_ RTC_GUARDED_BY(capture_race_checker_);
  std::vector<std::vector<rtc::ArrayView<float>>> render_sub_frame_view_
      RTC_GUARDED_BY(capture_race_checker_);
  std::vector<std::vector<rtc::ArrayView<float>>> linear_output_sub_frame_view_
      RTC_GUARDED_BY(capture_race_checker_);
  std::vector<std::vector<rtc::ArrayView<float>>> capture_sub_frame_view_
      RTC_GUARDED_BY(capture_race_checker_);
  std::unique_ptr<BlockDelayBuffer> block_delay_buffer_
      RTC_GUARDED_BY(capture_race_checker_);
  ApiCallJitterMetrics api_call_metrics_ RTC_GUARDED_BY(capture_race_checker_);
};
}

#endif
