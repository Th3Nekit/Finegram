/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_DECODER_MULTI_CHANNEL_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_DECODER_MULTI_CHANNEL_OPUS_CONFIG_H_

#include <vector>

#include "api/audio_codecs/audio_decoder.h"

namespace webrtc {
struct AudioDecoderMultiChannelOpusConfig {

  int num_channels;

  int num_streams;

  int coupled_streams;

  std::vector<unsigned char> channel_mapping;

  bool IsOk() const {
    if (num_channels < 1 || num_channels > AudioDecoder::kMaxNumberOfChannels ||
        num_streams < 0 || coupled_streams < 0) {
      return false;
    }
    if (num_streams < coupled_streams) {
      return false;
    }
    if (channel_mapping.size() != static_cast<size_t>(num_channels)) {
      return false;
    }

    const int max_coded_channel = num_streams + coupled_streams;
    for (const auto& x : channel_mapping) {

      if (x >= max_coded_channel && x != 255) {
        return false;
      }
    }

    if (num_channels > 255 || max_coded_channel >= 255) {
      return false;
    }
    return true;
  }
};

}

#endif
