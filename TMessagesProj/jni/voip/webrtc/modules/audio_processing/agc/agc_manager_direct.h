/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC_AGC_MANAGER_DIRECT_H_
#define MODULES_AUDIO_PROCESSING_AGC_AGC_MANAGER_DIRECT_H_

#include <atomic>
#include <memory>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/agc/agc.h"
#include "modules/audio_processing/agc2/clipping_predictor.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gtest_prod_util.h"

namespace webrtc {

class MonoAgc;
class GainControl;

class AgcManagerDirect final {
 public:

  AgcManagerDirect(
      int num_capture_channels,
      const AudioProcessing::Config::GainController1::AnalogGainController&
          analog_config);

  ~AgcManagerDirect();
  AgcManagerDirect(const AgcManagerDirect&) = delete;
  AgcManagerDirect& operator=(const AgcManagerDirect&) = delete;

  void Initialize();

  void SetupDigitalGainControl(GainControl& gain_control) const;

  void set_stream_analog_level(int level);

  void AnalyzePreProcess(const AudioBuffer& audio_buffer);

  void Process(const AudioBuffer& audio_buffer,
               absl::optional<float> speech_probability,
               absl::optional<float> speech_level_dbfs);

  void Process(const AudioBuffer& audio_buffer);

  int recommended_analog_level() const { return recommended_input_volume_; }

  void HandleCaptureOutputUsedChange(bool capture_output_used);

  float voice_probability() const;

  int num_channels() const { return num_capture_channels_; }

  absl::optional<int> GetDigitalComressionGain();

  bool clipping_predictor_enabled() const { return !!clipping_predictor_; }

  bool use_clipping_predictor_step() const {
    return use_clipping_predictor_step_;
  }

 private:
  friend class AgcManagerDirectTestHelper;

  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest, DisableDigitalDisablesDigital);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentDefault);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentDisabled);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentOutOfRangeAbove);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentOutOfRangeBelow);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentEnabled50);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectTest,
                           AgcMinMicLevelExperimentEnabledAboveStartupLevel);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           ClippingParametersVerified);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           DisableClippingPredictorDoesNotLowerVolume);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           UsedClippingPredictionsProduceLowerAnalogLevels);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           UnusedClippingPredictionsProduceEqualAnalogLevels);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           EmptyRmsErrorOverrideHasNoEffect);
  FRIEND_TEST_ALL_PREFIXES(AgcManagerDirectParametrizedTest,
                           NonEmptyRmsErrorOverrideHasEffect);

  AgcManagerDirect(
      const AudioProcessing::Config::GainController1::AnalogGainController&
          analog_config,
      Agc* agc);

  void AggregateChannelLevels();

  const bool analog_controller_enabled_;

  const absl::optional<int> min_mic_level_override_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  static std::atomic<int> instance_counter_;
  const int num_capture_channels_;
  const bool disable_digital_adaptive_;

  int frames_since_clipped_;

  int recommended_input_volume_ = 0;

  bool capture_output_used_;
  int channel_controlling_gain_ = 0;

  const int clipped_level_step_;
  const float clipped_ratio_threshold_;
  const int clipped_wait_frames_;

  std::vector<std::unique_ptr<MonoAgc>> channel_agcs_;
  std::vector<absl::optional<int>> new_compressions_to_set_;

  const std::unique_ptr<ClippingPredictor> clipping_predictor_;
  const bool use_clipping_predictor_step_;
  float clipping_rate_log_;
  int clipping_rate_log_counter_;
};

class MonoAgc {
 public:
  MonoAgc(ApmDataDumper* data_dumper,
          int clipped_level_min,
          bool disable_digital_adaptive,
          int min_mic_level);
  ~MonoAgc();
  MonoAgc(const MonoAgc&) = delete;
  MonoAgc& operator=(const MonoAgc&) = delete;

  void Initialize();
  void HandleCaptureOutputUsedChange(bool capture_output_used);

  void set_stream_analog_level(int level) { recommended_input_volume_ = level; }

  void HandleClipping(int clipped_level_step);

  void Process(rtc::ArrayView<const int16_t> audio,
               absl::optional<int> rms_error_override);

  int recommended_analog_level() const { return recommended_input_volume_; }

  float voice_probability() const { return agc_->voice_probability(); }
  void ActivateLogging() { log_to_histograms_ = true; }
  absl::optional<int> new_compression() const {
    return new_compression_to_set_;
  }

  void set_agc(Agc* agc) { agc_.reset(agc); }
  int min_mic_level() const { return min_mic_level_; }

 private:

  void SetLevel(int new_level);

  void SetMaxLevel(int level);

  int CheckVolumeAndReset();
  void UpdateGain(int rms_error_db);
  void UpdateCompressor();

  const int min_mic_level_;
  const bool disable_digital_adaptive_;
  std::unique_ptr<Agc> agc_;
  int level_ = 0;
  int max_level_;
  int max_compression_gain_;
  int target_compression_;
  int compression_;
  float compression_accumulator_;
  bool capture_output_used_ = true;
  bool check_volume_on_next_process_ = true;
  bool startup_ = true;

  int recommended_input_volume_ = 0;

  absl::optional<int> new_compression_to_set_;
  bool log_to_histograms_ = false;
  const int clipped_level_min_;

  int frames_since_update_gain_ = 0;

  bool is_first_frame_ = true;
};

}

#endif
