/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AUDIO_UTILITY_CHANNEL_MIXING_MATRIX_H_
#define AUDIO_UTILITY_CHANNEL_MIXING_MATRIX_H_

#include <vector>

#include "api/audio/channel_layout.h"

namespace webrtc {

class ChannelMixingMatrix {
 public:
  ChannelMixingMatrix(ChannelLayout input_layout,
                      int input_channels,
                      ChannelLayout output_layout,
                      int output_channels);

  ~ChannelMixingMatrix();

  bool CreateTransformationMatrix(std::vector<std::vector<float>>* matrix);

 private:
  const bool use_voip_channel_mapping_adjustments_;

  std::vector<std::vector<float>>* matrix_;

  ChannelLayout input_layout_;
  int input_channels_;
  ChannelLayout output_layout_;
  int output_channels_;

  std::vector<Channels> unaccounted_inputs_;

  void AccountFor(Channels ch);
  bool IsUnaccounted(Channels ch) const;

  bool HasInputChannel(Channels ch) const;
  bool HasOutputChannel(Channels ch) const;

  void Mix(Channels input_ch, Channels output_ch, float scale);
  void MixWithoutAccounting(Channels input_ch, Channels output_ch, float scale);

  ChannelMixingMatrix(const ChannelMixingMatrix& other) = delete;
  ChannelMixingMatrix& operator=(const ChannelMixingMatrix& other) = delete;
};

}

#endif
