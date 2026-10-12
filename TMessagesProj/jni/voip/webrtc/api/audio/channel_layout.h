/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CHANNEL_LAYOUT_H_
#define API_AUDIO_CHANNEL_LAYOUT_H_

namespace webrtc {

enum ChannelLayout {
  CHANNEL_LAYOUT_NONE = 0,
  CHANNEL_LAYOUT_UNSUPPORTED = 1,

  CHANNEL_LAYOUT_MONO = 2,

  CHANNEL_LAYOUT_STEREO = 3,

  CHANNEL_LAYOUT_2_1 = 4,

  CHANNEL_LAYOUT_SURROUND = 5,

  CHANNEL_LAYOUT_4_0 = 6,

  CHANNEL_LAYOUT_2_2 = 7,

  CHANNEL_LAYOUT_QUAD = 8,

  CHANNEL_LAYOUT_5_0 = 9,

  CHANNEL_LAYOUT_5_1 = 10,

  CHANNEL_LAYOUT_5_0_BACK = 11,

  CHANNEL_LAYOUT_5_1_BACK = 12,

  CHANNEL_LAYOUT_7_0 = 13,

  CHANNEL_LAYOUT_7_1 = 14,

  CHANNEL_LAYOUT_7_1_WIDE = 15,

  CHANNEL_LAYOUT_STEREO_DOWNMIX = 16,

  CHANNEL_LAYOUT_2POINT1 = 17,

  CHANNEL_LAYOUT_3_1 = 18,

  CHANNEL_LAYOUT_4_1 = 19,

  CHANNEL_LAYOUT_6_0 = 20,

  CHANNEL_LAYOUT_6_0_FRONT = 21,

  CHANNEL_LAYOUT_HEXAGONAL = 22,

  CHANNEL_LAYOUT_6_1 = 23,

  CHANNEL_LAYOUT_6_1_BACK = 24,

  CHANNEL_LAYOUT_6_1_FRONT = 25,

  CHANNEL_LAYOUT_7_0_FRONT = 26,

  CHANNEL_LAYOUT_7_1_WIDE_BACK = 27,

  CHANNEL_LAYOUT_OCTAGONAL = 28,

  CHANNEL_LAYOUT_DISCRETE = 29,

  CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC = 30,

  CHANNEL_LAYOUT_4_1_QUAD_SIDE = 31,

  CHANNEL_LAYOUT_BITSTREAM = 32,

  CHANNEL_LAYOUT_MAX = CHANNEL_LAYOUT_BITSTREAM
};

enum Channels {
  LEFT = 0,
  RIGHT,
  CENTER,
  LFE,
  BACK_LEFT,
  BACK_RIGHT,
  LEFT_OF_CENTER,
  RIGHT_OF_CENTER,
  BACK_CENTER,
  SIDE_LEFT,
  SIDE_RIGHT,
  CHANNELS_MAX =
      SIDE_RIGHT,
};

constexpr int kMaxConcurrentChannels = 8;

int ChannelOrder(ChannelLayout layout, Channels channel);

int ChannelLayoutToChannelCount(ChannelLayout layout);

ChannelLayout GuessChannelLayout(int channels);

const char* ChannelLayoutToString(ChannelLayout layout);

}

#endif
