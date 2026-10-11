/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_DEVICE_FINE_AUDIO_BUFFER_H_
#define MODULES_AUDIO_DEVICE_FINE_AUDIO_BUFFER_H_

#include <cstddef>
#include <cstdint>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "rtc_base/buffer.h"

namespace webrtc {

class AudioDeviceBuffer;

class FineAudioBuffer {
 public:

  FineAudioBuffer(AudioDeviceBuffer* audio_device_buffer);
  ~FineAudioBuffer();

  void ResetPlayout();
  void ResetRecord();

  bool IsReadyForPlayout() const;
  bool IsReadyForRecord() const;

  void GetPlayoutData(rtc::ArrayView<int16_t> audio_buffer,
                      int playout_delay_ms);

  void DeliverRecordedData(rtc::ArrayView<const int16_t> audio_buffer,
                           int record_delay_ms) {
    DeliverRecordedData(audio_buffer, record_delay_ms, absl::nullopt);
  }
  void DeliverRecordedData(rtc::ArrayView<const int16_t> audio_buffer,
                           int record_delay_ms,
                           absl::optional<int64_t> capture_time_ns);

 private:

  AudioDeviceBuffer* const audio_device_buffer_;

  const size_t playout_samples_per_channel_10ms_;
  const size_t record_samples_per_channel_10ms_;

  const size_t playout_channels_;
  const size_t record_channels_;

  rtc::BufferT<int16_t> playout_buffer_;

  rtc::BufferT<int16_t> record_buffer_;

  int playout_delay_ms_ = 0;
};

}

#endif
