/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_TYPES_H_
#define COMMON_TYPES_H_

#include <stddef.h>

#include <cstdint>

namespace webrtc {

struct FrameCounts {
  FrameCounts() : key_frames(0), delta_frames(0) {}
  int key_frames;
  int delta_frames;
};

class FrameCountObserver {
 public:
  virtual ~FrameCountObserver() {}
  virtual void FrameCountUpdated(const FrameCounts& frame_counts,
                                 uint32_t ssrc) = 0;
};

struct SpatialLayer {
  bool operator==(const SpatialLayer& other) const;
  bool operator!=(const SpatialLayer& other) const { return !(*this == other); }

  unsigned short width;
  unsigned short height;
  float maxFramerate;
  unsigned char numberOfTemporalLayers;
  unsigned int maxBitrate;
  unsigned int targetBitrate;
  unsigned int minBitrate;
  unsigned int qpMax;
  bool active;
};

typedef SpatialLayer SimulcastStream;

struct PlayoutDelay {
  PlayoutDelay(int min_ms, int max_ms) : min_ms(min_ms), max_ms(max_ms) {}
  int min_ms;
  int max_ms;

  static PlayoutDelay Noop() { return PlayoutDelay(-1, -1); }

  bool IsNoop() const { return min_ms == -1 && max_ms == -1; }
  bool operator==(const PlayoutDelay& rhs) const {
    return min_ms == rhs.min_ms && max_ms == rhs.max_ms;
  }
};

}

#endif
