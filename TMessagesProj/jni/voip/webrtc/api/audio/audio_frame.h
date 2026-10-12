/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_AUDIO_FRAME_H_
#define API_AUDIO_AUDIO_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#include "api/audio/channel_layout.h"
#include "api/rtp_packet_infos.h"

namespace webrtc {

class AudioFrame {
 public:

  enum : size_t {

    kMaxDataSizeSamples = 7680,
    kMaxDataSizeBytes = kMaxDataSizeSamples * sizeof(int16_t),
  };

  enum VADActivity { kVadActive = 0, kVadPassive = 1, kVadUnknown = 2 };
  enum SpeechType {
    kNormalSpeech = 0,
    kPLC = 1,
    kCNG = 2,
    kPLCCNG = 3,
    kCodecPLC = 5,
    kUndefined = 4
  };

  AudioFrame();

  AudioFrame(const AudioFrame&) = delete;
  AudioFrame& operator=(const AudioFrame&) = delete;

  void Reset();

  void ResetWithoutMuting();

  void UpdateFrame(uint32_t timestamp,
                   const int16_t* data,
                   size_t samples_per_channel,
                   int sample_rate_hz,
                   SpeechType speech_type,
                   VADActivity vad_activity,
                   size_t num_channels = 1);

  void CopyFrom(const AudioFrame& src);

  void UpdateProfileTimeStamp();

  int64_t ElapsedProfileTimeMs() const;

  const int16_t* data() const;
  int16_t* mutable_data();

  void Mute();

  bool muted() const;

  size_t max_16bit_samples() const { return kMaxDataSizeSamples; }
  size_t samples_per_channel() const { return samples_per_channel_; }
  size_t num_channels() const { return num_channels_; }
  ChannelLayout channel_layout() const { return channel_layout_; }
  int sample_rate_hz() const { return sample_rate_hz_; }

  void set_absolute_capture_timestamp_ms(
      int64_t absolute_capture_time_stamp_ms) {
    absolute_capture_timestamp_ms_ = absolute_capture_time_stamp_ms;
  }

  absl::optional<int64_t> absolute_capture_timestamp_ms() const {
    return absolute_capture_timestamp_ms_;
  }

  uint32_t timestamp_ = 0;

  int64_t elapsed_time_ms_ = -1;

  int64_t ntp_time_ms_ = -1;
  size_t samples_per_channel_ = 0;
  int sample_rate_hz_ = 0;
  size_t num_channels_ = 0;
  ChannelLayout channel_layout_ = CHANNEL_LAYOUT_NONE;
  SpeechType speech_type_ = kUndefined;
  VADActivity vad_activity_ = kVadUnknown;

  int64_t profile_timestamp_ms_ = 0;

  RtpPacketInfos packet_infos_;

 private:

  static const int16_t* empty_data();

  int16_t data_[kMaxDataSizeSamples];
  bool muted_ = true;

  absl::optional<int64_t> absolute_capture_timestamp_ms_;
};

}

#endif
