/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_
#define MODULES_AUDIO_PROCESSING_AEC3_AEC_STATE_H_

#include <stddef.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/delay_estimate.h"
#include "modules/audio_processing/aec3/echo_audibility.h"
#include "modules/audio_processing/aec3/echo_path_variability.h"
#include "modules/audio_processing/aec3/erl_estimator.h"
#include "modules/audio_processing/aec3/erle_estimator.h"
#include "modules/audio_processing/aec3/filter_analyzer.h"
#include "modules/audio_processing/aec3/render_buffer.h"
#include "modules/audio_processing/aec3/reverb_model_estimator.h"
#include "modules/audio_processing/aec3/subtractor_output.h"
#include "modules/audio_processing/aec3/subtractor_output_analyzer.h"
#include "modules/audio_processing/aec3/transparent_mode.h"

namespace webrtc {

class ApmDataDumper;

class AecState {
 public:
  AecState(const EchoCanceller3Config& config, size_t num_capture_channels);
  ~AecState();

  bool UsableLinearEstimate() const {
    return filter_quality_state_.LinearFilterUsable() &&
           config_.filter.use_linear_filter;
  }

  bool UseLinearFilterOutput() const {
    return filter_quality_state_.LinearFilterUsable() &&
           config_.filter.use_linear_filter;
  }

  bool ActiveRender() const { return blocks_with_active_render_ > 200; }

  void GetResidualEchoScaling(rtc::ArrayView<float> residual_scaling) const;

  bool UseStationarityProperties() const {
    return config_.echo_audibility.use_stationarity_properties;
  }

  rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> Erle(
      bool onset_compensated) const {
    return erle_estimator_.Erle(onset_compensated);
  }

  rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> ErleUnbounded()
      const {
    return erle_estimator_.ErleUnbounded();
  }

  float FullBandErleLog2() const { return erle_estimator_.FullbandErleLog2(); }

  const std::array<float, kFftLengthBy2Plus1>& Erl() const {
    return erl_estimator_.Erl();
  }

  float ErlTimeDomain() const { return erl_estimator_.ErlTimeDomain(); }

  int MinDirectPathFilterDelay() const {
    return delay_state_.MinDirectPathFilterDelay();
  }

  bool SaturatedCapture() const { return capture_signal_saturation_; }

  bool SaturatedEcho() const { return saturation_detector_.SaturatedEcho(); }

  void UpdateCaptureSaturation(bool capture_signal_saturation) {
    capture_signal_saturation_ = capture_signal_saturation;
  }

  bool TransparentModeActive() const {
    return transparent_state_ && transparent_state_->Active();
  }

  void HandleEchoPathChange(const EchoPathVariability& echo_path_variability);

  float ReverbDecay(bool mild) const {
    return reverb_model_estimator_.ReverbDecay(mild);
  }

  rtc::ArrayView<const float> GetReverbFrequencyResponse() const {
    return reverb_model_estimator_.GetReverbFrequencyResponse();
  }

  bool TransitionTriggered() const {
    return initial_state_.TransitionTriggered();
  }

  void Update(
      const absl::optional<DelayEstimate>& external_delay,
      rtc::ArrayView<const std::vector<std::array<float, kFftLengthBy2Plus1>>>
          adaptive_filter_frequency_responses,
      rtc::ArrayView<const std::vector<float>>
          adaptive_filter_impulse_responses,
      const RenderBuffer& render_buffer,
      rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> E2_refined,
      rtc::ArrayView<const std::array<float, kFftLengthBy2Plus1>> Y2,
      rtc::ArrayView<const SubtractorOutput> subtractor_output);

  int FilterLengthBlocks() const {

    return filter_analyzer_.FilterLengthBlocks();
  }

 private:
  static std::atomic<int> instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const EchoCanceller3Config config_;
  const size_t num_capture_channels_;
  const bool deactivate_initial_state_reset_at_echo_path_change_;
  const bool full_reset_at_echo_path_change_;
  const bool subtractor_analyzer_reset_at_echo_path_change_;

  class InitialState {
   public:
    explicit InitialState(const EchoCanceller3Config& config);

    void Reset();

    void Update(bool active_render, bool saturated_capture);

    bool InitialStateActive() const { return initial_state_; }

    bool TransitionTriggered() const { return transition_triggered_; }

   private:
    const bool conservative_initial_phase_;
    const float initial_state_seconds_;
    bool transition_triggered_ = false;
    bool initial_state_ = true;
    size_t strong_not_saturated_render_blocks_ = 0;
  } initial_state_;

  class FilterDelay {
   public:
    FilterDelay(const EchoCanceller3Config& config,
                size_t num_capture_channels);

    bool ExternalDelayReported() const { return external_delay_reported_; }

    rtc::ArrayView<const int> DirectPathFilterDelays() const {
      return filter_delays_blocks_;
    }

    int MinDirectPathFilterDelay() const { return min_filter_delay_; }

    void Update(
        rtc::ArrayView<const int> analyzer_filter_delay_estimates_blocks,
        const absl::optional<DelayEstimate>& external_delay,
        size_t blocks_with_proper_filter_adaptation);

   private:
    const int delay_headroom_blocks_;
    bool external_delay_reported_ = false;
    std::vector<int> filter_delays_blocks_;
    int min_filter_delay_;
    absl::optional<DelayEstimate> external_delay_;
  } delay_state_;

  std::unique_ptr<TransparentMode> transparent_state_;

  class FilteringQualityAnalyzer {
   public:
    FilteringQualityAnalyzer(const EchoCanceller3Config& config,
                             size_t num_capture_channels);

    bool LinearFilterUsable() const { return overall_usable_linear_estimates_; }

    const std::vector<bool>& UsableLinearFilterOutputs() const {
      return usable_linear_filter_estimates_;
    }

    void Reset();

    void Update(bool active_render,
                bool transparent_mode,
                bool saturated_capture,
                const absl::optional<DelayEstimate>& external_delay,
                bool any_filter_converged);

   private:
    const bool use_linear_filter_;
    bool overall_usable_linear_estimates_ = false;
    size_t filter_update_blocks_since_reset_ = 0;
    size_t filter_update_blocks_since_start_ = 0;
    bool convergence_seen_ = false;
    std::vector<bool> usable_linear_filter_estimates_;
  } filter_quality_state_;

  class SaturationDetector {
   public:

    bool SaturatedEcho() const { return saturated_echo_; }

    void Update(const Block& x,
                bool saturated_capture,
                bool usable_linear_estimate,
                rtc::ArrayView<const SubtractorOutput> subtractor_output,
                float echo_path_gain);

   private:
    bool saturated_echo_ = false;
  } saturation_detector_;

  ErlEstimator erl_estimator_;
  ErleEstimator erle_estimator_;
  size_t strong_not_saturated_render_blocks_ = 0;
  size_t blocks_with_active_render_ = 0;
  bool capture_signal_saturation_ = false;
  FilterAnalyzer filter_analyzer_;
  EchoAudibility echo_audibility_;
  ReverbModelEstimator reverb_model_estimator_;
  ReverbModel avg_render_reverb_;
  SubtractorOutputAnalyzer subtractor_output_analyzer_;
};

}

#endif
