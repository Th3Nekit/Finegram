/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/speech_level_estimator.h"

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

float ClampLevelEstimateDbfs(float level_estimate_dbfs) {
  return rtc::SafeClamp<float>(level_estimate_dbfs, -90.0f, 30.0f);
}

float GetInitialSpeechLevelEstimateDbfs(
    const AudioProcessing::Config::GainController2::AdaptiveDigital& config) {
  return ClampLevelEstimateDbfs(-kSaturationProtectorInitialHeadroomDb -
                                config.initial_gain_db - config.headroom_db);
}

}

bool SpeechLevelEstimator::LevelEstimatorState::operator==(
    const SpeechLevelEstimator::LevelEstimatorState& b) const {
  return time_to_confidence_ms == b.time_to_confidence_ms &&
         level_dbfs.numerator == b.level_dbfs.numerator &&
         level_dbfs.denominator == b.level_dbfs.denominator;
}

float SpeechLevelEstimator::LevelEstimatorState::Ratio::GetRatio() const {
  RTC_DCHECK_NE(denominator, 0.f);
  return numerator / denominator;
}

SpeechLevelEstimator::SpeechLevelEstimator(
    ApmDataDumper* apm_data_dumper,
    const AudioProcessing::Config::GainController2::AdaptiveDigital& config,
    int adjacent_speech_frames_threshold)
    : apm_data_dumper_(apm_data_dumper),
      initial_speech_level_dbfs_(GetInitialSpeechLevelEstimateDbfs(config)),
      adjacent_speech_frames_threshold_(adjacent_speech_frames_threshold),
      level_dbfs_(initial_speech_level_dbfs_),

      is_confident_(false) {
  RTC_DCHECK(apm_data_dumper_);
  RTC_DCHECK_GE(adjacent_speech_frames_threshold_, 1);
  Reset();
}

void SpeechLevelEstimator::Update(float rms_dbfs,
                                  float peak_dbfs,
                                  float speech_probability) {
  RTC_DCHECK_GT(rms_dbfs, -150.0f);
  RTC_DCHECK_LT(rms_dbfs, 50.0f);
  RTC_DCHECK_GT(peak_dbfs, -150.0f);
  RTC_DCHECK_LT(peak_dbfs, 50.0f);
  RTC_DCHECK_GE(speech_probability, 0.0f);
  RTC_DCHECK_LE(speech_probability, 1.0f);
  if (speech_probability < kVadConfidenceThreshold) {

    if (adjacent_speech_frames_threshold_ > 1) {

      if (num_adjacent_speech_frames_ >= adjacent_speech_frames_threshold_) {

        reliable_state_ = preliminary_state_;
      } else if (num_adjacent_speech_frames_ > 0) {

        preliminary_state_ = reliable_state_;
      }
    }
    num_adjacent_speech_frames_ = 0;
  } else {

    num_adjacent_speech_frames_++;

    RTC_DCHECK_GE(preliminary_state_.time_to_confidence_ms, 0);
    const bool buffer_is_full = preliminary_state_.time_to_confidence_ms == 0;
    if (!buffer_is_full) {
      preliminary_state_.time_to_confidence_ms -= kFrameDurationMs;
    }

    RTC_DCHECK_GT(speech_probability, 0.0f);
    const float leak_factor = buffer_is_full ? kLevelEstimatorLeakFactor : 1.0f;
    preliminary_state_.level_dbfs.numerator =
        preliminary_state_.level_dbfs.numerator * leak_factor +
        rms_dbfs * speech_probability;
    preliminary_state_.level_dbfs.denominator =
        preliminary_state_.level_dbfs.denominator * leak_factor +
        speech_probability;

    const float level_dbfs = preliminary_state_.level_dbfs.GetRatio();

    if (num_adjacent_speech_frames_ >= adjacent_speech_frames_threshold_) {

      level_dbfs_ = ClampLevelEstimateDbfs(level_dbfs);
    }
  }
  UpdateIsConfident();
  DumpDebugData();
}

void SpeechLevelEstimator::UpdateIsConfident() {
  if (adjacent_speech_frames_threshold_ == 1) {

    is_confident_ = preliminary_state_.time_to_confidence_ms == 0;
    return;
  }

  RTC_DCHECK(reliable_state_.time_to_confidence_ms != 0 ||
             preliminary_state_.time_to_confidence_ms == 0);

  is_confident_ =
      reliable_state_.time_to_confidence_ms == 0 ||
      (num_adjacent_speech_frames_ >= adjacent_speech_frames_threshold_ &&
       preliminary_state_.time_to_confidence_ms == 0);
}

void SpeechLevelEstimator::Reset() {
  ResetLevelEstimatorState(preliminary_state_);
  ResetLevelEstimatorState(reliable_state_);
  level_dbfs_ = initial_speech_level_dbfs_;
  num_adjacent_speech_frames_ = 0;
}

void SpeechLevelEstimator::ResetLevelEstimatorState(
    LevelEstimatorState& state) const {
  state.time_to_confidence_ms = kLevelEstimatorTimeToConfidenceMs;
  state.level_dbfs.numerator = initial_speech_level_dbfs_;
  state.level_dbfs.denominator = 1.0f;
}

void SpeechLevelEstimator::DumpDebugData() const {
  if (!apm_data_dumper_)
    return;
  apm_data_dumper_->DumpRaw("agc2_speech_level_dbfs", level_dbfs_);
  apm_data_dumper_->DumpRaw("agc2_speech_level_is_confident", is_confident_);
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_level_estimator_num_adjacent_speech_frames",
      num_adjacent_speech_frames_);
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_level_estimator_preliminary_level_estimate_num",
      preliminary_state_.level_dbfs.numerator);
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_level_estimator_preliminary_level_estimate_den",
      preliminary_state_.level_dbfs.denominator);
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_level_estimator_preliminary_time_to_confidence_ms",
      preliminary_state_.time_to_confidence_ms);
  apm_data_dumper_->DumpRaw(
      "agc2_adaptive_level_estimator_reliable_time_to_confidence_ms",
      reliable_state_.time_to_confidence_ms);
}

}
