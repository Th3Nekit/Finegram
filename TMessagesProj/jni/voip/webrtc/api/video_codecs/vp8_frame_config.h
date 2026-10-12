/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VP8_FRAME_CONFIG_H_
#define API_VIDEO_CODECS_VP8_FRAME_CONFIG_H_

#include <stdint.h>

namespace webrtc {

struct Vp8FrameConfig {
  static Vp8FrameConfig GetIntraFrameConfig() {
    Vp8FrameConfig frame_config = Vp8FrameConfig(
        BufferFlags::kUpdate, BufferFlags::kUpdate, BufferFlags::kUpdate);
    frame_config.packetizer_temporal_idx = 0;
    return frame_config;
  }

  enum BufferFlags : int {
    kNone = 0,
    kReference = 1,
    kUpdate = 2,
    kReferenceAndUpdate = kReference | kUpdate,
  };

  enum FreezeEntropy { kFreezeEntropy };

  enum class Vp8BufferReference : uint8_t {
    kNone = 0,
    kLast = 1,
    kGolden = 2,
    kAltref = 4
  };

  Vp8FrameConfig();

  Vp8FrameConfig(BufferFlags last, BufferFlags golden, BufferFlags arf);
  Vp8FrameConfig(BufferFlags last,
                 BufferFlags golden,
                 BufferFlags arf,
                 FreezeEntropy);

  enum class Buffer : int { kLast = 0, kGolden = 1, kArf = 2, kCount };

  bool References(Buffer buffer) const;

  bool Updates(Buffer buffer) const;

  bool IntraFrame() const {

    return last_buffer_flags == kUpdate && golden_buffer_flags == kUpdate &&
           arf_buffer_flags == kUpdate;
  }

  bool drop_frame;
  BufferFlags last_buffer_flags;
  BufferFlags golden_buffer_flags;
  BufferFlags arf_buffer_flags;

  int encoder_layer_id;

  int packetizer_temporal_idx;

  bool layer_sync;

  bool freeze_entropy;

  Vp8BufferReference first_reference;
  Vp8BufferReference second_reference;

  bool retransmission_allowed;

 private:
  Vp8FrameConfig(BufferFlags last,
                 BufferFlags golden,
                 BufferFlags arf,
                 bool freeze_entropy);
};

}

#endif
