/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/loss_notification_controller.h"

#include <stdint.h>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/sequence_number_util.h"

namespace webrtc {
namespace {

template <typename Container>
void PareDown(Container* container,
              size_t max_allowed_size,
              size_t target_size) {
  if (container->size() > max_allowed_size) {
    const size_t entries_to_delete = container->size() - target_size;
    auto erase_to = container->begin();
    std::advance(erase_to, entries_to_delete);
    container->erase(container->begin(), erase_to);
    RTC_DCHECK_EQ(container->size(), target_size);
  }
}
}

LossNotificationController::LossNotificationController(
    KeyFrameRequestSender* key_frame_request_sender,
    LossNotificationSender* loss_notification_sender)
    : key_frame_request_sender_(key_frame_request_sender),
      loss_notification_sender_(loss_notification_sender),
      current_frame_potentially_decodable_(true) {
  RTC_DCHECK(key_frame_request_sender_);
  RTC_DCHECK(loss_notification_sender_);
}

LossNotificationController::~LossNotificationController() = default;

void LossNotificationController::OnReceivedPacket(
    uint16_t rtp_seq_num,
    const LossNotificationController::FrameDetails* frame) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (last_received_seq_num_ &&
      !AheadOf(rtp_seq_num, *last_received_seq_num_)) {
    return;
  }

  DiscardOldInformation();

  const bool seq_num_gap =
      last_received_seq_num_ &&
      rtp_seq_num != static_cast<uint16_t>(*last_received_seq_num_ + 1u);

  last_received_seq_num_ = rtp_seq_num;

  if (frame != nullptr) {

    if (last_received_frame_id_.has_value() &&
        frame->frame_id <= last_received_frame_id_.value()) {
      RTC_LOG(LS_WARNING) << "Repeated or reordered frame ID ("
                          << frame->frame_id << ").";
      return;
    }

    last_received_frame_id_ = frame->frame_id;

    if (frame->is_keyframe) {

      decodable_frame_ids_.clear();
      current_frame_potentially_decodable_ = true;
    } else {
      const bool all_dependencies_decodable =
          AllDependenciesDecodable(frame->frame_dependencies);
      current_frame_potentially_decodable_ = all_dependencies_decodable;
      if (seq_num_gap || !current_frame_potentially_decodable_) {
        HandleLoss(rtp_seq_num, current_frame_potentially_decodable_);
      }
    }
  } else if (seq_num_gap || !current_frame_potentially_decodable_) {
    current_frame_potentially_decodable_ = false;

    HandleLoss(rtp_seq_num, false);
  }
}

void LossNotificationController::OnAssembledFrame(
    uint16_t first_seq_num,
    int64_t frame_id,
    bool discardable,
    rtc::ArrayView<const int64_t> frame_dependencies) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  DiscardOldInformation();

  if (discardable) {
    return;
  }

  if (!AllDependenciesDecodable(frame_dependencies)) {
    return;
  }

  last_decodable_non_discardable_.emplace(first_seq_num);
  const auto it = decodable_frame_ids_.insert(frame_id);
  RTC_DCHECK(it.second);
}

void LossNotificationController::DiscardOldInformation() {
  constexpr size_t kExpectedKeyFrameIntervalFrames = 3000;
  constexpr size_t kMaxSize = 2 * kExpectedKeyFrameIntervalFrames;
  constexpr size_t kTargetSize = kExpectedKeyFrameIntervalFrames;
  PareDown(&decodable_frame_ids_, kMaxSize, kTargetSize);
}

bool LossNotificationController::AllDependenciesDecodable(
    rtc::ArrayView<const int64_t> frame_dependencies) const {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  for (int64_t ref_frame_id : frame_dependencies) {
    const auto ref_frame_it = decodable_frame_ids_.find(ref_frame_id);
    if (ref_frame_it == decodable_frame_ids_.end()) {

      return false;
    }
  }

  return true;
}

void LossNotificationController::HandleLoss(uint16_t last_received_seq_num,
                                            bool decodability_flag) {
  RTC_DCHECK_RUN_ON(&sequence_checker_);

  if (last_decodable_non_discardable_) {
    RTC_DCHECK(AheadOf(last_received_seq_num,
                       last_decodable_non_discardable_->first_seq_num));
    loss_notification_sender_->SendLossNotification(
        last_decodable_non_discardable_->first_seq_num, last_received_seq_num,
        decodability_flag,                       true);
  } else {
    key_frame_request_sender_->RequestKeyFrame();
  }
}
}
