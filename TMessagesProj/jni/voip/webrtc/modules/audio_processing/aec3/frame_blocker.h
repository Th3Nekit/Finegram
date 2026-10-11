/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AEC3_FRAME_BLOCKER_H_
#define MODULES_AUDIO_PROCESSING_AEC3_FRAME_BLOCKER_H_

#include <stddef.h>

#include <vector>

#include "api/array_view.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/block.h"

namespace webrtc {

class FrameBlocker {
 public:
  FrameBlocker(size_t num_bands, size_t num_channels);
  ~FrameBlocker();
  FrameBlocker(const FrameBlocker&) = delete;
  FrameBlocker& operator=(const FrameBlocker&) = delete;

  void InsertSubFrameAndExtractBlock(
      const std::vector<std::vector<rtc::ArrayView<float>>>& sub_frame,
      Block* block);

  bool IsBlockAvailable() const;

  void ExtractBlock(Block* block);

 private:
  const size_t num_bands_;
  const size_t num_channels_;
  std::vector<std::vector<std::vector<float>>> buffer_;
};
}

#endif
