/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_AUDIO_FRAME_PROCESSOR_H_
#define API_AUDIO_AUDIO_FRAME_PROCESSOR_H_

#include <functional>
#include <memory>

namespace webrtc {

class AudioFrame;

class AudioFrameProcessor {
 public:
  using OnAudioFrameCallback = std::function<void(std::unique_ptr<AudioFrame>)>;
  virtual ~AudioFrameProcessor() = default;

  virtual void Process(std::unique_ptr<AudioFrame> frame) = 0;

  virtual void SetSink(OnAudioFrameCallback sink_callback) = 0;
};

}

#endif
