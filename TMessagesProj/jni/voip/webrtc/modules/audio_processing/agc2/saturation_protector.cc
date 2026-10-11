/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector.h"

#include <memory>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/saturation_protector_buffer.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_minmax.h"

namespace webrtc {
namespace {

constexpr int kPeakEnveloperSuperFrameLengthMs = 400;
constexpr float kMinMarginDb = 12.0f;
constexpr float kMaxMarginDb = 25.0f;
constexpr float kAttack = 0.9988493699365052f;
constexpr float kDecay = 0.9997697679981565f;

struct SaturationProtectorState {
  bool operator==(const SaturationProtectorState& s) const {
    return headroom_db == s.headroom_db &&
           peak_delay_buffer == s.peak_delay_buffer &&
           max_peaks_dbfs == s.max_peaks_dbfs &&
           time_since_push_ms == s.time_since_push_ms;
  }
  inline bool operator!=(const SaturationProtectorState& s) const {
    return !(*this == s);
  }

  float headroom_db;
  SaturationProtectorBuffer peak_delay_buffer;
  float max_peaks_dbfs;
  int time_since_push_ms;
};

void ResetSaturationProtectorState(float initial_headroom_db,
                                   SaturationProtectorState& state) {
  state.headroom_db = initial_headroom_db;
  state.peak_delay_buffer.Reset();
  state.max_peaks_dbfs = kMinLevelDbfs;
  state.time_since_push_ms = 0;
}

void UpdateSaturationProtectorState(float peak_dbfs,
                                    float speech_level_dbfs,
                                    SaturationProtectorState& state) {

  state.max_peaks_dbfs = std::max(state.max_peaks_dbfs, peak_dbfs);
  state.time_since_push_ms += kFrameDurationMs;
  if (rtc::SafeGt(state.time_since_push_ms, kPeakEnveloperSuperFrameLengthMs)) {

    state.peak_delay_buffer.PushBack(state.max_peaks_dbfs);

    state.max_peaks_dbfs = kMinLevelDbfs;
    state.time_since_push_ms = 0;
  }

  const float delayed_peak_dbfs =
      state.peak_delay_buffer.Front().value_or(state.max_peaks_dbfs);
  const float difference_db = delayed_peak_dbfs - speech_level_dbfs;
  if (difference_db > state.headroom_db) {

    state.headroom_db =
        state.headroom_db * kAttack + difference_db * (1.0f - kAttack);
  } else {

    state.headroom_db =
        state.headroom_db * kDecay + difference_db * (1.0f - kDecay);
  }

  state.headroom_db =
      rtc::SafeClamp<float>(state.headroom_db, kMinMarginDb, kMaxMarginDb);
}

class SaturationProtectorImpl : public SaturationProtector {
 public:
  explicit SaturationProtectorImpl(float initial_headroom_db,
                                   int adjacent_speech_frames_threshold,
                                   ApmDataDumper* apm_data_dumper)
      : apm_data_dumper_(apm_data_dumper),
        initial_headroom_db_(initial_headroom_db),
        adjacent_speech_frames_threshold_(adjacent_speech_frames_threshold) {
    Reset();
  }
  SaturationProtectorImpl(const SaturationProtectorImpl&) = delete;
  SaturationProtectorImpl& operator=(const SaturationProtectorImpl&) = delete;
  ~SaturationProtectorImpl() = default;

  float HeadroomDb() override { return headroom_db_; }

  void Analyze(float speech_probability,
               float peak_dbfs,
               float speech_level_dbfs) override {
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

      UpdateSaturationProtectorState(peak_dbfs, speech_level_dbfs,
                                     preliminary_state_);

      if (num_adjacent_speech_frames_ >= adjacent_speech_frames_threshold_) {

        headroom_db_ = preliminary_state_.headroom_db;
      }
    }
    DumpDebugData();
  }

  void Reset() override {
    num_adjacent_speech_frames_ = 0;
    headroom_db_ = initial_headroom_db_;
    ResetSaturationProtectorState(initial_headroom_db_, preliminary_state_);
    ResetSaturationProtectorState(initial_headroom_db_, reliable_state_);
  }

 private:
  void DumpDebugData() {
    apm_data_dumper_->DumpRaw(
        "agc2_saturation_protector_preliminary_max_peak_dbfs",
        preliminary_state_.max_peaks_dbfs);
    apm_data_dumper_->DumpRaw(
        "agc2_saturation_protector_reliable_max_peak_dbfs",
        reliable_state_.max_peaks_dbfs);
  }

  ApmDataDumper* const apm_data_dumper_;
  const float initial_headroom_db_;
  const int adjacent_speech_frames_threshold_;
  int num_adjacent_speech_frames_;
  float headroom_db_;
  SaturationProtectorState preliminary_state_;
  SaturationProtectorState reliable_state_;
};

}

std::unique_ptr<SaturationProtector> CreateSaturationProtector(
    float initial_headroom_db,
    int adjacent_speech_frames_threshold,
    ApmDataDumper* apm_data_dumper) {
  return std::make_unique<SaturationProtectorImpl>(
      initial_headroom_db, adjacent_speech_frames_threshold, apm_data_dumper);
}

}
