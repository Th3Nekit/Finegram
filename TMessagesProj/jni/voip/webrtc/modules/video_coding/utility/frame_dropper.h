/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_FRAME_DROPPER_H_
#define MODULES_VIDEO_CODING_UTILITY_FRAME_DROPPER_H_

#include <stddef.h>
#include <stdint.h>

#include "rtc_base/numerics/exp_filter.h"

namespace webrtc {

class FrameDropper {
 public:
  FrameDropper();
  ~FrameDropper();

  void Reset();

  void Enable(bool enable);

  bool DropFrame();

  void Fill(size_t framesize_bytes, bool delta_frame);

  void Leak(uint32_t input_framerate);

  void SetRates(float bitrate, float incoming_frame_rate);

 private:
  void UpdateRatio();
  void CapAccumulator();

  rtc::ExpFilter key_frame_ratio_;
  rtc::ExpFilter delta_frame_size_avg_kbits_;

  float large_frame_accumulation_spread_;

  int large_frame_accumulation_count_;

  float large_frame_accumulation_chunk_size_;

  float accumulator_;
  float accumulator_max_;
  float target_bitrate_;
  bool drop_next_;
  rtc::ExpFilter drop_ratio_;
  int drop_count_;
  float incoming_frame_rate_;
  bool was_below_max_;
  bool enabled_;
  const float max_drop_duration_secs_;
};

}

#endif
