/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_CODEC_PAIR_ID_H_
#define API_AUDIO_CODECS_AUDIO_CODEC_PAIR_ID_H_

#include <stdint.h>

#include <utility>

namespace webrtc {

class AudioCodecPairId final {
 public:

  AudioCodecPairId() = delete;
  AudioCodecPairId(const AudioCodecPairId&) = default;
  AudioCodecPairId(AudioCodecPairId&&) = default;
  AudioCodecPairId& operator=(const AudioCodecPairId&) = default;
  AudioCodecPairId& operator=(AudioCodecPairId&&) = default;

  friend void swap(AudioCodecPairId& a, AudioCodecPairId& b) {
    using std::swap;
    swap(a.id_, b.id_);
  }

  static AudioCodecPairId Create();

  friend bool operator==(AudioCodecPairId a, AudioCodecPairId b) {
    return a.id_ == b.id_;
  }
  friend bool operator!=(AudioCodecPairId a, AudioCodecPairId b) {
    return a.id_ != b.id_;
  }

  friend bool operator<(AudioCodecPairId a, AudioCodecPairId b) {
    return a.id_ < b.id_;
  }
  friend bool operator<=(AudioCodecPairId a, AudioCodecPairId b) {
    return a.id_ <= b.id_;
  }
  friend bool operator>=(AudioCodecPairId a, AudioCodecPairId b) {
    return a.id_ >= b.id_;
  }
  friend bool operator>(AudioCodecPairId a, AudioCodecPairId b) {
    return a.id_ > b.id_;
  }

  uint64_t NumericRepresentation() const { return id_; }

 private:
  explicit AudioCodecPairId(uint64_t id) : id_(id) {}

  uint64_t id_;
};

}

#endif
