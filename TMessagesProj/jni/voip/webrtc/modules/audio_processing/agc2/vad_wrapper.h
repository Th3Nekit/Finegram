/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_AGC2_VAD_WRAPPER_H_
#define MODULES_AUDIO_PROCESSING_AGC2_VAD_WRAPPER_H_

#include <memory>
#include <vector>

#include "api/array_view.h"
#include "common_audio/resampler/include/push_resampler.h"
#include "modules/audio_processing/agc2/cpu_features.h"
#include "modules/audio_processing/include/audio_frame_view.h"

namespace webrtc {

class VoiceActivityDetectorWrapper {
 public:

  class MonoVad {
   public:
    virtual ~MonoVad() = default;

    virtual int SampleRateHz() const = 0;

    virtual void Reset() = 0;

    virtual float Analyze(rtc::ArrayView<const float> frame) = 0;
  };

  VoiceActivityDetectorWrapper(const AvailableCpuFeatures& cpu_features,
                               int sample_rate_hz);

  VoiceActivityDetectorWrapper(int vad_reset_period_ms,
                               const AvailableCpuFeatures& cpu_features,
                               int sample_rate_hz);

  VoiceActivityDetectorWrapper(int vad_reset_period_ms,
                               std::unique_ptr<MonoVad> vad,
                               int sample_rate_hz);

  VoiceActivityDetectorWrapper(const VoiceActivityDetectorWrapper&) = delete;
  VoiceActivityDetectorWrapper& operator=(const VoiceActivityDetectorWrapper&) =
      delete;
  ~VoiceActivityDetectorWrapper();

  void Initialize(int sample_rate_hz);

  float Analyze(AudioFrameView<const float> frame);

 private:
  const int vad_reset_period_frames_;
  int frame_size_;
  int time_to_vad_reset_;
  PushResampler<float> resampler_;
  std::unique_ptr<MonoVad> vad_;
  std::vector<float> resampled_buffer_;
};

}

#endif
