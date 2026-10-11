/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_INPUT_VOLUME_CONTROLLER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_INPUT_VOLUME_CONTROLLER_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "modules/audio_processing/agc2/clipping_predictor.h"
#include "modules/audio_processing/audio_buffer.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "rtc_base/gtest_prod_util.h"

namespace webrtc {

class MonoInputVolumeController;

class InputVolumeController final {
 public:

  struct Config {

    int min_input_volume = 20;

    int clipped_level_min = 70;

    int clipped_level_step = 15;

    float clipped_ratio_threshold = 0.1f;

    int clipped_wait_frames = 300;

    bool enable_clipping_predictor = true;

    int target_range_max_dbfs = -30;
    int target_range_min_dbfs = -50;

    int update_input_volume_wait_frames = 100;

    float speech_probability_threshold = 0.7f;

    float speech_ratio_threshold = 0.6f;
  };

  InputVolumeController(int num_capture_channels, const Config& config);

  ~InputVolumeController();
  InputVolumeController(const InputVolumeController&) = delete;
  InputVolumeController& operator=(const InputVolumeController&) = delete;

  void Initialize();

  void AnalyzeInputAudio(int applied_input_volume,
                         const AudioBuffer& audio_buffer);

  absl::optional<int> RecommendInputVolume(
      float speech_probability,
      absl::optional<float> speech_level_dbfs);

  void HandleCaptureOutputUsedChange(bool capture_output_used);

  bool clipping_predictor_enabled() const { return !!clipping_predictor_; }

  bool use_clipping_predictor_step() const {
    return use_clipping_predictor_step_;
  }

  int recommended_input_volume() const { return recommended_input_volume_; }

  bool capture_output_used() const { return capture_output_used_; }

 private:
  friend class InputVolumeControllerTestHelper;

  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest, MinInputVolumeDefault);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest, MinInputVolumeDisabled);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           MinInputVolumeOutOfRangeAbove);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest,
                           MinInputVolumeOutOfRangeBelow);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerTest, MinInputVolumeEnabled50);
  FRIEND_TEST_ALL_PREFIXES(InputVolumeControllerParametrizedTest,
                           ClippingParametersVerified);

  void SetAppliedInputVolume(int level);

  void AggregateChannelLevels();

  const int num_capture_channels_;

  const int min_input_volume_;

  int recommended_input_volume_ = 0;

  absl::optional<int> applied_input_volume_;

  bool capture_output_used_;

  const int clipped_level_step_;
  const float clipped_ratio_threshold_;
  const int clipped_wait_frames_;
  const std::unique_ptr<ClippingPredictor> clipping_predictor_;
  const bool use_clipping_predictor_step_;
  int frames_since_clipped_;
  int clipping_rate_log_counter_;
  float clipping_rate_log_;

  const int target_range_max_dbfs_;
  const int target_range_min_dbfs_;

  std::vector<std::unique_ptr<MonoInputVolumeController>> channel_controllers_;
  int channel_controlling_gain_ = 0;
};

class MonoInputVolumeController {
 public:
  MonoInputVolumeController(int min_input_volume_after_clipping,
                            int min_input_volume,
                            int update_input_volume_wait_frames,
                            float speech_probability_threshold,
                            float speech_ratio_threshold);
  ~MonoInputVolumeController();
  MonoInputVolumeController(const MonoInputVolumeController&) = delete;
  MonoInputVolumeController& operator=(const MonoInputVolumeController&) =
      delete;

  void Initialize();
  void HandleCaptureOutputUsedChange(bool capture_output_used);

  void set_stream_analog_level(int input_volume) {
    recommended_input_volume_ = input_volume;
  }

  void HandleClipping(int clipped_level_step);

  void Process(absl::optional<int> rms_error_dbfs, float speech_probability);

  int recommended_analog_level() const { return recommended_input_volume_; }

  void ActivateLogging() { log_to_histograms_ = true; }

  int min_input_volume_after_clipping() const {
    return min_input_volume_after_clipping_;
  }

  int min_input_volume() const { return min_input_volume_; }

 private:

  void SetInputVolume(int new_volume);

  void SetMaxLevel(int level);

  int CheckVolumeAndReset();

  void UpdateInputVolume(int rms_error_dbfs);

  const int min_input_volume_;
  const int min_input_volume_after_clipping_;
  int max_input_volume_;

  int last_recommended_input_volume_ = 0;

  bool capture_output_used_ = true;
  bool check_volume_on_next_process_ = true;
  bool startup_ = true;

  int recommended_input_volume_ = 0;

  bool log_to_histograms_ = false;

  const int update_input_volume_wait_frames_;
  int frames_since_update_input_volume_ = 0;
  int speech_frames_since_update_input_volume_ = 0;
  bool is_first_frame_ = true;

  const float speech_probability_threshold_;

  const float speech_ratio_threshold_;
};

}

#endif
