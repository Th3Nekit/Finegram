/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio/remix_resample.h"

#include "api/audio/audio_frame.h"
#include "audio/utility/audio_frame_operations.h"
#include "common_audio/resampler/include/push_resampler.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace voe {

void RemixAndResample(const AudioFrame& src_frame,
                      PushResampler<int16_t>* resampler,
                      AudioFrame* dst_frame) {
  RemixAndResample(src_frame.data(), src_frame.samples_per_channel_,
                   src_frame.num_channels_, src_frame.sample_rate_hz_,
                   resampler, dst_frame);
  dst_frame->timestamp_ = src_frame.timestamp_;
  dst_frame->elapsed_time_ms_ = src_frame.elapsed_time_ms_;
  dst_frame->ntp_time_ms_ = src_frame.ntp_time_ms_;
  dst_frame->packet_infos_ = src_frame.packet_infos_;
}

void RemixAndResample(const int16_t* src_data,
                      size_t samples_per_channel,
                      size_t num_channels,
                      int sample_rate_hz,
                      PushResampler<int16_t>* resampler,
                      AudioFrame* dst_frame) {
  const int16_t* audio_ptr = src_data;
  size_t audio_ptr_num_channels = num_channels;
  int16_t downmixed_audio[AudioFrame::kMaxDataSizeSamples];

  if (num_channels > dst_frame->num_channels_) {
    RTC_DCHECK(num_channels == 2 || num_channels == 4)
        << "num_channels: " << num_channels;
    RTC_DCHECK(dst_frame->num_channels_ == 1 || dst_frame->num_channels_ == 2)
        << "dst_frame->num_channels_: " << dst_frame->num_channels_;

    AudioFrameOperations::DownmixChannels(
        src_data, num_channels, samples_per_channel, dst_frame->num_channels_,
        downmixed_audio);
    audio_ptr = downmixed_audio;
    audio_ptr_num_channels = dst_frame->num_channels_;
  }

  if (resampler->InitializeIfNeeded(sample_rate_hz, dst_frame->sample_rate_hz_,
                                    audio_ptr_num_channels) == -1) {
    RTC_FATAL() << "InitializeIfNeeded failed: sample_rate_hz = "
                << sample_rate_hz << ", dst_frame->sample_rate_hz_ = "
                << dst_frame->sample_rate_hz_
                << ", audio_ptr_num_channels = " << audio_ptr_num_channels;
  }

  const size_t src_length = samples_per_channel * audio_ptr_num_channels;
  int out_length =
      resampler->Resample(audio_ptr, src_length, dst_frame->mutable_data(),
                          AudioFrame::kMaxDataSizeSamples);
  if (out_length == -1) {
    RTC_FATAL() << "Resample failed: audio_ptr = " << audio_ptr
                << ", src_length = " << src_length
                << ", dst_frame->mutable_data() = "
                << dst_frame->mutable_data();
  }
  dst_frame->samples_per_channel_ = out_length / audio_ptr_num_channels;

  if (num_channels == 1 && dst_frame->num_channels_ == 2) {

    dst_frame->num_channels_ = 1;
    AudioFrameOperations::UpmixChannels(2, dst_frame);
  }
}

}
}
