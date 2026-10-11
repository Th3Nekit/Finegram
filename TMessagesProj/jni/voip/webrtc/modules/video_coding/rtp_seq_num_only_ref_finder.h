/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_RTP_SEQ_NUM_ONLY_REF_FINDER_H_
#define MODULES_VIDEO_CODING_RTP_SEQ_NUM_ONLY_REF_FINDER_H_

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "modules/rtp_rtcp/source/frame_object.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {

class RtpSeqNumOnlyRefFinder {
 public:
  RtpSeqNumOnlyRefFinder() = default;

  RtpFrameReferenceFinder::ReturnVector ManageFrame(
      std::unique_ptr<RtpFrameObject> frame);
  RtpFrameReferenceFinder::ReturnVector PaddingReceived(uint16_t seq_num);
  void ClearTo(uint16_t seq_num);

 private:
  static constexpr int kMaxStashedFrames = 100;
  static constexpr int kMaxPaddingAge = 100;

  enum FrameDecision { kStash, kHandOff, kDrop };

  FrameDecision ManageFrameInternal(RtpFrameObject* frame);
  void RetryStashedFrames(RtpFrameReferenceFinder::ReturnVector& res);
  void UpdateLastPictureIdWithPadding(uint16_t seq_num);

  std::map<uint16_t,
           std::pair<uint16_t, uint16_t>,
           DescendingSeqNumComp<uint16_t>>
      last_seq_num_gop_;

  std::set<uint16_t, DescendingSeqNumComp<uint16_t>> stashed_padding_;

  std::deque<std::unique_ptr<RtpFrameObject>> stashed_frames_;

  SeqNumUnwrapper<uint16_t> rtp_seq_num_unwrapper_;
};

}

#endif
