/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_UTILITY_CHANNEL_MIXER_H_
#define AUDIO_UTILITY_CHANNEL_MIXER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "api/audio/audio_frame.h"
#include "api/audio/channel_layout.h"

namespace webrtc {

class ChannelMixer {
 public:

  static constexpr float kHalfPower = 0.707106781186547524401f;

  ChannelMixer(ChannelLayout input_layout, ChannelLayout output_layout);
  ~ChannelMixer();

  void Transform(AudioFrame* frame);

 private:
  bool IsUpMixing() const { return output_channels_ > input_channels_; }

  const ChannelLayout input_layout_;
  const ChannelLayout output_layout_;

  const size_t input_channels_;
  const size_t output_channels_;

  std::vector<std::vector<float> > matrix_;

  std::unique_ptr<int16_t[]> audio_vector_;

  size_t audio_vector_size_ = 0;

  bool remapping_;

  ChannelMixer(const ChannelMixer& other) = delete;
  ChannelMixer& operator=(const ChannelMixer& other) = delete;
};

}

#endif
