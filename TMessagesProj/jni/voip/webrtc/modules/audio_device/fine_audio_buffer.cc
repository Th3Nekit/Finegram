/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_device/fine_audio_buffer.h"

#include <cstdint>
#include <cstring>

#include "api/array_view.h"
#include "modules/audio_device/audio_device_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

FineAudioBuffer::FineAudioBuffer(AudioDeviceBuffer* audio_device_buffer)
    : audio_device_buffer_(audio_device_buffer),
      playout_samples_per_channel_10ms_(rtc::dchecked_cast<size_t>(
          audio_device_buffer->PlayoutSampleRate() * 10 / 1000)),
      record_samples_per_channel_10ms_(rtc::dchecked_cast<size_t>(
          audio_device_buffer->RecordingSampleRate() * 10 / 1000)),
      playout_channels_(audio_device_buffer->PlayoutChannels()),
      record_channels_(audio_device_buffer->RecordingChannels()) {
  RTC_DCHECK(audio_device_buffer_);
  RTC_DLOG(LS_INFO) << __FUNCTION__;
  if (IsReadyForPlayout()) {
    RTC_DLOG(LS_INFO) << "playout_samples_per_channel_10ms: "
                      << playout_samples_per_channel_10ms_;
    RTC_DLOG(LS_INFO) << "playout_channels: " << playout_channels_;
  }
  if (IsReadyForRecord()) {
    RTC_DLOG(LS_INFO) << "record_samples_per_channel_10ms: "
                      << record_samples_per_channel_10ms_;
    RTC_DLOG(LS_INFO) << "record_channels: " << record_channels_;
  }
}

FineAudioBuffer::~FineAudioBuffer() {
  RTC_DLOG(LS_INFO) << __FUNCTION__;
}

void FineAudioBuffer::ResetPlayout() {
  playout_buffer_.Clear();
}

void FineAudioBuffer::ResetRecord() {
  record_buffer_.Clear();
}

bool FineAudioBuffer::IsReadyForPlayout() const {
  return playout_samples_per_channel_10ms_ > 0 && playout_channels_ > 0;
}

bool FineAudioBuffer::IsReadyForRecord() const {
  return record_samples_per_channel_10ms_ > 0 && record_channels_ > 0;
}

void FineAudioBuffer::GetPlayoutData(rtc::ArrayView<int16_t> audio_buffer,
                                     int playout_delay_ms) {
  RTC_DCHECK(IsReadyForPlayout());

  while (playout_buffer_.size() < audio_buffer.size()) {

    if (audio_device_buffer_->RequestPlayoutData(
            playout_samples_per_channel_10ms_) ==
        static_cast<int32_t>(playout_samples_per_channel_10ms_)) {

      const size_t num_elements_10ms =
          playout_channels_ * playout_samples_per_channel_10ms_;
      const size_t written_elements = playout_buffer_.AppendData(
          num_elements_10ms, [&](rtc::ArrayView<int16_t> buf) {
            const size_t samples_per_channel_10ms =
                audio_device_buffer_->GetPlayoutData(buf.data());
            return playout_channels_ * samples_per_channel_10ms;
          });
      RTC_DCHECK_EQ(num_elements_10ms, written_elements);
    } else {

      const size_t num_bytes = audio_buffer.size() * sizeof(int16_t);
      std::memset(audio_buffer.data(), 0, num_bytes);
      return;
    }
  }

  const size_t num_bytes = audio_buffer.size() * sizeof(int16_t);
  memcpy(audio_buffer.data(), playout_buffer_.data(), num_bytes);

  memmove(playout_buffer_.data(), playout_buffer_.data() + audio_buffer.size(),
          (playout_buffer_.size() - audio_buffer.size()) * sizeof(int16_t));
  playout_buffer_.SetSize(playout_buffer_.size() - audio_buffer.size());

  playout_delay_ms_ = playout_delay_ms;
}

void FineAudioBuffer::DeliverRecordedData(
    rtc::ArrayView<const int16_t> audio_buffer,
    int record_delay_ms,
    absl::optional<int64_t> capture_time_ns) {
  RTC_DCHECK(IsReadyForRecord());

  record_buffer_.AppendData(audio_buffer.data(), audio_buffer.size());

  const size_t num_elements_10ms =
      record_channels_ * record_samples_per_channel_10ms_;
  while (record_buffer_.size() >= num_elements_10ms) {
    audio_device_buffer_->SetRecordedBuffer(record_buffer_.data(),
                                            record_samples_per_channel_10ms_,
                                            capture_time_ns);
    audio_device_buffer_->SetVQEData(playout_delay_ms_, record_delay_ms);
    audio_device_buffer_->DeliverRecordedData();
    memmove(record_buffer_.data(), record_buffer_.data() + num_elements_10ms,
            (record_buffer_.size() - num_elements_10ms) * sizeof(int16_t));
    record_buffer_.SetSize(record_buffer_.size() - num_elements_10ms);
  }
}

}
