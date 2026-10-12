/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_
#define MODULES_VIDEO_CODING_RTP_FRAME_REFERENCE_FINDER_H_

#include <memory>

#include "modules/rtp_rtcp/source/frame_object.h"

namespace webrtc {
namespace internal {
class RtpFrameReferenceFinderImpl;
}

class RtpFrameReferenceFinder {
 public:
  using ReturnVector = absl::InlinedVector<std::unique_ptr<RtpFrameObject>, 3>;

  RtpFrameReferenceFinder();
  explicit RtpFrameReferenceFinder(int64_t picture_id_offset);
  ~RtpFrameReferenceFinder();

  ReturnVector ManageFrame(std::unique_ptr<RtpFrameObject> frame);

  ReturnVector PaddingReceived(uint16_t seq_num);

  void ClearTo(uint16_t seq_num);

 private:
  void AddPictureIdOffset(ReturnVector& frames);

  int cleared_to_seq_num_ = -1;
  const int64_t picture_id_offset_;
  std::unique_ptr<internal::RtpFrameReferenceFinderImpl> impl_;
};

}

#endif
