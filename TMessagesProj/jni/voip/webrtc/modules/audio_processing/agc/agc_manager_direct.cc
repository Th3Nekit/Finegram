/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/agc_manager_direct.h"

#include <algorithm>
#include <cmath>

#include "api/array_view.h"
#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc/gain_control.h"
#include "modules/audio_processing/agc2/gain_map_internal.h"
#include "modules/audio_processing/agc2/input_volume_stats_reporter.h"
#include "modules/audio_processing/include/audio_frame_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

namespace {

constexpr int kLevelQuantizationSlack = 25;

constexpr int kDefaultCompressionGain = 7;
constexpr int kMaxCompressionGain = 12;
constexpr int kMinCompressionGain = 2;

constexpr float kCompressionGainStep = 0.05f;

constexpr int kMaxMicLevel = 255;
static_assert(kGainMapSize > kMaxMicLevel, "gain map too small");
constexpr int kMinMicLevel = 12;

constexpr int kMaxResidualGainChange = 15;

constexpr int kSurplusCompressionGain = 6;

constexpr float kOverrideTargetSpeechLevelDbfs = -18.0f;
constexpr float kOverrideSpeechProbabilitySilenceThreshold = 0.5f;

constexpr int kOverrideWaitFrames = 0;

using AnalogAgcConfig =
    AudioProcessing::Config::GainController1::AnalogGainController;

absl::optional<int> GetMinMicLevelOverride() {
  constexpr char kMinMicLevelFieldTrial[] =
      "WebRTC-Audio-2ndAgcMinMicLevelExperiment";
  if (!webrtc::field_trial::IsEnabled(kMinMicLevelFieldTrial)) {
    return absl::nullopt;
  }
  const auto field_trial_string =
      webrtc::field_trial::FindFullName(kMinMicLevelFieldTrial);
  int min_mic_level = -1;
  sscanf(field_trial_string.c_str(), "Enabled-%d", &min_mic_level);
  if (min_mic_level >= 0 && min_mic_level <= 255) {
    return min_mic_level;
  } else {
    RTC_LOG(LS_WARNING) << "[agc] Invalid parameter for "
                        << kMinMicLevelFieldTrial << ", ignored.";
    return absl::nullopt;
  }
}

int LevelFromGainError(int gain_error, int level, int min_mic_level) {
  RTC_DCHECK_GE(level, 0);
  RTC_DCHECK_LE(level, kMaxMicLevel);
  if (gain_error == 0) {
    return level;
  }

  int new_level = level;
  if (gain_error > 0) {
    while (kGainMap[new_level] - kGainMap[level] < gain_error &&
           new_level < kMaxMicLevel) {
      ++new_level;
    }
  } else {
    while (kGainMap[new_level] - kGainMap[level] > gain_error &&
           new_level > min_mic_level) {
      --new_level;
    }
  }
  return new_level;
}

float ComputeClippedRatio(const float* const* audio,
                          size_t num_channels,
                          size_t samples_per_channel) {
  RTC_DCHECK_GT(samples_per_channel, 0);
  int num_clipped = 0;
  for (size_t ch = 0; ch < num_channels; ++ch) {
    int num_clipped_in_ch = 0;
    for (size_t i = 0; i < samples_per_channel; ++i) {
      RTC_DCHECK(audio[ch]);
      if (audio[ch][i] >= 32767.0f || audio[ch][i] <= -32768.0f) {
        ++num_clipped_in_ch;
      }
    }
    num_clipped = std::max(num_clipped, num_clipped_in_ch);
  }
  return static_cast<float>(num_clipped) / (samples_per_channel);
}

void LogClippingMetrics(int clipping_rate) {
  RTC_LOG(LS_INFO) << "Input clipping rate: " << clipping_rate << "%";
  RTC_HISTOGRAM_COUNTS_LINEAR(         "WebRTC.Audio.Agc.InputClippingRate",
                                         clipping_rate,         0,         100,
                                               50);
}

int GetSpeechLevelErrorDb(float speech_level_dbfs, float speech_probability) {
  constexpr float kMinSpeechLevelDbfs = -90.0f;
  constexpr float kMaxSpeechLevelDbfs = 30.0f;
  RTC_DCHECK_GE(speech_level_dbfs, kMinSpeechLevelDbfs);
  RTC_DCHECK_LE(speech_level_dbfs, kMaxSpeechLevelDbfs);
  RTC_DCHECK_GE(speech_probability, 0.0f);
  RTC_DCHECK_LE(speech_probability, 1.0f);

  if (speech_probability < kOverrideSpeechProbabilitySilenceThreshold) {
    return 0;
  }

  const float speech_level = rtc::SafeClamp<float>(
      speech_level_dbfs, kMinSpeechLevelDbfs, kMaxSpeechLevelDbfs);

  return std::round(kOverrideTargetSpeechLevelDbfs - speech_level);
}

}

MonoAgc::MonoAgc(ApmDataDumper* data_dumper,
                 int clipped_level_min,
                 bool disable_digital_adaptive,
                 int min_mic_level)
    : min_mic_level_(min_mic_level),
      disable_digital_adaptive_(disable_digital_adaptive),
      agc_(std::make_unique<Agc>()),
      max_level_(kMaxMicLevel),
      max_compression_gain_(kMaxCompressionGain),
      target_compression_(kDefaultCompressionGain),
      compression_(target_compression_),
      compression_accumulator_(compression_),
      clipped_level_min_(clipped_level_min) {}

MonoAgc::~MonoAgc() = default;

void MonoAgc::Initialize() {
  max_level_ = kMaxMicLevel;
  max_compression_gain_ = kMaxCompressionGain;
  target_compression_ = disable_digital_adaptive_ ? 0 : kDefaultCompressionGain;
  compression_ = disable_digital_adaptive_ ? 0 : target_compression_;
  compression_accumulator_ = compression_;
  capture_output_used_ = true;
  check_volume_on_next_process_ = true;
  frames_since_update_gain_ = 0;
  is_first_frame_ = true;
}

void MonoAgc::Process(rtc::ArrayView<const int16_t> audio,
                      absl::optional<int> rms_error_override) {
  new_compression_to_set_ = absl::nullopt;

  if (check_volume_on_next_process_) {
    check_volume_on_next_process_ = false;

    CheckVolumeAndReset();
  }

  agc_->Process(audio);

  int rms_error = 0;
  bool update_gain = agc_->GetRmsErrorDb(&rms_error);
  if (rms_error_override.has_value()) {
    if (is_first_frame_ || frames_since_update_gain_ < kOverrideWaitFrames) {
      update_gain = false;
    } else {
      rms_error = *rms_error_override;
      update_gain = true;
    }
  }

  if (update_gain) {
    UpdateGain(rms_error);
  }

  if (!disable_digital_adaptive_) {
    UpdateCompressor();
  }

  is_first_frame_ = false;
  if (frames_since_update_gain_ < kOverrideWaitFrames) {
    ++frames_since_update_gain_;
  }
}

void MonoAgc::HandleClipping(int clipped_level_step) {
  RTC_DCHECK_GT(clipped_level_step, 0);

  SetMaxLevel(std::max(clipped_level_min_, max_level_ - clipped_level_step));
  if (log_to_histograms_) {
    RTC_HISTOGRAM_BOOLEAN("WebRTC.Audio.AgcClippingAdjustmentAllowed",
                          level_ - clipped_level_step >= clipped_level_min_);
  }
  if (level_ > clipped_level_min_) {

    SetLevel(std::max(clipped_level_min_, level_ - clipped_level_step));

    agc_->Reset();
    frames_since_update_gain_ = 0;
    is_first_frame_ = false;
  }
}

void MonoAgc::SetLevel(int new_level) {
  int voe_level = recommended_input_volume_;
  if (voe_level == 0) {
    RTC_DLOG(LS_INFO)
        << "[agc] VolumeCallbacks returned level=0, taking no action.";
    return;
  }
  if (voe_level < 0 || voe_level > kMaxMicLevel) {
    RTC_LOG(LS_ERROR) << "VolumeCallbacks returned an invalid level="
                      << voe_level;
    return;
  }

  if (voe_level > level_ + kLevelQuantizationSlack ||
      voe_level < level_ - kLevelQuantizationSlack) {
    RTC_DLOG(LS_INFO) << "[agc] Mic volume was manually adjusted. Updating "
                         "stored level from "
                      << level_ << " to " << voe_level;
    level_ = voe_level;

    if (level_ > max_level_) {
      SetMaxLevel(level_);
    }

    agc_->Reset();
    frames_since_update_gain_ = 0;
    is_first_frame_ = false;
    return;
  }

  new_level = std::min(new_level, max_level_);
  if (new_level == level_) {
    return;
  }

  recommended_input_volume_ = new_level;
  RTC_DLOG(LS_INFO) << "[agc] voe_level=" << voe_level << ", level_=" << level_
                    << ", new_level=" << new_level;
  level_ = new_level;
}

void MonoAgc::SetMaxLevel(int level) {
  RTC_DCHECK_GE(level, clipped_level_min_);
  max_level_ = level;

  max_compression_gain_ =
      kMaxCompressionGain + std::floor((1.f * kMaxMicLevel - max_level_) /
                                           (kMaxMicLevel - clipped_level_min_) *
                                           kSurplusCompressionGain +
                                       0.5f);
  RTC_DLOG(LS_INFO) << "[agc] max_level_=" << max_level_
                    << ", max_compression_gain_=" << max_compression_gain_;
}

void MonoAgc::HandleCaptureOutputUsedChange(bool capture_output_used) {
  if (capture_output_used_ == capture_output_used) {
    return;
  }
  capture_output_used_ = capture_output_used;

  if (capture_output_used) {

    check_volume_on_next_process_ = true;
  }
}

int MonoAgc::CheckVolumeAndReset() {
  int level = recommended_input_volume_;

  if (level == 0 && !startup_) {
    RTC_DLOG(LS_INFO)
        << "[agc] VolumeCallbacks returned level=0, taking no action.";
    return 0;
  }
  if (level < 0 || level > kMaxMicLevel) {
    RTC_LOG(LS_ERROR) << "[agc] VolumeCallbacks returned an invalid level="
                      << level;
    return -1;
  }
  RTC_DLOG(LS_INFO) << "[agc] Initial GetMicVolume()=" << level;

  if (level < min_mic_level_) {
    level = min_mic_level_;
    RTC_DLOG(LS_INFO) << "[agc] Initial volume too low, raising to " << level;
    recommended_input_volume_ = level;
  }
  agc_->Reset();
  level_ = level;
  startup_ = false;
  frames_since_update_gain_ = 0;
  is_first_frame_ = true;
  return 0;
}

void MonoAgc::UpdateGain(int rms_error_db) {
  int rms_error = rms_error_db;

  frames_since_update_gain_ = 0;

  rms_error += kMinCompressionGain;

  int raw_compression =
      rtc::SafeClamp(rms_error, kMinCompressionGain, max_compression_gain_);

  if ((raw_compression == max_compression_gain_ &&
       target_compression_ == max_compression_gain_ - 1) ||
      (raw_compression == kMinCompressionGain &&
       target_compression_ == kMinCompressionGain + 1)) {

    target_compression_ = raw_compression;
  } else {
    target_compression_ =
        (raw_compression - target_compression_) / 2 + target_compression_;
  }

  const int residual_gain =
      rtc::SafeClamp(rms_error - raw_compression, -kMaxResidualGainChange,
                     kMaxResidualGainChange);
  RTC_DLOG(LS_INFO) << "[agc] rms_error=" << rms_error
                    << ", target_compression=" << target_compression_
                    << ", residual_gain=" << residual_gain;
  if (residual_gain == 0)
    return;

  int old_level = level_;
  SetLevel(LevelFromGainError(residual_gain, level_, min_mic_level_));
  if (old_level != level_) {

    agc_->Reset();
  }
}

void MonoAgc::UpdateCompressor() {
  if (compression_ == target_compression_) {
    return;
  }

  if (target_compression_ > compression_) {
    compression_accumulator_ += kCompressionGainStep;
  } else {
    compression_accumulator_ -= kCompressionGainStep;
  }

  int new_compression = compression_;
  int nearest_neighbor = std::floor(compression_accumulator_ + 0.5);
  if (std::fabs(compression_accumulator_ - nearest_neighbor) <
      kCompressionGainStep / 2) {
    new_compression = nearest_neighbor;
  }

  if (new_compression != compression_) {
    compression_ = new_compression;
    compression_accumulator_ = new_compression;
    new_compression_to_set_ = compression_;
  }
}

std::atomic<int> AgcManagerDirect::instance_counter_(0);

AgcManagerDirect::AgcManagerDirect(
    const AudioProcessing::Config::GainController1::AnalogGainController&
        analog_config,
    Agc* agc)
    : AgcManagerDirect(                         1, analog_config) {
  RTC_DCHECK(channel_agcs_[0]);
  RTC_DCHECK(agc);
  channel_agcs_[0]->set_agc(agc);
}

AgcManagerDirect::AgcManagerDirect(int num_capture_channels,
                                   const AnalogAgcConfig& analog_config)
    : analog_controller_enabled_(analog_config.enabled),
      min_mic_level_override_(GetMinMicLevelOverride()),
      data_dumper_(new ApmDataDumper(instance_counter_.fetch_add(1) + 1)),
      num_capture_channels_(num_capture_channels),
      disable_digital_adaptive_(!analog_config.enable_digital_adaptive),
      frames_since_clipped_(analog_config.clipped_wait_frames),
      capture_output_used_(true),
      clipped_level_step_(analog_config.clipped_level_step),
      clipped_ratio_threshold_(analog_config.clipped_ratio_threshold),
      clipped_wait_frames_(analog_config.clipped_wait_frames),
      channel_agcs_(num_capture_channels),
      new_compressions_to_set_(num_capture_channels),
      clipping_predictor_(
          CreateClippingPredictor(num_capture_channels,
                                  analog_config.clipping_predictor)),
      use_clipping_predictor_step_(
          !!clipping_predictor_ &&
          analog_config.clipping_predictor.use_predicted_step),
      clipping_rate_log_(0.0f),
      clipping_rate_log_counter_(0) {
  RTC_LOG(LS_INFO) << "[agc] analog controller enabled: "
                   << (analog_controller_enabled_ ? "yes" : "no");
  const int min_mic_level = min_mic_level_override_.value_or(kMinMicLevel);
  RTC_LOG(LS_INFO) << "[agc] Min mic level: " << min_mic_level
                   << " (overridden: "
                   << (min_mic_level_override_.has_value() ? "yes" : "no")
                   << ")";
  for (size_t ch = 0; ch < channel_agcs_.size(); ++ch) {
    ApmDataDumper* data_dumper_ch = ch == 0 ? data_dumper_.get() : nullptr;

    channel_agcs_[ch] = std::make_unique<MonoAgc>(
        data_dumper_ch, analog_config.clipped_level_min,
        disable_digital_adaptive_, min_mic_level);
  }
  RTC_DCHECK(!channel_agcs_.empty());
  RTC_DCHECK_GT(clipped_level_step_, 0);
  RTC_DCHECK_LE(clipped_level_step_, 255);
  RTC_DCHECK_GT(clipped_ratio_threshold_, 0.0f);
  RTC_DCHECK_LT(clipped_ratio_threshold_, 1.0f);
  RTC_DCHECK_GT(clipped_wait_frames_, 0);
  channel_agcs_[0]->ActivateLogging();
}

AgcManagerDirect::~AgcManagerDirect() {}

void AgcManagerDirect::Initialize() {
  RTC_DLOG(LS_INFO) << "AgcManagerDirect::Initialize";
  data_dumper_->InitiateNewSetOfRecordings();
  for (size_t ch = 0; ch < channel_agcs_.size(); ++ch) {
    channel_agcs_[ch]->Initialize();
  }
  capture_output_used_ = true;

  AggregateChannelLevels();
  clipping_rate_log_ = 0.0f;
  clipping_rate_log_counter_ = 0;
}

void AgcManagerDirect::SetupDigitalGainControl(
    GainControl& gain_control) const {
  if (gain_control.set_mode(GainControl::kFixedDigital) != 0) {
    RTC_LOG(LS_ERROR) << "set_mode(GainControl::kFixedDigital) failed.";
  }
  const int target_level_dbfs = disable_digital_adaptive_ ? 0 : 2;
  if (gain_control.set_target_level_dbfs(target_level_dbfs) != 0) {
    RTC_LOG(LS_ERROR) << "set_target_level_dbfs() failed.";
  }
  const int compression_gain_db =
      disable_digital_adaptive_ ? 0 : kDefaultCompressionGain;
  if (gain_control.set_compression_gain_db(compression_gain_db) != 0) {
    RTC_LOG(LS_ERROR) << "set_compression_gain_db() failed.";
  }
  const bool enable_limiter = !disable_digital_adaptive_;
  if (gain_control.enable_limiter(enable_limiter) != 0) {
    RTC_LOG(LS_ERROR) << "enable_limiter() failed.";
  }
}

void AgcManagerDirect::AnalyzePreProcess(const AudioBuffer& audio_buffer) {
  const float* const* audio = audio_buffer.channels_const();
  size_t samples_per_channel = audio_buffer.num_frames();
  RTC_DCHECK(audio);

  AggregateChannelLevels();
  if (!capture_output_used_) {
    return;
  }

  if (!!clipping_predictor_) {
    AudioFrameView<const float> frame = AudioFrameView<const float>(
        audio, num_capture_channels_, static_cast<int>(samples_per_channel));
    clipping_predictor_->Analyze(frame);
  }

  float clipped_ratio =
      ComputeClippedRatio(audio, num_capture_channels_, samples_per_channel);
  clipping_rate_log_ = std::max(clipped_ratio, clipping_rate_log_);
  clipping_rate_log_counter_++;
  constexpr int kNumFramesIn30Seconds = 3000;
  if (clipping_rate_log_counter_ == kNumFramesIn30Seconds) {
    LogClippingMetrics(std::round(100.0f * clipping_rate_log_));
    clipping_rate_log_ = 0.0f;
    clipping_rate_log_counter_ = 0;
  }

  if (frames_since_clipped_ < clipped_wait_frames_) {
    ++frames_since_clipped_;
    return;
  }

  const bool clipping_detected = clipped_ratio > clipped_ratio_threshold_;
  bool clipping_predicted = false;
  int predicted_step = 0;
  if (!!clipping_predictor_) {
    for (int channel = 0; channel < num_capture_channels_; ++channel) {
      const auto step = clipping_predictor_->EstimateClippedLevelStep(
          channel, recommended_input_volume_, clipped_level_step_,
          channel_agcs_[channel]->min_mic_level(), kMaxMicLevel);
      if (step.has_value()) {
        predicted_step = std::max(predicted_step, step.value());
        clipping_predicted = true;
      }
    }
  }
  if (clipping_detected) {
    RTC_DLOG(LS_INFO) << "[agc] Clipping detected. clipped_ratio="
                      << clipped_ratio;
  }
  int step = clipped_level_step_;
  if (clipping_predicted) {
    predicted_step = std::max(predicted_step, clipped_level_step_);
    RTC_DLOG(LS_INFO) << "[agc] Clipping predicted. step=" << predicted_step;
    if (use_clipping_predictor_step_) {
      step = predicted_step;
    }
  }
  if (clipping_detected ||
      (clipping_predicted && use_clipping_predictor_step_)) {
    for (auto& state_ch : channel_agcs_) {
      state_ch->HandleClipping(step);
    }
    frames_since_clipped_ = 0;
    if (!!clipping_predictor_) {
      clipping_predictor_->Reset();
    }
  }
  AggregateChannelLevels();
}

void AgcManagerDirect::Process(const AudioBuffer& audio_buffer) {
  Process(audio_buffer,                        absl::nullopt,
                                absl::nullopt);
}

void AgcManagerDirect::Process(const AudioBuffer& audio_buffer,
                               absl::optional<float> speech_probability,
                               absl::optional<float> speech_level_dbfs) {
  AggregateChannelLevels();
  const int volume_after_clipping_handling = recommended_input_volume_;

  if (!capture_output_used_) {
    return;
  }

  const size_t num_frames_per_band = audio_buffer.num_frames_per_band();
  absl::optional<int> rms_error_override = absl::nullopt;
  if (speech_probability.has_value() && speech_level_dbfs.has_value()) {
    rms_error_override =
        GetSpeechLevelErrorDb(*speech_level_dbfs, *speech_probability);
  }
  for (size_t ch = 0; ch < channel_agcs_.size(); ++ch) {
    std::array<int16_t, AudioBuffer::kMaxSampleRate / 100> audio_data;
    int16_t* audio_use = audio_data.data();
    FloatS16ToS16(audio_buffer.split_bands_const_f(ch)[0], num_frames_per_band,
                  audio_use);
    channel_agcs_[ch]->Process({audio_use, num_frames_per_band},
                               rms_error_override);
    new_compressions_to_set_[ch] = channel_agcs_[ch]->new_compression();
  }

  AggregateChannelLevels();
  if (volume_after_clipping_handling != recommended_input_volume_) {

    UpdateHistogramOnRecommendedInputVolumeChangeToMatchTarget(
        recommended_input_volume_);
  }
}

absl::optional<int> AgcManagerDirect::GetDigitalComressionGain() {
  return new_compressions_to_set_[channel_controlling_gain_];
}

void AgcManagerDirect::HandleCaptureOutputUsedChange(bool capture_output_used) {
  for (size_t ch = 0; ch < channel_agcs_.size(); ++ch) {
    channel_agcs_[ch]->HandleCaptureOutputUsedChange(capture_output_used);
  }
  capture_output_used_ = capture_output_used;
}

float AgcManagerDirect::voice_probability() const {
  float max_prob = 0.f;
  for (const auto& state_ch : channel_agcs_) {
    max_prob = std::max(max_prob, state_ch->voice_probability());
  }

  return max_prob;
}

void AgcManagerDirect::set_stream_analog_level(int level) {
  if (!analog_controller_enabled_) {
    recommended_input_volume_ = level;
  }

  for (size_t ch = 0; ch < channel_agcs_.size(); ++ch) {
    channel_agcs_[ch]->set_stream_analog_level(level);
  }

  AggregateChannelLevels();
}

void AgcManagerDirect::AggregateChannelLevels() {
  int new_recommended_input_volume =
      channel_agcs_[0]->recommended_analog_level();
  channel_controlling_gain_ = 0;
  for (size_t ch = 1; ch < channel_agcs_.size(); ++ch) {
    int level = channel_agcs_[ch]->recommended_analog_level();
    if (level < new_recommended_input_volume) {
      new_recommended_input_volume = level;
      channel_controlling_gain_ = static_cast<int>(ch);
    }
  }

  if (min_mic_level_override_.has_value() && new_recommended_input_volume > 0) {
    new_recommended_input_volume =
        std::max(new_recommended_input_volume, *min_mic_level_override_);
  }

  if (analog_controller_enabled_) {
    recommended_input_volume_ = new_recommended_input_volume;
  }
}

}
