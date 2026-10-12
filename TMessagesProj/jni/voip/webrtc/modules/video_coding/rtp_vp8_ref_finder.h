/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_RTP_VP8_REF_FINDER_H_
#define MODULES_VIDEO_CODING_RTP_VP8_REF_FINDER_H_

#include <deque>
#include <map>
#include <memory>
#include <set>

#include "absl/container/inlined_vector.h"
#include "modules/rtp_rtcp/source/frame_object.h"
#include "modules/video_coding/rtp_frame_reference_finder.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {

class RtpVp8RefFinder {
 public:
  RtpVp8RefFinder() = default;

  RtpFrameReferenceFinder::ReturnVector ManageFrame(
      std::unique_ptr<RtpFrameObject> frame);
  void ClearTo(uint16_t seq_num);

 private:
  static constexpr int kFrameIdLength = 1 << 15;
  static constexpr int kMaxLayerInfo = 50;
  static constexpr int kMaxNotYetReceivedFrames = 100;
  static constexpr int kMaxStashedFrames = 100;
  static constexpr int kMaxTemporalLayers = 5;

  struct UnwrappedTl0Frame {
    int64_t unwrapped_tl0;
    std::unique_ptr<RtpFrameObject> frame;
  };

  enum FrameDecision { kStash, kHandOff, kDrop };

  FrameDecision ManageFrameInternal(RtpFrameObject* frame,
                                    const RTPVideoHeaderVP8& codec_header,
                                    int64_t unwrapped_tl0);
  void RetryStashedFrames(RtpFrameReferenceFinder::ReturnVector& res);
  void UpdateLayerInfoVp8(RtpFrameObject* frame,
                          int64_t unwrapped_tl0,
                          uint8_t temporal_idx);
  void UnwrapPictureIds(RtpFrameObject* frame);

  int last_picture_id_ = -1;

  std::set<uint16_t, DescendingSeqNumComp<uint16_t, kFrameIdLength>>
      not_yet_received_frames_;

  std::deque<UnwrappedTl0Frame> stashed_frames_;

  std::map<int64_t, std::array<int64_t, kMaxTemporalLayers>> layer_info_;

  SeqNumUnwrapper<uint16_t, kFrameIdLength> unwrapper_;

  SeqNumUnwrapper<uint8_t> tl0_unwrapper_;
};

}

#endif
