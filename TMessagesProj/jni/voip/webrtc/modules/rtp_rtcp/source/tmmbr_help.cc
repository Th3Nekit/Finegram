/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/tmmbr_help.h"

#include <stddef.h>

#include <limits>

#include "absl/algorithm/container.h"
#include "rtc_base/checks.h"

namespace webrtc {
std::vector<rtcp::TmmbItem> TMMBRHelp::FindBoundingSet(
    std::vector<rtcp::TmmbItem> candidates) {

  for (auto it = candidates.begin(); it != candidates.end();) {
    if (!it->bitrate_bps())
      it = candidates.erase(it);
    else
      ++it;
  }

  if (candidates.size() <= 1)
    return candidates;

  size_t num_candidates = candidates.size();

  absl::c_sort(candidates,
               [](const rtcp::TmmbItem& lhs, const rtcp::TmmbItem& rhs) {
                 return lhs.packet_overhead() < rhs.packet_overhead();
               });

  for (auto it = candidates.begin(); it != candidates.end();) {
    RTC_DCHECK(it->bitrate_bps());
    auto current_min = it;
    auto next_it = it + 1;

    while (next_it != candidates.end() &&
           next_it->packet_overhead() == current_min->packet_overhead()) {
      if (next_it->bitrate_bps() < current_min->bitrate_bps()) {
        current_min->set_bitrate_bps(0);
        current_min = next_it;
      } else {
        next_it->set_bitrate_bps(0);
      }
      ++next_it;
      --num_candidates;
    }
    it = next_it;
  }

  auto min_bitrate_it = candidates.end();
  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    if (it->bitrate_bps()) {
      min_bitrate_it = it;
      break;
    }
  }

  for (auto it = min_bitrate_it; it != candidates.end(); ++it) {
    if (it->bitrate_bps() &&
        it->bitrate_bps() <= min_bitrate_it->bitrate_bps()) {

      min_bitrate_it = it;
    }
  }

  std::vector<rtcp::TmmbItem> bounding_set;
  bounding_set.reserve(num_candidates);
  std::vector<float> intersection(num_candidates);
  std::vector<float> max_packet_rate(num_candidates);

  bounding_set.push_back(*min_bitrate_it);
  intersection[0] = 0;

  uint16_t packet_overhead = bounding_set.back().packet_overhead();
  if (packet_overhead == 0) {

    max_packet_rate[0] = std::numeric_limits<float>::max();
  } else {
    max_packet_rate[0] =
        bounding_set.back().bitrate_bps() / static_cast<float>(packet_overhead);
  }

  min_bitrate_it->set_bitrate_bps(0);
  --num_candidates;

  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    if (it->bitrate_bps() &&
        it->packet_overhead() < bounding_set.front().packet_overhead()) {
      it->set_bitrate_bps(0);
      --num_candidates;
    }
  }

  bool get_new_candidate = true;
  rtcp::TmmbItem cur_candidate;
  while (num_candidates > 0) {
    if (get_new_candidate) {

      for (auto it = candidates.begin(); it != candidates.end(); ++it) {
        if (it->bitrate_bps()) {
          cur_candidate = *it;
          it->set_bitrate_bps(0);
          break;
        }
      }
    }

    RTC_DCHECK_NE(cur_candidate.packet_overhead(),
                  bounding_set.back().packet_overhead());
    float packet_rate = static_cast<float>(cur_candidate.bitrate_bps() -
                                           bounding_set.back().bitrate_bps()) /
                        (cur_candidate.packet_overhead() -
                         bounding_set.back().packet_overhead());

    if (packet_rate <= intersection[bounding_set.size() - 1]) {

      bounding_set.pop_back();
      get_new_candidate = false;
    } else {

      if (packet_rate < max_packet_rate[bounding_set.size() - 1]) {
        bounding_set.push_back(cur_candidate);
        intersection[bounding_set.size() - 1] = packet_rate;
        uint16_t packet_overhead = bounding_set.back().packet_overhead();
        RTC_DCHECK_NE(packet_overhead, 0);
        max_packet_rate[bounding_set.size() - 1] =
            bounding_set.back().bitrate_bps() /
            static_cast<float>(packet_overhead);
      }
      --num_candidates;
      get_new_candidate = true;
    }

  }
  RTC_DCHECK(!bounding_set.empty());
  return bounding_set;
}

bool TMMBRHelp::IsOwner(const std::vector<rtcp::TmmbItem>& bounding,
                        uint32_t ssrc) {
  for (const rtcp::TmmbItem& item : bounding) {
    if (item.ssrc() == ssrc) {
      return true;
    }
  }
  return false;
}

uint64_t TMMBRHelp::CalcMinBitrateBps(
    const std::vector<rtcp::TmmbItem>& candidates) {
  RTC_DCHECK(!candidates.empty());
  uint64_t min_bitrate_bps = std::numeric_limits<uint64_t>::max();
  for (const rtcp::TmmbItem& item : candidates)
    if (item.bitrate_bps() < min_bitrate_bps)
      min_bitrate_bps = item.bitrate_bps();
  return min_bitrate_bps;
}
}
