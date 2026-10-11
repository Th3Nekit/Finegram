/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/active_decode_targets_helper.h"

#include <stdint.h>

#include "api/array_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

std::bitset<32> LastSendOnChain(int frame_diff,
                                rtc::ArrayView<const int> chain_diffs) {
  std::bitset<32> bitmask = 0;
  for (size_t i = 0; i < chain_diffs.size(); ++i) {
    if (frame_diff == chain_diffs[i]) {
      bitmask.set(i);
    }
  }
  return bitmask;
}

std::bitset<32> AllActive(size_t num) {
  RTC_DCHECK_LE(num, 32);
  return (~uint32_t{0}) >> (32 - num);
}

std::bitset<32> ActiveChains(
    rtc::ArrayView<const int> decode_target_protected_by_chain,
    int num_chains,
    std::bitset<32> active_decode_targets) {
  std::bitset<32> active_chains = 0;
  for (size_t dt = 0; dt < decode_target_protected_by_chain.size(); ++dt) {
    if (dt < active_decode_targets.size() && !active_decode_targets[dt]) {
      continue;
    }
    int chain_idx = decode_target_protected_by_chain[dt];
    RTC_DCHECK_LT(chain_idx, num_chains);
    active_chains.set(chain_idx);
  }
  return active_chains;
}

}

void ActiveDecodeTargetsHelper::OnFrame(
    rtc::ArrayView<const int> decode_target_protected_by_chain,
    std::bitset<32> active_decode_targets,
    bool is_keyframe,
    int64_t frame_id,
    rtc::ArrayView<const int> chain_diffs) {
  const int num_chains = chain_diffs.size();
  if (num_chains == 0) {

    if (last_active_decode_targets_ != active_decode_targets &&
        !active_decode_targets.all()) {
      RTC_LOG(LS_WARNING) << "No chains are configured, but some decode "
                             "targets might be inactive. Unsupported.";
    }
    last_active_decode_targets_ = active_decode_targets;
    return;
  }
  const size_t num_decode_targets = decode_target_protected_by_chain.size();
  RTC_DCHECK_GT(num_decode_targets, 0);
  std::bitset<32> all_decode_targets = AllActive(num_decode_targets);

  active_decode_targets &= all_decode_targets;

  if (is_keyframe) {

    last_active_decode_targets_ = all_decode_targets;
    last_active_chains_ = AllActive(num_chains);
    unsent_on_chain_.reset();
  } else {

    unsent_on_chain_ &=
        ~LastSendOnChain(frame_id - last_frame_id_, chain_diffs);
  }

  last_frame_id_ = frame_id;

  if (active_decode_targets == last_active_decode_targets_) {
    return;
  }
  last_active_decode_targets_ = active_decode_targets;

  if (active_decode_targets.none()) {
    RTC_LOG(LS_ERROR) << "It is invalid to produce a frame (" << frame_id
                      << ") while there are no active decode targets";
    return;
  }
  last_active_chains_ = ActiveChains(decode_target_protected_by_chain,
                                     num_chains, active_decode_targets);

  unsent_on_chain_ = last_active_chains_;
  RTC_DCHECK(!unsent_on_chain_.none());
}

}
