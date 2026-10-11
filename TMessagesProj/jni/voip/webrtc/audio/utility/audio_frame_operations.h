/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_UTILITY_AUDIO_FRAME_OPERATIONS_H_
#define AUDIO_UTILITY_AUDIO_FRAME_OPERATIONS_H_

#include <stddef.h>
#include <stdint.h>

#include "absl/base/attributes.h"
#include "api/audio/audio_frame.h"

namespace webrtc {

class AudioFrameOperations {
 public:

  static void Add(const AudioFrame& frame_to_add, AudioFrame* result_frame);

  ABSL_DEPRECATED("bugs.webrtc.org/8649")
  static int MonoToStereo(AudioFrame* frame);

  ABSL_DEPRECATED("bugs.webrtc.org/8649")
  static int StereoToMono(AudioFrame* frame);

  static void QuadToStereo(const int16_t* src_audio,
                           size_t samples_per_channel,
                           int16_t* dst_audio);

  static int QuadToStereo(AudioFrame* frame);

  static void DownmixChannels(const int16_t* src_audio,
                              size_t src_channels,
                              size_t samples_per_channel,
                              size_t dst_channels,
                              int16_t* dst_audio);

  static void DownmixChannels(size_t dst_channels, AudioFrame* frame);

  static void UpmixChannels(size_t target_number_of_channels,
                            AudioFrame* frame);

  static void SwapStereoChannels(AudioFrame* frame);

  static void Mute(AudioFrame* frame,
                   bool previous_frame_muted,
                   bool current_frame_muted);

  static void Mute(AudioFrame* frame);

  static void ApplyHalfGain(AudioFrame* frame);

  static int Scale(float left, float right, AudioFrame* frame);

  static int ScaleWithSat(float scale, AudioFrame* frame);
};

}

#endif
