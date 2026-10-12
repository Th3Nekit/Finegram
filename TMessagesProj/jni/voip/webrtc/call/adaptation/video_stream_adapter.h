/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_VIDEO_STREAM_ADAPTER_H_
#define CALL_ADAPTATION_VIDEO_STREAM_ADAPTER_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "api/adaptation/resource.h"
#include "api/field_trials_view.h"
#include "api/rtp_parameters.h"
#include "api/video/video_adaptation_counters.h"
#include "call/adaptation/adaptation_constraint.h"
#include "call/adaptation/degradation_preference_provider.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"
#include "video/video_stream_encoder_observer.h"

namespace webrtc {

class VideoSourceRestrictionsListener {
 public:
  virtual ~VideoSourceRestrictionsListener();

  virtual void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      rtc::scoped_refptr<Resource> reason,
      const VideoSourceRestrictions& unfiltered_restrictions) = 0;
};

class VideoStreamAdapter;

extern const int kMinFrameRateFps;

VideoSourceRestrictions FilterRestrictionsByDegradationPreference(
    VideoSourceRestrictions source_restrictions,
    DegradationPreference degradation_preference);

int GetLowerResolutionThan(int pixel_count);
int GetHigherResolutionThan(int pixel_count);

class Adaptation final {
 public:
  enum class Status {

    kValid,

    kLimitReached,

    kAwaitingPreviousAdaptation,

    kInsufficientInput,

    kAdaptationDisabled,

    kRejectedByConstraint,
  };

  static const char* StatusToString(Status status);

  Status status() const;
  const VideoStreamInputState& input_state() const;
  const VideoSourceRestrictions& restrictions() const;
  const VideoAdaptationCounters& counters() const;

 private:
  friend class VideoStreamAdapter;

  Adaptation(int validation_id,
             VideoSourceRestrictions restrictions,
             VideoAdaptationCounters counters,
             VideoStreamInputState input_state);

  Adaptation(int validation_id, Status invalid_status);

  const int validation_id_;
  const Status status_;

  const VideoStreamInputState input_state_;
  const VideoSourceRestrictions restrictions_;
  const VideoAdaptationCounters counters_;
};

class VideoStreamAdapter {
 public:
  VideoStreamAdapter(VideoStreamInputStateProvider* input_state_provider,
                     VideoStreamEncoderObserver* encoder_stats_observer,
                     const FieldTrialsView& field_trials);
  ~VideoStreamAdapter();

  VideoSourceRestrictions source_restrictions() const;
  const VideoAdaptationCounters& adaptation_counters() const;
  void ClearRestrictions();

  void AddRestrictionsListener(
      VideoSourceRestrictionsListener* restrictions_listener);
  void RemoveRestrictionsListener(
      VideoSourceRestrictionsListener* restrictions_listener);
  void AddAdaptationConstraint(AdaptationConstraint* adaptation_constraint);
  void RemoveAdaptationConstraint(AdaptationConstraint* adaptation_constraint);

  void SetDegradationPreference(DegradationPreference degradation_preference);

  Adaptation GetAdaptationUp();
  Adaptation GetAdaptationDown();
  Adaptation GetAdaptationTo(const VideoAdaptationCounters& counters,
                             const VideoSourceRestrictions& restrictions);

  Adaptation GetAdaptDownResolution();

  void ApplyAdaptation(const Adaptation& adaptation,
                       rtc::scoped_refptr<Resource> resource);

  struct RestrictionsWithCounters {
    VideoSourceRestrictions restrictions;
    VideoAdaptationCounters counters;
  };

  static absl::optional<uint32_t> GetSingleActiveLayerPixels(
      const VideoCodec& codec);

 private:
  void BroadcastVideoRestrictionsUpdate(
      const VideoStreamInputState& input_state,
      const rtc::scoped_refptr<Resource>& resource);

  bool HasSufficientInputForAdaptation(const VideoStreamInputState& input_state)
      const RTC_RUN_ON(&sequence_checker_);

  using RestrictionsOrState =
      absl::variant<RestrictionsWithCounters, Adaptation::Status>;
  RestrictionsOrState GetAdaptationUpStep(
      const VideoStreamInputState& input_state) const
      RTC_RUN_ON(&sequence_checker_);
  RestrictionsOrState GetAdaptationDownStep(
      const VideoStreamInputState& input_state,
      const RestrictionsWithCounters& current_restrictions) const
      RTC_RUN_ON(&sequence_checker_);
  RestrictionsOrState GetAdaptDownResolutionStepForBalanced(
      const VideoStreamInputState& input_state) const
      RTC_RUN_ON(&sequence_checker_);
  RestrictionsOrState AdaptIfFpsDiffInsufficient(
      const VideoStreamInputState& input_state,
      const RestrictionsWithCounters& restrictions) const
      RTC_RUN_ON(&sequence_checker_);

  Adaptation GetAdaptationUp(const VideoStreamInputState& input_state) const
      RTC_RUN_ON(&sequence_checker_);
  Adaptation GetAdaptationDown(const VideoStreamInputState& input_state) const
      RTC_RUN_ON(&sequence_checker_);

  static RestrictionsOrState DecreaseResolution(
      const VideoStreamInputState& input_state,
      const RestrictionsWithCounters& current_restrictions);
  static RestrictionsOrState IncreaseResolution(
      const VideoStreamInputState& input_state,
      const RestrictionsWithCounters& current_restrictions);

  RestrictionsOrState DecreaseFramerate(
      const VideoStreamInputState& input_state,
      const RestrictionsWithCounters& current_restrictions) const
      RTC_RUN_ON(&sequence_checker_);
  RestrictionsOrState IncreaseFramerate(
      const VideoStreamInputState& input_state,
      const RestrictionsWithCounters& current_restrictions) const
      RTC_RUN_ON(&sequence_checker_);

  struct RestrictionsOrStateVisitor;
  Adaptation RestrictionsOrStateToAdaptation(
      RestrictionsOrState step_or_state,
      const VideoStreamInputState& input_state) const
      RTC_RUN_ON(&sequence_checker_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_
      RTC_GUARDED_BY(&sequence_checker_);

  VideoStreamInputStateProvider* input_state_provider_;

  VideoStreamEncoderObserver* const encoder_stats_observer_;

  const BalancedDegradationSettings balanced_settings_;

  int adaptation_validation_id_ RTC_GUARDED_BY(&sequence_checker_);

  DegradationPreference degradation_preference_
      RTC_GUARDED_BY(&sequence_checker_);

  struct AwaitingFrameSizeChange {
    AwaitingFrameSizeChange(bool pixels_increased, int frame_size);
    const bool pixels_increased;
    const int frame_size_pixels;
  };
  absl::optional<AwaitingFrameSizeChange> awaiting_frame_size_change_
      RTC_GUARDED_BY(&sequence_checker_);

  VideoSourceRestrictions last_video_source_restrictions_
      RTC_GUARDED_BY(&sequence_checker_);
  VideoSourceRestrictions last_filtered_restrictions_
      RTC_GUARDED_BY(&sequence_checker_);

  std::vector<VideoSourceRestrictionsListener*> restrictions_listeners_
      RTC_GUARDED_BY(&sequence_checker_);
  std::vector<AdaptationConstraint*> adaptation_constraints_
      RTC_GUARDED_BY(&sequence_checker_);

  RestrictionsWithCounters current_restrictions_
      RTC_GUARDED_BY(&sequence_checker_);
};

}

#endif
