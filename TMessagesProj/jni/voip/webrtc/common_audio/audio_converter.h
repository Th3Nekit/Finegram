/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef COMMON_AUDIO_AUDIO_CONVERTER_H_
#define COMMON_AUDIO_AUDIO_CONVERTER_H_

#include <stddef.h>

#include <memory>

namespace webrtc {

class AudioConverter {
 public:

  static std::unique_ptr<AudioConverter> Create(size_t src_channels,
                                                size_t src_frames,
                                                size_t dst_channels,
                                                size_t dst_frames);
  virtual ~AudioConverter() {}

  AudioConverter(const AudioConverter&) = delete;
  AudioConverter& operator=(const AudioConverter&) = delete;

  virtual void Convert(const float* const* src,
                       size_t src_size,
                       float* const* dst,
                       size_t dst_capacity) = 0;

  size_t src_channels() const { return src_channels_; }
  size_t src_frames() const { return src_frames_; }
  size_t dst_channels() const { return dst_channels_; }
  size_t dst_frames() const { return dst_frames_; }

 protected:
  AudioConverter();
  AudioConverter(size_t src_channels,
                 size_t src_frames,
                 size_t dst_channels,
                 size_t dst_frames);

  void CheckSizes(size_t src_size, size_t dst_capacity) const;

 private:
  const size_t src_channels_;
  const size_t src_frames_;
  const size_t dst_channels_;
  const size_t dst_frames_;
};

}

#endif
