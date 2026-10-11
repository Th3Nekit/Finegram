/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_LOSS_NOTIFICATION_CONTROLLER_H_
#define MODULES_VIDEO_CODING_LOSS_NOTIFICATION_CONTROLLER_H_

#include <stdint.h>

#include <set>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "modules/include/module_common_types.h"
#include "rtc_base/system/no_unique_address.h"

namespace webrtc {

class LossNotificationController {
 public:
  struct FrameDetails {
    bool is_keyframe;
    int64_t frame_id;
    rtc::ArrayView<const int64_t> frame_dependencies;
  };

  LossNotificationController(KeyFrameRequestSender* key_frame_request_sender,
                             LossNotificationSender* loss_notification_sender);
  ~LossNotificationController();

  void OnReceivedPacket(uint16_t rtp_seq_num, const FrameDetails* frame);

  void OnAssembledFrame(uint16_t first_seq_num,
                        int64_t frame_id,
                        bool discardable,
                        rtc::ArrayView<const int64_t> frame_dependencies);

 private:
  void DiscardOldInformation();

  bool AllDependenciesDecodable(
      rtc::ArrayView<const int64_t> frame_dependencies) const;

  void HandleLoss(uint16_t last_received_seq_num, bool decodability_flag);

  KeyFrameRequestSender* const key_frame_request_sender_
      RTC_GUARDED_BY(sequence_checker_);

  LossNotificationSender* const loss_notification_sender_
      RTC_GUARDED_BY(sequence_checker_);

  absl::optional<int64_t> last_received_frame_id_
      RTC_GUARDED_BY(sequence_checker_);

  absl::optional<uint16_t> last_received_seq_num_
      RTC_GUARDED_BY(sequence_checker_);

  bool current_frame_potentially_decodable_ RTC_GUARDED_BY(sequence_checker_);

  struct FrameInfo {
    explicit FrameInfo(uint16_t first_seq_num) : first_seq_num(first_seq_num) {}
    uint16_t first_seq_num;
  };
  absl::optional<FrameInfo> last_decodable_non_discardable_
      RTC_GUARDED_BY(sequence_checker_);

  std::set<int64_t> decodable_frame_ids_ RTC_GUARDED_BY(sequence_checker_);

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_;
};

}

#endif
