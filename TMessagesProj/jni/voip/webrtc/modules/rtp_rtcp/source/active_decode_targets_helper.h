/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_ACTIVE_DECODE_TARGETS_HELPER_H_
#define MODULES_RTP_RTCP_SOURCE_ACTIVE_DECODE_TARGETS_HELPER_H_

#include <stdint.h>

#include <bitset>

#include "absl/types/optional.h"
#include "api/array_view.h"

namespace webrtc {

class ActiveDecodeTargetsHelper {
 public:
  ActiveDecodeTargetsHelper() = default;
  ActiveDecodeTargetsHelper(const ActiveDecodeTargetsHelper&) = delete;
  ActiveDecodeTargetsHelper& operator=(const ActiveDecodeTargetsHelper&) =
      delete;
  ~ActiveDecodeTargetsHelper() = default;

  void OnFrame(rtc::ArrayView<const int> decode_target_protected_by_chain,
               std::bitset<32> active_decode_targets,
               bool is_keyframe,
               int64_t frame_id,
               rtc::ArrayView<const int> chain_diffs);

  absl::optional<uint32_t> ActiveDecodeTargetsBitmask() const {
    if (unsent_on_chain_.none())
      return absl::nullopt;
    return last_active_decode_targets_.to_ulong();
  }

  std::bitset<32> ActiveChainsBitmask() const { return last_active_chains_; }

 private:

  std::bitset<32> unsent_on_chain_ = 0;
  std::bitset<32> last_active_decode_targets_ = 0;
  std::bitset<32> last_active_chains_ = 0;
  int64_t last_frame_id_ = 0;
};

}

#endif
