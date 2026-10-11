/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_
#define MODULES_AUDIO_PROCESSING_ECHO_CONTROL_MOBILE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "api/array_view.h"

namespace webrtc {

class AudioBuffer;

class EchoControlMobileImpl {
 public:
  EchoControlMobileImpl();

  ~EchoControlMobileImpl();

  enum RoutingMode {
    kQuietEarpieceOrHeadset,
    kEarpiece,
    kLoudEarpiece,
    kSpeakerphone,
    kLoudSpeakerphone
  };

  int set_routing_mode(RoutingMode mode);
  RoutingMode routing_mode() const;

  int enable_comfort_noise(bool enable);
  bool is_comfort_noise_enabled() const;

  void ProcessRenderAudio(rtc::ArrayView<const int16_t> packed_render_audio);
  int ProcessCaptureAudio(AudioBuffer* audio, int stream_delay_ms);

  void Initialize(int sample_rate_hz,
                  size_t num_reverse_channels,
                  size_t num_output_channels);

  static void PackRenderAudioBuffer(const AudioBuffer* audio,
                                    size_t num_output_channels,
                                    size_t num_channels,
                                    std::vector<int16_t>* packed_buffer);

  static size_t NumCancellersRequired(size_t num_output_channels,
                                      size_t num_reverse_channels);

 private:
  class Canceller;
  struct StreamProperties;

  int Configure();

  RoutingMode routing_mode_;
  bool comfort_noise_enabled_;

  std::vector<std::unique_ptr<Canceller>> cancellers_;
  std::unique_ptr<StreamProperties> stream_properties_;
  std::vector<std::array<int16_t, 160>> low_pass_reference_;
  bool reference_copied_ = false;
};
}

#endif
